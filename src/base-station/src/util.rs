use tokio::runtime;
use std::future::Future;

//pub type Point = opencv::core::Point_<u16>;

pub fn sync<F: Future>(future: F) -> F::Output {
  let rt = runtime::Runtime::new().unwrap();
  rt.block_on(future)
}
