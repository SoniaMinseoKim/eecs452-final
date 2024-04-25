'''
Generates synthetic data for the motion tracking project
Samples random points in a 3D space and then moves them around
over time. Then, it generates camera data for each frame, choosing
a random point on a sphere to look from and projecting the points
to 2D.

OUTPUT FORMAT:
saved as a .npz file with the following keys
    ground_truth_clouds: (NUM_FRAMES, NUM_MARKERS, 3) array of the ground truth marker positions over time
    camera_projections: (NUM_FRAMES, NUM_CAMERAS, NUM_MARKERS, 2) array of the 2D projections of the markers for each camera over time
    camera_positions: (NUM_CAMERAS, 3) array of the camera positions (assuming the camera is looking at the origin)
    camera_matrices: (NUM_CAMERAS, 3, 4) array of the camera matrices for each camera for use in Kalman filtering
'''

import numpy as np
import matplotlib.pyplot as plt
from itertools import combinations

NUM_MARKERS = 10 # How many points to track
NUM_FRAMES = 100 # How many time steps to generate
FRAME_DELTA = 0.1 # How much to move each point each frame
DOMAIN = 10 # How big the space is (side length of a cube)
NUM_CAMERAS = 10 # How many cameras to generate
ENFORCEMENT_DISTANCE = 3 # Minimum distance between points in the first frame

'''
Gives the rotation matrix to look from from_pt to to_pt
'''
def lookat(from_pt, to_pt):
    forward = to_pt - from_pt
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, np.array([0, 0, 1]))
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    up /= np.linalg.norm(up)
    return np.array([right, up, forward])

'''
Gives the full camera matrix
'''
def camera_matrix(from_pt, to_pt):
    R = lookat(from_pt, to_pt)
    T = -R @ from_pt
    return np.column_stack([R, T])

'''
Applies camera matrix
'''
def project(P, from_pt, to_pt):
    C = camera_matrix(from_pt, to_pt)
    P_hom = np.column_stack([P, np.ones(len(P))])
    return C @ P_hom.T  

def polar_to_cartesian(r, theta, phi):
    x = r * np.sin(theta) * np.cos(phi)
    y = r * np.sin(theta) * np.sin(phi)
    z = r * np.cos(theta)
    return x, y, z

# Generate Points
while True:
    P = (np.random.rand(10, 3) - 0.5) * DOMAIN
    if all(np.linalg.norm(p - q) > ENFORCEMENT_DISTANCE
            for p, q in combinations(P, 2)):
        break

# Update Points
clouds = [P]
for i in range(NUM_FRAMES):
    moves = FRAME_DELTA * np.random.choice([-1, 1], size=30).reshape((10, 3))
    clouds.append(clouds[-1] + moves)

# Generate Camera Data
camera_projections = np.zeros((NUM_CAMERAS, NUM_FRAMES, NUM_MARKERS, 2))
camera_positions = np.zeros((NUM_CAMERAS, 3))
camera_matrices = np.zeros((NUM_CAMERAS, 3, 4))

for camera_idx in range(NUM_CAMERAS):

    #randomly sample a point on the sphere
    theta = np.random.rand() * np.pi
    phi = np.random.rand() * 2 * np.pi

    #convert to cartesian
    radius = np.sqrt(3 * (DOMAIN/2)**2)
    from_pt = polar_to_cartesian(radius, theta, phi)
    to_pt = np.array([0, 0, 0], dtype=np.float64)

    #project the cloud to 2D
    proj_clouds = []
    for t in range(NUM_FRAMES):
        projected = project(clouds[t], from_pt, to_pt)
        proj_clouds.append(projected[:2].T)

    proj_clouds = np.array(proj_clouds)
    camera_projections[camera_idx] = proj_clouds
    camera_positions[camera_idx] = from_pt
    camera_matrices[camera_idx] = camera_matrix(from_pt, to_pt)

# make frames first dimension of camera_projections
camera_projections = np.swapaxes(camera_projections, 0, 1)

# Save Data
np.savez('synth_data.npz', 
         ground_truth_clouds=clouds, 
         camera_projections=camera_projections, 
         camera_positions=camera_positions, 
         camera_matrices=camera_matrices
        )