/*
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa library
 * @version 2.1.0
 * @author Christian Schwinne
 * @license MIT
 */
#define DEBUG_ESPALEXA
 
#ifdef DEBUG_ESPALEXA
 #define DEBUG(x)  Serial.print (x)
 #define DEBUGLN(x) Serial.println (x)
#else
 #define DEBUG(x)
 #define DEBUGLN(x)
#endif
 
#ifdef ARDUINO_ARCH_ESP32
#include "dependencies/webserver/WebServer.h" //https://github.com/bbx10/WebServer_tng
#include <WiFi.h>
#else
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#endif
#include <WiFiUdp.h>
#include "Espalexa.h"

#define MAXDEVICES 20 //this limit only has memory reasons, set it higher should you need to

uint8_t currentDeviceCount = 0;

EspalexaDevice* devices[MAXDEVICES] = {};
//Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!

WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
bool udpConnected = false;
unsigned int portMulti = 1900;      // local port to listen on
char packetBuffer[255]; //buffer to hold incoming packet,
String escapedMac; //lowercase mac address
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
        int len = UDP.read(packetBuffer, 255);
        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;
		DEBUGLN(request);
        if(request.indexOf("M-SEARCH") >= 0) {
          if(request.indexOf("upnp:rootdevice") > 0 || request.indexOf("device:basic:1") > 0) {
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
	if (currentDeviceCount >= MAXDEVICES) return false;
	devices[currentDeviceCount] = d;
	currentDeviceCount++;
	return true;
}

bool Espalexa::addDevice(String deviceName, CallbackBriFunction callback, uint8_t initialValue)
{
	DEBUG("Constructing device ");
	DEBUGLN((currentDeviceCount+1));
	if (currentDeviceCount >= MAXDEVICES) return false;
	EspalexaDevice* d = new EspalexaDevice(deviceName, callback, initialValue);
	return addDevice(d);
}

String Espalexa::deviceJsonString(uint8_t deviceId)
{
  if (deviceId < 1 || deviceId > currentDeviceCount) return "{}"; //error
  return "{\"type\":\"Extended color light\",\"manufacturername\":\"OpenSource\",\"swversion\":\"0.1\",\"name\":\""+ devices[deviceId-1]->getName() +"\",\"uniqueid\":\""+ WiFi.macAddress() +"-"+ (deviceId+1) +"\",\"modelid\":\"LST001\",\"state\":{\"on\":"+ boolString(devices[deviceId-1]->getValue()) +",\"bri\":"+ (String)(devices[deviceId-1]->getLastValue()-1) +",\"xy\":[0.00000,0.00000],\"colormode\":\"hs\",\"effect\":\"none\",\"ct\":500,\"hue\":0,\"sat\":0,\"alert\":\"none\",\"reachable\":true}}";
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
    int tempDeviceId = req.substring(req.indexOf("lights")+7).toInt();
    DEBUG("ls"); DEBUGLN(tempDeviceId);
    if (body.indexOf("bri")>0) {alexaDim(tempDeviceId, body.substring(body.indexOf("bri") +5).toInt()); return true;}
    if (body.indexOf("false")>0) {alexaOff(tempDeviceId); return true;}
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
	  res += "\r\n\r\nEspalexa library V2.1.0 by Christian Schwinne 2018";
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
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":true}}]";

  server->send(200, "text/xml", body.c_str());

  devices[deviceId-1]->setValue(devices[deviceId-1]->getLastValue());
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaOff(uint8_t deviceId)
{
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":false}}]";

  server->send(200, "application/json", body.c_str());
  devices[deviceId-1]->setValue(0);
  devices[deviceId-1]->doCallback();
}

void Espalexa::alexaDim(uint8_t deviceId, uint8_t briL)
{
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/bri\":"+ String(briL) +"}}]";

  server->send(200, "application/json", body.c_str());
  
  if (briL == 255)
  {
	 devices[deviceId-1]->setValue(255);
  } else {
	 devices[deviceId-1]->setValue(briL+1); 
  }
  devices[deviceId-1]->doCallback();
}

void Espalexa::respondToSearch() {
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

//EspalexaDevice Class

EspalexaDevice::EspalexaDevice(){}

EspalexaDevice::EspalexaDevice(String deviceName, CallbackBriFunction gnCallback, uint8_t initialValue) { //constructor
	
	_deviceName = deviceName;
	_callback = gnCallback;
	_val = initialValue;
	_val_last = _val;
}

EspalexaDevice::~EspalexaDevice(){/*nothing to destruct*/}

String EspalexaDevice::getName()
{
	return _deviceName;
}

uint8_t EspalexaDevice::getValue()
{
	return _val;
}

uint8_t EspalexaDevice::getLastValue()
{
	if (_val_last == 0) return 255;
	return _val_last;
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

void EspalexaDevice::doCallback()
{
	_callback(_val);
}