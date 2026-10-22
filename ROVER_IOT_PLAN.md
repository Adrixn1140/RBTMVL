# Plan de Organización - Rover IoT RBTMVL

## 1. Objetivo
Tener una interfaz (PWA) que permita **controlar el rover de forma híbrida**: dejarlo en modo autónomo, tomar control manual cuando lo necesitemos, recibir alertas y tener control total sobre él.

## 2. Funcionalidades por Prioridad

### A) Imprescindible (MVP)
| Funcionalidad | ¿Por qué? | Estado |
|---|---|---|
| **Dashboard en vivo** | Ver temperatura, humedad, calidad de aire, llama, batería, distancia frontal y modo actual. | Pendiente |
| **Modo AUTO / MANUAL** | Poder alternar entre navegación autónoma y control por el usuario. | Pendiente |
| **Control manual (joystick/flechas)** | Mover el rover cuando esté en modo MANUAL. | Pendiente |
| **Parada de Emergencia (E-STOP)** | Botón grande que detenga los motores inmediatamente. Debe tener mayor prioridad que cualquier otro comando. | Pendiente |
| **Encendido/Apagado lógico** | Activar o desactivar el rover sin tener que desenchufar baterías. | Pendiente |
| **Alertas críticas** | Recibir alerta visual + notificación al celular al detectar llama, batería baja o distancia crítica. | Pendiente |
| **Estado online/offline** | Saber si el ESP32 está conectado o se perdió la comunicación (heartbeat). | Pendiente |

### B) Deseable (Para después del MVP)
| Funcionalidad | ¿Por qué? | Estado |
|---|---|---|
| **Historial de alertas** | Guardar las alertas con fecha y hora para revisarlas luego. | Pendiente |
| **Umbrales configurables desde la app** | Cambiar umbrales (batería, distancia, etc) sin reprogramar el ESP32. | Pendiente |
| **Indicador de batería con porcentaje** | Más intuitivo que solo mostrar el voltaje. | Pendiente |
| **Sonido/vibración en alertas** | Útil para reaccionar rápido ante una alerta crítica. | Pendiente |
| **Logs de telemetría** | Poder ver un breve historial de las últimas lecturas. | Pendiente |

## 3. Arquitectura a seguir

```text
[ESP32] --MQTT--> [Broker Mosquitto] --WebSocket--> [PWA]
                        ^                              |
                        |------------------------------|
                              Comandos (modo, move, stop)
```

- **ESP32**: Sigue teniendo la lógica de seguridad local (evasión de obstáculos + detección de llama). **Nunca** debe esperar a la app para frenar.
- **Broker MQTT**: Por ahora, lo mejor es `Mosquitto` en tu red local.
- **PWA**: Será nuestro "centro de mando". Se conectará por WebSocket, mostrará datos, enviará comandos y lanzará notificaciones.

## 4. Comunicación MQTT (Tópicos y Comandos)

### Tópicos
| Tópico | Dirección | Uso |
|---|---|---|
| `rover/rover-01/telemetry` | ESP32 -> App | Datos agrupados de todos los sensores, modo, distancia y estado. |
| `rover/rover-01/alerts` | ESP32 -> App | Solo alertas críticas (fuego, batería crítica, obstáculo bloqueado). |
| `rover/rover-01/heartbeat` | ESP32 -> App | Señal para saber que sigue vivo (cada 2-5 segundos). |
| `rover/rover-01/state` | ESP32 -> App | Estado actual (activo/inactivo, AUTO/MANUAL, patrol/running, motores). Ideal al abrir/reconectar la app. |
| `rover/rover-01/cmd` | App -> ESP32 | Todos los comandos unificados en un solo JSON. |

### Comandos para `cmd`
| Comando | Ejemplo JSON | Acción |
|---|---|---|
| Cambiar modo | `{"type": "mode", "value": "AUTO"}` o `"MANUAL"` | Cambia entre autónomo y manual |
| Mover (solo MANUAL) | `{"type": "move", "dir": "FORWARD", "speed": 150}` | Avanzar/retroceder/girar/izquierda/derecha |
| Parada de emergencia | `{"type": "estop"}` | Detener motores inmediatamente |
| Encendido/Apagado lógico | `{"type": "power", "value": true/false}` | Activar o desactivar the rover |
| Iniciar/Detener recorrido | `{"type": "start", "value": true/false}` | Iniciar o detener el patrullaje en modo AUTO |
| Actualizar umbrales (futuro) | `{"type": "config", "thresholds": {...}}` | Editar umbrales desde la app |

## 5. Estructura JSON base para Telemetría
```json
{
  "deviceId": "rover-01",
  "timestamp": 1729612345678,
  "online": true,
  "mode": "AUTO",
  "active": true,
  "patrol": { "running": false },
  "sensors": {
    "temperature": { "value": 24.5, "unit": "C", "valid": true },
    "humidity": { "value": 55.2, "unit": "%", "valid": true },
    "airQuality": { "raw": 1234, "valid": true },
    "flame": { "detected": false, "raw": 4095, "valid": true },
    "battery": { "voltage": 7.62, "percent": 78, "valid": true }
  },
  "navigation": {
    "distanceFront": { "value": 45, "unit": "cm", "valid": true },
    "action": "FORWARD"
  },
  "network": {
    "rssi": -65
  }
}
```

## 6. Integración con Telegram (Definida)

**Decisión:** Optaremos por **Telegram Bot** para recibir alertas, consultar datos y ejecutar comandos rápidos desde el chat.

### Razones
- Gratis, rápido y fácil de crear con `@BotFather`
- Notificaciones instantáneas para alertas críticas
- Menú con botones inline
- Comandos por texto y respuestas formateadas
- Sin riesgo de bloqueo
- Funciona fuera de casa con acceso a internet
- Permite controlar funciones rápidas sin abrir la PWA

### Arquitectura
```text
[ESP32] --MQTT--> [Broker Mosquitto] --MQTT--> [Puente (Telegram Bot)] --Telegram API--> [Chat]
                                                        |                           ^
                                                        |                           | Comandos
                                                        v                           |
                                                      [App PWA] <-------------------
```

### Funcionalidades
| Comando/Botón | Acción |
|---|---|
| `/start` o Menú | Muestra menú con botones |
| `📊 Ver datos` / `/datos` | Resumen de sensores, batería, distancia, modo, estado, patrol |
| `ℹ️ Estado` / `/estado` | online/offline, modo, active, patrol.running, última actualización |
| `🔋 Batería` / `/bateria` | Voltaje + porcentaje |
| `🚨 Alertas` / `/alertas` | Últimas 3-5 alertas guardadas |
| `🤖 Modo AUTO` / `/modo_auto` | Envía `{"type": "mode", "value": "AUTO"}` |
| `🎮 Modo MANUAL` / `/modo_manual` | Envía `{"type": "mode", "value": "MANUAL"}` |
| `▶️ Iniciar recorrido` / `/start_rover` | Envía `{"type": "start", "value": true}` |
| `⏹️ Detener recorrido` / `/stop_rover` | Envía `{"type": "start", "value": false}` |
| `🛑 E-STOP` / `/estop` | Envía `{"type": "estop"}` (máxima prioridad) |

### Alertas automáticas
Formato al recibir desde `rover/rover-01/alerts`:
```text
🚨 ALERTA CRÍTICA - ROVER-01
Tipo: LLAMA DETECTADA / BATERÍA CRÍTICA / OBSTÁCULO BLOQUEADO
Hora: DD/MM/YYYY HH:MM:SS
Detalle: ...
Acción: Revisar zona inmediata
```

### Stack
- **Lenguaje:** Python + `python-telegram-bot` + `paho-mqtt`
- **Almacenamiento:** Última telemetría en memoria + últimas alertas en `alerts.json`
- **Ejecución:** Corriendo en PC/Raspberry Pi 24/7
- **Seguridad:** Whitelist por `chat_id`

## 7. Plan de acción
| Paso | Tarea | Notas |
|---|---|---|
| **1. Organización** | Crear archivo `ROVER_IOT_PLAN.md` | Hecho. |
| **2. MQTT** | Levantar `Mosquitto` localmente y probar con MQTTX | Pendiente - Probar publish/subscribe |
| **3. ESP32** | Publicar `telemetry`, `heartbeat`, `alerts` con JSON. Escuchar `cmd`. Añadir lógica para `start/patrol` y alerta `OBSTACLE_BLOCKED` | Pendiente - Mantener lógica local de seguridad |
| **4. Puente Telegram** | Crear bot con @BotFather, implementar puente MQTT-Telegram con whitelist, menú, comandos y alertas | Pendiente |
| **5. PWA Base** | Crear proyecto con `Vite + React + TypeScript + TailwindCSS + mqtt.js` | Pendiente |
| **6. Dashboard** | Crear tarjetas, indicador online/offline, modo, patrol, botones AUTO/MANUAL/start/stop, joystick, power, E-STOP | Pendiente |
| **7. Alertas** | Mostrar alerta en pantalla + notificaciones push | Pendiente |
| **8. Pruebas reales** | Probar flujo completo: PWA + Telegram + ESP32 | Pendiente |
