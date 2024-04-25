pub mod udp;

use bytes::Buf;
use std::io::Error;
use super::protocol::Packet;
use tokio::sync::mpsc::Sender;

#[derive(Copy, Clone)]
pub enum Status {
  Connected,
  Error,
  Unconnected,
}

pub struct CamCtnInfo {
  pub addr: &'static str,
  pub port: usize,
  pub id: usize
}

pub trait CamCtn<P: Packet + Sized> {
  fn new(info: CamCtnInfo, packet_tx: Sender<P>)
         -> Result<Self, std::io::Error> where Self: Sized;
  fn close(self) -> Result<(), Error>;
  fn send(&mut self, p: P);
  fn get_status(&mut self) -> Status;
  fn get_addr(&self) -> &'static str;
}
