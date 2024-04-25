use apriltag::{
  Detector,
  DetectorBuilder,
  Detection,
  Family,
  Image
};
use tokio::sync::Mutex; 
pub type TagID = usize;
pub struct SafeDetector(Detector);

pub struct Detwrapper {
  pub det: Detector
}
unsafe impl Send for Detwrapper {} 



pub trait DetectorExt {
  fn new(family: &str) -> Detector {
    let builder = DetectorBuilder::new();
    let family: Family = family.parse().unwrap();
    builder.add_family_bits(family,1).build().unwrap()
  }
  fn detect_one(&mut self, img: &Image) -> Option<(TagID, [f64;2])>;
}


impl DetectorExt for Detector {
  fn detect_one(&mut self, img: &Image) -> Option<(TagID, [f64;2])> {
    match &self.detect(img)[..] {
      [det, ..] => {
       Some((det.id(), det.center())) 
      },
      [] => {
        None
      }
    }
  }
}
