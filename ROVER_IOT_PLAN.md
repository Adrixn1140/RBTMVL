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
| `rover/rover-01/alerts` | ESP32 -> App | Solo alertas críticas (fuego, batería crítica). |
| `rover/rover-01/heartbeat` | ESP32 -> App | Señal para saber que sigue vivo (cada 2-5 segundos). |
| `rover/rover-01/state` | ESP32 -> App | Estado actual (activo/inactivo, AUTO/MANUAL, motores). Ideal al abrir/reconectar la app. |
| `rover/rover-01/cmd` | App -> ESP32 | Todos los comandos unificados en un solo JSON. |

### Comandos para `cmd`
| Comando | Ejemplo JSON | Acción |
|---|---|---|
| Cambiar modo | `{"type": "mode", "value": "AUTO"}` o `"MANUAL"` | Cambia entre autónomo y manual |
| Mover (solo MANUAL) | `{"type": "move", "dir": "FORWARD", "speed": 150}` | Avanzar/retroceder/girar/izquierda/derecha |
| Parada de emergencia | `{"type": "estop"}` | Detener motores inmediatamente |
| Encendido/Apagado lógico | `{"type": "power", "value": true/false}` | Activar o desactivar el rover |
| Actualizar umbrales (futuro) | `{"type": "config", "thresholds": {...}}` | Editar umbrales desde la app |

## 5. Estructura JSON base para Telemetría
```json
{
  "deviceId": "rover-01",
  "timestamp": 1729612345678,
  "online": true,
  "mode": "AUTO",
  "active": true,
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

## 6. Plan de acción
| Paso | Tarea | Notas |
|---|---|---|
| **1. Organización** | Crear archivo `ROVER_IOT_PLAN.md` | Hecho. |
| **2. MQTT** | Levantar `Mosquitto` localmente y probar con MQTTX | Pendiente - Probar publish/subscribe |
| **3. ESP32** | Publicar `telemetry`, `heartbeat`, `alerts` con JSON. Escuchar `cmd` | Pendiente - Mantener lógica local de seguridad |
| **4. PWA Base** | Crear con `Vite + React + TypeScript + TailwindCSS + mqtt.js` | Pendiente |
| **5. Dashboard** | Crear tarjetas con sensores, batería, distancia, modo, online/offline | Pendiente |
| **6. Control** | Añadir toggle AUTO/MANUAL, joystick, power, E-STOP | Pendiente |
| **7. Alertas** | Mostrar alerta en pantalla + notificaciones push | Pendiente |
| **8. Pruebas reales** | Probar flujo completo con rover | Pendiente |
