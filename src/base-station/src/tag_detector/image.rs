use apriltag::Image;
use apriltag::image_buf::DEFAULT_ALIGNMENT_U8;
use apriltag_sys::image_u8;
//use opencv::prelude::*;

pub trait ImageExt {
	fn as_aprilimg(&mut self, w:usize, h:usize) -> Image;
}
impl ImageExt for [u8] {
	fn as_aprilimg(&mut self, w:usize, h:usize) -> Image {
    let mut image = Image::zeros_with_stride(
      w, h, w
    ).unwrap();
    let len = image.as_slice().len();
    for i in 0..(self.len()) {
      if(len <= i) {
        break;
      }
      image[(i%w, i/h)] = self[i];
    }
    image
  }
}
/*
impl ImageExt for Mat {
	fn as_aprilimg(&mut self, w:usize, h:usize) -> Image {
		let w = self.cols() as usize;
		let h = self.rows() as usize;
		let buf = self.data_bytes().unwrap();
		let mut image = Image::zeros_with_alignment(w, h, DEFAULT_ALIGNMENT_U8).unwrap();
		for (i, pixel) in buf.iter().enumerate() {
			image[(i%w, i/h)] = *pixel; 
		}
		image
	}
}
*/
