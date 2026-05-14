#include "esp_camera.h"

#define LED_PIN 4

namespace CAM {

    class MJPEGStream : public AsyncAbstractResponse {
        public:
          MJPEGStream(AsyncWebServerRequest *request);
          bool _sourceValid() const override;
          size_t _fillBuffer(uint8_t* data, size_t maxLen) override;
          void SetDisconnected();

        private:
          enum {
              STATE_HEADER,
              STATE_SENDING_HEADER,
              STATE_SENDING_IMAGE,
              STATE_SENDING_CRLF
          } _state;

          private:

          AsyncWebServerRequest *_request;
          camera_fb_t *_fb;
          uint8_t _header[128];
          size_t _headerLen;
          size_t _index;
          unsigned long _lastFrame;
          bool _disconnected;
    };

    class BMPStreamResponse : public AsyncAbstractResponse {
      public:
        BMPStreamResponse();
        ~BMPStreamResponse();
        bool _sourceValid() const override;
        size_t _fillBuffer(uint8_t* data, size_t maxLen) override;

      private:
        camera_fb_t *fb;
        int bmp_width;
        int bmp_height;
        int bmp_rowSize;
        int bmp_y;
        uint8_t header[54];
        bool header_sent;
    };

    class RGBStreamResponse : public AsyncAbstractResponse {
      public:
        RGBStreamResponse();
        ~RGBStreamResponse();
        bool _sourceValid() const override;
        size_t _fillBuffer(uint8_t* data, size_t maxLen) override;

      private:
        camera_fb_t *fb;
        int bmp_width;
        int bmp_height;
        int cnt;
        uint8_t header[12];
        bool header_sent;
    };

    camera_config_t CreateCameraConfig (pixformat_t pixformat);
    void StartCamera();

    void handleBmpCapture(AsyncWebServerRequest *request);

}