#include "Device.h"
#include <list>
#include <string>
using namespace std;


Measurement::Measurement(string measurementName, string deviceClass, string stateTopic, string UoM)
{
  measurementName = measurementName;
  deviceClass = deviceClass;
  stateTopic = stateTopic;
  UoM = UoM;
}

Device::Device(string deviceName, string platform)
{
  deviceName = deviceName;
  platform = platform;
  list<Measurement> measurements;
}

void Device::addMeasurement(Measurement measurement)
{
  measurements.push_back(measurement);
}
