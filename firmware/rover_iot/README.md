# Firmware Rover IoT (ESP32)

Sketch Arduino para el rover: lee 5 sensores, controla los motores con L298N y habla MQTT con el broker. La seguridad (evasión y E-STOP) se resuelve localmente en el ESP32.

## Requisitos
- Arduino IDE 2.x o `arduino-cli`
- Core **esp32 by Espressif** (probado con 3.3.11)
- Librerías:
  - `DHT sensor library` (Adafruit) + `Adafruit Unified Sensor`
  - `PubSubClient` (Nick O'Leary)
  - `ArduinoJson` (Benoit Blanchon) 7.x

Instalación rápida con `arduino-cli`:
```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "DHT sensor library" "PubSubClient" "ArduinoJson"
```

## Configuración
Edita `config.h` antes de compilar:

| Define | Qué poner |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | Datos de tu red |
| `MQTT_HOST` | IP del equipo con Mosquitto (no `127.0.0.1`: el ESP32 es otro dispositivo) |
| `ROVER_ID` | `rover-01` (debe coincidir con el puente y la app) |
| `GAS_BASELINE` | Lectura del MQ-135 en aire limpio |
| `BATTERY_CAL` | Ajuste fino con multímetro |

Compilar y flashear:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/rover_iot
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 firmware/rover_iot
```

## Pines
| Componente | Señal | GPIO |
|---|---|---|
| DHT22 | Datos | 4 |
| MQ-135 | AO (con divisor 10k/20k) | 34 |
| Llama IR | DO | 35 |
| Divisor batería (20k/10k) | Punto medio | 32 |
| HC-SR04 | Trig | 5 |
| HC-SR04 | Echo (divisor 1k/2k) | 18 |
| L298N | IN1 / IN2 | 26 / 27 |
| L298N | IN3 / IN4 | 14 / 13 |
| L298N | ENA / ENB (PWM) | 25 / 33 |

> El GPIO13 se usa en IN4 para evitar GPIO12 (pin de arranque). Los sensores analógicos van en ADC1 (funciona con WiFi activo).

## Tópicos MQTT
| Tópico | Dirección | Contenido |
|---|---|---|
| `rover/<ROVER_ID>/telemetry` | ESP32 -> app/bot | JSON completo cada 10 s |
| `rover/<ROVER_ID>/state` | ESP32 -> app/bot | Estado (retained + LWT) |
| `rover/<ROVER_ID>/alerts` | ESP32 -> app/bot | Alertas críticas |
| `rover/<ROVER_ID>/heartbeat` | ESP32 -> app/bot | Cada 3 s |
| `rover/<ROVER_ID>/cmd` | app/bot -> ESP32 | Comandos JSON |

Además, con `PUBLISH_LEGACY_TOPICS 1` se publican los tópicos planos de la Fase 1: `rover/temp`, `rover/hum`, `rover/gas`, `rover/flama`, `rover/bateria`.

## Comandos (`rover/<ROVER_ID>/cmd`)
```json
{"type": "mode", "value": "AUTO"}
{"type": "mode", "value": "MANUAL"}
{"type": "move", "dir": "FORWARD", "speed": 150}
{"type": "move", "dir": "STOP"}
{"type": "start", "value": true}
{"type": "start", "value": false}
{"type": "power", "value": false}
{"type": "estop"}
{"type": "estop", "value": false}
```
Direcciones: `FORWARD`, `BACKWARD`, `LEFT`, `RIGHT`, `STOP`.

## Comportamiento
- **AUTO + patrol activo**: avanza; a menos de 20 cm frena, retrocede 0.5 s y gira 0.6 s. Si no logra avanzar durante 10 s, publica `OBSTACLE_BLOCKED`, se detiene y desactiva el patrullaje.
- **MANUAL**: obedece el joystick. Si pasan 1.2 s sin comandos, se detiene (hombre muerto).
- **E-STOP**: bloquea todos los comandos (salvo desbloqueo explícito con `{"type":"estop","value":false}`) y detiene motores.
- **Pérdida de WiFi/MQTT**: detiene motores y reintenta conexión. Al reconectar publica el estado.
- **Alertas**: temperatura > 40 °C, gas sobre baseline, llama detectada, batería < 20 %, obstáculo bloqueado, E-STOP.

## Notas de seguridad eléctrica
- Alimenta el ESP32 y sensores desde el buck de 5 V; tierras comunes.
- No alimentes el HC-SR04 Echo directo al ESP32: usa el divisor 1k/2k.
- El AO del MQ-135 necesita divisor 10k/20k antes del ADC.
- Añade protección/BMS a las 18650 en serie.

## Pendiente (Fase 2)
- Calibración real de `GAS_BASELINE` y `BATTERY_CAL`.
- Definir cómo delimitar el campo (requiere encoders/IMU, cambia hardware y pines).
- Umbrales configurables desde la app (`{"type":"config", ...}`).
