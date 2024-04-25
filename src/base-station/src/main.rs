#![allow(unused_imports)]
#![allow(unused)]
extern crate nalgebra as na;

//mod obsmat;
mod tag_detector;
mod net;
mod util;
mod config;
mod ekf;
mod visualization;

use tag_detector::detector::*;
use tag_detector::image::*;
use net::cam_ctn::udp::*;
use net::cam_ctn::{CamCtn, CamCtnInfo};
use net::protocol::ts_custom::TagStreamPacket;
use net::protocol::ts_custom::TagStreamHeader;
use net::protocol::Packet;
use apriltag::Detector;
use tokio::sync::mpsc;
use tokio::time::Instant;
use tokio::task::JoinHandle;
use std::sync::Arc;
use na::Vector3;


#[tokio::main]
async fn main() {

  let (tag_pos_tx, mut tag_pos_rx) = mpsc::unbounded_channel::<(TagID, Vector3<f64>)>();
  let mut ctns: Vec<(UdpCtn<TagStreamPacket>, JoinHandle<()>)> = Vec::new();
  let ekf_tp = Arc::new(ekf::EKFThreadPool::new(
    tag_pos_tx.clone(),
    config::CALIBRATION_FILE,
    &config::TAGS));

  for i in 0..config::NUM_CAMERAS {
    let ekf_tp = ekf_tp.clone();
    let (ts_tx, mut ts_rx) = mpsc::channel::<TagStreamPacket>(config::MTU);

    let info: CamCtnInfo = CamCtnInfo {
      addr : config::ADDRESS,
      port : config::START_PORT + i,
      id : i,
    };

    // STAGE 1: Window Detection
    let ctn = UdpCtn::<TagStreamPacket>::new(info, ts_tx).unwrap();
    let cam_loop = tokio::spawn(async move {
      let mut wrap = Detwrapper{det:Detector::new("tagCustom48h12")};
      wrap.det.set_thread_number(8);
      wrap.det.set_decimation(1.0);
      wrap.det.set_sigma(2.0);

      println!("Finished building detector"); 

      loop {
        tokio::select!{
          p = ts_rx.recv() => {

            let mut packet = p.unwrap();
            let head = &mut packet.header;
            let w = head.width as usize;

            let img = &packet.data.as_aprilimg(w,w);

            let maybe_det = wrap.det.detect_one(img);
            if let Some((id, [center_x, center_y])) = maybe_det {
              println!("Detected tag id {}", id);
              // STAGE 2: EKF
              ekf_tp.send(
                id, i,
                (head.px as f64 + center_x),
                (head.py as f64 + center_y),
                head.ts
              );
            }

          }
        }
      }
    });
    ctns.push((ctn, cam_loop));
  }

  // STAGE 3: Visualization
  visualization::visualize(&mut tag_pos_rx);
}


