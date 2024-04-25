use crate::util;
use super::Packet;
use super::{Status, CamCtnInfo, CamCtn};

use tokio::sync::oneshot;
use tokio::sync::mpsc;
use tokio::net::UdpSocket;
use std::io::Cursor;
use std::sync::Arc;
use std::io::Error;

pub const MTU: usize = 10000000;

struct NetThreadHandle<P: Packet> {
  close: mpsc::Sender<()>,
  status: (mpsc::UnboundedSender<()>, mpsc::Receiver<Status>),
  send_packet: mpsc::Sender<P>,
  handle: tokio::task::JoinHandle<()>
}

pub struct UdpCtn<P: Packet> {
  info: CamCtnInfo,
  socket_h: NetThreadHandle<P>
}

impl<P: Packet> NetThreadHandle<P> {
  fn req_close(self) -> Result<(), tokio::task::JoinError> {
    self.close.send(());
    util::sync(async {
      self.handle.await
    })
  }

  fn send_packet(&mut self, packet: P) -> Result<(), mpsc::error::TrySendError<P>> {
    self.send_packet.try_send(packet)
  }

  fn get_status(&mut self) -> Option<Status> {
    let (send,recv) = &mut self.status;
    send.send(());
    recv.blocking_recv()
  }
}

impl<P: Packet + 'static > CamCtn<P> for UdpCtn<P> {
  fn new(info: CamCtnInfo, packet_out: mpsc::Sender<P> )
         -> Result<UdpCtn<P> , std::io::Error> {

    let mut status = Status::Connected;

    let (close_tx , mut close_rx) = mpsc::channel::<()>(1);
    let (packet_tx , mut packet_rx) = mpsc::channel::<P>(MTU);
    let (status_poll_tx , mut status_poll_rx) = mpsc::unbounded_channel();
    let (status_info_tx , mut status_info_rx) = mpsc::channel::<Status>(MTU);

    let id = info.id.clone(); 
    let handle = tokio::spawn(async move {
      let mut status = Status::Unconnected;
      let mut buf = Vec::with_capacity(MTU);
      buf.resize(MTU, 0);
      let socket = UdpSocket::bind(format!(
          "{}:{}",
          info.addr,
          info.port
      )).await.expect("Unable to bind to the specified address, check config.rs");
      loop {
        tokio::select! {
          p = socket.recv(&mut buf) => {
            match p {
              Ok(size) =>{
                let slc = &buf[0..size];
                let p = P::unmarshal(&mut Cursor::new(slc)).unwrap();
                packet_out
                  .send(p).await
                  .expect(&format!("unable to receive packet from camera {}", id));

              }
              Err(_) => println!("invalid packet received!"),
            }
          },
          _ = status_poll_rx.recv() => {
            status_info_tx.send(status);
          }
          _ = close_rx.recv() => {
            println!("shutting down thread for camera {}",  id);
            return;
          }
        }
      }
    });

    let mut ctn = UdpCtn::<P> { 
      info,
      socket_h : NetThreadHandle::<P> {
        close: close_tx,
        send_packet: packet_tx,
        status: (status_poll_tx, status_info_rx),
        handle
      }
    };

    Ok(ctn)
  }

  fn close(self) -> Result<(), Error> {
    Ok(self.socket_h.req_close()?)
  }

  fn send(&mut self, p:P) {
    self.socket_h.send_packet(p);
  }

  fn get_status(&mut self) -> Status {
    self.socket_h.get_status().unwrap()
  }

  fn get_addr(&self) -> &'static str {
    self.info.addr
  }

}
