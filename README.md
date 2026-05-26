# IoT Projekt — Distribuovaný systém monitorovania environmentálnych veličín

Systém meria environmentálne veličiny v troch miestnostiach FEI pomocou senzorov napojených na NodeMCU jednotky. Namerané dáta sú odosielané cez MQTT protokol na centrálny server (Raspberry Pi 4B), kde sú ukladané do databázy a vizualizované v reálnom čase cez webový dashboard.

**Tím:** Lea Lenka Ondrejková · Kristián Pauliny · Marian Sušina · Samuel Zorvan · Jozef Csabi

---

## Architektúra systému

```
┌─────────────────────────────────────────────┐
│ VRSTVA SENZOROV                             │
│ DHT11 · GP2Y10 · ENS160 · MQ-4 · BME280    │
└────────────────────┬────────────────────────┘
                     │ I²C / UART / ADC
┌────────────────────▼────────────────────────┐
│ VRSTVA UZLOV (3× NodeMCU ESP8266)           │
│ Čítanie senzorov · JSON · MQTT publish      │
└────────────────────┬────────────────────────┘
                     │ MQTT over WiFi (TCP/IP)
┌────────────────────▼────────────────────────┐
│ MOSQUITTO BROKER (Raspberry Pi 4B)          │
│ Port 1883 · autentifikácia · retain=True    │
└────────────────────┬────────────────────────┘
                     │ lokálny subscribe
┌────────────────────▼────────────────────────┐
│ DATABÁZA SQLite (Raspberry Pi 4B)           │
│ mqtt_to_db.py · časové rady                 │
└────────────────────┬────────────────────────┘
                     │ HTTP
┌────────────────────▼────────────────────────┐
│ NODE-RED DASHBOARD (Raspberry Pi 4B)        │
│ Live grafy · história 24h · verejná IP      │
└─────────────────────────────────────────────┘
```

Schéma zapojenia jednotlivých uzlov: [`docs/schema.png`](docs/schema.png)

---

## Hardvér a senzory

| Komponent | Popis |
|---|---|
| Raspberry Pi 4B | Centrálny server — broker, databáza, dashboard |
| 3× NodeMCU ESP8266 | Meracie uzly |
| DHT11 | Teplota (°C) a vlhkosť (%RH) — Uzol 1 |
| GP2Y10 | Koncentrácia prachu (µg/m³) — Uzol 1 |
| ENS160 | AQI, TVOC (ppb), eCO2 (ppm) — Uzol 2 |
| MQ-4 | Metán CH4 (ppm) — Uzol 3 |
| BME280 | Teplota (°C), vlhkosť (%RH), tlak (hPa) — Uzol 3 |

---

## MQTT topiky

| Topik | Veličina | Jednotka | Senzor |
|---|---|---|---|
| fei/uzol1/temperature | Teplota | °C | DHT11 |
| fei/uzol1/humidity | Vlhkosť | %RH | DHT11 |
| fei/uzol1/dust | Prach | µg/m³ | GP2Y10 |
| fei/uzol2/aqi | Index kvality ovzdušia | index | ENS160 |
| fei/uzol2/tvoc | Celkové organické zlúčeniny | ppb | ENS160 |
| fei/uzol2/eco2 | Ekvivalentný CO2 | ppm | ENS160 |
| fei/uzol3/ch4 | Metán | ppm | MQ-4 |
| fei/uzol3/temperature | Teplota | °C | BME280 |
| fei/uzol3/humidity | Vlhkosť | %RH | BME280 |
| fei/uzol3/pressure | Atmosferický tlak | hPa | BME280 |

---

## Formát správ (JSON)

Každý uzol posiela správy v nasledujúcom formáte:

```json
{
  "value": 23.4,
  "unit": "°C",
  "ts": 1778771360,
  "node": "uzol1",
  "sensor": "DHT11"
}
```

Všetky správy sú publikované s `retain=True` a periódou **30 sekúnd**. Perióda bola zvolená s ohľadom na pomalú dynamiku meraných veličín (teplota, vlhkosť, kvalita ovzdušia) a obmedzenia senzorov (DHT11 min. 2s, ENS160 warm-up 3 min).

---

## Inštalačný návod — Raspberry Pi

### 1. Základná konfigurácia OS

```bash
# Aktualizácia systému
sudo apt update && sudo apt upgrade -y

# Nastavenie časovej zóny
sudo timedatectl set-timezone Europe/Bratislava

# Povolenie SSH (ak nie je aktívne)
sudo systemctl enable ssh
sudo systemctl start ssh
```

### 2. Inštalácia Mosquitto

```bash
sudo apt install mosquitto mosquitto-clients -y

# Skopíruj konfiguračný súbor
sudo cp server/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf

# Vytvor používateľské účty
sudo mosquitto_passwd -c /etc/mosquitto/passwd uzol1
sudo mosquitto_passwd /etc/mosquitto/passwd uzol2
sudo mosquitto_passwd /etc/mosquitto/passwd uzol3
sudo mosquitto_passwd /etc/mosquitto/passwd vizualizacia

# Spusti a povoľ autostart
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# Overenie
sudo systemctl status mosquitto
```

### 3. Inštalácia závislostí pre databázu

```bash
sudo apt install python3-pip sqlite3 -y
pip3 install paho-mqtt --break-system-packages
```

### 4. Spustenie databázového skriptu

```bash
# Skopíruj skript
mkdir -p ~/Desktop/server
cp server/database/mqtt_to_db.py ~/Desktop/server/mqtt_to_db.py

# Manuálne spustenie (test)
python3 ~/Desktop/server/mqtt_to_db.py

# Autostart cez systemd
sudo cp server/database/mqtt_to_db.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mqtt_to_db
sudo systemctl start mqtt_to_db
```

### 5. Inštalácia Node-RED

```bash
bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered)

# Autostart
sudo systemctl enable nodered
sudo systemctl start nodered
```

### 6. Import Node-RED flow

1. Otvor `http://<IP>:1880` v prehliadači
2. ☰ menu → **Import** → vlož obsah `server/visualization/MISA_Visualisation_NodeRED.json`
3. Klikni **Deploy**

### 7. Nastavenie firewall (UFW)

```bash
sudo apt install ufw -y
sudo ufw allow 22
sudo ufw allow 1880
sudo ufw allow 1883
sudo ufw enable
```

---

## Nahranie firmvéru do NodeMCU

### Požiadavky

- PlatformIO (odporúčané) alebo Arduino IDE
- Knižnice: `PubSubClient`, `ArduinoJson`, knižnice pre príslušné senzory

### Postup

```bash
# 1. Uprav konfiguračný súbor
nano firmware/shared/config.h
# Nastav: WIFI_SSID, WIFI_PASSWORD, MQTT_HOST (IP RPi)

# 2. Pripoj NodeMCU cez USB

# 3. Nahraj firmvér (PlatformIO)
cd firmware/uzol1
pio run --target upload

# Opakuj pre uzol2 a uzol3
```

---

## Prístup na dashboard

Dashboard je dostupný cez verejnú IP adresu Raspberry Pi:

```
http://147.175.105.185:1880/ui
```

Node-RED editor (len v lokálnej sieti):
```
http://10.9.5.159:1880
```

---

## Štruktúra repozitára

```
/
├── firmware/
│   ├── shared/          # Spoločná konfigurácia (config.h)
│   ├── uzol1/           # Firmvér Uzol 1 (DHT11 + GP2Y10)
│   ├── uzol2/           # Firmvér Uzol 2 (ENS160)
│   └── uzol3/           # Firmvér Uzol 3 (MQ-4 + BME280)
├── server/
│   ├── mosquitto/       # Konfigurácia Mosquitto brokera
│   ├── database/        # Python skript pre ukladanie do SQLite
│   └── visualization/   # Node-RED flow (JSON export)
├── docs/
│   ├── schema.png       # Schéma architektúry systému
│   └── topics.md        # Tabuľka MQTT topikov
└── README.md
```
