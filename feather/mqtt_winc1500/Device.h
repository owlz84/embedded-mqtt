#ifndef Device_h
#define Device_h
#include <list>
#include <string>
using namespace std;

class Measurement
{
  public:
    Measurement(string measurementName, string deviceClass, string stateTopic, string UoM);
    string measurementName;
    string deviceClass;
    string stateTopic;
    string UoM;
};

class Device
{
  public:
    Device(string deviceName, string platform);
    void addMeasurement(Measurement measurement);
    list<Measurement> measurements;
    string deviceName;
    string platform;
};  

#endif
