#include <Arduino.h>
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

// ── Piny pre DHT11 ───────────────────────────────────────────
#define DHTPIN  D2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ── Piny pre GP2Y1014AU0F (Senzor prachu) ────────────────────
#define DUST_MEASURE_PIN A0
#define DUST_LED_PIN     D5

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

// ── Publikovanie jednej hodnoty ──────────────────────────────
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

  // Inicializácia LED pre senzor prachu
  pinMode(DUST_LED_PIN, OUTPUT);
  digitalWrite(DUST_LED_PIN, HIGH); // Východzí stav: LED zhasnutá (senzor číta logiku opačne)

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

    // 1. Čítanie DHT11
    float teplota = dht.readTemperature();
    float vlhkost = dht.readHumidity();

    // 2. Čítanie prachového senzora
    digitalWrite(DUST_LED_PIN, LOW);       // Zapnutie IR LED
    delayMicroseconds(280);                // Podľa datasheetu treba čakať 280 µs
    int voMeasured = analogRead(DUST_MEASURE_PIN); // Prečítanie analógovej hodnoty
    delayMicroseconds(40);                 // Ďalších 40 µs
    digitalWrite(DUST_LED_PIN, HIGH);      // Vypnutie IR LED
    delayMicroseconds(9680);               // Zvyšok cyklu do 10ms

    // Výpočet hustoty prachu
    float calcVoltage = voMeasured * (3.3 / 1024.0); // Prevod pre 10-bit ADC a 3.3V logiku ESP8266
    float dustDensity = (0.17 * calcVoltage - 0.1) * 1000; // Prevod na µg/m³
    
    if (dustDensity < 0) dustDensity = 0; // V úplne čistom vzduchu môže výpočet klesnúť do mínusu

    // 3. Publikovanie dát na broker
    if (!isnan(teplota)) publishValue("temperature", teplota, "°C",  "DHT11");
    if (!isnan(vlhkost)) publishValue("humidity",    vlhkost, "%RH", "DHT11");
    publishValue("dust", dustDensity, "µg/m³", "GP2Y10");
  }
}