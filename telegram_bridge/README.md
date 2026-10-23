# Puente MQTT - Telegram Bot (RBTMVL)

Puente para recibir alertas, consultar datos y enviar comandos rápidos al Rover IoT desde Telegram sin depender de la PWA.

## Requisitos
- Python 3.10+
- Mosquitto corriendo (por defecto `127.0.0.1:1883`)
- Bot creado con `@BotFather`
- Tu `chat_id` (puedes obtenerlo enviando un mensaje al bot y consultando `getUpdates`)

## Instalación
```bash
cd telegram_bridge
pip install --user --break-system-packages -r requirements.txt
cp .env.example .env
# Editar .env con TELEGRAM_TOKEN y ALLOWED_CHAT_ID
```

## Configuración (`.env`)
| Variable | Descripción |
|---|---|
| `TELEGRAM_TOKEN` | Token entregado por `@BotFather` |
| `ALLOWED_CHAT_ID` | Tu chat_id (whitelist). `0` desactiva la whitelist (solo pruebas) |
| `MQTT_BROKER` | IP del broker (por defecto `127.0.0.1`) |
| `MQTT_PORT` | Puerto del broker (por defecto `1883`) |
| `ROVER_ID` | Identificador del rover (por defecto `rover-01`) |

## Ejecución
```bash
python3 bridge.py
```

## Funcionalidades
- **Alertas automáticas** desde `rover/<ROVER_ID>/alerts` enviadas al chat autorizado.
- **Menú con botones inline** al enviar `/start`.
- **Consultas:** `/datos`, `/estado`, `/bateria`, `/alertas`.
- **Control rápido:** botones de Modo AUTO/MANUAL, Iniciar/Detener recorrido y E-STOP.
- Guarda la última telemetría en memoria y las últimas 20 alertas en `alerts.json`.
- Whitelist por `chat_id`.

## Tópicos MQTT usados
| Tópico | Dirección |
|---|---|
| `rover/<ROVER_ID>/telemetry` | ESP32 -> puente |
| `rover/<ROVER_ID>/state` | ESP32 -> puente |
| `rover/<ROVER_ID>/alerts` | ESP32 -> puente |
| `rover/<ROVER_ID>/cmd` | puente -> ESP32 |

## Notas
- El puente usa long polling con `requests` (sin `python-telegram-bot`) para evitar problemas de asyncio en Python 3.14.
- La lógica de seguridad (evasión de obstáculos, detección de llama) permanece **100% local en el ESP32**. El bot solo envía comandos.
- Para que el E-STOP sea confiable, el ESP32 debe priorizar su procesamiento por encima de cualquier otro comando.
