#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"

#define MQTT_USER     "uzol1"
#define TOPIC_PREFIX  "fei/uzol1"
#define NODE_ID       "uzol1"

#define DHTPIN  4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient mqtt(espClient);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);  // +1 hod (SK zimný čas)

unsigned long lastMeasurement = 0;

// ── WiFi ─────────────────────────────────────────────────────
void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Pripájam WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" OK " + WiFi.localIP().toString());
  timeClient.begin();
  timeClient.update();
}

// ── MQTT ─────────────────────────────────────────────────────
void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("Pripájam MQTT...");
    if (mqtt.connect(NODE_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("OK");
    } else {
      Serial.print("chyba rc=");
      Serial.println(mqtt.state());
      delay(3000);
    }
  }
}

// ── Publikovanie jednej hodnoty ───────────────────────────────
void publishValue(const char* topic, float value, const char* unit, const char* sensor) {
  timeClient.update();

  StaticJsonDocument<200> doc;
  doc["value"]  = round(value * 100.0) / 100.0;
  doc["unit"]   = unit;
  doc["ts"]     = timeClient.getEpochTime();
  doc["node"]   = NODE_ID;
  doc["sensor"] = sensor;

  char payload[200];
  serializeJson(doc, payload);

  char fullTopic[64];
  snprintf(fullTopic, sizeof(fullTopic), "%s/%s", TOPIC_PREFIX, topic);

  mqtt.publish(fullTopic, payload, true);
  Serial.printf("→ %s : %s\n", fullTopic, payload);
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  dht.begin();

  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
  connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  if (millis() - lastMeasurement >= MEASUREMENT_PERIOD_MS) {
    lastMeasurement = millis();

    float teplota = dht.readTemperature();
    float vlhkost = dht.readHumidity();

    if (!isnan(teplota)) publishValue("temperature", teplota, "°C",  "DHT11");
    if (!isnan(vlhkost)) publishValue("humidity",    vlhkost, "%RH", "DHT11");
  }
}