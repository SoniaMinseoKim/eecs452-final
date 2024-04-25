extern crate nalgebra as na;
use na::{Vector2, Vector3, Matrix2, Matrix3, Matrix3x4, Matrix2x3};
use std::collections::HashMap;
use tokio::task::JoinHandle;
use tokio::sync::{mpsc, mpsc::{UnboundedSender, UnboundedReceiver}};
use npyz;
use crate::config;

use crate::tag_detector::detector::TagID;

// Assuming the camera_matrix is a 3x4 matrix and x is a 3x1 vector
fn motion_model(x: &Vector3<f64>, dt: u32) -> Vector3<f64> {
  *x
}

fn motion_jacobian(x: &Vector3<f64>) -> Matrix3<f64> {
  Matrix3::identity()
}

fn measurement_model(x: &Vector3<f64>, camera_matrix: &Matrix3x4<f64>) -> Vector2<f64> {
  camera_matrix.fixed_slice::<2,3>(0, 0) * x + camera_matrix.fixed_slice::<2,1>(0, 3)
}
fn measurement_jacobian(camera_matrix: &Matrix3x4<f64>) -> Matrix2x3<f64> {
  Matrix2x3::from(camera_matrix.fixed_slice::<2,3>(0, 0))
}

pub struct EKF {
  r: Matrix2<f64>,
  q: Matrix3<f64>,
  pub x: Vector3<f64>,
  cov: Matrix3<f64>,
  cov_init: Matrix3<f64>,
  most_recent_timestep: u32,
  dt: u32
}

impl EKF {
  pub fn new(meas_cov: Matrix2<f64>, proc_cov: Matrix3<f64>, 
             x_init: Vector3<f64>, cov_init: Matrix3<f64>) -> EKF {
    EKF {
      r: meas_cov,
      q: proc_cov,
      x: x_init,
      cov: cov_init,
      cov_init: cov_init,
      most_recent_timestep: 0,
      dt: 0
    }
  }

  pub fn filter(&mut self, meas: Vector2<f64>, t: Matrix3x4<f64>, timestep: u32) {
    if timestep > self.most_recent_timestep {
      self.dt = timestep - self.most_recent_timestep;
      self.most_recent_timestep = timestep;

      let f = motion_jacobian(&self.x);
      self.x = motion_model(&self.x, self.dt);
      self.cov = &f * &self.cov * f.transpose() + &self.q;
    }
    let h = measurement_jacobian(&t);
    let y = meas - measurement_model(&self.x, &t);
    let s = &h * &self.cov * h.transpose() + &self.r;
    let s_dg = s.pseudo_inverse(1e-8).unwrap();
    let k = &self.cov * (h.transpose() * s_dg);
    self.x += &k * y;
    self.cov = (Matrix3::identity() - &k * &h) * &self.cov;
  }
}

pub type CamID = usize;
pub type Timestamp = u32;
pub type DetectionInfo = (CamID, TagID, Timestamp, f64,f64);
pub type CalData = Matrix3x4<f64>;
pub type FilterArgs = (CalData, Vector2<f64>, Timestamp);

type Snd<T> = UnboundedSender<T>;
type Recv<T> = UnboundedReceiver<T>;

pub struct EKFThreadPool {
  threads: HashMap<TagID, (Snd<FilterArgs>, JoinHandle<()>)>,
  calibration: HashMap<TagID, CalData>,
  tx: Snd<(TagID, Vector3<f64>)>,
}


impl EKFThreadPool {
  pub fn new(tagpos_tx:Snd<(TagID, Vector3<f64>)>,
             cal_file_path: &str,
             ids: &[TagID]) -> EKFThreadPool {
    //read in calibration data
    let cal_file = std::fs::read(cal_file_path)
      .expect(&format!("unable to find the specified configuration at \"{}\"", cal_file_path));
    let cal_data = npyz::NpyFile::new(&cal_file[..]).unwrap().into_vec::<f64>().unwrap();
    let mut calibration: HashMap<usize, Matrix3x4<f64>> = HashMap::new();
    for i in 0..config::NUM_CAMERAS {
      calibration.insert(i,Matrix3x4::<f64>::from_row_slice(&cal_data[i*12..(i*12+12)]));
    }

    // create an ekf for each tag and parallelize it into a thread
    let threads = ids.iter().fold(HashMap::new(), |mut tds, id| {
      let (detinfo_tx, mut detinfo_rx) = mpsc::unbounded_channel::<FilterArgs>();
      let ekf = EKF::new(1.0 * Matrix2::identity(),
                         1.0 * Matrix3::identity(),
                         Vector3::zeros(),
                         Matrix3::zeros());
      tds.insert(*id, (detinfo_tx, Self::new_proxy(ekf, tagpos_tx.clone(), detinfo_rx, *id))); 
      return tds
    });

    EKFThreadPool {
      threads, 
      calibration,
      tx: tagpos_tx,
    }
  }
  // Wraps an ekf into a thread and consolidates
  // output into the same channel
  fn new_proxy(mut ekf: EKF, current_pos_tx: Snd<(TagID, Vector3<f64>)>,
                mut rx: Recv<FilterArgs>, id: usize) -> JoinHandle<()>{
    tokio::spawn(async move {
      loop {
        let (calmat, pos, timestamp) = rx.recv().await.unwrap();
        //println!("reaceived detected tag on ekf {}", id);
        ekf.filter(pos, calmat, timestamp);
        //println!("[EKF {}] sending <{},{},{}>", id, ekf.x.x, ekf.x.y, ekf.x.z );
        current_pos_tx.send((id, ekf.x));
      }
    })
  }

  pub fn send(&self, tid: TagID, camid: usize,
              px: f64, py: f64, timestamp: u32 ) {
    let calmat = self.calibration[&camid];
    let pos = Vector2::<f64>::new(px, py);
    self.threads[&tid].0.send((calmat, pos, timestamp));
  }
}


