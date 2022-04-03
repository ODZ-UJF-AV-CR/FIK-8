# Camera system based on ESP32-Cam AI Thinker module
- esp32 code uploading: https://randomnerdtutorials.com/program-upload-code-esp32-cam/
- `esp32cam_photo/esp32cam_photo.ino` - record video as images in folder while `pin 16` (can be changed on `line 29`) is connected to ground
  -  records time in millisecondswith for every image
- `pic_to_vid.py` - converts folders with PNGs to .AVI files
  - requires `pip install opencv-python`
  - **Usage:** download `pictures/` folder from SD card, run `pic_to_vid.py` next to it

## ESP32CAM Pinout
![ESP32CAM Pinout](https://www.voxcafe.cz/data/Articles/esp32/esp32-cam-pinout-new.png)
