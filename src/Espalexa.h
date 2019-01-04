#ifndef Espalexa_h
#define Espalexa_h

/*
 * Alexa Voice On/Off/Brightness/Color Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.3.0
 * @author Christian Schwinne
 * @license MIT
 */

#include "Arduino.h"

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include "dependencies/webserver/WebServer.h" //https://github.com/bbx10/WebServer_tng
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>

#ifndef ESPALEXA_ASYNC
#define ESPALEXA_ASYNC 0
#endif

#ifndef ESPALEXA_MAXDEVICES
#pragma message "not defined"
#define ESPALEXA_MAXDEVICES 20 //this limit only has memory reasons, set it higher should you need to
#else
#pragma message "was defined"
#endif

#ifdef ESPALEXA_DEBUG
 #pragma message "Espalexa Debugging enabled"
 #define DEBUG(x)  Serial.print (x)
 #define DEBUGLN(x) Serial.println (x)
#else
 #define DEBUG(x)
 #define DEBUGLN(x)
#endif

#include "EspalexaDevice.h"

class Espalexa {
private:
  #ifdef ARDUINO_ARCH_ESP32
  WebServer* server;
  #else
  ESP8266WebServer* server;
  #endif
  uint8_t currentDeviceCount = 0;

  EspalexaDevice* devices[ESPALEXA_MAXDEVICES] = {};
  //Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!
  
  WiFiUDP espalexaUdp;
  IPAddress ipMulti;
  bool udpConnected = false;
  char packetBuffer[255]; //buffer to hold incoming udp packet
  String escapedMac=""; //lowercase mac address
  
  
  String deviceJsonString(uint8_t deviceId)
  {
    if (deviceId < 1 || deviceId > currentDeviceCount) return "{}"; //error
    EspalexaDevice* dev = devices[deviceId-1];
    String json = "{\"type\":\"";
    json += dev->isColorDevice() ? "Extended color light" : "Dimmable light";
    json += "\",\"manufacturername\":\"OpenSource\",\"swversion\":\"0.1\",\"name\":\"";
    json += dev->getName();
    json += "\",\"uniqueid\":\""+ WiFi.macAddress() +"-"+ (deviceId+1) ;
    json += "\",\"modelid\":\"LST001\",\"state\":{\"on\":";
    json += boolString(dev->getValue()) +",\"bri\":"+ (String)(dev->getLastValue()-1) ;
    if (dev->isColorDevice()) 
    {
      json += ",\"xy\":[0.00000,0.00000],\"colormode\":\"";
      json += (dev->isColorTemperatureMode()) ? "ct":"hs";
      json += "\",\"effect\":\"none\",\"ct\":" + (String)(dev->getCt()) + ",\"hue\":" + (String)(dev->getHue()) + ",\"sat\":" + (String)(dev->getSat());
    }
    json +=",\"alert\":\"none\",\"reachable\":true}}";
    return json;
  }
  
  void servePage()
  {
    DEBUGLN("HTTP Req espalexa ...\n");
    String res = "Hello from Espalexa!\r\n\r\n";
    for (int i=0; i<currentDeviceCount; i++)
    {
      res += "Value of device " + String(i+1) + " (" + devices[i]->getName() + "): " + String(devices[i]->getValue()) + "\r\n";
    }
    res += "\r\nFree Heap: " + (String)ESP.getFreeHeap();
    res += "\r\nUptime: " + (String)millis();
    res += "\r\n\r\nEspalexa library V2.2.0 by Christian Schwinne 2019";
    server->send(200, "text/plain", res);
  }

  void serveNotFound()
  {
    DEBUGLN("Not-Found HTTP call:");
    DEBUGLN("URI: " + server->uri());
    DEBUGLN("Body: " + server->arg(0));
    if(!handleAlexaApiCall(server->uri(),server->arg(0)))
      server->send(404, "text/plain", "Not Found (espalexa-internal)");
  }

  void serveDescription()
  {
    DEBUGLN("# Responding to description.xml ... #\n");
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String setup_xml = "<?xml version=\"1.0\" ?>"
        "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
        "<specVersion><major>1</major><minor>0</minor></specVersion>"
        "<URLBase>http://"+ String(s) +":80/</URLBase>"
        "<device>"
          "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
          "<friendlyName>Philips hue ("+ String(s) +")</friendlyName>"
          "<manufacturer>Royal Philips Electronics</manufacturer>"
          "<manufacturerURL>http://www.philips.com</manufacturerURL>"
          "<modelDescription>Philips hue Personal Wireless Lighting</modelDescription>"
          "<modelName>Philips hue bridge 2012</modelName>"
          "<modelNumber>929000226503</modelNumber>"
          "<modelURL>http://www.meethue.com</modelURL>"
          "<serialNumber>"+ escapedMac +"</serialNumber>"
          "<UDN>uuid:2f402f80-da50-11e1-9b23-"+ escapedMac +"</UDN>"
          "<presentationURL>index.html</presentationURL>"
          "<iconList>"
          "  <icon>"
          "    <mimetype>image/png</mimetype>"
          "    <height>48</height>"
          "    <width>48</width>"
          "    <depth>24</depth>"
          "    <url>hue_logo_0.png</url>"
          "  </icon>"
          "  <icon>"
          "    <mimetype>image/png</mimetype>"
          "    <height>120</height>"
          "    <width>120</width>"
          "    <depth>24</depth>"
          "    <url>hue_logo_3.png</url>"
          "  </icon>"
          "</iconList>"
        "</device>"
        "</root>";
          
    server->send(200, "text/xml", setup_xml.c_str());
    
    DEBUG("Sending :");
    DEBUGLN(setup_xml);
  }
  
  void startHttpServer() {
    if (server == nullptr) {
      #ifdef ARDUINO_ARCH_ESP32
      server = new WebServer(80);
      #else
      server = new ESP8266WebServer(80);  
      #endif
      server->onNotFound([=](){serveNotFound();});
    }

    server->on("/espalexa", HTTP_GET, [=](){servePage();});

    server->on("/description.xml", HTTP_GET, [=](){serveDescription();});

    server->begin();
  }

  void alexaOn(uint8_t deviceId)
  {
    devices[deviceId-1]->setValue(devices[deviceId-1]->getLastValue());
    devices[deviceId-1]->doCallback();
  }

  void alexaOff(uint8_t deviceId)
  {
    devices[deviceId-1]->setValue(0);
    devices[deviceId-1]->doCallback();
  }

  void alexaDim(uint8_t deviceId, uint8_t briL)
  {
    if (briL == 255)
    {
     devices[deviceId-1]->setValue(255);
    } else {
     devices[deviceId-1]->setValue(briL+1); 
    }
    devices[deviceId-1]->doCallback();
  }

  void alexaCol(uint8_t deviceId, uint16_t hue, uint8_t sat)
  {
    devices[deviceId-1]->setColor(hue, sat);
    devices[deviceId-1]->doCallback();
  }

  void alexaCt(uint8_t deviceId, uint16_t ct)
  {
    devices[deviceId-1]->setColor(ct);
    devices[deviceId-1]->doCallback();
  }

  void respondToSearch() {
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response = 
      "HTTP/1.1 200 OK\r\n"
      "EXT:\r\n"
      "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
      "LOCATION: http://"+ String(s) +":80/description.xml\r\n"
      "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
      "hue-bridgeid: "+ escapedMac +"\r\n"
      "ST: urn:schemas-upnp-org:device:basic:1\r\n"  // _deviceType
      "USN: uuid:2f402f80-da50-11e1-9b23-"+ escapedMac +"::upnp:rootdevice\r\n" // _uuid::_deviceType
      "\r\n";

    espalexaUdp.beginPacket(espalexaUdp.remoteIP(), espalexaUdp.remotePort());
    #ifdef ARDUINO_ARCH_ESP32
    espalexaUdp.write((uint8_t*)response.c_str(), response.length());
    #else
    espalexaUdp.write(response.c_str());
    #endif
    espalexaUdp.endPacket();                    
  }

  String boolString(bool st)
  {
    return(st)?"true":"false";
  }

  uint8_t toPercent(uint8_t bri)
  {
    uint16_t perc = bri * 100;
    return perc / 255;
  }

public:
  Espalexa(){}

  #ifdef ARDUINO_ARCH_ESP32
  bool begin(WebServer* externalServer = nullptr)
  #else
  bool begin(ESP8266WebServer* externalServer = nullptr)
  #endif
  {
    DEBUGLN("Espalexa Begin...");
    DEBUG("MAXDEVICES ");
    DEBUGLN(ESPALEXA_MAXDEVICES);
    escapedMac = WiFi.macAddress();
    escapedMac.replace(":", "");
    escapedMac.toLowerCase();

    server = externalServer;
    #ifdef ARDUINO_ARCH_ESP32
    udpConnected = espalexaUdp.beginMulticast(IPAddress(239, 255, 255, 250), 1900);
    #else
    udpConnected = espalexaUdp.beginMulticast(WiFi.localIP(), IPAddress(239, 255, 255, 250), 1900);
    #endif

    if (udpConnected){
      
      startHttpServer();
      DEBUGLN("Done");
      return true;
    }
    DEBUGLN("Failed");
    return false;
  }

  void loop() {
    if (server == nullptr) return; //only if begin() was not called
    server->handleClient();
    
    if (!udpConnected) return;   
    int packetSize = espalexaUdp.parsePacket();    
    if (!packetSize) return; //no new udp packet
    
    DEBUGLN("Got UDP!");
    int len = espalexaUdp.read(packetBuffer, 254);
    if (len > 0) {
      packetBuffer[len] = 0;
    }

    String request = packetBuffer;
    DEBUGLN(request);
    if(request.indexOf("M-SEARCH") >= 0) {
      if(request.indexOf("upnp:rootdevice") > 0 || request.indexOf("asic:1") > 0) {
        DEBUGLN("Responding search req...");
        respondToSearch();
      }
    }
  }

  bool addDevice(EspalexaDevice* d)
  {
    DEBUG("Adding device ");
    DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
    devices[currentDeviceCount] = d;
    currentDeviceCount++;
    return true;
  }

  bool addDevice(String deviceName, CallbackBriFunction callback, uint8_t initialValue = 0)
  {
    DEBUG("Constructing device ");
    DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
    EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }

  bool addDevice(String deviceName, CallbackColFunction callback, uint8_t initialValue = 0)
  {
    DEBUG("Constructing device ");
    DEBUGLN((currentDeviceCount+1));
    if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
    EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
    return addDevice(d);
  }
  
  bool handleAlexaApiCall(String req, String body) //basic implementation of Philips hue api functions needed for basic Alexa control
  {
    DEBUGLN("AlexaApiCall");
    if (req.indexOf("api") <0) return false; //return if not an API call
    DEBUGLN("ok");
    
    if (body.indexOf("devicetype") > 0) //client wants a hue api username, we dont care and give static
    {
      DEBUGLN("devType");
      server->send(200, "application/json", "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]");
      return true;
    }

    if (req.indexOf("state") > 0) //client wants to control light
    {
      server->send(200, "application/json", "[{\"success\":true}]"); //short valid response
    
      int tempDeviceId = req.substring(req.indexOf("lights")+7).toInt();
      DEBUG("ls"); DEBUGLN(tempDeviceId);
      if (body.indexOf("false")>0) {alexaOff(tempDeviceId); return true;}
      if (body.indexOf("bri")>0  ) {alexaDim(tempDeviceId, body.substring(body.indexOf("bri") +5).toInt()); return true;}
      if (body.indexOf("hue")>0  ) {alexaCol(tempDeviceId, body.substring(body.indexOf("hue") +5).toInt(), body.substring(body.indexOf("sat") +5).toInt()); return true;}
      if (body.indexOf("ct") >0  ) {alexaCt (tempDeviceId, body.substring(body.indexOf("ct") +4).toInt()); return true;}
      alexaOn(tempDeviceId);
      
      return true;
    }
    
    int pos = req.indexOf("lights");
    if (pos > 0) //client wants light info
    {
      int tempDeviceId = req.substring(pos+7).toInt();
      DEBUG("l"); DEBUGLN(tempDeviceId);

      if (tempDeviceId == 0) //client wants all lights
      {
        DEBUGLN("lAll");
        String jsonTemp = "{";
        for (int i = 0; i<currentDeviceCount; i++)
        {
          jsonTemp += "\"" + String(i+1) + "\":";
          jsonTemp += deviceJsonString(i+1);
          if (i < currentDeviceCount-1) jsonTemp += ",";
        }
        jsonTemp += "}";
        server->send(200, "application/json", jsonTemp);
      } else //client wants one light (tempDeviceId)
      {
        server->send(200, "application/json", deviceJsonString(tempDeviceId));
      }
      
      return true;
    }

    //we dont care about other api commands at this time and send empty JSON
    server->send(200, "application/json", "{}");
    return true;
  }
  
  String getEscapedMac()
  {
    return escapedMac;
  }
  
  ~Espalexa(){delete devices;} //note: Espalexa is NOT meant to be destructed
};

#endif

