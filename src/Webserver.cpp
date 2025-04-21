#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LITTLEFS.h"
#include "esp_camera.h"
#include "cam.h"


namespace WEBSERVER {

    bool ledState = false;


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
            request->send(new CAM::MJPEGStream());
          });

        // Web Server Root URL
        server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(LittleFS, "/index.html", "text/html");
        });

        server->on("/led", HTTP_GET, [](AsyncWebServerRequest *request){
            if (request->hasParam("on")) {
              ledState = request->getParam("on")->value() == "1";
              digitalWrite(LED_PIN, ledState ? HIGH : LOW);
              request->send(200, "text/plain", ledState ? "LED ON" : "LED OFF");
            } else {
              request->send(400, "text/plain", "Missing 'on' parameter");
            }
          });

        server->serveStatic("/", LittleFS, "/");

        Serial.println("Starting Server ...");

        server->begin();

    }
}