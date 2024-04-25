
import numpy as np
import cv2 as cv
import glob

# termination criteria
criteria = (cv.TERM_CRITERIA_EPS + cv.TERM_CRITERIA_MAX_ITER, 30, 0.001)

# prepare object points, like (0,0,0), (1,0,0), (2,0,0) ....,(6,5,0)
objp = np.zeros((7*7, 3), np.float32)
objp[:, :2] = np.mgrid[0:7, 0:7].T.reshape(-1, 2)

# Arrays to store object points and image points from all the images.
objpoints = []  # 3d point in real-world space
imgpoints = []  # 2d points in image plane.

# images = glob.glob('calibrate/*.jpg')
images = glob.glob('calibration_images/*.jpg') # path to our esp images from each camera

for fname in images:
    img = cv.imread(fname)
    gray = cv.cvtColor(img, cv.COLOR_BGR2GRAY)

    # Apply Gaussian blur and denoising
    gray = cv.GaussianBlur(gray, (5, 5), 0)
    gray = cv.fastNlMeansDenoising(gray, None, 10, 7, 21)

    # Apply adaptive thresholding
    thresh = cv.adaptiveThreshold(gray, 255, cv.ADAPTIVE_THRESH_GAUSSIAN_C, cv.THRESH_BINARY, 11, 2)
    
    cv.imwrite('thresh.jpg', thresh)
    
    # Find the chessboard corners
    corners = cv.goodFeaturesToTrack(thresh, 49, 0.01, 10)
    # print(corners)
    
    # If found, add object points, image points (after refining them)
    if corners is not None:
        # corners = np.int0(corners)
        objpoints.append(objp)

        # Refine corner locations
        # gray = gray.astype(np.uint8)
        corners2 = cv.cornerSubPix(gray, corners, (11, 11), (-1, -1), criteria).astype('float32')
        imgpoints.append(corners2)

        # Draw and display the corners
        # img = cv.drawChessboardCorners(img, (7, 7), corners2, True)
        # cv.imshow('img', img)
        # cv.waitKey(500)

# cv.destroyAllWindows()

# you must flatten the imgpoints 
num_cameras = 3
imgpoints = np.reshape(imgpoints, (num_cameras,49,2)) 

import numpy as np
import cv2
from scipy.spatial.transform import Rotation

# calculate focal length in pixels instead of mm

camera_matrices = []
for i in range(num_cameras):
    fx = 3.6 / 0.0022
    fy = 3.6 / 0.0022
    cx = 0 # assuming intersection point of the optical axis with the image plane is 0
    cy = 0
    intrinsic_matrix = np.array([[fx, 0, cx],
                            [0, fy, cy],
                            [0, 0, 1]], dtype=np.float32)
    dist_coeffs = np.zeros((5,))
    objpoints_ind = objpoints[i]
    imgpoints_ind = imgpoints[i]
    rvecs = [np.zeros((1, 1, 3), dtype=np.float64) for _ in range(len(objpoints_ind))]
    tvecs = [np.zeros((1, 1, 3), dtype=np.float64) for _ in range(len(objpoints_ind))]

    image_size = (640, 480)  # actual resolution of images
    # Calibrate camera with aspect ratio fixed to 1.0
    _, _, dist_coeffs, rvecs, tvecs = cv2.calibrateCamera([objpoints_ind],
                                                        [imgpoints_ind],
                                                        image_size,
                                                        intrinsic_matrix,
                                                        dist_coeffs,
                                                        rvecs,
                                                        tvecs,
                                                        flags=cv2.CALIB_FIX_FOCAL_LENGTH)
    # print("Intrinsics:")
    # print(intrinsic_matrix)
    print("Extrinsics:")
    print(rvecs)
    print(tvecs)
    
    rot_vec = rvecs[0][:,0]
    rotation_object = Rotation.from_rotvec(rot_vec)
    rotation_matrix = rotation_object.as_matrix()
    # print(rotation_matrix)
    # print('rotation_matrix dimension: ', rotation_matrix.shape)
    
    # print('tvecs dimension: ',  tvecs[0][:].shape)
    # save into camera matrix
    camera_matrix = np.concatenate((rotation_matrix, tvecs[0][:]), axis=1)
    print('camera matrix: ', camera_matrix)
    camera_matrices.append(camera_matrix)

print('camera_matrices length: ', len(camera_matrices))
camera_ids = np.arange(1,num_cameras+1)
np.savez_compressed('calibration_results', camera_ids = camera_ids, camera_matrices = camera_matrices)