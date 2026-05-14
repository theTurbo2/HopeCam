#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "esp_camera.h"
#include "cam.h"
#include <Ticker.h>
//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

#include "camera_pins.h"

namespace CAM
{
  void onClientDisconnect(void *arg, AsyncClient *client) {
    MJPEGStream *p = (MJPEGStream *) arg;
    p->SetDisconnected();
    
    Serial.println("Client disconnected");
  }
  
  void MJPEGStream::SetDisconnected() {
    _disconnected = true;

    if (_fb) {
      esp_camera_fb_return(_fb);
      _fb = nullptr;
    }
  }

  // MJPEG Streaming Response Handler
  MJPEGStream::MJPEGStream(AsyncWebServerRequest *request) : AsyncAbstractResponse() {
    Serial.println("Init MJPEGStream class");

    _request = request;

    _fb = esp_camera_fb_get();
    if (!_fb) {
        // Camera failed
        _code = 500;
        _contentLength = 0;
        _contentType = "text/plain";
        _sendContentLength = true;
        return;
    }

    if (_fb->format != PIXFORMAT_JPEG) {
      Serial.printf("must change pixformat");
      esp_camera_fb_return(_fb);      
      if (esp_camera_deinit() != ESP_OK)
        Serial.printf("de-init failed");

      camera_config_t config = CreateCameraConfig(PIXFORMAT_JPEG);

      esp_err_t err = esp_camera_init(&config);
      if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
      }  

      _fb = esp_camera_fb_get();
      if (!_fb) {
          // Camera failed
          _code = 500;
          _contentLength = 0;
          _contentType = "text/plain";
          _sendContentLength = true;
          Serial.println("2nd  fb_get failed");
          return;
      }  
    }

    _request->client()->onDisconnect(onClientDisconnect, this);
    
    _code = 200;
    _contentType = "multipart/x-mixed-replace; boundary=frame";
    _sendContentLength = false;
    _chunked = true;
    _index = 0;
    _state = STATE_HEADER;
    _lastFrame = 0;
    _disconnected = false;
  }
  
  bool MJPEGStream::_sourceValid() const {
      //unsigned long diff = millis() - _lastFrame; 
      //Serial.println("source valid - diff:" + String(diff));
      //return diff > 0; // 50: ~20 FPS - this is not working with this AsyncWebServer
      return (!_disconnected);
  }
    
  size_t MJPEGStream::_fillBuffer(uint8_t* data, size_t maxLen) 
  {
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

  BMPStreamResponse::BMPStreamResponse() 
  {
    Serial.println("Init BMPStream class");

    fb = esp_camera_fb_get();
    if (!fb) {
        // Camera failed
        _code = 500;
        _contentLength = 0;
        _contentType = "text/plain";
        _sendContentLength = true;
        return;
    }

    if (fb->format != PIXFORMAT_RGB565) {
      Serial.printf("must change pixformat");
      esp_camera_fb_return(fb);      
      if (esp_camera_deinit() != ESP_OK)
        Serial.printf("de-init failed");

      camera_config_t config = CreateCameraConfig(PIXFORMAT_RGB565);

      esp_err_t err = esp_camera_init(&config);
      if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
      }  

      fb = esp_camera_fb_get();
      if (!fb) {
          // Camera failed
          _code = 500;
          _contentLength = 0;
          _contentType = "text/plain";
          _sendContentLength = true;
          Serial.println("2nd  fb_get failed");
          return;
      }  
    }
    
    _code = 200;
    _contentType = "image/bmp";
    _sendContentLength = false; // because streaming

    bmp_width = fb->width;
    bmp_height = fb->height;
    bmp_rowSize = (bmp_width * 3 + 3) & ~3;
    bmp_y = bmp_height - 1;
    //DEBUG:Serial.println(fb->len);

    // Prepare BMP Header
    int fileSize = 54 + bmp_rowSize * bmp_height;

    memset(header, 0, 54);
    header[0] = 'B';
    header[1] = 'M';
    header[2] = fileSize;
    header[3] = fileSize >> 8;
    header[4] = fileSize >> 16;
    header[5] = fileSize >> 24;
    header[10] = 54;
    header[14] = 40;
    header[18] = (uint8_t)(bmp_width & 0xFF);
    header[19] = (uint8_t)((bmp_width >> 8) & 0xFF);
    header[22] = (uint8_t)(bmp_height & 0xFF);
    header[23] = (uint8_t)((bmp_height >> 8) & 0xFF);
    header[26] = 1;
    header[28] = 24;

    header_sent = false;
  }

  BMPStreamResponse::~BMPStreamResponse() {
      if (fb) {
          esp_camera_fb_return(fb);
      }
  }

  bool BMPStreamResponse::_sourceValid() const {
      return fb != nullptr;
  }

  size_t BMPStreamResponse::_fillBuffer(uint8_t *buffer, size_t maxLen)  
  {
    if (!fb) return 0;

    size_t bytesWritten = 0;

    if (!header_sent) {
        size_t headerLen = sizeof(header);
        if (maxLen < headerLen) return 0;
        memcpy(buffer, header, headerLen);
        bytesWritten = headerLen;
        header_sent = true;
        return bytesWritten;
    }

    while (bmp_y >= 0 && (bytesWritten + bmp_rowSize) <= maxLen) {
        uint8_t *lineBuf = buffer + bytesWritten;
        int idx = 0;
        for (int x = 0; x < bmp_width; x++) {
          uint8_t high  = fb->buf[(bmp_y * bmp_width + x) * 2];
          uint8_t low = fb->buf[(bmp_y * bmp_width + x) * 2 + 1];

          uint16_t pixel = (high << 8) | low;

          uint8_t r = (uint8_t)(((pixel >> 11) & 0x1F) * 255 / 31);
          uint8_t g = (uint8_t)(((pixel >> 5) & 0x3F) * 255 / 63);
          uint8_t b = (uint8_t)((pixel & 0x1F) * 255 / 31);
            lineBuf[idx++] = b;
            lineBuf[idx++] = g;
            lineBuf[idx++] = r;
        }
        while (idx < bmp_rowSize) lineBuf[idx++] = 0x00;
        bytesWritten += bmp_rowSize;
        bmp_y--;
    }

    return bytesWritten;
  }

  RGBStreamResponse::RGBStreamResponse() 
  {
    Serial.println("Init RGBStream class");

    fb = esp_camera_fb_get();
    if (!fb) {
        // Camera failed
        _code = 500;
        _contentLength = 0;
        _contentType = "text/plain";
        _sendContentLength = true;
        Serial.println("1st fb_get failed");
        return;
    }

    if (fb->format != PIXFORMAT_RGB565) {
      Serial.printf("must change pixformat");
      esp_camera_fb_return(fb);      
      if (esp_camera_deinit() != ESP_OK)
        Serial.printf("de-init failed");

      camera_config_t config = CreateCameraConfig(PIXFORMAT_RGB565);

      esp_err_t err = esp_camera_init(&config);
      if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
      }  

      fb = esp_camera_fb_get();
      if (!fb) {
          // Camera failed
          _code = 500;
          _contentLength = 0;
          _contentType = "text/plain";
          _sendContentLength = true;
          Serial.println("2nd  fb_get failed");
          return;
      }  
    }

    _code = 200;
    _contentType = "image/rgb";
    _sendContentLength = false; // because streaming

    bmp_width = fb->width;
    bmp_height = fb->height;

    // Prepare BMP Header
    int fileSize = fb->len + 8;
    cnt = 0;

    Serial.println(bmp_width);
    Serial.println(bmp_height);
    Serial.println(fileSize);

    memset(header, 0, 12);
    header[0] = fileSize;
    header[1] = fileSize >> 8;
    header[2] = fileSize >> 16;
    header[3] = fileSize >> 24;
    header[4] = (uint8_t)(bmp_width & 0xFF);
    header[5] = (uint8_t)((bmp_width >> 8) & 0xFF);
    header[8] = (uint8_t)(bmp_height & 0xFF);
    header[9] = (uint8_t)((bmp_height >> 8) & 0xFF);

    header_sent = false;
  }

  RGBStreamResponse::~RGBStreamResponse() {
      if (fb) {
          esp_camera_fb_return(fb);
      }
  }

  bool RGBStreamResponse::_sourceValid() const {
      return fb != nullptr;
  }

  size_t RGBStreamResponse::_fillBuffer(uint8_t *buffer, size_t maxLen)  {
    if (!fb) return 0;

    size_t bytesWritten = 0;

    if (!header_sent) {
        size_t headerLen = sizeof(header);
        if (maxLen < headerLen) return 0;
        memcpy(buffer, header, headerLen);
        bytesWritten = headerLen;
        header_sent = true;
        return bytesWritten;
    }

    int remaining = fb->len - cnt;
    if (remaining > maxLen)
      remaining = maxLen;

    memcpy (buffer, fb->buf + cnt, remaining);
    cnt += remaining;
    //Serial.println(remaining);

    return remaining;
  }

  camera_config_t CreateCameraConfig (pixformat_t pixformat) 
  {
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
    config.pixel_format = pixformat;

    //init with high specs to pre-allocate larger buffers
    if(psramFound()){
      Serial.println("psram found");

    //      config.frame_size = FRAMESIZE_UXGA;
    //config.frame_size = FRAMESIZE_QVGA; // 5: 320x240
      config.frame_size = FRAMESIZE_HQVGA; // 3: 240x176
      config.jpeg_quality = 12;
      config.fb_count = 1;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.jpeg_quality = 12;
      config.fb_count = 1;
    }

    return config;

  }

  void StartCamera() {

    camera_config_t config = CreateCameraConfig(PIXFORMAT_JPEG);

    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return;
    }

    sensor_t * s = esp_camera_sensor_get();
    //initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);//flip it back
      s->set_brightness(s, 1);//up the blightness just a bit
      s->set_saturation(s, -2);//lower the saturation
    }
/*
    s->set_colorbar(s, 0);
    s->set_whitebal(s, 1);
    s->set_brightness(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
*/
    //drop down frame size for higher initial frame rate
    //s->set_framesize(s, FRAMESIZE_QVGA);
    //config.jpeg_quality = 25;

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
