#!/usr/bin/env python3
"""Puente MQTT <-> Telegram para Rover IoT (RBTMVL).

Usa long polling con requests para evitar problemas de asyncio.
"""
import json
import os
import time
from datetime import datetime
from pathlib import Path

import paho.mqtt.client as mqtt
import requests
from dotenv import load_dotenv

load_dotenv()

TELEGRAM_TOKEN = os.getenv("TELEGRAM_TOKEN", "")
ALLOWED_CHAT_ID = int(os.getenv("ALLOWED_CHAT_ID", "0"))
MQTT_BROKER = os.getenv("MQTT_BROKER", "127.0.0.1")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
ROVER_ID = os.getenv("ROVER_ID", "rover-01")

BASE_DIR = Path(__file__).parent
ALERTS_FILE = BASE_DIR / "alerts.json"
API_URL = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}"

last_telemetry = {}
alerts_history = []
last_update_id = 0

if ALERTS_FILE.exists():
    try:
        with open(ALERTS_FILE, "r", encoding="utf-8") as f:
            alerts_history = json.load(f)
    except Exception:
        alerts_history = []


def save_alerts():
    try:
        data = alerts_history[-20:]
        with open(ALERTS_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
    except Exception as e:
        print(f"Error guardando alertas: {e}", flush=True)


def send_message(chat_id, text, reply_markup=None):
    try:
        data = {"chat_id": chat_id, "text": text}
        if reply_markup:
            data["reply_markup"] = json.dumps(reply_markup)
        r = requests.post(f"{API_URL}/sendMessage", data=data, timeout=10)
        return r.json()
    except Exception as e:
        print(f"Error enviando: {e}", flush=True)
        return None


def answer_callback(callback_id):
    try:
        requests.post(
            f"{API_URL}/answerCallbackQuery",
            data={"callback_query_id": callback_id},
            timeout=5,
        )
    except Exception:
        pass


def main_menu():
    keyboard = [
        [
            {"text": "📊 Ver datos", "callback_data": "cmd_datos"},
            {"text": "ℹ️ Estado", "callback_data": "cmd_estado"},
        ],
        [
            {"text": "🔋 Batería", "callback_data": "cmd_bateria"},
            {"text": "🚨 Alertas", "callback_data": "cmd_alertas"},
        ],
        [
            {"text": "🤖 Modo AUTO", "callback_data": "cmd_mode_auto"},
            {"text": "🎮 Modo MANUAL", "callback_data": "cmd_mode_manual"},
        ],
        [
            {"text": "▶️ Iniciar recorrido", "callback_data": "cmd_start"},
            {"text": "⏹️ Detener recorrido", "callback_data": "cmd_stop"},
        ],
        [{"text": "🛑 E-STOP", "callback_data": "cmd_estop"}],
    ]
    return {"inline_keyboard": keyboard}


# ---------------- MQTT ----------------
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)


def on_mqtt_connect(client, userdata, flags, reason_code, properties):
    print(f"MQTT conectado: {reason_code}", flush=True)
    client.subscribe(f"rover/{ROVER_ID}/telemetry")
    client.subscribe(f"rover/{ROVER_ID}/alerts")
    client.subscribe(f"rover/{ROVER_ID}/heartbeat")
    client.subscribe(f"rover/{ROVER_ID}/state")
    print("Suscrito a tópicos del rover", flush=True)


def on_mqtt_message(client, userdata, msg):
    global alerts_history
    try:
        payload = msg.payload.decode()
        topic = msg.topic
        if topic.endswith("/telemetry") or topic.endswith("/state"):
            data = json.loads(payload)
            last_telemetry.update(data)
            last_telemetry["last_update"] = datetime.now().strftime("%H:%M:%S")
        elif topic.endswith("/alerts"):
            data = json.loads(payload)
            alert = {
                "timestamp": datetime.now().strftime("%d/%m/%Y %H:%M:%S"),
                "type": data.get("type", "ALERTA"),
                "message": data.get("message", payload),
                "raw": data,
            }
            alerts_history.append(alert)
            save_alerts()
            text = f"🚨 ALERTA CRÍTICA - {ROVER_ID.upper()}\n\n"
            text += f"Tipo: {alert['type']}\n"
            text += f"Hora: {alert['timestamp']}\n"
            text += f"Detalle: {alert['message']}\n"
            text += "\nAcción: Revisar zona inmediata"
            if ALLOWED_CHAT_ID:
                send_message(ALLOWED_CHAT_ID, text)
    except Exception as e:
        print(f"Error MQTT: {e}", flush=True)


mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_message = on_mqtt_message
mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
mqtt_client.loop_start()


def publish_cmd(cmd):
    try:
        mqtt_client.publish(f"rover/{ROVER_ID}/cmd", json.dumps(cmd))
        print(f"CMD enviado: {cmd}", flush=True)
    except Exception as e:
        print(f"Error cmd: {e}", flush=True)


# ---------------- Telegram ----------------
def handle_message(chat_id, text):
    if text == "/start":
        send_message(
            chat_id,
            "🤖 Rover IoT Control\n\nSelecciona una opción:",
            reply_markup=main_menu(),
        )
    elif text == "/datos":
        if not last_telemetry:
            send_message(chat_id, "⏳ Sin datos aún", reply_markup=main_menu())
            return
        t = last_telemetry
        s = t.get("sensors", {})
        n = t.get("navigation", {})
        txt = f"📊 DATOS - {t.get('deviceId', ROVER_ID)}\n🕒 {t.get('last_update', 'N/A')}\n\n"
        txt += f"🌡️ Temp: {s.get('temperature', {}).get('value', 'N/A')} °C\n"
        txt += f"💧 Humedad: {s.get('humidity', {}).get('value', 'N/A')} %\n"
        txt += f"💨 Aire (raw): {s.get('airQuality', {}).get('raw', 'N/A')}\n"
        flame = "DETECTADA" if s.get("flame", {}).get("detected") else "NO DETECTADA"
        txt += f"🔥 Llama: {flame}\n"
        b = s.get("battery", {})
        txt += f"🔋 Batería: {b.get('voltage', 'N/A')}V ({b.get('percent', 'N/A')}%)\n"
        txt += f"📏 Distancia frontal: {n.get('distanceFront', {}).get('value', 'N/A')} cm\n"
        patrol = "SI" if t.get("patrol", {}).get("running") else "NO"
        activo = "SI" if t.get("active") else "NO"
        txt += f"\n🤖 Modo: {t.get('mode', 'N/A')} | Activo: {activo} | Patrol: {patrol}"
        send_message(chat_id, txt, reply_markup=main_menu())
    elif text == "/estado":
        if not last_telemetry:
            send_message(chat_id, "⏳ Sin datos aún", reply_markup=main_menu())
            return
        t = last_telemetry
        txt = (
            "ℹ️ ESTADO\n\n"
            f"Online: {'SI' if t.get('online') else 'NO'}\n"
            f"Modo: {t.get('mode', 'N/A')}\n"
            f"Activo: {'SI' if t.get('active') else 'NO'}\n"
            f"Patrol: {'SI' if t.get('patrol', {}).get('running') else 'NO'}\n"
            f"RSSI: {t.get('network', {}).get('rssi', 'N/A')}\n"
            f"Última actualización: {t.get('last_update', 'N/A')}"
        )
        send_message(chat_id, txt, reply_markup=main_menu())
    elif text == "/bateria":
        if not last_telemetry:
            send_message(chat_id, "⏳ Sin datos aún", reply_markup=main_menu())
            return
        b = last_telemetry.get("sensors", {}).get("battery", {})
        send_message(
            chat_id,
            f"🔋 BATERÍA\n\nVoltaje: {b.get('voltage', 'N/A')} V\n"
            f"Porcentaje: {b.get('percent', 'N/A')} %",
            reply_markup=main_menu(),
        )
    elif text == "/alertas":
        if not alerts_history:
            send_message(chat_id, "✅ Sin alertas registradas", reply_markup=main_menu())
            return
        txt = "🚨 ÚLTIMAS ALERTAS\n\n"
        for a in alerts_history[-5:]:
            txt += f"• [{a['timestamp']}] {a['type']}\n  {a['message']}\n\n"
        send_message(chat_id, txt, reply_markup=main_menu())
    elif text == "/estop":
        publish_cmd({"type": "estop"})
        send_message(chat_id, "🛑 E-STOP ENVIADO", reply_markup=main_menu())


def handle_callback(chat_id, callback_id, data):
    answer_callback(callback_id)
    if data == "cmd_datos":
        handle_message(chat_id, "/datos")
    elif data == "cmd_estado":
        handle_message(chat_id, "/estado")
    elif data == "cmd_bateria":
        handle_message(chat_id, "/bateria")
    elif data == "cmd_alertas":
        handle_message(chat_id, "/alertas")
    elif data == "cmd_mode_auto":
        publish_cmd({"type": "mode", "value": "AUTO"})
        send_message(chat_id, "🤖 Modo cambiado a AUTO", reply_markup=main_menu())
    elif data == "cmd_mode_manual":
        publish_cmd({"type": "mode", "value": "MANUAL"})
        send_message(chat_id, "🎮 Modo cambiado a MANUAL", reply_markup=main_menu())
    elif data == "cmd_start":
        publish_cmd({"type": "start", "value": True})
        send_message(chat_id, "▶️ Recorrido iniciado", reply_markup=main_menu())
    elif data == "cmd_stop":
        publish_cmd({"type": "start", "value": False})
        send_message(chat_id, "⏹️ Recorrido detenido", reply_markup=main_menu())
    elif data == "cmd_estop":
        publish_cmd({"type": "estop"})
        send_message(chat_id, "🛑 E-STOP ENVIADO", reply_markup=main_menu())


def handle_update(update):
    global last_update_id
    if "message" in update:
        msg = update["message"]
        chat_id = msg["chat"]["id"]
        text = msg.get("text", "")
        if ALLOWED_CHAT_ID != 0 and chat_id != ALLOWED_CHAT_ID:
            send_message(chat_id, "⛔ No autorizado")
            return
        handle_message(chat_id, text)
    elif "callback_query" in update:
        cq = update["callback_query"]
        chat_id = cq["message"]["chat"]["id"]
        if ALLOWED_CHAT_ID != 0 and chat_id != ALLOWED_CHAT_ID:
            answer_callback(cq["id"])
            return
        handle_callback(chat_id, cq["id"], cq.get("data", ""))


def main():
    global last_update_id
    print("Iniciando puente MQTT-Telegram...", flush=True)
    if not TELEGRAM_TOKEN:
        print("ERROR: Falta TELEGRAM_TOKEN en .env", flush=True)
        return
    if ALLOWED_CHAT_ID == 0:
        print("WARNING: whitelist desactivada (modo prueba)", flush=True)
    send_message(ALLOWED_CHAT_ID, "🔌 Puente MQTT-Telegram iniciado")
    while True:
        try:
            r = requests.get(
                f"{API_URL}/getUpdates",
                params={"timeout": 30, "offset": last_update_id + 1},
                timeout=35,
            )
            data = r.json()
            if data.get("ok"):
                for update in data.get("result", []):
                    last_update_id = update["update_id"]
                    handle_update(update)
            time.sleep(1)
        except Exception as e:
            print(f"Error polling: {e}", flush=True)
            time.sleep(5)


if __name__ == "__main__":
    main()
