### EKF Python Implementation

This folder is simply a prototype for what the Extended Kalman Filter (EKF) looks like and functions like in code. This is completely replaced functionally by the `ekf.rs` file in the `base-station` folder. While this code is largely written in numpy, which is a wrapper for the host system's HPC library like BLAS or LAPACK, the overhead to call those foreign functions from python is simply too high for our real-time use case.

Rewriting this in rust allows us to eliminate that overhead and call everything within a single binary, improving performance. The most important file in this folder for understanding our research process is `recon.ipynb` which imports `ekf.py` as a module and tests it interactively. 