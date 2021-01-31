
/***************************************************
  Adafruit MQTT Library WINC1500 Example

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
 ****************************************************/
#include <SPI.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <WiFi101.h>
#include <ArduinoJson.h>
#include "config.h"
#include <list>
using namespace std;


/************************* WiFI Setup *****************************/
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2     // or, tie EN to VCC

char ssid[] = WIFI_SSID;     //  your network SSID (name)
char pass[] = WIFI_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;

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


/************ Global State (you don't need to change this!) ******************/

//Set up the wifi client
WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

/****************************** Feeds ***************************************/

char rootTopic[1024], stateTopic[1024];

/*************************** Sketch Code ************************************/

#define LEDPIN 13


void setup() {
  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST, WINC_EN);

  while (!Serial);
  Serial.begin(115200);

  // Initialise the Client
  Serial.print(F("\nInit the WiFi module..."));
  // check for the presence of the breakout
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WINC1500 not present");
    // don't continue:
    while (true);
  }
  Serial.println("ATWINC OK!");

  // build root and state topic paths
  strcpy(rootTopic, HA_TOPIC);
  strcat(rootTopic, "sensor/");
  strcat(rootTopic, NODE_ID);
  strcat(rootTopic, "/");
  strcat(rootTopic, DEVICE_NAME);

  Serial.print("Root topic: ");
  Serial.println(rootTopic);

  strcpy(stateTopic, rootTopic);
  strcat(stateTopic, "/state");

  Serial.print("State topic: ");
  Serial.println(stateTopic);

  // add measurements to the list
  list<Measurement> measurements;
  measurements.push_back(Measurement {"Test Temperature 2", "temperature", stateTopic, "degC"});
  measurements.push_back(Measurement {"Test Temperature 2", "humidity", stateTopic, "%"});
  measurements.push_back(Measurement {"Test Temperature 2", "pressure", stateTopic, "hPa"});

  Device myDevice = {"test_atmospherics_2", "sensor", measurements};

  for ( Measurement& measurement : myDevice.measurements )
  {
    char configTopic[1024];
    strcpy(configTopic, rootTopic);
    strcat(configTopic, "_");
    strcat(configTopic, measurement.deviceClass);
    strcat(configTopic, "/config");
    Serial.print("Config topic: ");
    Serial.println(configTopic);
    
    MQTT_connect();

    DynamicJsonDocument configPayload(1024);
    configPayload["name"] = measurement.measurementName;
    configPayload["device_class"] = measurement.deviceClass;
    configPayload["state_topic"] = stateTopic;
    configPayload["unit_of_measurement"] = measurement.UoM;
    char valueTemplate[128];
    strcpy(valueTemplate, "{{ value_json.");
    strcat(valueTemplate, measurement.deviceClass);
    strcat(valueTemplate, " }}");
    configPayload["value_template"] = valueTemplate;

    char configPayloadChar[1024];
    serializeJson(configPayload, configPayloadChar);
    
    delay(5000);

    Serial.println(F("\nSending config "));
    Serial.println(configPayloadChar);
    Serial.print("...");
//  if (! Adafruit_MQTT_Publish(&mqtt, stateTopic).publish(configPayloadChar)) {
//    Serial.println(F("Failed"));
//  } else {
//    Serial.println(F("OK!"));
//  }
  }
}

uint32_t x = 0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // Now we can publish stuff!
  DynamicJsonDocument statePayload(1024);
  statePayload["temperature"] = 18.3;
  statePayload["humidity"] = 42.0;
  statePayload["pressure"] = 999.0;
  char statePayloadChar[1024];
  serializeJson(statePayload, statePayloadChar);

  Serial.print(F("\nSending state "));
  Serial.print(statePayloadChar);
  Serial.print("...");
  if (! Adafruit_MQTT_Publish(&mqtt, stateTopic).publish(statePayloadChar)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  delay(10000);
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // attempt to connect to Wifi network:
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    uint8_t timeout = 10;
    while (timeout && (WiFi.status() != WL_CONNECTED)) {
      timeout--;
      delay(1000);
    }
  }

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}
