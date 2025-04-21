#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_camera.h"
#include "cam.h"

//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

#include "camera_pins.h"

namespace CAM
{
    // MJPEG Streaming Response Handler
    MJPEGStream::MJPEGStream() {
        _code = 200;
        _contentType = "multipart/x-mixed-replace; boundary=frame";
        _sendContentLength = false;
        _chunked = true;
        _index = 0;
        _state = STATE_HEADER;
        _fb = nullptr;
        _lastFrame = 0;
    }
  
    bool MJPEGStream::_sourceValid() const {
        //unsigned long diff = millis() - _lastFrame; 
        //Serial.println("source valid - diff:" + String(diff));
        //return diff > 0; // 50: ~20 FPS - this is not working with this AsyncWebServer
        return true;
    }
    
    size_t MJPEGStream::_fillBuffer(uint8_t* data, size_t maxLen) {
      size_t toCopy = 0;

      // At top of fillBuffer()
      if (_state == STATE_HEADER) {
          static unsigned long lastPrint = 0;
          static int frameCounter = 0;
      
          frameCounter++;
          unsigned long now = millis();
          if (now - lastPrint >= 1000) {
          Serial.printf("ESP FPS: %d\n", frameCounter);
          frameCounter = 0;
          lastPrint = now;
          }
      }
      
      switch (_state) {
        case STATE_HEADER:
          // Get a new frame from camera
          if (_fb) {
            esp_camera_fb_return(_fb);
            _fb = nullptr;
          }
  
          _fb = esp_camera_fb_get();
          if (!_fb || _fb->format != PIXFORMAT_JPEG) {
            if (_fb) esp_camera_fb_return(_fb);
            return 0;
          }
  
          // Build multipart header
          _headerLen = snprintf((char*)_header, sizeof(_header),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            _fb->len);
          _index = 0;
          _state = STATE_SENDING_HEADER;
  
          // fallthrough
  
        case STATE_SENDING_HEADER:
          toCopy = min(maxLen, _headerLen - _index);
          memcpy(data, _header + _index, toCopy);
          _index += toCopy;
          if (_index >= _headerLen) {
            _index = 0;
            _state = STATE_SENDING_IMAGE;
          }
          return toCopy;
  
        case STATE_SENDING_IMAGE:
          toCopy = min(maxLen, _fb->len - _index);
          memcpy(data, _fb->buf + _index, toCopy);
          _index += toCopy;
          if (_index >= _fb->len) {
            _index = 0;
            _state = STATE_SENDING_CRLF;
          }
          return toCopy;
  
        case STATE_SENDING_CRLF:
          toCopy = min(maxLen, 2 - _index);
          memcpy(data, "\r\n" + _index, toCopy);
          _index += toCopy;
          if (_index >= 2) {
            _index = 0;
            _state = STATE_HEADER;
            _lastFrame = millis();
          }
          return toCopy;
  
        default:
          return 0;
      }
  }

  void StartCamera() {

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

    //init with high specs to pre-allocate larger buffers
    if(psramFound()){
      Serial.println("psram found");

      config.frame_size = FRAMESIZE_UXGA;
      config.jpeg_quality = 10;
      config.fb_count = 2;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
    }

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);
      return;
    }

    sensor_t * s = esp_camera_sensor_get();
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);//flip it back
      s->set_brightness(s, 1);//up the blightness just a bit
      s->set_saturation(s, -2);//lower the saturation
    }

    //drop down frame size for higher initial frame rate
    s->set_framesize(s, FRAMESIZE_QVGA);
    config.jpeg_quality = 25;

    Serial.println("Camera Ready!");

    // init LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // LED off by default
  } 
/*
  void doFlashLight( void * pvParameters ) 
  {
    for (;;) {
  //    Serial.print("flashlight start: ");
  //    Serial.println(startlight);
      if ((startlight > 0) && (startlight < millis())) {
        digitalWrite(4,0);
        startlight = 0;
      }
      vTaskDelay(1000);
    }
  }
*/
}
