# This example displays a live image 
# (frame_size = FRAMESIZE_96X96, pixel_format = PIXFORMAT_RAW) 
# from the ESP over serial, used for fast prototyping of image outputs. 

import serial
import struct
import numpy as np
from PIL import Image
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from pupil_apriltags import Detector

img_height = 96
img_width = 96
img_size = img_height * img_width
pixels = np.zeros((img_height, img_width), dtype=np.uint8)
index = 0  # Global index to keep track of pixel reading progress

# Initialize the plot
fig, ax = plt.subplots()
im = ax.imshow(pixels, cmap='gray', vmin=0, vmax=255)

def detect_april_tags(image):
    # Create an AprilTag detector
    detector = Detector(families='tag16h5',
                        nthreads=1,
                        quad_decimate=1.0,
                        quad_sigma=0.0,
                        refine_edges=True,
                        decode_sharpening=0.25,
                        debug=False)
    # Detect the AprilTags in the image
    detections = detector.detect(image, estimate_tag_pose=False, camera_params=None, tag_size=None)
    return detections



def update_image(*args):
    global index, pixels

    while True:
        byte = ser.read(2)
        if len(byte) < 2:
            break  # Break the loop if we didn't receive enough bytes

        if byte == b'%%':
            index = 0  # Start of new frame
            pixels = np.zeros((img_height, img_width), dtype=np.uint8)
        else:
            # Assuming the data structure is two ASCII characters representing one byte in hexadecimal
            hex_string = byte.decode('ascii')  # Decode to get a string as hexadecimal
            value = int(hex_string, 16)  # Directly convert from hexadecimal to integer (0-255)

            row = index // img_width  # Calculate row
            col = index % img_width   # Calculate column
            pixels[row, col] = value
            index += 1

        if index == img_size:
            im.set_data(pixels)  # Update the image with new data
            index = 0  # Reset the index for the next frame

                 # APRIL TAG CALL 
            detected_tags = detect_april_tags(pixels)
            # Print out detection information
            if detected_tags is not None:
                for tag in detected_tags:
                    print(f"Detected tag id: {tag.tag_id}")
            break
    return im,

ser = serial.Serial(port="COM10", baudrate=115200, timeout=0.1)

# Start the animation
ani = animation.FuncAnimation(fig, update_image, interval=10, blit=True)
plt.show()

# Close the serial port after the program is done
ser.close()