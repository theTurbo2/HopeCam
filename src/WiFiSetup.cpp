#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <DNSServer.h>

#include "WiFiSetup.h"

namespace WiFiSetup 
{
  String ssid;
  String pass;
  String ip;
  String gateway;
  String netmask;
  String nodename;
  bool dhcp;

  void setSSID (String value) {ssid = value; };
  String getSSID() { return ssid;};
  void setPASS (String value) {pass = value; };
  String getPASS() { return pass;};
  void setIP (String value) {ip = value; };
  String getIP() { return ip;};
  void setGATEWAY (String value) {gateway = value; };
  String getGATEWAY() { return gateway;};
  void setNETMASK (String value) {netmask = value; };
  String getNETMASK() { return netmask;};
  void setNODENAME (String value) {nodename = value; };
  String getNODENAME () { return nodename;};
  void setDHCP (bool value) {dhcp = value; };
  bool getDHCP() { return dhcp;};

  // Search for parameter in HTTP POST request
  const char* PARAM_INPUT_1 = "ssid";
  const char* PARAM_INPUT_2 = "pass";
  const char* PARAM_INPUT_3 = "ip";
  const char* PARAM_INPUT_4 = "gateway";
  const char* PARAM_INPUT_5 = "nodename";

  // File paths to save input values permanently
  const char* ssidPath = "/ssid.txt";
  const char* passPath = "/pass.txt";
  const char* ipPath = "/ip.txt";
  const char* gatewayPath = "/gateway.txt";
  const char* nodenamePath = "/nodename.txt";

  int wifimode;
  DNSServer dnsServer;
  bool dnsServerActive = false;

  #define DEBUG_WIFISETUP 1

  // Timer variables
  unsigned long previousMillis = 0;
  const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)


  void resetConfig() {
      LittleFS.remove(ssidPath);
      Serial.println("Double Reset. Clean SSID and reboot ...");
      delay(3000);
      ESP.restart();
  }

  // setup WiFi
  // mode: WIFI_STARTUP_NORMAL    - start in AP mode and try to configure if no config is available. 
  //                                start in configured mode with stored parameters after config is done
  //       WIFI_STARTUP_AP_ALWAYS - always run in AP mode providing fixed setup
  // 
  void setup (int mode)
  {
    wifimode = mode;

    if (!LittleFS.exists("/wifimanager.html"))
      Serial.println("no wifimanager.html found!");

    // Load values saved in LittleFS
    ssid = readFile(LittleFS, ssidPath);
    pass = readFile(LittleFS, passPath);
    ip = readFile(LittleFS, ipPath);
    gateway = readFile(LittleFS, gatewayPath);
    nodename = readFile(LittleFS, nodenamePath);

    #ifdef DEBUG_WIFISETUP
      if (wifimode == WIFI_STARTUP_AP_ALWAYS)
        Serial.println("WiFi Mode: AP always");
      else
        Serial.println("WiFi Mode: normal");

      Serial.println(ssid);
      Serial.println(pass);
      Serial.println(ip);
      Serial.println(gateway);
      Serial.println(nodename);
    #endif
  }

  bool initWifi(AsyncWebServer *server) 
  {  
    IPAddress localIP;
    IPAddress localGateway;
  
    bool check = checkSetup();
    dnsServerActive = false;
    
    if (check && (wifimode == WIFI_STARTUP_NORMAL)) {
      Serial.println("WiFi setup OK.");
      return true;
    }

    Serial.println("Setting AP (Access Point)");

    // NULL sets an open Access Point
    if (check) 
    {
      Serial.println("... using configured params");
      Serial.println(ssid);
      Serial.println(pass);
      WiFi.softAP(ssid.c_str(), pass.c_str());
    }
    else
    {
      Serial.println("... using initial params");
      WiFi.softAP("WIFI-SETUP", NULL);
    }

    IPAddress IP = WiFi.softAPIP();
    ip = IP.toString();
    gateway = ip;

    Serial.print("AP IP address: ");
    Serial.println(IP);

    if (!dnsServer.start(53, "*", IP)) {
      Serial.println("Failed to start DNS");
    } else {
      Serial.println("DNS started - pointing all request to " + IP.toString());
      dnsServerActive = true;
    }

    if (!check) {
      runConfigServer(server);
  	  server->begin();

      return false;
    }

    return true;
  }
    
  bool checkSetup() 
  {
    IPAddress localIP;
    IPAddress localGateway;
    IPAddress localNetmask(255,255,255,0);

    ssid = "turbos_wlan";
    pass = "11223344556677889900112233";
    
    if (
      ssid == "") {
      Serial.println("Undefined SSID");
      return false;
    }

    if (nodename != "")
      WiFi.setHostname(nodename.c_str());

    if (wifimode == WIFI_STARTUP_AP_ALWAYS)
      return true;

    WiFi.mode(WIFI_STA);

    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (netmask != "")
      localNetmask.fromString(netmask.c_str());

    if (ip != "") {
      if (!WiFi.config(localIP, localGateway, localNetmask)) {
        Serial.println("STA Failed to configure - redo config please");
        return false;
      }
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to WiFi...");
    ip = WiFi.localIP().toString();
    netmask = WiFi.subnetMask().toString();
    gateway = WiFi.gatewayIP().toString();
    dhcp = true;


    unsigned long currentMillis = millis();
    previousMillis = currentMillis;

    while (WiFi.status() != WL_CONNECTED) {
      currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
        Serial.println("Failed to connect.");
        return false;
      }
    }

    Serial.println(WiFi.localIP());
    return true;
  };

  void runConfigServer(AsyncWebServer *server) 
  {
    Serial.println("running the config web server");

    // Web Server Root URL
    server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
      Serial.println("/ requested for wifimanager");
      if (!LittleFS.exists("/wifimanager.html"))
        Serial.println("no wifimanager.html");
    
      request->send(LittleFS, "/wifimanager.html", "text/html");
    });

    server->serveStatic("/", LittleFS, "/");

    server->on("/", HTTP_POST, [](AsyncWebServerRequest* request) {
      int params = request->params();
      Serial.println("post received");
    
      for (int i = 0; i < params; i++) {
        const AsyncWebParameter* p = request->getParam(i);
        if (p->isPost()) {
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(LittleFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(LittleFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(LittleFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(LittleFS, gatewayPath, gateway.c_str());
          }
          // HTTP POST nodename value
          if (p->name() == PARAM_INPUT_5) {
            nodename = p->value().c_str();
            Serial.print("Nodename set to: ");
            Serial.println(nodename);
            // Write file to save value
            writeFile(LittleFS, nodenamePath, nodename.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      request->send(200, "text/plain", "Network config done. Device will restart, connect to your router and go to IP address: " + ip);
      delay(3000);
      ESP.restart();
    });
  }

  void loop() {
    if (dnsServerActive)
      dnsServer.processNextRequest();
  }
/*
  void reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
      int reconnectCount = 0;
      while (WiFi.status() != WL_CONNECTED)
      {
        if (reconnectCount > 5) {
          Serial.println("restarting wifi");
          WiFi.mode(WIFI_OFF);
          delay(1000);
          WiFi.mode(WIFI_STA);
          WiFi.begin(ssid, password);
          reconnectCount = 0;
        }
        delay(1000);
        Serial.print(".");
        reconnectCount++;
      }
      Serial.println("");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }
  }
*/

  // Read File from LittleFS
  String readFile(fs::FS& fs, const char* path) {
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if (!file || file.isDirectory()) {
      Serial.println("- failed to open file for reading");
      return String();
    }

    String fileContent;
    while (file.available()) {
      fileContent = file.readStringUntil('\n');
      break;
    }
    return fileContent;
  }

  // Write file to LittleFS
  void writeFile(fs::FS& fs, const char* path, const char* message) {
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file) {
      Serial.println("- failed to open file for writing");
      return;
    }
    if (file.print(message)) {
      Serial.println("- file written");
    } else {
      Serial.println("- write failed");
    }
  }
}

