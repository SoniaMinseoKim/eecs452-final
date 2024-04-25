'''
Extended Kalman Filter for 3D reconstruction from 2D camera data
'''

import numpy as np

def motion_model(x, dt):
    return x + dt * np.array([0.1, 0.1, 0.1])

def motion_jacobian(x):
    return np.eye(len(x))
        
# measurement model is the main focus here
def measurement_model(x, camera_matrix):
    return camera_matrix[:2, :3] @ x + camera_matrix[:2, 3]

def measurement_jacobian(camera_matrix):
    return camera_matrix[:2, :3]

class EKF:
    def __init__(self, meas_cov, proc_cov, x_init, cov_init):
        self.R = meas_cov
        self.Q = proc_cov

        self.x = x_init
        self.cov_init = cov_init
        self.cov = cov_init

        self.most_recent_timestep = 0
        self.dt = 0

    def filter(self, meas, T, timestep):
        if timestep < self.most_recent_timestep: return
        elif timestep > self.most_recent_timestep:
            self.dt = timestep - self.most_recent_timestep
            self.most_recent_timestep = timestep
            self.cov = self.cov_init

            #prediction step
            F = motion_jacobian(self.x)
            self.x = motion_model(self.x, self.dt)
            self.cov = F @ self.cov @ F.T + self.Q
            
        #update step
        H = measurement_jacobian(T)
        y = meas - measurement_model(self.x, T)

        S = H @ self.cov @ H.T + self.R
        K = self.cov @ H.T @ np.linalg.inv(S)

        self.x += K @ y
        self.cov = (np.eye(3) - K @ H) @ self.cov

    # runs multiple iterations of the filter (for synthetic testing)
    def multifilter(self, meas, Ts, timesteps):
        for i in range(len(Ts)):
            self.filter(meas[i], Ts[i], timesteps[i])
    