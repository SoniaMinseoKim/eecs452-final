use rtp::packet::Packet as RtpPacket;
use rtp::Error;
use rtp::extension::HeaderExtension;
use webrtc_util::{Marshal, MarshalSize, Unmarshal};
use bytes::buf::{Buf, BufMut};
use serde::{Deserialize, Serialize};
use crate::util::Point;
use super::Packet;


pub const TAGSTREAM_HEADER_EXTENSION_SIZE: usize = 4;
pub const TAGSTREAM_HEADER_UID: &'static str = "urn:params:ts:rtp-ts:img_offset";

#[derive(PartialEq, Eq, Debug, Default, Copy, Clone )]
pub struct TagStreamExtension {
  pub offset: Point,
}

impl MarshalSize for TagStreamExtension {
  fn marshal_size(&self) -> usize {
    return TAGSTREAM_HEADER_EXTENSION_SIZE;
  }
}

impl Marshal for TagStreamExtension {
  fn marshal_to(&self, mut buf: &mut [u8]) -> webrtc_util::Result<usize> {
    buf.put_u16(self.offset.x);
    buf.put_u16(self.offset.y);
    Ok(TAGSTREAM_HEADER_EXTENSION_SIZE)
  }
}

impl Unmarshal for TagStreamExtension {
  fn unmarshal<B>(buf: &mut B) -> webrtc_util::Result<Self>
  where
    Self: Sized,
    B: Buf {
    if buf.remaining() < TAGSTREAM_HEADER_EXTENSION_SIZE {
      Err(Error::ErrBufferTooSmall.into())
    } else {
      Ok(TagStreamExtension {
        offset: Point {
          x: buf.get_u16(),
          y: buf.get_u16()
        }
      })
    }
  }
}


impl Packet for RtpPacket {
  fn unmarshal<B: Buf>(buf: &mut B) -> Result<Self, ()> {
    Ok(Unmarshal::unmarshal(buf).unwrap())
  }

  fn marshal<B: BufMut>(&self, buf: &mut B) {
    //ferris please optimize this out
    let mut slice: Vec<u8> = Vec::with_capacity(buf.remaining_mut());
    self.marshal_to(&mut slice);
    buf.put_slice(&slice)
  }
}
