from flask import Flask, request, jsonify, render_template
import sqlite3
import json
from pathlib import Path
from datetime import datetime, timezone
import threading
import paho.mqtt.client as mqtt


app = Flask(__name__)

BASE_DIR = Path(__file__).resolve().parent
DB = BASE_DIR / "telemetry.db"

MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
MQTT_TOPIC = "asset/+/telemetry"


# ---------------------------------------------------------
# Database
# ---------------------------------------------------------

def conn():
    c = sqlite3.connect(DB, check_same_thread=False)
    c.row_factory = sqlite3.Row
    return c


def init_db():

    with conn() as c:

        c.execute("""
        CREATE TABLE IF NOT EXISTS telemetry
        (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            received_at TEXT,
            asset_id TEXT,
            event TEXT,
            fix INTEGER,
            lat REAL,
            lon REAL,
            temp REAL,
            hum REAL,
            ax REAL,
            ay REAL,
            az REAL,
            speed REAL,
            course REAL,
            utc INTEGER,
            raw TEXT
        )
        """)

        c.commit()


# ---------------------------------------------------------
# Store telemetry
# ---------------------------------------------------------

def store_telemetry(d):

    if not isinstance(d, dict):
        return False

    if "id" not in d or "event" not in d:
        return False

    with conn() as c:

        c.execute("""
        INSERT INTO telemetry
        (
            received_at,
            asset_id,
            event,
            fix,
            lat,
            lon,
            temp,
            hum,
            ax,
            ay,
            az,
            speed,
            course,
            utc,
            raw
        )
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,

        (
            datetime.now(timezone.utc).isoformat(),

            d.get("id"),

            d.get("event"),

            int(bool(d.get("fix", 0))),

            d.get("lat"),
            d.get("lon"),

            d.get("temp"),
            d.get("hum"),

            d.get("ax"),
            d.get("ay"),
            d.get("az"),

            d.get("speed"),
            d.get("course"),

            d.get("utc"),

            json.dumps(d)
        ))

        c.commit()

    return True


# ---------------------------------------------------------
# MQTT callbacks
# ---------------------------------------------------------

def mqtt_on_connect(client, userdata, flags, reason_code, properties):

    print("MQTT connected:", reason_code)

    client.subscribe(MQTT_TOPIC)

    print("Subscribed to:", MQTT_TOPIC)


def mqtt_on_message(client, userdata, msg):

    try:

        payload = msg.payload.decode("utf-8")

        print("MQTT:", msg.topic)
        print(payload)

        data = json.loads(payload)

        if store_telemetry(data):

            print("Telemetry stored")

        else:

            print("Invalid telemetry")

    except Exception as e:

        print("MQTT processing error:", e)


# ---------------------------------------------------------
# MQTT thread
# ---------------------------------------------------------

def mqtt_thread():

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id="asset-tracker-dashboard"
    )

    client.on_connect = mqtt_on_connect
    client.on_message = mqtt_on_message

    try:

        print(
            "Connecting to MQTT broker:",
            MQTT_BROKER,
            MQTT_PORT
        )

        client.connect(
            MQTT_BROKER,
            MQTT_PORT,
            60
        )

        client.loop_forever()

    except Exception as e:

        print("MQTT connection error:", e)


# ---------------------------------------------------------
# Dashboard
# ---------------------------------------------------------

@app.get("/")
def index():

    return render_template("index.html")


# ---------------------------------------------------------
# Latest telemetry
# ---------------------------------------------------------

@app.get("/api/latest")
def latest():

    with conn() as c:

        row = c.execute("""
        SELECT *
        FROM telemetry
        ORDER BY id DESC
        LIMIT 1
        """).fetchone()

    if row is None:

        return jsonify({})

    return jsonify(dict(row))


# ---------------------------------------------------------
# History
# ---------------------------------------------------------

@app.get("/api/history")
def history():

    n = request.args.get(
        "limit",
        50,
        type=int
    )

    n = min(
        max(n, 1),
        500
    )

    with conn() as c:

        rows = c.execute("""
        SELECT *
        FROM telemetry
        ORDER BY id DESC
        LIMIT ?
        """, (n,)).fetchall()

    return jsonify([
        dict(row)
        for row in rows
    ])


# ---------------------------------------------------------
# Manual REST telemetry
#
# Useful for testing before ESP-01 is connected.
# ---------------------------------------------------------

@app.post("/api/telemetry")
def post_telemetry():

    data = request.get_json(silent=True)

    if not store_telemetry(data):

        return jsonify({
            "ok": False,
            "error": "invalid telemetry"
        }), 400

    return jsonify({
        "ok": True
    })


# ---------------------------------------------------------
# Main
# ---------------------------------------------------------

if __name__ == "__main__":
    init_db()

    mqtt_thread_obj = threading.Thread(
        target=mqtt_thread,
        daemon=True
    )

    mqtt_thread_obj.start()

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=True,
        use_reloader=False
    )