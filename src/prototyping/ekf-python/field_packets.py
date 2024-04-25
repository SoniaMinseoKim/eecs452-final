'''
This file reads in the synthetic data and converts it into simulated packets of observations.
Then, it creates a thread for each marker and runs the EKF on each thread.
'''

import numpy as np
import sys
from ekf import EKF
import threading
import concurrent.futures
import time

data = np.load('/Users/vishalchandra_1/Desktop/EECS 452/Project/eecs452/synth_data/synth_data.npz')
observations = data['camera_projections']
matrices = data['camera_matrices']

obs_packets = []
for timestep in range(observations.shape[0]):
    for point_num in range(observations.shape[1]):
        for camera_number in range(observations.shape[2]):
                obs_packets.append({
                     'timestep': timestep, 
                     'point': point_num, 
                     'camera': camera_number, 
                     'meas': observations[timestep, point_num, camera_number, :],
                })

def packet_generator():
    for packet in obs_packets:
        yield packet

class EKFThread(threading.Thread):
    def __init__(self, point_id):
        super().__init__()
        self.point_id = point_id
        self.ekf = EKF(meas_cov=1e-3 * np.eye(2), proc_cov=1e-6 * np.eye(3),
                       x_init=np.zeros(3), cov_init=1e3 * np.eye(3))

        self.proc_count = 0
        self.obs_queue = [] # Queue to store incoming measurements
        self.running = True


    def run(self):
        # keep running even after stop until all meas processed
        while self.running or self.obs_queue:
            if self.obs_queue:
                self.ekf.filter(*self.obs_queue.pop(0))
                self.proc_count += 1

    def add_measurement(self, meas, T, timestep):
        self.obs_queue.append((meas, T, timestep))

    def stop(self):
        self.running = False

NUM_MARKERS = 10
ekfs = [EKFThread(i) for i in range(NUM_MARKERS)]
for filter_thread in ekfs:
    filter_thread.start()

for packet in packet_generator():
    point = packet['point']
    if point == 0:
        ekfs[point].add_measurement(
            packet['meas'], 
            matrices[packet['camera']], 
            packet['timestep']
        )

for filter_thread in ekfs:
    filter_thread.stop()
    filter_thread.join()

if not any(filter_thread.is_alive() for filter_thread in ekfs):
    print("All filters stopped successfully.")
    for filter_thread in ekfs:
        print(f"{filter_thread.proc_count} measurements processed for marker {filter_thread.point_id}.")
        print(f"Marker {filter_thread.point_id} final pos: {filter_thread.ekf.x}")


# THREAD POOL VERSION
# ekfs = [EKF(meas_cov=1e-3 * np.eye(2), proc_cov=1e-6 * np.eye(3),
#             x_init=np.zeros(3), cov_init=1e3 * np.eye(3) ) for i in range(10)]

# with concurrent.futures.ThreadPoolExecutor(10) as executor:
#     for packet in packet_generator():
#         point = packet['point']
#         executor.submit(ekfs[point].filter, packet['meas'], matrices[packet['camera']], packet['timestep'])


# for i in range(10):
#     print(f"Marker {i}: {ekfs[i].x}")