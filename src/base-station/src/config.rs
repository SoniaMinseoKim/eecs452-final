use crate::tag_detector::detector::TagID;

pub const CALIBRATION_FILE: &str = "./calibration.npy";
pub const ADDRESS: &str = "0.0.0.0";
// Ports range form START_PORT to (START_PORT + NUM_CAMERAS - 1)
pub const START_PORT: usize = 3310;
pub const MTU: usize = 100000;
pub const NUM_CAMERAS: usize = 3;
pub const TAGS: [usize;3] = [0, 1, 2];
