#include <PubSubClient.h>
#include <CooperativeMultitasking.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <WiFi101.h>
#include "config.h"
#include "freemem.h"
#include <list>
using namespace std;

#define VBATPIN A7

CooperativeMultitasking tasks;
Continuation beginWiFiIfNeeded;
Continuation connectMQTTClientIfNeeded;
Continuation discovery;
Continuation state;
Guard mqttConnected;

const int maxPathLength = 256, maxPayloadLength = 1024;
char rootTopic[maxPathLength];

enum DeviceClass {
  battery, current, energy,
  humidity, illuminance, signal_strength,
  temperature, power, power_factor,
  pressure, timestamp, voltage, free_memory
};

static const char *DeviceClassName[] {
  "battery", "current", "energy",
  "humidity", "illuminance", "signal_strength",
  "temperature", "power", "power_factor",
  "pressure", "timestamp", "voltage", "free_memory"
};

struct Measurement {
  char* measurementName;
  DeviceClass deviceClass;
  char* stateTopic;
  char* UoM;
};

struct Device {
  char* deviceName;
  char* platform;
  char* manufacturer;
  char* model;
  char* SWVersion;
  char* identifier;
  list<Measurement> measurements;
};

WiFiClient wificlient;

//MQTTClient mqttclient(&tasks, &wificlient, MQTT_HOST, MQTT_PORT, NODE_ID, MQTT_USER, MQTT_PASSWORD);
PubSubClient client(MQTT_HOST, MQTT_PORT, wificlient);

void setup() {  
  WiFi.setPins(8,7,4,2);
  WiFi.lowPowerMode();

  while (!Serial);
  Serial.begin(115200);

  randomSeed(analogRead(0));

  // Connect to WiFi
  tasks.now(beginWiFiIfNeeded);  
  // Connect to MQTT Broker
  tasks.now(connectMQTTClientIfNeeded);
  // Send discovery data to broker

  rootTopicBuilder(rootTopic, HA_TOPIC, DEVICE_NAME);
  tasks.ifThen(mqttConnected, discovery);
  tasks.after(5000, state);
  
}

void loop() {
  tasks.run();
}

void rootTopicBuilder(char* path, const char* discoveryPath, const char* deviceName){
  strncpy(path, discoveryPath, maxPathLength);
  strcat(path, "sensor/");
  strcat(path, deviceName);
}

void configTopicBuilder(char* path, const char* rootTopic, const char* deviceClass){
  strncpy(path, rootTopic, maxPathLength);
  strcat(path, "_");
  strcat(path, deviceClass);
  strcat(path, "/config");
}

void configPayload(char* payload, const Measurement measurement, const Device device){

  int maxAttributeLength = 64;
  char uniqueId[maxAttributeLength], valueTemplate[maxAttributeLength], configPayload[maxPayloadLength];
  
  DynamicJsonDocument doc(maxPayloadLength);
  JsonObject configPayloadJSON = doc.to<JsonObject>();
  configPayloadJSON["name"] = measurement.measurementName;
  if (measurement.deviceClass != DeviceClass::free_memory) {
    configPayloadJSON["device_class"] = DeviceClassName[measurement.deviceClass];
  }
  configPayloadJSON["state_topic"] = measurement.stateTopic;
  configPayloadJSON["unit_of_measurement"] = measurement.UoM;

  strncpy(uniqueId, DEVICE_ID, maxAttributeLength);
  strcat(uniqueId, "_");
  strcat(uniqueId, DeviceClassName[measurement.deviceClass]);
  configPayloadJSON["unique_id"] = uniqueId;

  strncpy(valueTemplate, "{{ value_json.", maxAttributeLength);
  strcat(valueTemplate, DeviceClassName[measurement.deviceClass]);
  strcat(valueTemplate, " }}");
  configPayloadJSON["value_template"] = valueTemplate;

  JsonObject deviceJSON = configPayloadJSON.createNestedObject("device");
  deviceJSON["name"] = device.deviceName;
  deviceJSON["manufacturer"] = device.manufacturer;
  deviceJSON["model"] = device.model;
  deviceJSON["sw_version"] = device.SWVersion;
  
  JsonArray deviceIdentifiersJSON = deviceJSON.createNestedArray("identifiers");
  deviceIdentifiersJSON.add(device.identifier);
  
  serializeJson(configPayloadJSON, payload, maxPayloadLength);
}

void chkFreeMem(long &memory) {
  memory = freeMemory();
}

void chkBattery(float &measuredvbat){
  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
}

void discovery() {

  char stateTopic[maxPathLength], configTopic[maxPathLength], payload[maxPayloadLength];
  bool success;

  // set stateTopic
  strncpy(stateTopic, rootTopic, maxPathLength);
  strcat(stateTopic, "/state");
  
  // add measurements to the list
  list<Measurement> measurements;
  measurements.push_back(Measurement {"Test Temperature 2", DeviceClass::temperature, stateTopic, "degC"});
  measurements.push_back(Measurement {"Test Humidity 2", DeviceClass::humidity, stateTopic, "%"});
  measurements.push_back(Measurement {"Test Pressure 2", DeviceClass::pressure, stateTopic, "hPa"});
  measurements.push_back(Measurement {"Test Voltage 2", DeviceClass::voltage, stateTopic, "V"});
  measurements.push_back(Measurement {"Test Free Memory 2", DeviceClass::free_memory, stateTopic, "Bytes"});

  Device myDevice = {DEVICE_NAME, DEVICE_PLATFORM, DEVICE_MANUFACTURER, DEVICE_MODEL, DEVICE_SW_VERSION, DEVICE_ID, measurements};

  for ( Measurement& measurement : myDevice.measurements )
  {
    configTopicBuilder(configTopic, rootTopic, DeviceClassName[measurement.deviceClass]);
    Serial.print("Config topic: ");
    Serial.println(configTopic);

    success = client.publish(configTopic, "", false);
    client.loop();
    delay(2000);
    configPayload(payload, measurement, myDevice);
    Serial.println(payload);
    
    client.publish(configTopic, payload, false);
    
  }
  tasks.after(300000, discovery); // re-run every 5 mins
}

double round2(double value) {
   return (int)(value * 100 + 0.5) / 100.0;
}

void statePayload(char* payload){

  long freeMem;
  float voltage;
  
  float temperature = {round2(random(500, 2500) / 100)};
  float humidity = {round2(random(4000, 9000) / 100)};
  long pressure = {random(900, 1100)};
  chkFreeMem(freeMem);
  chkBattery(voltage);  
  
  DynamicJsonDocument statePayloadJSON(256);
  statePayloadJSON["temperature"] = temperature;
  statePayloadJSON["humidity"] = humidity;
  statePayloadJSON["pressure"] = pressure;
  statePayloadJSON["voltage"] = voltage;
  statePayloadJSON["free_memory"] = freeMem;
  serializeJson(statePayloadJSON, payload, maxPayloadLength);
}

void state(){
  char stateTopic[maxPathLength], payload[maxPayloadLength];
  bool success;
  
  // set stateTopic
  strncpy(stateTopic, rootTopic, maxPathLength);
  strcat(stateTopic, "/state");
  Serial.print("State topic: ");
  Serial.println(stateTopic);

  Serial.print("State: ");
  statePayload(payload);
  Serial.println(payload);
  
  success = client.publish(stateTopic, payload, false);
  
  tasks.after(5000, state);
  
}

void beginWiFiIfNeeded() {
  
  Serial.println("Checking WiFi state...");
  switch (WiFi.status()) {
//    case WL_IDLE_STATUS:
    case WL_CONNECTED: tasks.after(30000, beginWiFiIfNeeded); return; // after 30 seconds call beginWiFiIfNeeded() again
    case WL_NO_SHIELD: Serial.println("no wifi shield"); return; // do not check again
    case WL_CONNECT_FAILED: Serial.println("wifi connect failed"); break;
    case WL_CONNECTION_LOST: Serial.println("wifi connection lost"); break;
    case WL_DISCONNECTED: Serial.println("wifi disconnected"); break;
  }
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    // wait 10 seconds for connection:
    uint8_t timeout = 10;
    while (timeout && (WiFi.status() != WL_CONNECTED)) {
      timeout--;
      delay(1000);
    }
  }
  
  Serial.println("OK!");
  tasks.after(30000, beginWiFiIfNeeded); // after 30 seconds call beginWiFiIfNeeded() again
}

void connectMQTTClientIfNeeded() {
  Serial.println("Checking MQTT state...");
  switch(client.state()) {
    case MQTT_CONNECTED: client.loop(); tasks.after(10000, connectMQTTClientIfNeeded); return; // after 30 seconds call connectMQTTClientIfNeeded() again
    case MQTT_DISCONNECTED: Serial.println("MQTT client disconnected cleanly"); break;
    case MQTT_CONNECT_FAILED: Serial.println("MQTT network connection failed"); break;
    case MQTT_CONNECTION_LOST: Serial.println("MQTT network connection was broken"); break;
    case MQTT_CONNECTION_TIMEOUT: Serial.println("MQTT server didn't respond within the keepalive time"); break;
  }
  Serial.print("Connecting to MQTT client: ");
  Serial.print(MQTT_HOST);
  Serial.print("...");
  client.setBufferSize(maxPayloadLength);
  client.connect(DEVICE_ID, MQTT_USER, MQTT_PASSWORD);
  Serial.println("OK!");

  tasks.after(10000, connectMQTTClientIfNeeded);
}

bool mqttConnected(){
  return client.connected();
}
