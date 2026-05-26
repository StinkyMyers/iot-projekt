// ============================================================
// config.h — Spoločná konfigurácia pre všetky uzly
// Tento súbor je rovnaký pre uzol1, uzol2, uzol3
// Každý uzol je si musí upraviť WiFi konfiguráciu
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// --- WiFi --- 
#define WIFI_SSID     "nazov WiFi"
#define WIFI_PASSWORD "heslo od wifi"          

// --- MQTT Broker ---
#define MQTT_HOST     "147.175.105.185"
#define MQTT_PORT     1883
#define MQTT_PASSWORD "misa"

// --- Perióda merania ---
#define MEASUREMENT_PERIOD_MS 30000     // 30 sekúnd - každý uzol môže 
                                        // upraviť podla potreby senzora

#endif
