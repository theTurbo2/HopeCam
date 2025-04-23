#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LITTLEFS.h"
#include "esp_camera.h"
#include "cam.h"
#include "WiFiSetup.h"

namespace WEBSERVER {

    bool ledState = false;
    WiFiSetup::networkConfig newNetworkConfig;

    bool checkIPfailure(String val, AsyncWebServerRequest *request)
    {
      IPAddress dummy; dummy.fromString(val);
      if (dummy.toString() == "0.0.0.0") {
        request->send(400, "application/json", "{\"error\": \"invalid input\", \"details\": \"invalid IP address format '" + val + "' - this data will not be stored\"}");
        return true;
      }
      return false;
    }

    void netSaveRequestHandler(AsyncWebServerRequest *request){
      request->send(200, "text/plain");
      WiFiSetup::setNetworkConfig(newNetworkConfig);

      delay(3000);

      ESP.restart();
    }

    void netCommandRequestHandler(AsyncWebServerRequest *request){
      char*  buf;
      size_t buf_len;
      char variable[32] = {0,};
      char value[32] = {0,};
  
      bool param1Available = request->hasParam("var");
      bool param2Available = request->hasParam("val");
      if (!param1Available || !param2Available) {
        request->send(400, "text/plain", "bad request - expected: var=...&val=...");
        return;
      }

      const AsyncWebParameter *param = request->getParam((size_t)0);
      String var = request->getParam("var")->value();
      String val = request->getParam("val")->value();

      Serial.println(var + " = " + val);
      if(var == "ip") {
          if (checkIPfailure(var, request))
            return;
          newNetworkConfig.ssid = val;
      } else if(var == "gateway") {
        if (checkIPfailure(var, request))
          return;
        newNetworkConfig.gateway = val;
      } else if(var == "netmask") {
        if (checkIPfailure(var, request))
          return;
        newNetworkConfig.netmask = val;
      } else if(var == "nodename") {
        newNetworkConfig.nodename = val;
      } else if(var == "ssid") {
        newNetworkConfig.ssid = val;
      } else if(var == "password") {
        newNetworkConfig.pass = val;
      } else if(var == "dhcp") {
        newNetworkConfig.dhcp = val.toInt();
      } else if(var == "apmode") {
        newNetworkConfig.apmode = val.toInt();
      }

      request->send(200, "text/plain");
    }

    void netStatusRequestHandler(AsyncWebServerRequest *request){
        static uint8_t json_response[1024];
    
        char * p = (char *)json_response;
        *p++ = '{';
    
        p+=sprintf(p, "\"ssid\":\"%s\",", newNetworkConfig.ssid.c_str());
        p+=sprintf(p, "\"password\":\"%s\",", newNetworkConfig.pass.c_str());
        p+=sprintf(p, "\"dhcp\":%u,", newNetworkConfig.dhcp);
        p+=sprintf(p, "\"apmode\":%u,", newNetworkConfig.apmode);
        p+=sprintf(p, "\"ip\":\"%s\",", newNetworkConfig.ip.c_str());
        p+=sprintf(p, "\"netmask\":\"%s\",", newNetworkConfig.netmask.c_str());
        p+=sprintf(p, "\"gateway\":\"%s\",", newNetworkConfig.gateway.c_str());
        p+=sprintf(p, "\"nodename\":\"%s\"", newNetworkConfig.nodename.c_str());
        *p++ = '}';
        *p++ = 0;

        Serial.println(WiFiSetup::getSSID());
        Serial.println(String((char*)json_response));
        Serial.println("Len:" + (String)(strlen((char*)json_response)));
        request->send(200, "text/plain",(char*)json_response);
    }
  
    void StartServer (AsyncWebServer *server) 
    {
        newNetworkConfig = WiFiSetup::getNetworkConfig();

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
        server->on("/net_control", HTTP_GET, netCommandRequestHandler);
        server->on("/net_save", HTTP_GET, netSaveRequestHandler);

        server->serveStatic("/", LittleFS, "/");

        Serial.println("Starting Server ...");

        server->begin();

    }
}