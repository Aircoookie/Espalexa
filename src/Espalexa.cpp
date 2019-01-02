/*
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.2.1
 * @author Christian Schwinne
 * @license MIT
 */
 
#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h" //https://github.com/bbx10/WebServer_tng
#include <WiFi.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>
#include "Espalexa.h"

uint8_t currentDeviceCount = 0;

EspalexaDevice* devices[ESPALEXA_MAXDEVICES] = {};
//Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!

WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
bool udpConnected = false;
unsigned int portMulti = 1900;      // local port to listen on
char packetBuffer[255]; //buffer to hold incoming packet,
String escapedMac=""; //lowercase mac address
Espalexa* instance;

Espalexa::Espalexa() { //constructor
  instance = this;
}

Espalexa::~Espalexa(){/*nothing to destruct*/}

#ifdef ARDUINO_ARCH_ESP32
bool Espalexa::begin(WebServer* externalServer)
#else
bool Espalexa::begin(ESP8266WebServer* externalServer)
#endif
{
  DEBUGLN("Espalexa Begin...");
  DEBUG("MAXDEVICES ");
  DEBUGLN(ESPALEXA_MAXDEVICES);
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();

  server = externalServer;
  udpConnected = connectUDP();

  if (udpConnected){
    
    startHttpServer();
    DEBUGLN("Done");
    return true;
  }
  DEBUGLN("Failed");
  return false;
}

void Espalexa::loop() {
  server->handleClient();
  
  if(udpConnected){    
    int packetSize = UDP.parsePacket();
    
    if(packetSize) {
      DEBUGLN("Got UDP!");
      int len = UDP.read(packetBuffer, 254);
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
  }
}

bool Espalexa::addDevice(EspalexaDevice* d)
{
  DEBUG("Adding device ");
  DEBUGLN((currentDeviceCount+1));
  if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
  devices[currentDeviceCount] = d;
  currentDeviceCount++;
  return true;
}

bool Espalexa::addDevice(String deviceName, CallbackBriFunction callback, uint8_t initialValue)
{
  DEBUG("Constructing device ");
  DEBUGLN((currentDeviceCount+1));
  if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
  EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
  return addDevice(d);
}

bool Espalexa::addDevice(String deviceName, CallbackColFunction callback, uint8_t initialValue)
{
  DEBUG("Constructing device ");
  DEBUGLN((currentDeviceCount+1));
  if (currentDeviceCount >= ESPALEXA_MAXDEVICES) return false;
  EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
  return addDevice(d);
}

String Espalexa::deviceJsonString(uint8_t deviceId)
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

bool Espalexa::handleAlexaApiCall(String req, String body) //basic implementation of Philips hue api functions needed for basic Alexa control
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
  res += "\r\n\r\nEspalexa library V2.2.1 by Christian Schwinne 2019";
  instance->server->send(200, "text/plain", res);
}

void serveNotFound()
{
  DEBUGLN("Not-Found HTTP call:");
  DEBUGLN("URI: " + instance->server->uri());
  DEBUGLN("Body: " + instance->server->arg(0));
  if(!instance->handleAlexaApiCall(instance->server->uri(),instance->server->arg(0)))
    instance->server->send(404, "text/plain", "Not Found (espalexa-internal)");
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
        
  instance->server->send(200, "text/xml", setup_xml.c_str());
  
  DEBUG("Sending :");
  DEBUGLN(setup_xml);
}

void Espalexa::startHttpServer() {
  if (server == nullptr) {
    #ifdef ARDUINO_ARCH_ESP32
    server = new WebServer(80);
    #else
    server = new ESP8266WebServer(80);  
    #endif
    server->onNotFound(serveNotFound);
  }
  
  server->on("/espalexa", HTTP_GET, servePage);
  
  server->on("/description.xml", HTTP_GET, serveDescription);
  
  server->begin();
}

void Espalexa::alexaOn(uint8_t deviceId)
{
  devices[deviceId-1]->setValue(devices[deviceId-1]->getLastValue());
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaOff(uint8_t deviceId)
{
  devices[deviceId-1]->setValue(0);
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaDim(uint8_t deviceId, uint8_t briL)
{
  if (briL == 255)
  {
   devices[deviceId-1]->setValue(255);
  } else {
   devices[deviceId-1]->setValue(briL+1); 
  }
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaCol(uint8_t deviceId, uint16_t hue, uint8_t sat)
{
  devices[deviceId-1]->setColor(hue, sat);
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaCt(uint8_t deviceId, uint16_t ct)
{
  devices[deviceId-1]->setColor(ct);
  devices[deviceId-1]->doCallback();
}

void Espalexa::respondToSearch() {
  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  String response = 
    "HTTP/1.1 200 OK\r\n"
    "HOST : 239.255.255.250:1900\r\n"
    "EXT:\r\n"
    "CACHE-CONTROL: max-age=100\r\n" // SSDP_INTERVAL
    "LOCATION: http://"+ String(s) +":80/description.xml\r\n"
    "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/1.17.0\r\n" // _modelName, _modelNumber
    "hue-bridgeid: "+ escapedMac +"\r\n"
    "ST: upnp:rootdevice\r\n"  // _deviceType
    "USN: uuid:2f402f80-da50-11e1-9b23-"+ escapedMac +"::upnp:rootdevice\r\n" // _uuid::_deviceType
    "\r\n";

  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  #ifdef ARDUINO_ARCH_ESP32
  UDP.write((uint8_t*)response.c_str(), response.length());
  #else
  UDP.write(response.c_str());
  #endif
  UDP.endPacket();                    
}

String Espalexa::boolString(bool st)
{
  return(st)?"true":"false";
}

bool Espalexa::connectUDP(){
  #ifdef ARDUINO_ARCH_ESP32
  return UDP.beginMulticast(ipMulti, portMulti);
  #else
  return UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti);
  #endif
}

uint8_t Espalexa::toPercent(uint8_t bri)
{
  uint16_t perc = bri * 100;
  return perc / 255;
}

//EspalexaDevice Class

EspalexaDevice::EspalexaDevice(){}

EspalexaDevice::EspalexaDevice(String deviceName, CallbackBriFunction gnCallback, uint8_t initialValue) { //constructor
  
  _deviceName = deviceName;
  _callback = gnCallback;
  _val = initialValue;
  _val_last = _val;
}

EspalexaDevice::EspalexaDevice(String deviceName, CallbackColFunction gnCallback, uint8_t initialValue) { //constructor for color device
  
  _deviceName = deviceName;
  _callbackCol = gnCallback;
  _callback = nullptr;
  _val = initialValue;
  _val_last = _val;
}

EspalexaDevice::~EspalexaDevice(){/*nothing to destruct*/}

bool EspalexaDevice::isColorDevice()
{
  //if brightness-only callback is null, we have color device
  return (_callback == nullptr);
}

bool EspalexaDevice::isColorTemperatureMode()
{
  return _ct;
}

String EspalexaDevice::getName()
{
  return _deviceName;
}

uint8_t EspalexaDevice::getValue()
{
  return _val;
}

uint16_t EspalexaDevice::getHue()
{
  return _hue;
}

uint8_t EspalexaDevice::getSat()
{
  return _sat;
}

uint16_t EspalexaDevice::getCt()
{
  if (_ct == 0) return 500;
  return _ct;
}

uint32_t EspalexaDevice::getColorRGB()
{
  uint8_t rgb[3];
  
  if (isColorTemperatureMode())
  {
    //this is only an approximation using WS2812B with gamma correction enabled
    //TODO replace with better formula
    if (_ct > 475) {
      rgb[0]=255;rgb[1]=199;rgb[2]=92;//500
    } else if (_ct > 425) {
      rgb[0]=255;rgb[1]=213;rgb[2]=118;//450
    } else if (_ct > 375) {
      rgb[0]=255;rgb[1]=216;rgb[2]=118;//400
    } else if (_ct > 325) {
      rgb[0]=255;rgb[1]=234;rgb[2]=140;//350
    } else if (_ct > 275) {
      rgb[0]=255;rgb[1]=243;rgb[2]=160;//300
    } else if (_ct > 225) {
      rgb[0]=250;rgb[1]=255;rgb[2]=188;//250
    } else if (_ct > 175) {
      rgb[0]=247;rgb[1]=255;rgb[2]=215;//200
    } else {
      rgb[0]=237;rgb[1]=255;rgb[2]=239;//150
    }
  } else { //hue + sat mode
    float h = ((float)_hue)/65535.0;
    float s = ((float)_sat)/255.0;
    byte i = floor(h*6);
    float f = h * 6-i;
    float p = 255 * (1-s);
    float q = 255 * (1-f*s);
    float t = 255 * (1-(1-f)*s);
    switch (i%6) {
      case 0: rgb[0]=255,rgb[1]=t,rgb[2]=p;break;
      case 1: rgb[0]=q,rgb[1]=255,rgb[2]=p;break;
      case 2: rgb[0]=p,rgb[1]=255,rgb[2]=t;break;
      case 3: rgb[0]=p,rgb[1]=q,rgb[2]=255;break;
      case 4: rgb[0]=t,rgb[1]=p,rgb[2]=255;break;
      case 5: rgb[0]=255,rgb[1]=p,rgb[2]=q;
    }
  }
  return ((rgb[0] << 16) | (rgb[1] << 8) | (rgb[2]));
}

uint8_t EspalexaDevice::getLastValue()
{
  if (_val_last == 0) return 255;
  return _val_last;
}

//you need to re-discover the device for the Alexa name to change
void EspalexaDevice::setName(String name)
{
  _deviceName = name;
}

void EspalexaDevice::setValue(uint8_t val)
{
  if (_val != 0)
  {
    _val_last = _val;
  }
  if (val != 0)
  {
    _val_last = val;
  }
  _val = val;
}

void EspalexaDevice::setPercent(uint8_t perc)
{
  uint16_t val = perc * 255;
  val /= 100;
  if (val > 255) val = 255;
  setValue(val);
}

void EspalexaDevice::setColor(uint16_t hue, uint8_t sat)
{
  _hue = hue;
  _sat = sat;
  _ct = 0;
}

void EspalexaDevice::setColor(uint16_t ct)
{
  _ct = ct;
}

void EspalexaDevice::doCallback()
{
  (_callback != nullptr) ? _callback(_val) : _callbackCol(_val, getColorRGB());
}