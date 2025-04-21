#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LITTLEFS.h"
#include "esp_camera.h"


namespace WEBSERVER {

    // MJPEG Streaming Response Handler
    class MJPEGStream : public AsyncAbstractResponse {
        public:
            MJPEGStream() {
                _code = 200;
                _contentType = "multipart/x-mixed-replace; boundary=frame";
                _sendContentLength = false;
                _chunked = true;
                _index = 0;
                _state = STATE_HEADER;
                _fb = nullptr;
                _lastFrame = 0;
            }
    
            bool _sourceValid() const override {
                unsigned long diff = millis() - _lastFrame; 
                Serial.println("source valid - diff:" + String(diff));
                return diff > 0; // 50: ~20 FPS
            }
      
            size_t _fillBuffer(uint8_t* data, size_t maxLen)  override {
                size_t toCopy = 0;

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

        private:
            enum {
                STATE_HEADER,
                STATE_SENDING_HEADER,
                STATE_SENDING_IMAGE,
                STATE_SENDING_CRLF
            } _state;

            camera_fb_t *_fb;
            uint8_t _header[128];
            size_t _headerLen;
            size_t _index;
            unsigned long _lastFrame;
    };
    
    void StartServer (AsyncWebServer *server) {

        Serial.println("Install handlers ...");

        // JPEG image route
        server->on("/cam.jpg", HTTP_GET, [](AsyncWebServerRequest *request){

            Serial.println("/cam.jpg requested");

            camera_fb_t * fb = esp_camera_fb_get();
            if (!fb) {
                request->send(500, "text/plain", "Camera capture failed");
                return;
            }

            AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);
            response->addHeader("Content-Disposition", "inline; filename=capture.jpg");
            request->send(response);
            esp_camera_fb_return(fb);
        });

        // MJPEG route
        // This route uses a raw client stream for MJPEG
        server->on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(new MJPEGStream());
          });

        // Web Server Root URL
        server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(LittleFS, "/index.html", "text/html");
        });
        server->serveStatic("/", LittleFS, "/");

        Serial.println("Starting Server ...");

        server->begin();

    }
}