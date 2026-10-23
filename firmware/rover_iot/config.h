#ifndef CONFIG_H
#define CONFIG_H

// ===================== WiFi =====================
#define WIFI_SSID "TU_WIFI"
#define WIFI_PASSWORD "TU_PASSWORD"

// ===================== MQTT =====================
#define MQTT_HOST "192.168.1.100"   // IP del equipo donde corre Mosquitto
#define MQTT_PORT 1883
#define MQTT_USER ""                // dejar vacío si el broker no pide auth
#define MQTT_PASSWORD ""

#define ROVER_ID "rover-01"

// Prefijo de tópicos: rover/<ROVER_ID>/...
#define TOPIC_TELEMETRY "telemetry"
#define TOPIC_STATE "state"
#define TOPIC_ALERTS "alerts"
#define TOPIC_HEARTBEAT "heartbeat"
#define TOPIC_CMD "cmd"

// Publicar también los tópicos planos de la Fase 1 (rover/temp, rover/hum, ...)
#define PUBLISH_LEGACY_TOPICS 1

// ===================== Pines =====================
// Sensores
#define PIN_DHT 4
#define PIN_MQ135 34
#define PIN_FLAME 35
#define PIN_BATTERY 32

// HC-SR04
#define PIN_TRIG 5
#define PIN_ECHO 18

// Puente H L298N
#define PIN_IN1 26
#define PIN_IN2 27
#define PIN_IN3 14
#define PIN_IN4 13
#define PIN_ENA 25
#define PIN_ENB 33

// Canales PWM (ESP32 core 3.x usa ledcAttach)
#define PWM_FREQ 1000
#define PWM_RES 8

// ===================== Calibración =====================
// Divisor de batería: 20k (arriba) / 10k (abajo) -> factor 3.0
#define BATTERY_DIVIDER 3.0f
// Ajusta con multímetro: voltaje real / voltaje calculado
#define BATTERY_CAL 1.00f
// 2S 18650: 6.0 V = 0 %, 8.4 V = 100 %
#define BATTERY_MIN_V 6.0f
#define BATTERY_MAX_V 8.4f

// Divisor AO del MQ-135: 10k / 20k -> factor 1.5 (solo informativo)
#define MQ135_DIVIDER 1.5f
// Lectura considerada "aire limpio". Calibrar en Fase 2.
#define GAS_BASELINE 1200
// Alerta si la lectura supera baseline * este factor
#define GAS_ALERT_FACTOR 1.30f

// Sensor de llama: la mayoría de módulos entregan LOW al detectar
#define FLAME_ACTIVE_LOW 1

// ===================== Umbrales =====================
#define TEMP_ALERT_C 40.0f
#define TEMP_CLEAR_C 38.0f
#define BATTERY_ALERT_PERCENT 20
#define BATTERY_CLEAR_PERCENT 25

// Navegación
#define OBSTACLE_DISTANCE_CM 20
#define OBSTACLE_CLEAR_CM 30
#define AUTO_SPEED 170
#define TURN_SPEED 160
#define MANUAL_MAX_SPEED 220
#define MANUAL_TIMEOUT_MS 1200   // sin comandos -> se detiene
#define BACKUP_MS 500
#define TURN_MS 600
#define NAV_INTERVAL_MS 60

// Si no logra avanzar durante este tiempo -> OBSTACLE_BLOCKED
#define BLOCKED_TIMEOUT_MS 10000

// ===================== Frecuencias =====================
#define TELEMETRY_INTERVAL_MS 10000
#define HEARTBEAT_INTERVAL_MS 3000
#define SENSOR_INTERVAL_MS 10000
#define ALERT_REPEAT_MS 30000
#define MQTT_RETRY_MS 5000
#define WIFI_RETRY_MS 5000

// ===================== MQTT buffer =====================
#define MQTT_BUFFER_SIZE 1536

#endif
