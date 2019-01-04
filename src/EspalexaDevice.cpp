//EspalexaDevice Class

#include "EspalexaDevice.h"

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