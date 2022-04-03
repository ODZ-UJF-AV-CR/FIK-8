import cv2
import os

image_folder = 'pictures'

subfolders = [ f.path for f in os.scandir(folder) if f.is_dir() ]

for image_folder in subfolders:
    video_name = 'videos/' + str(image_folder) + '.avi'

    images = [img for img in os.listdir(image_folder) if img.endswith(".png")]
    frame = cv2.imread(os.path.join(image_folder, images[0]))
    height, width, layers = frame.shape

    video = cv2.VideoWriter(video_name, 0, 1, (width,height))

    for image in images:
        video.write(cv2.imread(os.path.join(image_folder, image)))

    cv2.destroyAllWindows()
    video.release()

print("done!")
