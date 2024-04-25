import io
from PIL import Image
import socket 

# Setup UDP socket
UDP_IP = None # use your IP address e.g. "35.2.34.134"
UDP_PORT = None # use your port number e.g. 3333
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

# Create a buffer to store JPEG data
jpeg_buffer = io.BytesIO()

def save_image(image, filename):
    # Save the image to a file
    image.save(filename)

def update_image():
    global jpeg_buffer

    # Read JPEG data from UDP socket until a complete image is received
    while True:
        data, addr = sock.recvfrom(42769)
        if data:
            id = data[-1]
            data = data[:-1]
        # Use a context manager to open a file in binary write mode
        # You can replace 'image.jpg' with whatever file path you want
            with open(f"calibration/image-{id}.jpg", 'wb') as image_file:
                image_file.write(data)
            print(f"Image received and saved from {addr}, id: {id}")
        
# Update the image
update_image()



