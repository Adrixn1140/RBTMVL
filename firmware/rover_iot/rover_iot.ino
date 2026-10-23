/*
 * Rover IoT - Fase 1/2
 * ESP32 + 5 sensores + L298N + MQTT
 *
 * Modos: AUTO (patrullaje con evasión) y MANUAL (joystick desde app/Telegram).
 * La seguridad (evasión, E-STOP) se resuelve 100% en local.
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <math.h>

#include "config.h"

// ===================== Estado global =====================
enum RoverMode { MODE_AUTO, MODE_MANUAL };
enum NavState { NAV_FORWARD, NAV_BACKUP, NAV_TURN };

RoverMode currentMode = MODE_AUTO;
NavState navState = NAV_FORWARD;
bool powerOn = true;
bool estopLatched = false;
bool patrolRunning = false;

unsigned long navStateSince = 0;
unsigned long blockedSince = 0;
unsigned long lastCmdMs = 0;
bool turnLeftNext = true;

// Sensores
float temperature = NAN;
float humidity = NAN;
int gasRaw = 0;
float gasVolts = 0;
bool flameDetected = false;
float batteryVoltage = 0;
int batteryPercent = 0;
float distanceCm = -1;

// Alertas (histéresis + anti-spam)
bool tempAlert = false;
bool gasAlert = false;
bool batteryAlert = false;
bool flameAlert = false;
bool blockedAlert = false;
unsigned long lastTempAlertMs = 0;
unsigned long lastGasAlertMs = 0;
unsigned long lastBatteryAlertMs = 0;
unsigned long lastFlameAlertMs = 0;

// Tiempos
unsigned long lastTelemetryMs = 0;
unsigned long lastHeartbeatMs = 0;
unsigned long lastSensorMs = 0;
unsigned long lastNavMs = 0;
unsigned long lastMqttAttemptMs = 0;
unsigned long lastWifiAttemptMs = 0;

// ===================== Clientes =====================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
DHT dht(PIN_DHT, DHT22);

String topicTelemetry;
String topicState;
String topicAlerts;
String topicHeartbeat;
String topicCmd;

// ===================== Utilidades =====================
void setMotor(int in1, int in2, int en, int speed) {
  speed = constrain(speed, 0, 255);
  digitalWrite(in1, speed > 0 ? HIGH : LOW);
  digitalWrite(in2, LOW);
  if (en >= 0) ledcWrite(en, speed);
}

void motorsStop() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENA, 0);
  ledcWrite(PIN_ENB, 0);
}

void motorsForward(int speed) {
  // Motor A
  digitalWrite(PIN_IN1, HIGH);
  digitalWrite(PIN_IN2, LOW);
  ledcWrite(PIN_ENA, constrain(speed, 0, 255));
  // Motor B
  digitalWrite(PIN_IN3, HIGH);
  digitalWrite(PIN_IN4, LOW);
  ledcWrite(PIN_ENB, constrain(speed, 0, 255));
}

void motorsBackward(int speed) {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, HIGH);
  ledcWrite(PIN_ENA, constrain(speed, 0, 255));
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, HIGH);
  ledcWrite(PIN_ENB, constrain(speed, 0, 255));
}

void motorsTurn(bool left, int speed) {
  speed = constrain(speed, 0, 255);
  if (left) {
    // izquierda: rueda A atrás, rueda B adelante
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_IN3, HIGH);
    digitalWrite(PIN_IN4, LOW);
  } else {
    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_IN3, LOW);
    digitalWrite(PIN_IN4, HIGH);
  }
  ledcWrite(PIN_ENA, speed);
  ledcWrite(PIN_ENB, speed);
}

float readDistanceCm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  unsigned long duration = pulseIn(PIN_ECHO, HIGH, 30000UL);
  if (duration == 0) return -1;  // sin eco
  float cm = duration * 0.0343f / 2.0f;
  if (cm <= 0 || cm > 400) return -1;
  return cm;
}

float readBatteryVoltage() {
  // analogReadMilliVolts aplica calibración del ADC
  uint32_t mv = analogReadMilliVolts(PIN_BATTERY);
  float pinVolts = mv / 1000.0f;
  return pinVolts * BATTERY_DIVIDER * BATTERY_CAL;
}

int batteryPercentFromVoltage(float v) {
  if (v <= BATTERY_MIN_V) return 0;
  if (v >= BATTERY_MAX_V) return 100;
  return (int)roundf((v - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V) * 100.0f);
}

bool flameIsActive() {
  int v = digitalRead(PIN_FLAME);
  return FLAME_ACTIVE_LOW ? (v == LOW) : (v == HIGH);
}

// ===================== MQTT =====================
void publishState() {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["deviceId"] = ROVER_ID;
  doc["timestamp"] = (long)(millis() / 1000);
  doc["online"] = true;
  doc["mode"] = (currentMode == MODE_AUTO) ? "AUTO" : "MANUAL";
  doc["active"] = powerOn;
  doc["patrol"]["running"] = patrolRunning;
  doc["estop"] = estopLatched;

  char buf[256];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(topicState.c_str(), (const uint8_t *)buf, n, true);
}

void publishHeartbeat() {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["deviceId"] = ROVER_ID;
  doc["timestamp"] = (long)(millis() / 1000);
  doc["uptime"] = (long)(millis() / 1000);
  doc["rssi"] = WiFi.RSSI();
  char buf[128];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(topicHeartbeat.c_str(), (const uint8_t *)buf, n, false);
}

void publishAlert(const char *type, const char *message) {
  if (!mqtt.connected()) return;
  JsonDocument doc;
  doc["deviceId"] = ROVER_ID;
  doc["timestamp"] = (long)(millis() / 1000);
  doc["type"] = type;
  doc["message"] = message;
  char buf[256];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(topicAlerts.c_str(), (const uint8_t *)buf, n, false);
}

void publishTelemetry() {
  if (!mqtt.connected()) return;

  JsonDocument doc;
  doc["deviceId"] = ROVER_ID;
  doc["timestamp"] = (long)(millis() / 1000);
  doc["online"] = true;
  doc["mode"] = (currentMode == MODE_AUTO) ? "AUTO" : "MANUAL";
  doc["active"] = powerOn;
  doc["patrol"]["running"] = patrolRunning;
  doc["estop"] = estopLatched;

  JsonObject s = doc["sensors"].to<JsonObject>();
  JsonObject t = s["temperature"].to<JsonObject>();
  t["value"] = isnan(temperature) ? 0 : temperature;
  t["unit"] = "C";
  t["valid"] = !isnan(temperature);

  JsonObject h = s["humidity"].to<JsonObject>();
  h["value"] = isnan(humidity) ? 0 : humidity;
  h["unit"] = "%";
  h["valid"] = !isnan(humidity);

  JsonObject g = s["airQuality"].to<JsonObject>();
  g["raw"] = gasRaw;
  g["voltage"] = gasVolts;
  g["valid"] = true;

  JsonObject f = s["flame"].to<JsonObject>();
  f["detected"] = flameDetected;
  f["raw"] = digitalRead(PIN_FLAME);
  f["valid"] = true;

  JsonObject b = s["battery"].to<JsonObject>();
  b["voltage"] = batteryVoltage;
  b["percent"] = batteryPercent;
  b["valid"] = batteryVoltage > 1.0f;

  JsonObject n = doc["navigation"].to<JsonObject>();
  JsonObject d = n["distanceFront"].to<JsonObject>();
  d["value"] = distanceCm;
  d["unit"] = "cm";
  d["valid"] = distanceCm > 0;
  n["action"] = (navState == NAV_FORWARD) ? "FORWARD"
                : (navState == NAV_BACKUP) ? "BACKUP" : "TURN";

  doc["network"]["rssi"] = WiFi.RSSI();

  char buf[MQTT_BUFFER_SIZE];
  size_t len = serializeJson(doc, buf, sizeof(buf));
  mqtt.publish(topicTelemetry.c_str(), (const uint8_t *)buf, len, false);

#if PUBLISH_LEGACY_TOPICS
  // Tópicos planos de la Fase 1
  char tmp[24];
  dtostrf(temperature, 0, 2, tmp);
  mqtt.publish("rover/temp", tmp);
  dtostrf(humidity, 0, 2, tmp);
  mqtt.publish("rover/hum", tmp);
  snprintf(tmp, sizeof(tmp), "%d", gasRaw);
  mqtt.publish("rover/gas", tmp);
  mqtt.publish("rover/flama", flameDetected ? "1" : "0");
  snprintf(tmp, sizeof(tmp), "%d", batteryPercent);
  mqtt.publish("rover/bateria", tmp);
#endif
}

// ===================== Lógica de sensores =====================
void readSensors() {
  // HC-SR04 puede leerse con más frecuencia desde la navegación
  if (millis() - lastSensorMs < SENSOR_INTERVAL_MS) return;
  lastSensorMs = millis();

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  gasRaw = analogRead(PIN_MQ135);
  gasVolts = (analogReadMilliVolts(PIN_MQ135) / 1000.0f) * MQ135_DIVIDER;

  flameDetected = flameIsActive();

  batteryVoltage = readBatteryVoltage();
  batteryPercent = batteryPercentFromVoltage(batteryVoltage);
}

void checkAlerts() {
  unsigned long now = millis();

  // Temperatura con histéresis
  if (!isnan(temperature)) {
    bool over = temperature > TEMP_ALERT_C;
    bool clear = temperature < TEMP_CLEAR_C;
    if (over && !tempAlert && now - lastTempAlertMs > ALERT_REPEAT_MS) {
      tempAlert = true;
      lastTempAlertMs = now;
      char msg[64];
      snprintf(msg, sizeof(msg), "Temperatura %.1f C supera %.0f C", temperature, TEMP_ALERT_C);
      publishAlert("TEMP_ALTA", msg);
    } else if (clear) {
      tempAlert = false;
    }
  }

  // Calidad de aire
  bool gasOver = gasRaw > (int)(GAS_BASELINE * GAS_ALERT_FACTOR);
  if (gasOver && !gasAlert && now - lastGasAlertMs > ALERT_REPEAT_MS) {
    gasAlert = true;
    lastGasAlertMs = now;
    char msg[64];
    snprintf(msg, sizeof(msg), "Calidad de aire %d sobre baseline %d", gasRaw, GAS_BASELINE);
    publishAlert("GAS_ALTO", msg);
  } else if (!gasOver) {
    gasAlert = false;
  }

  // Llama: alerta inmediata
  if (flameDetected && !flameAlert) {
    flameAlert = true;
    lastFlameAlertMs = now;
    publishAlert("LLAMA_DETECTADA", "Sensor IR detectó llama");
  } else if (!flameDetected) {
    flameAlert = false;
  }

  // Batería
  if (batteryPercent <= BATTERY_ALERT_PERCENT && !batteryAlert && now - lastBatteryAlertMs > ALERT_REPEAT_MS) {
    batteryAlert = true;
    lastBatteryAlertMs = now;
    char msg[64];
    snprintf(msg, sizeof(msg), "Batería al %d%% (%.2f V)", batteryPercent, batteryVoltage);
    publishAlert("BATERIA_BAJA", msg);
  } else if (batteryPercent >= BATTERY_CLEAR_PERCENT) {
    batteryAlert = false;
  }
}

// ===================== Navegación =====================
void enterNavState(NavState st) {
  navState = st;
  navStateSince = millis();
}

void stopRover() {
  motorsStop();
  blockedSince = 0;
  blockedAlert = false;
}

void runAutonomous() {
  unsigned long now = millis();
  if (now - lastNavMs < NAV_INTERVAL_MS) return;
  lastNavMs = now;

  distanceCm = readDistanceCm();

  switch (navState) {
    case NAV_FORWARD:
      motorsForward(AUTO_SPEED);
      if (distanceCm > 0 && distanceCm < OBSTACLE_DISTANCE_CM) {
        motorsStop();
        if (blockedSince == 0) blockedSince = now;
        enterNavState(NAV_BACKUP);
      } else if (distanceCm > OBSTACLE_CLEAR_CM) {
        // avanza con espacio: reinicia el conteo de bloqueo
        blockedSince = 0;
        blockedAlert = false;
      }
      break;

    case NAV_BACKUP:
      motorsBackward(AUTO_SPEED);
      if (now - navStateSince >= BACKUP_MS) {
        motorsStop();
        turnLeftNext = !turnLeftNext;
        enterNavState(NAV_TURN);
      }
      break;

    case NAV_TURN:
      motorsTurn(turnLeftNext, TURN_SPEED);
      if (now - navStateSince >= TURN_MS) {
        enterNavState(NAV_FORWARD);
      }
      break;
  }

  // Obstáculo que impide avanzar
  if (blockedSince > 0 && now - blockedSince >= BLOCKED_TIMEOUT_MS && !blockedAlert) {
    blockedAlert = true;
    patrolRunning = false;
    stopRover();
    publishAlert("OBSTACLE_BLOCKED", "Obstáculo grande: no puede seguir avanzando");
    publishState();
  }
}

void runManual() {
  if (millis() - lastCmdMs > MANUAL_TIMEOUT_MS) {
    motorsStop();
  }
}

// ===================== Comandos =====================
void applyMove(const char *dir, int speed) {
  if (currentMode != MODE_MANUAL || !powerOn || estopLatched) return;
  speed = constrain(speed, 0, MANUAL_MAX_SPEED);

  if (strcmp(dir, "FORWARD") == 0) motorsForward(speed);
  else if (strcmp(dir, "BACKWARD") == 0) motorsBackward(speed);
  else if (strcmp(dir, "LEFT") == 0) motorsTurn(true, speed);
  else if (strcmp(dir, "RIGHT") == 0) motorsTurn(false, speed);
  else motorsStop();

  lastCmdMs = millis();
}

void handleCommand(const char *payload) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) return;

  const char *type = doc["type"] | "";

  if (strcmp(type, "estop") == 0) {
    bool value = doc["value"] | true;
    if (value) {
      estopLatched = true;
      patrolRunning = false;
      stopRover();
      publishAlert("E_STOP", "Parada de emergencia activada");
    } else {
      estopLatched = false;
    }
    publishState();
    return;
  }

  if (estopLatched) {
    publishAlert("E_STOP_ACTIVO", "Comando ignorado: E-STOP activo");
    return;
  }

  if (strcmp(type, "mode") == 0) {
    const char *mode = doc["value"] | "AUTO";
    currentMode = (strcmp(mode, "MANUAL") == 0) ? MODE_MANUAL : MODE_AUTO;
    patrolRunning = false;
    stopRover();
    publishState();
    return;
  }

  if (strcmp(type, "power") == 0) {
    powerOn = doc["value"] | true;
    if (!powerOn) {
      patrolRunning = false;
      stopRover();
    }
    publishState();
    return;
  }

  if (strcmp(type, "start") == 0) {
    bool start = doc["value"] | true;
    if (start) {
      if (currentMode != MODE_AUTO) {
        publishAlert("START_RECHAZADO", "El recorrido solo inicia en modo AUTO");
        return;
      }
      if (!powerOn) {
        publishAlert("START_RECHAZADO", "Rover apagado");
        return;
      }
      patrolRunning = true;
      blockedSince = 0;
      blockedAlert = false;
      enterNavState(NAV_FORWARD);
    } else {
      patrolRunning = false;
      stopRover();
    }
    publishState();
    return;
  }

  if (strcmp(type, "move") == 0) {
    const char *dir = doc["dir"] | "STOP";
    int speed = doc["speed"] | AUTO_SPEED;
    applyMove(dir, speed);
    return;
  }

  if (strcmp(type, "config") == 0) {
    // Reservado para umbrales configurables desde la app
    return;
  }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  if (length == 0 || length > 1024) return;

  char buf[1025];
  memcpy(buf, payload, length);
  buf[length] = '\0';

  if (strcmp(topic, topicCmd.c_str()) == 0) {
    handleCommand(buf);
  }
}

// ===================== Conectividad =====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiAttemptMs < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = millis();

  Serial.print("Conectando WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return;
  if (millis() - lastMqttAttemptMs < MQTT_RETRY_MS) return;
  lastMqttAttemptMs = millis();

  JsonDocument doc;
  doc["deviceId"] = ROVER_ID;
  doc["online"] = false;
  char lwt[96];
  size_t n = serializeJson(doc, lwt, sizeof(lwt));

  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = mqtt.connect(ROVER_ID, MQTT_USER, MQTT_PASSWORD,
                      topicState.c_str(), 0, true, lwt);
  } else {
    ok = mqtt.connect(ROVER_ID, nullptr, nullptr,
                      topicState.c_str(), 0, true, lwt);
  }

  if (ok) {
    Serial.println("MQTT conectado");
    mqtt.subscribe(topicCmd.c_str());
    publishState();
  } else {
    Serial.printf("MQTT fallo rc=%d\n", mqtt.state());
  }
}

// ===================== Setup =====================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_FLAME, INPUT);

  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);

  ledcAttach(PIN_ENA, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_ENB, PWM_FREQ, PWM_RES);
  motorsStop();

  analogReadResolution(12);
  dht.begin();

  topicTelemetry = String("rover/") + ROVER_ID + "/" + TOPIC_TELEMETRY;
  topicState     = String("rover/") + ROVER_ID + "/" + TOPIC_STATE;
  topicAlerts    = String("rover/") + ROVER_ID + "/" + TOPIC_ALERTS;
  topicHeartbeat = String("rover/") + ROVER_ID + "/" + TOPIC_HEARTBEAT;
  topicCmd       = String("rover/") + ROVER_ID + "/" + TOPIC_CMD;

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);

  connectWiFi();
  Serial.println("Rover IoT listo");
}

// ===================== Loop =====================
void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    motorsStop();
  } else {
    if (!mqtt.connected()) connectMqtt();
    mqtt.loop();
  }

  // Sensores y alertas
  readSensors();
  checkAlerts();

  // Navegación / control
  if (powerOn && !estopLatched) {
    if (currentMode == MODE_AUTO && patrolRunning) {
      runAutonomous();
    } else if (currentMode == MODE_MANUAL) {
      runManual();
    } else {
      motorsStop();
    }
  } else {
    motorsStop();
  }

  // Sin conexión en modo manual -> detener (seguridad)
  if (currentMode == MODE_MANUAL && !mqtt.connected()) {
    motorsStop();
  }

  // Telemetría
  if (now - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = now;
    publishTelemetry();
  }

  if (now - lastHeartbeatMs >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatMs = now;
    publishHeartbeat();
  }
}
