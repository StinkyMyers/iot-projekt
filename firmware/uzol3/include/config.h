// ============================================================
// config.h — Spoločná konfigurácia pre všetky uzly
// Tento súbor je rovnaký pre uzol1, uzol2, uzol3
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi ---
//#define WIFI_SSID     "d201"
//#define WIFI_PASSWORD "MechaTronyka"  

#define WIFI_SSID       "D109"
#define WIFI_PASSWORD   "mackoUsko109"

// --- MQTT Broker ---
//#define MQTT_HOST     "192.168.11.100"
#define MQTT_HOST     "192.168.88.164"
#define MQTT_PORT     1883
#define MQTT_PASSWORD "misa"

// --- Perióda merania ---
#define MEASUREMENT_PERIOD_MS 30000   // 30 sekúnd

#endif
