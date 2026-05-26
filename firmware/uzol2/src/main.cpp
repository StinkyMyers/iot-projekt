#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <DFRobot_ENS160.h>
#include "config.h"

// ============================================================
// UZOL 2 — ENS160 (vzduch: AQI, TVOC, eCO2)
//   - MQTT_USER    → uzol2
//   - TOPIC_PREFIX → fei/uzol2
//   - NODE_ID      → uzol2
// ============================================================
#define MQTT_USER     "uzol2"
#define TOPIC_PREFIX  "fei/uzol2"
#define NODE_ID       "uzol2"

DFRobot_ENS160_I2C ens160(&Wire, 0x53);  // adresa 0x53 alebo 0x52
WiFiClient espClient;
PubSubClient mqtt(espClient);

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
  StaticJsonDocument<200> doc;
  doc["value"]  = round(value * 100.0) / 100.0;
  doc["unit"]   = unit;
  doc["ts"]     = millis() / 1000;   // ideálne: NTP čas
  doc["node"]   = NODE_ID;
  doc["sensor"] = sensor;

  char payload[200];
  serializeJson(doc, payload);

  char fullTopic[64];
  snprintf(fullTopic, sizeof(fullTopic), "%s/%s", TOPIC_PREFIX, topic);

  mqtt.publish(fullTopic, payload, true);   // retain = true
  Serial.printf("→ %s : %s\n", fullTopic, payload);
}

// ── Setup ────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // I2C pre ENS160 (D2=SDA=GPIO4, D1=SCL=GPIO5)
  Wire.begin(4, 5);

  // Inicializácia senzora
  while (ens160.begin() != 0) {
    Serial.println("ENS160 nenájdený!");
    delay(2000);
  }
  ens160.setPWRMode(ENS160_STANDARD_MODE);
  Serial.println("ENS160 pripravený.");

  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

// ── Loop ─────────────────────────────────────────────────────
void loop() {
  // Udržuj spojenia
  connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  // Meranie podľa periódy
  if (millis() - lastMeasurement >= MEASUREMENT_PERIOD_MS) {
    lastMeasurement = millis();

    // ── Čítanie ENS160 ──────────────────────────────────────
    uint8_t  aqi  = ens160.getAQI();
    uint16_t tvoc = ens160.getTVOC();
    uint16_t eco2 = ens160.getECO2();

    publishValue("aqi",  (float)aqi,  "index", "ENS160");
    publishValue("tvoc", (float)tvoc, "ppb",   "ENS160");
    publishValue("eco2", (float)eco2, "ppm",   "ENS160");
    // ────────────────────────────────────────────────────────
  }
}
