#include "esp_camera.h"

#define LED_PIN 4

namespace CAM {

    class MJPEGStream : public AsyncAbstractResponse {
        public:
          MJPEGStream();
          bool _sourceValid() const override;
          size_t _fillBuffer(uint8_t* data, size_t maxLen) override;

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

    void StartCamera();
}