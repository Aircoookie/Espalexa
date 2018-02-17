/*
 * If you don't want to use the new library for some reason, all the functionality is in this standalone sketch.
 * 
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa sketch
 * @version 1.2
 * @author Christian Schwinne
 * @license MIT
 */
 
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#include "src/dependencies/webserver/WebServer.h" //https://github.com/bbx10/WebServer_tng
#else
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif
#include <WiFiUdp.h>

#define DEVICES 3 //CHANGE: how many devices you need

const char* ssid = "...";  // CHANGE: Wifi name
const char* password = "...";  // CHANGE: Wifi password

String friendlyName[DEVICES];        // Array to hold names
//CHANGE:  Alexa invocation names in line 84!

uint8_t initialVals[DEVICES]; //Array to hold start values

int yourVal[DEVICES]; //Alexa will change this (off=0, on=255, brightness steps in-between)

//Keep in mind that Device IDs go from 1 to DEVICES, cpp arrays from 0 to DEVICES-1!!
int lastDeviceChanged = 0;

void prepareIds();
boolean connectWifi();
boolean connectUDP();
void startHttpServer();
void respondToSearch();

WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
#ifdef ARDUINO_ARCH_ESP32
WebServer server(80);
#else
ESP8266WebServer server(80);
#endif
boolean udpConnected = false;
unsigned int portMulti = 1900;      // local port to listen on
unsigned int localPort = 1900;      // local port to listen on
boolean wifiConnected = false;
char packetBuffer[255]; //buffer to hold incoming packet,
String escapedMac; //lowercase mac address
boolean cannotConnectToWifi = false;

//what to do if switched
void actionOn(uint8_t deviceId)
{
  if (deviceId > 0 && deviceId <= DEVICES)
  yourVal[deviceId-1] = 255;
}

void actionOff(uint8_t deviceId)
{
  if (deviceId > 0 && deviceId <= DEVICES)
  yourVal[deviceId-1] = 0;
}

void actionDim(uint8_t deviceId, int b) //1-255 range
{
  if (deviceId > 0 && deviceId <= DEVICES)
  yourVal[deviceId-1] = b;
}

void setup() {
  Serial.begin(115200);
  prepareIds();
  
  //BEGIN OF USER LIGHT CONFIG
  friendlyName[0] = "A light 1"; //CHANGE: Alexa device names
  friendlyName[1] = "A light 2";
  friendlyName[2] = "A light 3";
  //friendlyName[3] = "A light 4"; //more lights like this

  initialVals[0] = 0; //e.g. device 1 is off at boot
  initialVals[1] = 255; //e.g. device 2 is on at boot
  initialVals[2] = 127; //e.g. device 3 is at 50% at boot
  //initialVals[3] = 22; //more lights like this
  
  //END OF USER LIGHT CONFIG
  
  for (int i = 0; i <DEVICES; i++)
  {
    yourVal[i] = initialVals[i];
  }
  
  // Initialise wifi connection
  wifiConnected = connectWifi();

  // only proceed if wifi connection successful
  if (wifiConnected)
  {
    Serial.println("Ask Alexa to discover devices");
    udpConnected = connectUDP();
    
    if (udpConnected){
      // initialise pins if needed 
      startHttpServer();
    }
  } else
  {
    while (1) { //endless loop
      Serial.println("Cannot connect to WiFi!");
      Serial.print("Please check that the credentials for network ");
      Serial.print(ssid);
      Serial.println(" are correct and then reset the ESP.");
      delay(2500);
    }
  }
}

void loop() {
  server.handleClient();
  delay(1);
  
  
  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
    if(udpConnected){    
      // if there’s data available, read a packet
      int packetSize = UDP.parsePacket();
      
      if(packetSize) {
        //Serial.println("");
        //Serial.print("Received packet of size ");
        //Serial.println(packetSize);
        //Serial.print("From ");
        IPAddress remote = UDP.remoteIP();
        
        for (int i =0; i < 4; i++) {
          Serial.print(remote[i], DEC);
          if (i < 3) {
            Serial.print(".");
          }
        }
        
        Serial.print(", port ");
        Serial.println(UDP.remotePort());
        
        int len = UDP.read(packetBuffer, 255);
        
        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;

        if(request.indexOf("M-SEARCH") >= 0) {
          if(request.indexOf("upnp:rootdevice") > 0 || request.indexOf("device:basic:1") > 0) {
              Serial.println("Responding search req...");
              respondToSearch();
          }
        }
      }
        
      delay(10);
    }
}

String deviceJsonString(int deviceId)
{
  if (deviceId < 1 || deviceId > DEVICES) return "{}"; //error
  return "{\"type\":\"Extended color light\",\"manufacturername\":\"OpenSource\",\"swversion\":\"0.1\",\"name\":\""+ friendlyName[deviceId-1] +"\",\"uniqueid\":\""+ WiFi.macAddress() +"-"+ (deviceId+1) +"\",\"modelid\":\"LST001\",\"state\":{\"on\":"+ boolString(yourVal[deviceId-1]) +",\"bri\":"+ briForHue(yourVal[deviceId-1]) +",\"xy\":[0.00000,0.00000],\"colormode\":\"hs\",\"effect\":\"none\",\"ct\":500,\"hue\":0,\"sat\":0,\"alert\":\"none\",\"reachable\":true}}";
}

boolean handleAlexaApiCall(String req, String body) //basic implementation of Philips hue api functions needed for basic Alexa control
{
  Serial.println("AlexaApiCall");
  if (req.indexOf("api") <0) return false; //return if not an API call
  Serial.println("ok");
  
  if (body.indexOf("devicetype") > 0) //client wants a hue api username, we dont care and give static
  {
    Serial.println("devType");
    server.send(200, "application/json", "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]");
    return true;
  }

  if (req.indexOf("state") > 0) //client wants to control light
  {
    int tempDeviceId = req.substring(req.indexOf("lights")+7).toInt();
    Serial.print("ls"); Serial.println(tempDeviceId);
    if (body.indexOf("bri")>0) {alexaDim(tempDeviceId, body.substring(body.indexOf("bri") +5).toInt()); return true;}
    if (body.indexOf("false")>0) {alexaOff(tempDeviceId); return true;}
    alexaOn(tempDeviceId);
    
    return true;
  }
  int pos = req.indexOf("lights");
  if (pos > 0) //client wants light info
  {
    int tempDeviceId = req.substring(pos+7).toInt();
    Serial.print("l"); Serial.println(tempDeviceId);

    if (tempDeviceId == 0) //client wants all lights
    {
      Serial.println("lAll");
      String jsonTemp = "{";
      for (int i = 0; i<DEVICES; i++)
      {
        jsonTemp += "\"" + String(i+1) + "\":";
        jsonTemp += deviceJsonString(i+1);
        if (i < DEVICES-1) jsonTemp += ",";
      }
      jsonTemp += "}";
      server.send(200, "application/json", jsonTemp);
    } else //client wants one light (tempDeviceId)
    {
      server.send(200, "application/json", deviceJsonString(tempDeviceId));
    }
    
    return true;
  }

  //we dont care about other api commands at this time and send empty JSON
  server.send(200, "application/json", "{}");
  return true;
}

void startHttpServer() {
    server.on("/", HTTP_GET, [](){
      Serial.println("Got Request root ...\n");
      String res = "Hello from Espalexa!\r\n";
      for (int i=0; i<DEVICES; i++)
      {
        res += "Value of device " + String(i+1) + " (" + friendlyName[i] + "): " + String(yourVal[i]) + "\r\n";
      }
      server.send(200, "text/plain", res);
    });
    
    server.on("/description.xml", HTTP_GET, [](){
      Serial.println(" # Responding to description.xml ... #\n");

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
            
        server.send(200, "text/xml", setup_xml.c_str());
        
        Serial.print("Sending :");
        Serial.println(setup_xml);
    });

    // openHAB support
    server.on("/on.html", HTTP_GET, [](){ //these are not relevant, will be removed in library release and only support 1st device
         Serial.println("on req");
         server.send(200, "text/plain", "turned on");
         actionOn(0);
       });
 
     server.on("/off.html", HTTP_GET, [](){
        Serial.println("off req");
        server.send(200, "text/plain", "turned off");
        actionOff(0);
       });
 
      server.on("/status.html", HTTP_GET, [](){
        Serial.println("Got status request");
 
        String statrespone = "0"; 
        if (yourVal[0] > 0) {
          statrespone = "1"; 
        }
        server.send(200, "text/plain", statrespone);
      
    });
       
    server.onNotFound([](){
      Serial.println("Not-Found HTTP call:");
      Serial.println("URI: " + server.uri());
      Serial.println("Body: " + server.arg(0));
      if(!handleAlexaApiCall(server.uri(),server.arg(0)))
        server.send(404, "text/plain", "NotFound");
    });
    
    server.begin();  
    Serial.println("HTTP Server started ..");
}
      
// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 20){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
  }
  
  return state;
}

void alexaOn(uint8_t deviceId)
{
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":true}}]";

  server.send(200, "text/xml", body.c_str());

  actionOn(deviceId);
}

void alexaOff(uint8_t deviceId)
{
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/on\":false}}]";

  server.send(200, "application/json", body.c_str());

  actionOff(deviceId);
}

void alexaDim(uint8_t deviceId, uint8_t briL)
{
  String body = "[{\"success\":{\"/lights/"+ String(deviceId) +"/state/bri\":"+ String(briL) +"}}]";

  server.send(200, "application/json", body.c_str());

  if (briL <255)
  {
    actionDim(deviceId, briL+1);
  } else
  {
    actionDim(deviceId, 255);
  }
}

void prepareIds() {
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();

  for (int i = 0; i <DEVICES; i++)
  {
    friendlyName[i] = "Default light " + String(i+1);
    initialVals[i] = 0;
  }
}

void respondToSearch() {
    Serial.println("");
    Serial.print("Send resp to ");
    Serial.println(UDP.remoteIP());
    Serial.print("Port : ");
    Serial.println(UDP.remotePort());

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

     Serial.println("Response sent!");
}

String boolString(bool st)
{
  return (st)?"true":"false";
}

String briForHue(int realBri)
{
  realBri--;
  if (realBri < 0) realBri = 0;
  return String(realBri);
}

boolean connectUDP(){
  boolean state = false;
  
  Serial.println("");
  Serial.println("Con UDP");
  
  #ifdef ARDUINO_ARCH_ESP32
  if(UDP.beginMulticast(ipMulti, portMulti))
  #else
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti))
  #endif
  {
    Serial.println("Con success");
    state = true;
  }
  else{
    Serial.println("Con failed");
  }
  
  return state;
}
