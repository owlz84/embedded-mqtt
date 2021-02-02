#include <SPI.h>
#include <ArduinoJson.h>
#include <WiFi101.h>
#include <MQTTClient.h>
#include <CooperativeMultitasking.h>
#include "config.h"
#include <list>
using namespace std;

CooperativeMultitasking tasks;
Continuation beginWiFiIfNeeded;
Continuation connectMQTTClientIfNeeded;
Continuation discovery;
Continuation state;
Guard mqttConnected;

WiFiClient wificlient;

MQTTClient mqttclient(&tasks, &wificlient, MQTT_HOST, MQTT_PORT, NODE_ID, MQTT_USER, MQTT_PASSWORD);
const int maxPathLength = 256, maxPayloadLength = 1024;
char rootTopic[maxPathLength];

struct Measurement {
  char* measurementName;
  char* deviceClass;
  char* stateTopic;
  char* UoM;
};

struct Device {
  char* deviceName;
  char* platform;
  list<Measurement> measurements;
};

void setup() {  
  WiFi.setPins(8,7,4,2);

  while (!Serial);
  Serial.begin(115200);

  // Connect to WiFi
  tasks.now(beginWiFiIfNeeded);  
  // Connect to MQTT Broker
  tasks.now(connectMQTTClientIfNeeded);
  // Send discovery data to broker

  strcpy(rootTopic, rootTopicBuilder(HA_TOPIC, DEVICE_NAME));
  Serial.print("Root topic: ");
  Serial.println(rootTopic);

  tasks.ifThen(mqttConnected, discovery);
  
}

void loop() {
  tasks.run();
}

char* rootTopicBuilder(const char* discoveryPath, const char* deviceName){
  
  char* path = new char[maxPathLength];
  // build root topic path
  strncpy(path, discoveryPath, maxPathLength);
  strcat(path, "sensor/");
  strcat(path, deviceName);
  return path;
}

char* configTopicBuilder(const char* rootTopic, const char* deviceClass){
  char* path = new char[maxPathLength];
  strncpy(path, rootTopic, maxPathLength);
  strcat(path, "_");
  strcat(path, deviceClass);
  strcat(path, "/config");
  return path;
}

char* configPayload(const Measurement measurement){

  int maxValueTemplateLength = 64;
  char* valueTemplate = new char[maxValueTemplateLength];
  char* configPayload = new char[maxPayloadLength];
  
  DynamicJsonDocument configPayloadJSON(maxPayloadLength);
  configPayloadJSON["name"] = measurement.measurementName;
  configPayloadJSON["device_class"] = measurement.deviceClass;
  configPayloadJSON["state_topic"] = measurement.stateTopic;
  configPayloadJSON["unit_of_measurement"] = measurement.UoM;

  strncpy(valueTemplate, "{{ value_json.", maxValueTemplateLength);
  strcat(valueTemplate, measurement.deviceClass);
  strcat(valueTemplate, " }}");
  configPayloadJSON["value_template"] = valueTemplate;
  
  serializeJson(configPayloadJSON, configPayload, maxPayloadLength);
  return configPayload;  
}

char* statePayload(){
  char* statePayload = new char[maxPayloadLength];
  
  DynamicJsonDocument statePayloadJSON(256);
  statePayloadJSON["temperature"] = 18.3;
  statePayloadJSON["humidity"] = 42.0;
  statePayloadJSON["pressure"] = 999.0;
  serializeJson(statePayloadJSON, statePayload, maxPayloadLength);
  return statePayload;
}

void discovery() {

  char stateTopic[maxPathLength];
  char* configTopic = new char[maxPathLength];
  char* payload = new char[maxPayloadLength];

  // set stateTopic
  strncpy(stateTopic, rootTopic, maxPathLength);
  strcat(stateTopic, "/state");
  
  // add measurements to the list
  list<Measurement> measurements;
  measurements.push_back(Measurement {"Test Temperature 2", "temperature", stateTopic, "degC"});
  measurements.push_back(Measurement {"Test Humidity 2", "humidity", stateTopic, "%"});
  measurements.push_back(Measurement {"Test Pressure 2", "pressure", stateTopic, "hPa"});

  Device myDevice = {DEVICE_NAME, DEVICE_PLATFORM, measurements};

  for ( Measurement& measurement : myDevice.measurements )
  {
    
    strcpy(configTopic, configTopicBuilder(rootTopic, measurement.deviceClass));
    Serial.print("Config topic: ");
    Serial.println(configTopic);
    
    MQTTTopic confTopic = topic(&mqttclient, configTopic);

    strncpy(payload, configPayload(measurement), maxPayloadLength);
    Serial.println(payload);
    confTopic.publish(payload);
    
  }
  tasks.after(60000, state);
}

void state(){
  char stateTopic[maxPathLength];
  char* payload = new char[maxPayloadLength];
  // set stateTopic
  strncpy(stateTopic, rootTopic, maxPathLength);
  strcat(stateTopic, "/state");
  MQTTTopic topic(&mqttclient, stateTopic);
  strncpy(payload, statePayload(), maxPayloadLength);
  Serial.println(payload);
  topic.publish(payload);
  
}

void beginWiFiIfNeeded() {
  
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
  tasks.after(10000, beginWiFiIfNeeded); // after 10 seconds call beginWiFiIfNeeded() again
}

void connectMQTTClientIfNeeded() {
  if (!mqttclient.connected()) {
    Serial.print("Connecting to MQTT client: ");
    Serial.print(MQTT_HOST);
    Serial.print("...");
    mqttclient.connect();
  }
  tasks.after(30000, connectMQTTClientIfNeeded); // after 30 seconds call connectMQTTClientIfNeeded() again
}

bool mqttConnected(){
  return mqttclient.connected();
}
