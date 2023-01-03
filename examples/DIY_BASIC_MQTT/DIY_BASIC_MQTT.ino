/*
This is the code for the AirGradient DIY BASIC Air Quality Sensor with an ESP8266 Microcontroller.

It is a high quality sensor showing PM2.5, CO2, Temperature and Humidity on a small display and can send data over Wifi.

Build Instructions: https://www.airgradient.com/open-airgradient/instructions/diy/

Kits (including a pre-soldered version) are available: https://www.airgradient.com/open-airgradient/kits/

The codes needs the following libraries installed:
“WifiManager by tzapu, tablatronix” tested with version 2.0.11-beta
“U8g2” by oliver tested with version 2.32.15
"ESPPubSubClientWrapper" by Erik Foltin tested with version 0.1.0
"PubSubClient" by Nick O'Leary tested with version 2.8.0

Configuration:
Please set in the code below the configuration parameters.

If you have any questions please visit our forum at https://forum.airgradient.com/

If you are a school or university contact us for a free trial on the AirGradient platform.
https://www.airgradient.com/

MIT License

*/

#include <AirGradient.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <U8g2lib.h>
#include <ESPPubSubClientWrapper.h>

AirGradient ag = AirGradient();

U8G2_SSD1306_64X48_ER_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); //for DIY BASIC


// CONFIGURATION START

// set to true to switch from Celcius to Fahrenheit
boolean inF = false;

// PM2.5 in US AQI (default ug/m3)
boolean inUSAQI = true;

// Wifi Settings
// set to true if you want to connect to wifi. You have 60 seconds to connect. Then it will go into an offline mode.
boolean connectWIFI=true;

// MQTT Server Settings
const char* mqtt_server = "SERVER_IP";
const char* mqtt_id = "DESIRED_DEVICE_ID";
const char* mqtt_topic = "DESIRED_MQTT_TOPIC";
const char* mqtt_username = "USERNAME";
const char* mqtt_password = "PASSWORD";
ESPPubSubClientWrapper mqttClient(mqtt_server, 1883);

// MQTT Connection Callbacks
void onMqttConnect(uint16_t count){
  Serial.println("Successfully connected");
}

void onMqttDisconnect(uint16_t count){
  Serial.println("Disconnected from server");
}

bool onMqttConnectionFailed(uint16_t count){
  Serial.println("Connection to MQTT server failed, retrying");
  // set to true to re-try the connection
  return true;
}
// CONFIGURATION END


unsigned long currentMillis = 0;

const int oledInterval = 5000;
unsigned long previousOled = 0;

const int sendToServerInterval = 10000;
unsigned long previoussendToServer = 0;

const int co2Interval = 5000;
unsigned long previousCo2 = 0;
int Co2 = 0;

const int pm25Interval = 5000;
unsigned long previousPm25 = 0;
int pm25 = 0;

const int tempHumInterval = 2500;
unsigned long previousTempHum = 0;
float temp = 0;
int hum = 0;
long val;

void mqttSetup(){
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onConnectFail(onMqttConnectionFailed);
  // Remove if your MQTT server does not require authentication
  mqttClient.connect(mqtt_id, mqtt_username, mqtt_password);  
}

void setup()
{
  Serial.begin(115200);
  
  u8g2.begin();
  updateOLED();

  if (connectWIFI) {
    connectToWifi();
    mqttSetup();
  }

  updateOLED2("Warming", "up the", "sensors");

  ag.CO2_Init();
  ag.PMS_Init();
  ag.TMP_RH_Init(0x44);
}


void loop()
{
  currentMillis = millis();
  mqttClient.loop();
  updateOLED();
  updateCo2();
  updatePm25();
  updateTempHum();
  publishToMQTT();
}

void updateCo2()
{
    if (currentMillis - previousCo2 >= co2Interval) {
      previousCo2 += co2Interval;
      Co2 = ag.getCO2_Raw();
      Serial.println(String(Co2));
    }
}

void updatePm25()
{
    if (currentMillis - previousPm25 >= pm25Interval) {
      previousPm25 += pm25Interval;
      pm25 = ag.getPM2_Raw();
      Serial.println(String(pm25));
    }
}

void updateTempHum()
{
    if (currentMillis - previousTempHum >= tempHumInterval) {
      previousTempHum += tempHumInterval;
      TMP_RH result = ag.periodicFetchData();
      temp = result.t;
      hum = result.rh;
      Serial.println(String(temp));
    }
}

void updateOLED() {
   if (currentMillis - previousOled >= oledInterval) {
     previousOled += oledInterval;

    String ln1;
    String ln2;
    String ln3;


    if (inUSAQI){
       ln1 = "AQI:" + String(PM_TO_AQI_US(pm25)) ;
    } else {
       ln1 = "PM: " + String(pm25) +"ug" ;
    }

    ln2 = "CO2:" + String(Co2);

      if (inF) {
        ln3 = String((temp* 9 / 5) + 32).substring(0,4) + " " + String(hum)+"%";
        } else {
        ln3 = String(temp).substring(0,4) + " " + String(hum)+"%";
       }
     updateOLED2(ln1, ln2, ln3);
   }
}

void updateOLED2(String ln1, String ln2, String ln3) {
      char buf[9];
          u8g2.firstPage();
          u8g2.firstPage();
          do {
          u8g2.setFont(u8g2_font_t0_16_tf);
          u8g2.drawStr(1, 10, String(ln1).c_str());
          u8g2.drawStr(1, 28, String(ln2).c_str());
          u8g2.drawStr(1, 46, String(ln3).c_str());
            } while ( u8g2.nextPage() );
}

void publishToMQTT() {
  if (currentMillis - previoussendToServer >= sendToServerInterval) {
    previoussendToServer += sendToServerInterval;
    String payload = "{\"rc02\":" + (Co2 < 0 ? "0" : String(Co2))
    + ", \"pm02\":" + (pm25 < 0 ? "0" : String(pm25))
    + ", \"tmp\":" + String(temp)
    + ", \"rhum\":"+ (hum < 0 ? "0" : String(hum))
    + "}";
    int payload_len = payload.length() + 1;
    char char_payload[payload_len];

    if(WiFi.status() == WL_CONNECTED && mqttClient.connected()){
      Serial.println("Publishing to MQTT server");
      Serial.println("Payload: "+payload);
      mqttClient.publish(mqtt_topic, strcpy(char_payload, payload.c_str()));
    }
  }
}

// Wifi Manager
void connectToWifi() {
  WiFiManager wifiManager;
  
  //WiFi.disconnect(); //to delete previous saved hotspot
  String HOTSPOT = "AG-" + String(ESP.getChipId(), HEX);
  updateOLED2("Connect", "Wifi", HOTSPOT);
  delay(2000);
  wifiManager.setTimeout(60);
  if (!wifiManager.autoConnect((const char * ) HOTSPOT.c_str())) {
    updateOLED2("Booting", "offline", "mode");
    Serial.println("failed to connect and hit timeout");
    delay(6000);
  }
}

// Calculate PM2.5 US AQI
int PM_TO_AQI_US(int pm02) {
  if (pm02 <= 12.0) return ((50 - 0) / (12.0 - .0) * (pm02 - .0) + 0);
  else if (pm02 <= 35.4) return ((100 - 50) / (35.4 - 12.0) * (pm02 - 12.0) + 50);
  else if (pm02 <= 55.4) return ((150 - 100) / (55.4 - 35.4) * (pm02 - 35.4) + 100);
  else if (pm02 <= 150.4) return ((200 - 150) / (150.4 - 55.4) * (pm02 - 55.4) + 150);
  else if (pm02 <= 250.4) return ((300 - 200) / (250.4 - 150.4) * (pm02 - 150.4) + 200);
  else if (pm02 <= 350.4) return ((400 - 300) / (350.4 - 250.4) * (pm02 - 250.4) + 300);
  else if (pm02 <= 500.4) return ((500 - 400) / (500.4 - 350.4) * (pm02 - 350.4) + 400);
  else return 500;
};
