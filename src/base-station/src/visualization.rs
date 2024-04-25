use std::f32::consts::PI;
use std::collections::HashMap;
use kiss3d::{light::Light, scene::SceneNode};
use kiss3d::window::Window;
use kiss3d::nalgebra as na;
use na::{Translation3, Point3, Vector3};
use crate::{
  tag_detector::detector::TagID,
  config,
};

use tokio::sync::{
  mpsc,
  mpsc::UnboundedReceiver,
};


type TagPoint = (TagID, na::Vector3<f64>);
const POINT_RADIUS: f32 = 0.1; // Increase the radius for larger point representation


pub fn visualize(points_rx: &mut UnboundedReceiver<TagPoint>) {
  // Create a window for rendering.
  let mut window = Window::new("Kiss3d: moving point");
  println!("test");

  // Set the light to be in the front.
  window.set_light(Light::StickToCamera);
  window.set_background_color(0.8, 0.8, 0.8);

  // Time step for the animation.
  let mut time = 0.0;
  let time_step = 0.016; // Approximately 60Hz

  // Create a spheres that will represent the point.
  let mut spheres: HashMap<TagID, SceneNode> = HashMap::new();
  for i in config::TAGS {
    spheres.insert(i, window.add_sphere(POINT_RADIUS));
  }

  // Set the color of the sphere (optional, default is white).
  spheres.get_mut(&0).unwrap().set_color(1.0, 0.0, 0.0); // Red
  spheres.get_mut(&1).unwrap().set_color(0.0, 1.0, 0.0); // Blue
  spheres.get_mut(&2).unwrap().set_color(0.0, 0.0, 1.0); // Green

  // Axis length.
  let axis_length = 4.0;
  // Main loop.
  while window.render() {
    window.draw_line(&Point3::new(0.0, 0.0, 0.0),
                     &Point3::new(axis_length, 0.0, 0.0),
                     &Point3::new(1.0, 0.0, 0.0));

    window.draw_line(&Point3::new(0.0, 0.0, 0.0),
                     &Point3::new(0.0, axis_length, 0.0),
                     &Point3::new(0.0, 1.0, 0.0));

    window.draw_line(&Point3::new(0.0, 0.0, 0.0),
                     &Point3::new(0.0, 0.0, axis_length),
                     &Point3::new(0.0, 0.0, 1.0));


    if let Ok((id, pos)) = points_rx.try_recv() {
      // Draw axis
      // Set the new position for the sphere.
      (&mut spheres.get_mut(&id).unwrap()).set_local_translation(
        Translation3::new(
          pos.x as f32/ 100.0,
          pos.y as f32/100.0,
          pos.z as f32/ 100.0
        )
      );

    }
    // Increment the time for the animation.
    time += time_step;
  }
}
