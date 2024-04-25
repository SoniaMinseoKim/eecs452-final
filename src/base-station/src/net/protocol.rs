//mod rtp;
pub mod raw;
pub mod ts_custom;

use bytes::BufMut;
use bytes::Buf;
use tokio::io::Error;



pub trait Packet: Send + Sized {
  fn marshal<B: BufMut>(&self, buf: &mut B);
  fn unmarshal<B: Buf>(buf: &mut B) -> Result<Self, ()>; 
}

