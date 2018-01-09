/*
 * Alexa Voice On/Off/Brightness Control. Emulates a Philips Hue bridge to Alexa.
 * 
 * This was put together from these two excellent projects:
 * https://github.com/kakopappa/arduino-esp8266-alexa-wemo-switch
 * https://github.com/probonopd/ESP8266HueEmulator
 */
/*
 * @title Espalexa sketch
 * @version 1.0
 * @author Christian Schwinne
 * @license MIT
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>

const char* ssid = "...";  // CHANGE: Wifi name
const char* password = "...";  // CHANGE: Wifi password 
String friendlyName = "Light";        // CHANGE: Alexa device name
int yourVal = 0; //Alexa will change this (off=0, on=255, brightness steps in-between)

void prepareIds();
boolean connectWifi();
boolean connectUDP();
void startHttpServer();
void respondToSearch();

WiFiUDP UDP;
IPAddress ipMulti(239, 255, 255, 250);
ESP8266WebServer server(80);
boolean udpConnected = false;
unsigned int portMulti = 1900;      // local port to listen on
unsigned int localPort = 1900;      // local port to listen on
boolean wifiConnected = false;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
String escapedMac; //lowercase mac address
boolean cannotConnectToWifi = false;

//what to do if switched
void actionOn()
{
  yourVal = 255;
}

void actionOff()
{
  yourVal = 0;
}

void actionDim(int b) //1-255 range
{
  yourVal = b;
}

void setup() {
  Serial.begin(115200);
  
  prepareIds();
  
  // Initialise wifi connection
  wifiConnected = connectWifi();

  // only proceed if wifi connection successful
  if(wifiConnected){
    Serial.println("Ask Alexa to discover devices");
    udpConnected = connectUDP();
    
    if (udpConnected){
      // initialise pins if needed 
      startHttpServer();
    }
  }  
}

void loop() {
  server.handleClient();
  delay(1);
  
  
  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
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
          if(request.indexOf("upnp:rootdevice") > 0) {
              Serial.println("Responding search req...");
              respondToSearch();
          }
        }
      }
        
      delay(10);
    }
  } else {
      Serial.println("Cannot connect to Wifi");
  }
}

boolean handleAlexaApiCall(String req, String body) //basic implementation of Philips hue api functions needed for basic Alexa control
{
  Serial.println("AlexaApiCall");
  if (req.indexOf("api") <0) return false;
  Serial.println("ok");
  if (body.indexOf("devicetype") > 0) //client wants a hue api username, we dont care and give static
  {
    Serial.println("devType");
    server.send(200, "application/json", "[{\"success\":{\"username\": \"2WLEDHardQrI3WHYTHoMcXHgEspsM8ZZRpSKtBQr\"}}]");
    return true;
  }
  if (req.indexOf("state") > 0) //client wants to control light
  {
    Serial.println("ls");
    if (body.indexOf("bri")>0) {alexaDim(body.substring(body.indexOf("bri") +5).toInt()); return true;}
    if (body.indexOf("false")>0) {alexaOff(); return true;}
    alexaOn();
    
    return true;
  }
  if (req.indexOf("lights/1") > 0) //client wants light info
  {
    Serial.println("l1");
    server.send(200, "application/json", "{\"manufacturername\":\"OpenSource\",\"modelid\":\"LST001\",\"name\":\""+ friendlyName +"\",\"state\":{\"on\":"+ boolString(yourVal) +",\"hue\":0,\"bri\":"+ briForHue(yourVal) +",\"sat\":0,\"xy\":[0.00000,0.00000],\"ct\":500,\"alert\":\"none\",\"effect\":\"none\",\"colormode\":\"hs\",\"reachable\":true},\"swversion\":\"0.1\",\"type\":\"Extended color light\",\"uniqueid\":\"2\"}");

    return true;
  }
  if (req.indexOf("lights") > 0) //client wants all lights
  {
    Serial.println("lAll");
    server.send(200, "application/json", "{\"1\":{\"type\":\"Extended color light\",\"manufacturername\":\"OpenSource\",\"swversion\":\"0.1\",\"name\":\""+ friendlyName +"\",\"uniqueid\":\""+ WiFi.macAddress() +"-2\",\"modelid\":\"LST001\",\"state\":{\"on\":"+ boolString(yourVal) +",\"bri\":"+ briForHue(yourVal) +",\"xy\":[0.00000,0.00000],\"colormode\":\"hs\",\"effect\":\"none\",\"ct\":500,\"hue\":0,\"sat\":0,\"alert\":\"none\",\"reachable\":true}}}");

    return true;
  }

  //we dont care about other api commands at this time and send empty JSON
  server.send(200, "application/json", "{}");
  return true;
}

void startHttpServer() {
    server.on("/", HTTP_GET, [](){
      Serial.println("Got Request root ...\n");
      String res = "Hello from Espalexa! Value: " + String(yourVal);
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
    server.on("/on.html", HTTP_GET, [](){
         Serial.println("on req");
         server.send(200, "text/plain", "turned on");
         actionOn();
       });
 
     server.on("/off.html", HTTP_GET, [](){
        Serial.println("off req");
        server.send(200, "text/plain", "turned off");
        actionOff();
       });
 
      server.on("/status.html", HTTP_GET, [](){
        Serial.println("Got status request");
 
        String statrespone = "0"; 
        if (yourVal > 0) {
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
    if (i > 10){
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

void alexaOn()
{
  actionOn();

  String body = "[{\"success\":{\"/lights/1/state/on\":true}}]";

  server.send(200, "text/xml", body.c_str());
        
  Serial.print("Sending :");
  Serial.println(body);
}

void alexaOff()
{
  actionOff();
  
  String body = "[{\"success\":{\"/lights/1/state/on\":false}}]";

  server.send(200, "application/json", body.c_str());
        
  Serial.print("Sending:");
  Serial.println(body);
}

void alexaDim(uint8_t briL)
{
  actionDim(briL+1);
  
  String body = "[{\"success\":{\"/lights/1/state/bri\":"+ String(briL) +"}}]";

  server.send(200, "application/json", body.c_str());
}

void prepareIds() {
  escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();
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
    UDP.write(response.c_str());
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
  
  if(UDP.beginMulticast(WiFi.localIP(), ipMulti, portMulti)) {
    Serial.println("Con success");
    state = true;
  }
  else{
    Serial.println("Con failed");
  }
  
  return state;
}
