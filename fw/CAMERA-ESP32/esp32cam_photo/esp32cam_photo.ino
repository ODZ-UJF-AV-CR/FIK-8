#include "esp_camera.h"
#include "Arduino.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// settings
uint8_t control_pin = 16;  // pin controling recording
String image_folder_name = "/pictures";

// variables
uint32_t num, foldernum, picture_num, start_time=0, recording_start_time, previous_time;
bool recording = false;
String folder_name="";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
 
  Serial.begin(115200);

  // Camera config
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; 

  config.frame_size = FRAMESIZE_UXGA;  // QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // Init SD Card
  Serial.println("Initializing SD Card");
  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD Card attached");
    return;
  }

  // Init Camera
  Serial.println("Initializing Camera");

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  pinMode(control_pin, INPUT_PULLUP); // control pin
}

void loop() {
  fs::FS &fs = SD_MMC;
  if(!digitalRead(control_pin)){
    if(recording){
      capture(folder_name + "/image" + String(picture_num) + "_" +  String(millis() - start_time ) + ".jpg", fs);

      // print info
      Serial.print("Current FPS: ");
      Serial.println(1000.0 / (millis() - previous_time));
      previous_time = millis();
      picture_num++;
    
      Serial.print("Average FPS: ");
      Serial.println(picture_num * 1000.0 / (millis() - recording_start_time));
    }
    else {  // start recording
      if(!start_time){  // first time recording
        start_time = millis();  // FIK-8 initialization time
        }
      
      recording_start_time = previous_time = millis();
      picture_num = 0;

      num = get_last_folder_num(fs) + 1;
      Serial.print("Starting recording number ");
      Serial.println(num);

      // create folder for recording
      folder_name = image_folder_name + "/" + String(num);
      if(!fs.mkdir(folder_name)){
        Serial.println("Failed to create recording folder");
      }

      // log time of recording start
      File millis_file = fs.open(folder_name + "/millis.txt", FILE_WRITE);
      millis_file.print("start: " + String(millis() - start_time));
      millis_file.close();

      recording = true;
      
    }
  }
  else {
   if(recording){
    Serial.println("Ending recording...");
    
    // log time of recording end
    File millis_file = fs.open(folder_name + "/millis.txt", FILE_APPEND);
    millis_file.print("\nend: " + String(millis() - start_time));
    millis_file.close();
    
    recording=false;
    } 
   delay(100);
  }
}

void capture(String picture_name, fs::FS &fs){  
  // take a picture
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // save the picture
  File file = fs.open(picture_name.c_str(), FILE_WRITE);
  
  if(!file){
    Serial.println("Failed to open file in writing mode");
  }
  else {
    file.write(fb->buf, fb->len); // payload (image), payload length
  }
  file.close();
  
  esp_camera_fb_return(fb);
}

uint16_t get_last_folder_num(fs::FS &fs){
  uint16_t num = 0;

  // open directory
  File dir = fs.open(image_folder_name);
  File folder = dir.openNextFile();
  
  while(folder){
      if(folder.isDirectory()){
          foldernum = String(folder.name()).substring(10).toInt();
          if(foldernum > num){
            num = foldernum;  // num will end up as the highest number in dir
          }
      }
      folder = dir.openNextFile();
  }
  return num;
}
