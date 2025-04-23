#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "LittleFS.h"
#include <DNSServer.h>

#include "WiFiSetup.h"
#include "Webserver.h"

namespace WiFiSetup 
{
  // File paths to save input values permanently
  const char* ssidPath = "/ssid.txt";
  const char* passPath = "/pass.txt";
  const char* ipPath = "/ip.txt";
  const char* gatewayPath = "/gateway.txt";
  const char* nodenamePath = "/nodename.txt";
  const char* netmaskPath = "/netmask.txt";
  const char* dhcpPath = "/dhcp.txt";
  const char* apmodePath = "/apmode.txt";

  String ssid;
  String pass;
  String ip;
  String gateway;
  String netmask;
  String nodename;
  bool dhcp;
  bool apmode;

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
  void setAPMODE (bool value) {apmode = value; };
  bool getAPMODE() { return apmode;};
  void setNetworkConfig (networkConfig config) {
    ssid = config.ssid;
    pass = config.pass;
    ip = config.ip;
    netmask = config.netmask;
    gateway = config.gateway;
    nodename = config.nodename;
    dhcp = config.dhcp; 
    apmode = config.apmode;

    writeFile(LittleFS, ssidPath, ssid.c_str());    
    writeFile(LittleFS, passPath, pass.c_str());    
    writeFile(LittleFS, ipPath, ip.c_str());    
    writeFile(LittleFS, netmaskPath, netmask.c_str());    
    writeFile(LittleFS, gatewayPath, gateway.c_str());    
    writeFile(LittleFS, nodenamePath, nodename.c_str());    
    writeFile(LittleFS, dhcpPath, dhcp ? "1" : "0");    
    writeFile(LittleFS, apmodePath, apmode ? "1" : "0");    
  };

  networkConfig getNetworkConfig() { 
    networkConfig config;
    config.ssid = ssid;
    config.pass = pass;
    config.ip = ip;
    config.netmask = netmask;
    config.gateway = gateway;
    config.nodename = nodename;
    config.dhcp = dhcp;
    config.apmode = apmode;
    return config;
  };

  // Search for parameter in HTTP POST request
  const char* PARAM_INPUT_1 = "ssid";
  const char* PARAM_INPUT_2 = "pass";
  const char* PARAM_INPUT_3 = "ip";
  const char* PARAM_INPUT_4 = "gateway";
  const char* PARAM_INPUT_5 = "nodename";

  DNSServer dnsServer;
  bool dnsServerActive = false;
  networkConfig initialNetworkConfig;
  
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

  bool cheatInitialConfig() 
  {
      // Start the scan
      int n = WiFi.scanNetworks();

      if (n == 0) {
        Serial.println("No networks found.");
      } else {
        Serial.println("Networks found:");
        for (int i = 0; i < n; ++i) {
          Serial.printf("%d: %s (RSSI: %d, Encryption: %s)\n", i + 1,
                        WiFi.SSID(i).c_str(),
                        WiFi.RSSI(i),
                        WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
          if (WiFi.SSID(i) == "turbos_wlan") {
            Serial.println("Cheating with turbos_wlan");
            apmode = false;
            dhcp = true,
            nodename = "HopeCam";
            pass = "11223344556677889900112233";
            ssid = "turbos_wlan";

            WiFi.scanDelete();
            return true;
          } 
          else if (WiFi.SSID(i) == "turbos_wlan2") 
          {
            Serial.println("Cheating with turbos_wlan2");
            apmode = false;
            dhcp = true,
            nodename = "HopeCam";
            pass = "Tiger4711";
            ssid = "turbos_wlan2";

            WiFi.scanDelete();
            return true;
          }
        }
      }
    
      WiFi.scanDelete();

      return false;
  }
  
  // setup WiFi
  // mode: WIFI_STARTUP_NORMAL    - start in AP mode and try to configure if no config is available. 
  //                                start in configured mode with stored parameters after config is done
  //       WIFI_STARTUP_AP_ALWAYS - always run in AP mode providing fixed setup
  // 
  void setup ()
  {
    if (!LittleFS.exists("/wifimanager.html"))
      Serial.println("no wifimanager.html found!");

    if (!cheatInitialConfig()) {

      // Load values saved in LittleFS
      ssid = readFile(LittleFS, ssidPath);
      pass = readFile(LittleFS, passPath);
      ip = readFile(LittleFS, ipPath);
      gateway = readFile(LittleFS, gatewayPath);
      netmask = readFile(LittleFS, netmaskPath);
      nodename = readFile(LittleFS, nodenamePath);
      dhcp = (readFile(LittleFS, dhcpPath) == "0") ? 0 : 1; 
      apmode = (readFile(LittleFS, apmodePath) == "0") ? 0 : 1; 
    }

    #ifdef DEBUG_WIFISETUP
      //apmode = 0;
      //ssid = "turbos_wlan";
      //pass = "11223344556677889900112233";      
    #endif

    if (apmode)
      Serial.println("WiFi Mode: AP always");
    else
      Serial.println("WiFi Mode: normal");

    #ifdef DEBUG_WIFISETUP
      Serial.println(ssid);
      Serial.println(pass);
      Serial.println(dhcp);
      Serial.println(ip);
      Serial.println(gateway);
      Serial.println(netmask);
      Serial.println(nodename);
    #endif
  }

  bool initWifi(AsyncWebServer *server) 
  {  
    IPAddress localIP;
    IPAddress localGateway;
  
    bool check = checkSetup();
    dnsServerActive = false;
    
    if (check && (apmode == 0)) {
      Serial.println("WiFi setup OK.");
      return true;
    }

    Serial.println("Setting UP AP (Access Point)");

    if (check) 
    {
      Serial.println("... using configured params");
      Serial.println(ssid);
      Serial.println(pass);
      WiFi.softAP(ssid.c_str(), pass.c_str());
    }
    else
    {
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

    if (
      ssid == "") {
      Serial.println("Undefined SSID");
      return false;
    }

    if (nodename != "")
      WiFi.setHostname(nodename.c_str());

    if (apmode == 1)
      return true;

    WiFi.mode(WIFI_STA);

    localIP.fromString(ip.c_str());
    localGateway.fromString(gateway.c_str());
    if (netmask != "")
      localNetmask.fromString(netmask.c_str());

    if (!dhcp) {
      if (!WiFi.config(localIP, localGateway, localNetmask)) {
        Serial.println("STA Failed to configure - redo config please");
        return false;
      }
    }

    WiFi.begin(ssid.c_str(), pass.c_str());
    Serial.println("Connecting to WiFi...");
    WiFi.waitForConnectResult();

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

  void setupSaveRequestHandler(AsyncWebServerRequest *request){
    request->send(200, "text/plain");
    WiFiSetup::setNetworkConfig(initialNetworkConfig);
    delay(3000);

    ESP.restart();
  }

  void setupCommandRequestHandler(AsyncWebServerRequest *request){
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
        if (WEBSERVER::checkIPfailure(var, request))
          return;
        initialNetworkConfig.ssid = val;
    } else if(var == "gateway") {
      if (WEBSERVER::checkIPfailure(var, request))
        return;
        initialNetworkConfig.gateway = val;
    } else if(var == "netmask") {
      if (WEBSERVER::checkIPfailure(var, request))
        return;
        initialNetworkConfig.netmask = val;
    } else if(var == "nodename") {
      initialNetworkConfig.nodename = val;
    } else if(var == "ssid") {
      initialNetworkConfig.ssid = val;
    } else if(var == "password") {
      initialNetworkConfig.pass = val;
    } else if(var == "dhcp") {
      initialNetworkConfig.dhcp = val.toInt();
    } else if(var == "apmode") {
      initialNetworkConfig.apmode = val.toInt();
    }

    request->send(200, "text/plain");
  }

  void setupStatusRequestHandler(AsyncWebServerRequest *request){
    static uint8_t json_response[1024];

    char * p = (char *)json_response;
    *p++ = '{';

    p+=sprintf(p, "\"ssid\":\"%s\",", initialNetworkConfig.ssid.c_str());
    p+=sprintf(p, "\"password\":\"%s\",", initialNetworkConfig.pass.c_str());
    p+=sprintf(p, "\"dhcp\":%u,", initialNetworkConfig.dhcp);
    p+=sprintf(p, "\"apmode\":%u,", initialNetworkConfig.apmode);
    p+=sprintf(p, "\"ip\":\"%s\",", initialNetworkConfig.ip.c_str());
    p+=sprintf(p, "\"netmask\":\"%s\",", initialNetworkConfig.netmask.c_str());
    p+=sprintf(p, "\"gateway\":\"%s\",", initialNetworkConfig.gateway.c_str());
    p+=sprintf(p, "\"nodename\":\"%s\"", initialNetworkConfig.nodename.c_str());
    *p++ = '}';
    *p++ = 0;

    Serial.println(WiFiSetup::getSSID());
    Serial.println(String((char*)json_response));
    Serial.println("Len:" + (String)(strlen((char*)json_response)));
    request->send(200, "text/plain",(char*)json_response);
}

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

    server->on("/setup_status", HTTP_GET, setupStatusRequestHandler);
    server->on("/setup_control", HTTP_GET, setupCommandRequestHandler);
    server->on("/setup_save", HTTP_GET, setupSaveRequestHandler);

    server->serveStatic("/", LittleFS, "/");

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
    Serial.printf("Read content: %s\r\n", fileContent);
    return fileContent;
  }

  // Write file to LittleFS
  void writeFile(fs::FS& fs, const char* path, const char* message) {
    Serial.printf("Writing file: %s\r\n", path);
    Serial.printf("Writing content: %s\r\n", message);

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

