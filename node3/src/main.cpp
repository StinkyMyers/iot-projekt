#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <MQUnifiedsensor.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include "config.h"

#define MQTT_USER     "uzol3"
#define TOPIC_PREFIX  "fei/uzol3"
#define NODE_ID       "uzol3"

#define SDA_PIN D2
#define SCL_PIN D1

#define BOARD             "ESP8266"
#define MQ4_PIN           A0
#define VOLTAGE_RES       3.3
#define ADC_BITS          10
#define RATIO_MQ4_CLEAN   4.4

MQUnifiedsensor mq4(BOARD, VOLTAGE_RES, ADC_BITS, MQ4_PIN, "MQ-4");

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
bool ahtOk = false;
bool bmpOk = false;

WiFiClient espClient;
PubSubClient mqtt(espClient);

unsigned long lastMeasurement = 0;

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

void publishValue(const char* topic, float value, const char* unit, const char* sensor) {
  StaticJsonDocument<200> doc;
  doc["value"]  = round(value * 100.0) / 100.0;
  doc["unit"]   = unit;
  doc["ts"]     = millis() / 1000;
  doc["node"]   = NODE_ID;
  doc["sensor"] = sensor;

  char payload[200];
  serializeJson(doc, payload);

  char fullTopic[64];
  snprintf(fullTopic, sizeof(fullTopic), "%s/%s", TOPIC_PREFIX, topic);

  mqtt.publish(fullTopic, payload, true);
  Serial.printf("→ %s : %s\n", fullTopic, payload);
}

void setup() {
  Serial.begin(115200);
  delay(3000);

  Wire.begin(SDA_PIN, SCL_PIN);

  ahtOk = aht.begin();
  if (!ahtOk) {
    Serial.println("CHYBA: AHT20 nenájdený! Skontroluj zapojenie SDA/SCL.");
  } else {
    Serial.println("AHT20 OK.");
  }

  bmpOk = bmp.begin(0x76);
  if (!bmpOk) bmpOk = bmp.begin(0x77);
  if (!bmpOk) {
    Serial.println("CHYBA: BMP280 nenájdený! Skontroluj I2C adresu.");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
    Serial.println("BMP280 OK.");
  }

  mq4.setRegressionMethod(1);
  mq4.setA(1012.7);
  mq4.setB(-2.786);
  mq4.init();

  Serial.print("Kalibrujem MQ4 v čistom vzduchu");
  float calcR0 = 0;
  for (int i = 0; i < 10; i++) {
    mq4.update();
    calcR0 += mq4.calibrate(RATIO_MQ4_CLEAN);
    Serial.print(".");
    delay(1000);
  }
  mq4.setR0(calcR0 / 10.0);
  Serial.println(" hotovo.");

  if (isinf(calcR0) || calcR0 == 0) {
    Serial.println("CHYBA: Skontroluj zapojenie MQ4 (A0 pin)!");
    while (1);
  }

  connectWifi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
}

void loop() {
  connectWifi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  if (millis() - lastMeasurement >= MEASUREMENT_PERIOD_MS) {
    lastMeasurement = millis();

    // ── MQ4 ─────────────────────────────────────────────────
    mq4.update();
    float ch4_ppm = mq4.readSensor();
    publishValue("ch4", ch4_ppm, "ppm", "MQ4");

    // ── AHT20 ───────────────────────────────────────────────
    if (ahtOk) {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);
      publishValue("temperature", temp.temperature, "°C", "AHT20");
      publishValue("humidity", humidity.relative_humidity, "%RH", "AHT20");
      //publishValue("temperature", temp.temperature, "°C", "AHT20"); // duplicate removed
    }

    // ── BMP280 ──────────────────────────────────────────────
    if (bmpOk) {
      float pressure_hpa = bmp.readPressure() / 100.0F;
      //float bmp_temp     = bmp.readTemperature();
      publishValue("pressure", pressure_hpa, "hPa", "BMP280");
      //publishValue("temperature_bmp", bmp_temp, "°C", "BMP280");
    }
  }
}