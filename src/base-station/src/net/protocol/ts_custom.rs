use bytes::{Buf, BufMut};
use super::Packet;

pub struct TagStreamHeader {
  pub width : u16,
  pub px : u16,
  pub py : u16,
  pub ts: u32,
}

pub struct TagStreamPacket {
  pub header: TagStreamHeader,
  pub data: Vec<u8>,
}

impl Packet for TagStreamPacket {
  fn unmarshal<B: Buf>(buf: & mut B) -> Result<Self, ()> {
    let mut header = TagStreamHeader {
        width : u16::from_be(buf.get_u16()),
        px : u16::from_be(buf.get_u16()),
        py : u16::from_be(buf.get_u16()),
        ts:  0
      };
    //throw away spacer due to struct alignment
    buf.get_u16();
    header.ts = u32::from_be(buf.get_u32());

    Ok(TagStreamPacket {
      header: header,
      data: buf.chunk().to_vec()
    })
  }

  fn marshal<B: BufMut>(&self, buf: &mut B)  {
    buf.put_u16(self.header.width);
    buf.put_u16(self.header.px);
    buf.put_u16(self.header.py);
    buf.put_u32(self.header.ts);
    buf.put_slice(&self.data)
  }
}

