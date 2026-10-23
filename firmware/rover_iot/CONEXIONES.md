# Conexiones del Rover IoT (ESP32 + protoboard)

Guía de cableado sensor por sensor, con cada pata y cada resistencia. Basada en la asignación de pines de la Fase 1.

## Antes de empezar

- Riel **superior**: **5 V** (batería/buck).
- Riel **inferior**: **GND común**.
- Riel **lateral**: **3.3 V** del ESP32.
- **Todas las tierras unidas** en un solo riel de GND.
- Las resistencias no tienen polaridad.

> ⚠️ Los GPIO **34, 35 y 32** son **solo entrada** (ADC1). Correcto para MQ-135, llama y batería.
> Nunca conectes 5 V directos a un GPIO del ESP32.

---

## 1. DHT22 — temperatura y humedad

### Si es módulo (3 pines: VCC, DATA, GND)

| Pata | Va a |
|---|---|
| VCC | Riel **3.3 V** |
| DATA | **GPIO4** |
| GND | Riel **GND** |

### Si es sensor suelto (4 pines: VCC, DATA, NC, GND)

| Pata | Va a |
|---|---|
| VCC (1) | Riel **3.3 V** |
| DATA (2) | **GPIO4** |
| NC (3) | Sin conectar |
| GND (4) | Riel **GND** |

**Resistencia 10 kΩ (pull-up, solo sensor suelto):**
- Pata A → mismo punto de **VCC / 3.3 V**
- Pata B → mismo punto de **DATA / GPIO4**

```text
3.3V ──┬─────────── VCC
       │
     [10k]
       │
       └─────────── DATA ─── GPIO4
GND ─────────────── GND
```

---

## 2. MQ-135 — calidad de aire (con divisor)

Necesitas **2 resistencias: 10 kΩ y 20 kΩ**.

### Módulo

| Pata | Va a |
|---|---|
| VCC | Riel **5 V** |
| GND | Riel **GND** |
| DO | Sin conectar |
| AO | Pata A de la resistencia de **10 kΩ** |

### Divisor

1. **10 kΩ**: pata A → **AO del MQ-135**; pata B → punto central.
2. **20 kΩ**: pata A → **punto central**; pata B → riel **GND**.
3. **Punto central** (unión 10 kΩ + 20 kΩ) → **GPIO34**.

```text
MQ-135 AO ──[10k]──┬── GPIO34
                   │
                 [20k]
                   │
                  GND
```

> Deja el sensor encendido **1–3 minutos** antes de leer. La calibración de `GAS_BASELINE` se hace en aire limpio.

---

## 3. Llama IR — detección de fuego

| Pata | Va a |
|---|---|
| VCC | Riel **3.3 V** |
| GND | Riel **GND** |
| DO | **GPIO35** |
| AO | Sin conectar |

**No lleva resistencias.**

```text
3.3V ── VCC
GND  ── GND
DO   ── GPIO35
```

> La mayoría de módulos entregan **LOW al detectar llama**. El firmware usa `FLAME_ACTIVE_LOW 1`.

---

## 4. HC-SR04 — distancia (con divisor en Echo)

Necesitas **2 resistencias: 1 kΩ y 2 kΩ**.

| Pata | Va a |
|---|---|
| VCC | Riel **5 V** |
| GND | Riel **GND** |
| Trig | **GPIO5** (directo) |
| Echo | Pata A de la resistencia de **1 kΩ** |

### Divisor (solo Echo)

1. **1 kΩ**: pata A → **Echo**; pata B → punto central.
2. **2 kΩ**: pata A → **punto central**; pata B → riel **GND**.
3. **Punto central** (unión 1 kΩ + 2 kΩ) → **GPIO18**.

```text
HC-SR04 Echo ──[1k]──┬── GPIO18
                     │
                   [2k]
                     │
                    GND
```

> Trig va directo: el ESP32 entrega 3.3 V y el HC-SR04 lo acepta como HIGH. Echo devuelve 5 V y baja con el divisor.

---

## 5. Divisor de batería — nivel de carga

Necesitas **2 resistencias: 20 kΩ y 10 kΩ**.

1. **20 kΩ**: pata A → **positivo de la batería** (después del interruptor/BMS); pata B → punto central.
2. **10 kΩ**: pata A → **punto central**; pata B → riel **GND**.
3. **Punto central** (unión 20 kΩ + 10 kΩ) → **GPIO32**.
4. **Negativo de la batería** → riel **GND común**.

```text
Batería + ──[20k]──┬── GPIO32
                   │
                 [10k]
                   │
                  GND
Batería − ─────────┴── GND común
```

> 8.4 V quedan en ≈2.8 V, seguros para el ADC. **Nunca** conectes la batería directa a un GPIO.

---

## Orden recomendado de armado

1. Rieles primero: **5 V**, **GND** y **3.3 V**.
2. Unir **GND común**: ESP32, sensores, L298N y batería.
3. **DHT22** (3 cables).
4. **Llama IR** (3 cables).
5. **MQ-135** (VCC/GND + divisor 10k/20k).
6. **HC-SR04** (VCC/GND/Trig + divisor 1k/2k).
7. **Divisor de batería** (20k/10k).

---

## Tabla final de verificación

| Sensor | Pata | Destino ESP32 | Resistencia |
|---|---|---|---|
| DHT22 | VCC | 3.3 V | — |
| DHT22 | DATA | GPIO4 | 10 kΩ a 3.3 V (solo sensor suelto) |
| DHT22 | GND | GND | — |
| MQ-135 | VCC | 5 V | — |
| MQ-135 | GND | GND | — |
| MQ-135 | AO | GPIO34 | 10 kΩ serie + 20 kΩ a GND |
| Llama IR | VCC | 3.3 V | — |
| Llama IR | DO | GPIO35 | — |
| Llama IR | GND | GND | — |
| HC-SR04 | VCC | 5 V | — |
| HC-SR04 | Trig | GPIO5 | — |
| HC-SR04 | Echo | GPIO18 | 1 kΩ serie + 2 kΩ a GND |
| HC-SR04 | GND | GND | — |
| Batería | + | GPIO32 | 20 kΩ serie + 10 kΩ a GND |
| Batería | − | GND | — |

---

## L298N (referencia para cuando conectes motores)

| L298N | ESP32 |
|---|---|
| IN1 | GPIO26 |
| IN2 | GPIO27 |
| IN3 | GPIO14 |
| IN4 | GPIO13 |
| ENA | GPIO25 |
| ENB | GPIO33 |
| GND | GND común |

> Alimenta el L298N desde las 2×18650 en serie (7.4 V). Une su GND con el GND del ESP32.

---

## Comprobaciones antes de energizar

1. Revisa con multímetro que **5 V y 3.3 V no estén en corto con GND**.
2. Verifica que el **punto medio de cada divisor** no toque otro pin.
3. Confirma que el **AO del MQ-135** pasa por el divisor (no directo).
4. Confirma que **Echo del HC-SR04** pasa por el divisor (no directo).
5. Mide el punto medio del divisor de batería: debe dar ≈ `Vbatería / 3`.
6. Enciende primero **sin motores** y revisa lecturas por Serial/MQTT.
