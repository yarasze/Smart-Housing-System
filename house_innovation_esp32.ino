/*
  HOUSE INNOVATION - ESP32 firmware
  Board: ESP32 Dev Module / ESP32 DevKit V1
  Libraries (Arduino Library Manager):
    - PubSubClient by Nick O'Leary
    - ArduinoJson by Benoit Blanchon
    - DHT sensor library by Adafruit
    - Adafruit Unified Sensor

  MQTT topics:
    houseinnovation/<DEVICE_ID>/telemetry  (ESP32 -> app)
    houseinnovation/<DEVICE_ID>/status     (ESP32 -> app)
    houseinnovation/<DEVICE_ID>/command    (app -> ESP32)

  SAFETY:
  - This is a miniaturized low-voltage prototype, not a certified safety system.
  - MQ-2 output must NEVER exceed ESP32 ADC input limits (3.3V max).
    Use a suitable voltage divider if the module AO can output 5V.
  - Do not switch mains AC with an exposed relay. Start with low-voltage DC lamps.
  - TLS certificate validation is intentionally left in prototype mode below.
    For deployment, install the broker CA certificate and use setCACert().
*/
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <time.h>

// ---------- EDIT THESE SETTINGS ----------
const char* WIFI_SSID = "NAMA_WIFI";
const char* WIFI_PASSWORD = "PASSWORD_WIFI";
const char* MQTT_HOST = "GANTI_DENGAN_HOST_HIVEMQ";
const int   MQTT_PORT = 8883; // HiveMQ Cloud TLS MQTT port
const char* MQTT_USER = "USERNAME_MQTT";
const char* MQTT_PASSWORD = "PASSWORD_MQTT";
const char* DEVICE_ID = "rumah-miniatur-01";
// -----------------------------------------

// Pin mapping - change to match your wiring
#define PIN_DHT       4
#define DHT_TYPE      DHT22
#define PIN_MQ2       34  // ADC1 input-only pin
#define PIN_WATER     27  // Digital output from leak module
#define PIN_REED      26  // Reed switch to GND, INPUT_PULLUP
#define PIN_BUZZER    25
#define PIN_RELAY_LIVING 16
#define PIN_RELAY_BEDROOM 17
#define PIN_RELAY_PORCH 18

// Most 4-channel relay boards are active LOW. Change if your board differs.
const bool RELAY_ACTIVE_LOW = true;
// Many water sensor digital outputs go LOW when wet. Verify your module.
const bool WATER_WET_IS_LOW = true;
// MQ-2 raw ADC threshold for demonstration only. Calibrate experimentally.
const int MQ2_TEST_THRESHOLD = 1800;

WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
DHT dht(PIN_DHT, DHT_TYPE);

bool lightLiving=false, lightBedroom=false, lightPorch=false, awayMode=false;
unsigned long lastTelemetry=0, lastAwayAction=0;
bool awayPhase=false;
unsigned long awayDuration=0;
const char* baseTopic = "houseinnovation";

String topic(const char* leaf) {
  return String(baseTopic) + "/" + DEVICE_ID + "/" + leaf;
}
void setRelay(int pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}
void applyLights() {
  setRelay(PIN_RELAY_LIVING, lightLiving);
  setRelay(PIN_RELAY_BEDROOM, lightBedroom);
  setRelay(PIN_RELAY_PORCH, lightPorch);
}
void publishStatus() {
  StaticJsonDocument<256> doc;
  JsonObject lights=doc.createNestedObject("lights");
  lights["living"]=lightLiving; lights["bedroom"]=lightBedroom; lights["porch"]=lightPorch;
  doc["away_mode"]=awayMode;
  char out[256]; size_t n=serializeJson(doc,out);
  mqtt.publish(topic("status").c_str(),out,n,true);
}
void publishTelemetry() {
  float temp=dht.readTemperature();
  float hum=dht.readHumidity();
  int mq=analogRead(PIN_MQ2);
  bool waterRaw=(digitalRead(PIN_WATER)==LOW);
  bool waterLeak=WATER_WET_IS_LOW ? waterRaw : !waterRaw;
  // Reed switch is closed to GND when magnet is near (door closed).
  bool doorOpen=(digitalRead(PIN_REED)==HIGH);
  bool gasAlert=(mq>=MQ2_TEST_THRESHOLD);
  StaticJsonDocument<384> doc;
  if(!isnan(temp)) doc["temperature_c"]=temp;
  if(!isnan(hum)) doc["humidity_pct"]=hum;
  doc["mq2_adc"]=mq; doc["gas_alert"]=gasAlert;
  doc["water_leak"]=waterLeak; doc["door_open"]=doorOpen;
  JsonObject lights=doc.createNestedObject("lights");
  lights["living"]=lightLiving; lights["bedroom"]=lightBedroom; lights["porch"]=lightPorch;
  doc["away_mode"]=awayMode;
  char out[384]; size_t n=serializeJson(doc,out);
  mqtt.publish(topic("telemetry").c_str(),out,n,false);
  digitalWrite(PIN_BUZZER,(waterLeak || gasAlert) ? HIGH : LOW);
}
void setAway(bool enabled) {
  awayMode=enabled;
  awayPhase=false;
  lastAwayAction=0;
  if(!awayMode) { lightLiving=false; lightBedroom=false; lightPorch=false; applyLights(); }
  publishStatus();
}
void mqttCallback(char* incomingTopic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  if(deserializeJson(doc,payload,length)) return;
  if(doc.containsKey("room") && doc.containsKey("state")) {
    String room=doc["room"].as<String>();
    bool state=doc["state"].as<bool>();
    if(room=="living") lightLiving=state;
    else if(room=="bedroom") lightBedroom=state;
    else if(room=="porch") lightPorch=state;
    applyLights(); publishStatus();
  }
  if(doc["mode"]=="away" && doc.containsKey("enabled")) setAway(doc["enabled"].as<bool>());
}
void connectWiFi() {
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println(); Serial.print("IP: ");Serial.println(WiFi.localIP());
  // Prototype only: encrypts transport but does not authenticate broker certificate.
  // Replace with secureClient.setCACert(ROOT_CA_CERTIFICATE) for deployment.
  secureClient.setInsecure();
  mqtt.setServer(MQTT_HOST,MQTT_PORT); mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
}
void reconnectMQTT() {
  while(!mqtt.connected()) {
    String clientId=String("HI_")+DEVICE_ID+"_"+String((uint32_t)ESP.getEfuseMac(),HEX);
    Serial.print("Connecting MQTT...");
    if(mqtt.connect(clientId.c_str(),MQTT_USER,MQTT_PASSWORD)) {
      Serial.println("connected");
      mqtt.subscribe(topic("command").c_str(),1);
      publishStatus();
    } else {
      Serial.print("failed, rc=");Serial.print(mqtt.state());Serial.println(" retry in 3s");
      delay(3000);
    }
  }
}
bool isEveningWindow() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo,20)) return false;
  return timeinfo.tm_hour>=18 && timeinfo.tm_hour<23;
}
void runAwayMode() {
  if(!awayMode) return;
  // Safety fallback: do not run occupancy lighting outside the evening window.
  if(!isEveningWindow()) {
    if(lightLiving||lightBedroom||lightPorch) {
      lightLiving=false;lightBedroom=false;lightPorch=false;applyLights();publishStatus();
    }
    return;
  }
  unsigned long now=millis();
  if(lastAwayAction==0) {
    lastAwayAction=now;
    awayDuration=random(60000UL,180000UL); // initial delay 1-3 minutes
    return;
  }
  if(now-lastAwayAction>=awayDuration) {
    // Rotate one randomly selected room; staggered changes mimic normal use.
    int room=random(0,3);
    bool newState=random(0,100)<65;
    if(room==0) lightLiving=newState;
    if(room==1) lightBedroom=newState;
    if(room==2) lightPorch=newState;
    applyLights(); publishStatus();
    lastAwayAction=now;
    awayDuration=random(120000UL,480000UL); // next change 2-8 minutes
  }
}
void setup() {
  Serial.begin(115200);
  pinMode(PIN_WATER,INPUT_PULLUP); pinMode(PIN_REED,INPUT_PULLUP);
  pinMode(PIN_BUZZER,OUTPUT); digitalWrite(PIN_BUZZER,LOW);
  pinMode(PIN_RELAY_LIVING,OUTPUT);pinMode(PIN_RELAY_BEDROOM,OUTPUT);pinMode(PIN_RELAY_PORCH,OUTPUT);
  lightLiving=lightBedroom=lightPorch=false;applyLights();
  analogReadResolution(12);
  dht.begin();
  randomSeed(esp_random());
  connectWiFi();
  // Indonesia WIB = UTC+7; adjust to WITA/WIT if required.
  configTime(7*3600,0,"pool.ntp.org","time.nist.gov");
}
void loop() {
  if(WiFi.status()!=WL_CONNECTED) connectWiFi();
  if(!mqtt.connected()) reconnectMQTT();
  mqtt.loop();
  runAwayMode();
  if(millis()-lastTelemetry>=5000) {
    lastTelemetry=millis();
    publishTelemetry();
  }
}
