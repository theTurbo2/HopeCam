#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LITTLEFS.h"
#include "esp_camera.h"
#include "cam.h"
#include "WiFiSetup.h"

namespace WEBSERVER {

    bool ledState = false;

    void netStatusRequestHandler(AsyncWebServerRequest *request){
        static uint8_t json_response[1024];
    
        char * p = (char *)json_response;
        *p++ = '{';
    
        p+=sprintf(p, "\"ssid\":\"%s\",", WiFiSetup::getSSID().c_str());
        p+=sprintf(p, "\"password\":\"%s\",", WiFiSetup::getPASS().c_str());
        p+=sprintf(p, "\"dhcp\":%u,", WiFiSetup::getDHCP());
        p+=sprintf(p, "\"ip\":\"%s\",", WiFiSetup::getIP().c_str());
        p+=sprintf(p, "\"netmask\":\"%s\",", WiFiSetup::getNETMASK().c_str());
        p+=sprintf(p, "\"gateway\":\"%s\",", WiFiSetup::getGATEWAY().c_str());
        p+=sprintf(p, "\"nodename\":\"%s\"", WiFiSetup::getNODENAME().c_str());
        *p++ = '}';
        *p++ = 0;

        Serial.println(WiFiSetup::getSSID());
        Serial.println(String((char*)json_response));
        Serial.println("Len:" + (String)(strlen((char*)json_response)));
        request->send(200, "text/plain",(char*)json_response);
    }
  
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

        server->on("/net_status", HTTP_GET, netStatusRequestHandler);
//        server->on("/net_status", HTTP_GET, [](AsyncWebServerRequest *request){
//          request->send(200, "text/plain","{\"ssid\":\"MySSID\",\"password\":\"MyPWD\",\"dhcp\":0,\"ip\":\"1.2.3.4\",\"netmask\":\"5.6.7.8\",\"gateway\":\"1.3.5.7\"}");
//        });

        server->serveStatic("/", LittleFS, "/");

        Serial.println("Starting Server ...");

        server->begin();

    }
}