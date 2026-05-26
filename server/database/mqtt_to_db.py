import paho.mqtt.client as mqtt
import sqlite3
import json
import time

# --- Konfigurácia ---
MQTT_HOST = "localhost"
MQTT_PORT = 1883
MQTT_USER = "vizualizacia"
MQTT_PASS = ""  # doplň heslo pred spustením
DB_PATH = "/home/misa26/Desktop/server/data.db"

# --- Databáza ---
def init_db():
    con = sqlite3.connect(DB_PATH)
    con.execute("""
        CREATE TABLE IF NOT EXISTS merania (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            ts        INTEGER,
            node      TEXT,
            topic     TEXT,
            velicina  TEXT,
            value     REAL,
            unit      TEXT,
            sensor    TEXT
        )
    """)
    con.commit()
    return con

def save(con, topic, payload):
    try:
        data = json.loads(payload)
        velicina = topic.split("/")[-1]
        con.execute(
            "INSERT INTO merania (ts, node, topic, velicina, value, unit, sensor) VALUES (?,?,?,?,?,?,?)",
            (
                data.get("ts", int(time.time())),
                data.get("node", ""),
                topic,
                velicina,
                data.get("value"),
                data.get("unit", ""),
                data.get("sensor", ""),
            )
        )
        con.commit()
        print(f"✓ {topic} → {data.get('value')} {data.get('unit','')}")
    except Exception as e:
        print(f"✗ chyba: {e}")

# --- MQTT ---
con = init_db()

def on_message(client, userdata, msg):
    save(con, msg.topic, msg.payload.decode())

client = mqtt.Client()
client.username_pw_set(MQTT_USER, MQTT_PASS)
client.on_message = on_message
client.connect(MQTT_HOST, MQTT_PORT)
client.subscribe("fei/#")

print("Počúvam na fei/# ...")
client.loop_forever()