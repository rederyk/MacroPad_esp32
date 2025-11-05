# Gesture System Overview

## System Architecture

Il sistema di gesture è composto da diversi layer che collaborano per catturare, analizzare e riconoscere i movimenti del dispositivo.

### Layer 1: Hardware Abstraction

#### **MotionSensor** ([MotionSensor.h](lib/gesture/MotionSensor.h))
- Astrae l'interfaccia con i sensori hardware (MPU6050, ADXL345)
- Fornisce letture raw di accelerometro e giroscopio
- Gestisce la comunicazione I2C con i sensori

#### **GestureRead** ([gestureRead.h](lib/gesture/gestureRead.h:20-144))
- **Responsabilità principale**: acquisizione continua dei dati del sensore
- Implementa un task FreeRTOS dedicato per il campionamento in background
- Gestisce il buffer circolare dei campioni (`SampleBuffer`)
- Fornisce calibrazione degli assi e mapping
- Supporta power management (standby, wakeup, motion wake)
- Implementa streaming mode per applicazioni real-time (es. GyroMouse)

**Strutture dati chiave**:
```cpp
struct Sample {
    float x, y, z;              // Accelerometro (g)
    float gyroX, gyroY, gyroZ;  // Giroscopio (rad/s)
    float temperature;
    bool gyroValid;
    bool temperatureValid;
};

struct SampleBuffer {
    Sample *samples;         // Array dinamico
    uint16_t sampleCount;    // Campioni attuali
    uint16_t maxSamples;     // Capacità buffer
    uint16_t sampleHZ;       // Frequenza di campionamento
};
```

**Metodi principali**:
- `startSampling()` / `stopSampling()`: controllo acquisizione
- `ensureMinimumSamplingTime()`: garantisce tempo minimo di cattura
- `getCollectedSamples()`: accesso thread-safe al buffer
- `flushSensorBuffer()`: svuota buffer hardware per scartare dati obsoleti

### Layer 2: Analysis & Recognition

#### **GestureAnalyze** ([gestureAnalyze.h](lib/gesture/gestureAnalyze.h:6-36))
- **Responsabilità**: coordinamento del processo di riconoscimento
- Seleziona automaticamente la configurazione in base al tipo di sensore
- Applica threshold di confidenza configurabili
- Delega il riconoscimento effettivo a `SimpleGestureDetector`

**Configurazione sensori**:
```cpp
// MPU6050 (con giroscopio)
config.useGyro = true;
config.gyroSwipeThreshold = 100.0f;   // deg/s
config.gyroShakeThreshold = 150.0f;   // deg/s
config.accelSwipeThreshold = 0.6f;    // g (fallback)
config.accelShakeThreshold = 1.2f;    // g (fallback)

// ADXL345 (solo accelerometro)
config.useGyro = false;
config.accelSwipeThreshold = 0.6f;    // g
config.accelShakeThreshold = 1.2f;    // g
```

#### **SimpleGestureDetector** ([SimpleGestureDetector.cpp](lib/gesture/SimpleGestureDetector.cpp:1-353))
- **Algoritmo di riconoscimento**: analisi statistica del movimento
- Supporta due modalità: gyro-based (MPU6050) e accel-based (ADXL345)
- Implementa detection robusta con filtraggio del rumore

**Algoritmo di detection**:

1. **Gyro-based (MPU6050)** - linee [236-305](lib/gesture/SimpleGestureDetector.cpp:236-305):
   - Converte dati giroscopio in deg/s
   - Analizza l'asse con maggior velocità angolare
   - Conta i cambi di direzione (zero crossings)
   - **SHAKE**: movimento bidirezionale con ≥3 cambi direzione + peak ≥150 deg/s
   - **SWIPE**: movimento unidirezionale (<3 cambi) + peak ≥100 deg/s
   - Calcola "jerk" (derivata dell'accelerazione) per migliorare accuracy
   - Confidence: combinazione di peak velocity (70%) e jerk (30%)

2. **Accel-based (ADXL345)** - linee [307-349](lib/gesture/SimpleGestureDetector.cpp:307-349):
   - Analizza range di movimento su ciascun asse
   - Identifica attraversamenti dello zero (crossedZero)
   - **SHAKE**: crossedZero=true + range ≥1.2g
   - **SWIPE**: crossedZero=false + range ≥0.6g
   - Confidence: basata sul rapporto range/threshold

**Funzioni di analisi**:
- `analyzeAccelAxis()`: trova asse dominante e statistiche movimento
- `analyzeGyroAxis()`: identifica asse con maggior velocità angolare
- `countDirectionChanges()`: conta inversioni di direzione (con soglia rumore 30 deg/s)
- `isSampleValid()`: filtra campioni invalidi (NaN, infiniti)
- `accelMagnitude()`: calcola magnitudine vettore accelerazione

### Layer 3: Device Integration

#### **GestureDevice** ([GestureDevice.cpp](lib/gesture/GestureDevice.cpp:1-194))
- **Responsabilità**: integrazione con InputHub come InputDevice standard
- Implementa state machine a 3 stati: Idle → Capturing → PendingRecognition
- Converte gesture riconosciute in eventi InputEvent
- Supporta disabilitazione temporanea del riconoscimento

**State Machine**:
```
Idle ──startCapture()──> Capturing ──stopCapture()──> PendingRecognition ──recognize()──> Idle
                                                              │
                                                    (se disabled o failed)
                                                              ↓
                                                            Idle
```

**InputEvent generato**:
```cpp
pendingEvent.type = InputEvent::EventType::MOTION;
pendingEvent.value1 = gestureID;      // 201, 202, o 203
pendingEvent.value2 = sampleCount;    // Numero campioni usati
pendingEvent.state = true;            // Gesture riconosciuta
pendingEvent.text = gestureName;      // "G_SWIPE_LEFT", etc.
```

## Gesture Supportate

| Gesture ID | Nome            | Trigger Conditions                                    | Confidence Formula                    |
|------------|-----------------|-------------------------------------------------------|---------------------------------------|
| 201        | `G_SWIPE_RIGHT` | **MPU6050**: dirChanges<3, gyro peak≥100°/s, dir>0<br>**ADXL345**: !crossedZero, range≥0.6g, dir>0 | MPU: (peak×0.7 + jerk×0.3) / (thresh×2)<br>ADXL: range / (thresh×2) |
| 202        | `G_SWIPE_LEFT`  | **MPU6050**: dirChanges<3, gyro peak≥100°/s, dir<0<br>**ADXL345**: !crossedZero, range≥0.6g, dir<0 | MPU: (peak×0.7 + jerk×0.3) / (thresh×2)<br>ADXL: range / (thresh×2) |
| 203        | `G_SHAKE`       | **MPU6050**: dirChanges≥3, gyro peak≥150°/s<br>**ADXL345**: crossedZero, range≥1.2g | MPU: peak / (thresh×2)<br>ADXL: range / (thresh×2) |

**Note**:
- MPU6050 privilegia dati giroscopio per maggior precisione e sensibilità
- ADXL345 usa solo accelerometro (assenza giroscopio)
- Threshold configurabili via `GestureAnalyze::setConfidenceThreshold()` (default 0.5)
- Tutti i valori gyro sono convertiti in deg/s (moltiplicatore: 57.2957795)

## Pipeline di Esecuzione

```
1. User trigger (es: pulsante premuto)
        ↓
2. GestureDevice::startCapture()
        ↓
3. GestureRead::startSampling() → crea task FreeRTOS
        ↓
4. Campionamento continuo in background (200-400Hz)
        ↓ (accumula in SampleBuffer thread-safe)
5. User rilascia trigger
        ↓
6. GestureDevice::stopCapture()
        ↓
7. GestureRead::ensureMinimumSamplingTime() + stopSampling()
        ↓
8. State → PendingRecognition
        ↓
9. GestureAnalyze::recognize()
        ↓
10. SimpleGestureDetector::detectSimpleGesture()
        ↓ (analisi statistica + algoritmo decisionale)
11. GestureRecognitionResult (ID, confidence, name)
        ↓
12. Se confidence ≥ threshold → genera InputEvent
        ↓
13. InputHub distribuisce evento ai consumer
        ↓
14. flushSensorBuffer() + ritorno a Idle
```

## Logging e Diagnostica

Il sistema fornisce log dettagliati per debugging:

**MPU6050**:
```
MPU6050: analyzing 120 samples
MPU6050: gyro Y peak=234.5 deg/s [18.2 to 216.3] dirChanges=1 shake=NO
MPU6050: G_SWIPE_RIGHT detected (gyro peak=234.5 deg/s, jerk=89.3, dir=+, conf=0.87)
```

**ADXL345**:
```
ADXL345: analyzing 95 samples
ADXL345: accel range X=0.85g crossed_zero=NO
ADXL345: G_SWIPE_LEFT detected (accel range=0.85g, dir=-, conf=0.71)
```

**Filtri rumore**:
- Campioni < 0.05g magnitudine: scartati
- Velocità angolare < 30 deg/s: ignorata nel conteggio direzioni
- Campioni non-finiti (NaN, inf): filtrati

## Configurazione

La configurazione è gestita via `AccelerometerConfig` ([configTypes.h](lib/configManager/configTypes.h:66-84)):

```cpp
struct AccelerometerConfig {
    String type;                  // "MPU6050" o "ADXL345"
    uint8_t address;              // Indirizzo I2C
    byte sdaPin, sclPin;          // Pin hardware
    int sampleRate;               // Frequenza campionamento (Hz)
    float sensitivity;            // Sensibilità generale
    String axisMap;               // Mapping assi (es: "XYZ")
    String axisDir;               // Direzioni assi (es: "+++")
    int threshold;                // Threshold movimento
    bool active;                  // Abilitazione sensore
    String gestureMode;           // "auto", "mpu6050", "adxl345"

    // Motion Wake (low power)
    bool motionWakeEnabled;
    uint8_t motionWakeThreshold;
    uint8_t motionWakeDuration;
    uint8_t motionWakeHighPass;
    uint8_t motionWakeCycleRate;
};
```

## Power Management

GestureRead supporta gestione avanzata del consumo:

- **Low Power Mode**: riduce frequenza aggiornamenti
- **Standby Mode**: spegne sensore mantenendo configurazione
- **Motion Wake**: interrupt hardware per wakeup da movimento
  - Configurabile: threshold, duration, filtro high-pass, cycle rate
  - Permette sleep profondo con risveglio automatico

## Integrazione con altri sistemi

### GyroMouse
- Usa `GestureRead` in streaming mode (`setStreamingMode(true)`)
- Accede direttamente ai dati mappati via `getMappedGyro()`, `getMappedX/Y/Z()`
- Non usa il buffer di gesture (bypassa SimpleGestureDetector)

### InputHub
- `GestureDevice` implementa interfaccia `InputDevice`
- Eventi gesture trattati come input standard (al pari di keypad, encoder)
- Possibilità di mappare gesture a macro, combinazioni, azioni speciali

## Differenze Legacy

**Rimosso**:
- Recognizer MPU6050/ADXL345 specifici (file vuoti)
- Algoritmi shape/orientation recognition
- Filtri complessi (Madgwick, complementary filter)
- KNN-based recognition
- Gesture training/learning

**Semplificazione**:
- Singolo detector unificato (`SimpleGestureDetector`)
- Solo 3 gesture (swipe left/right, shake)
- Algoritmo statistico deterministico (no ML)
- Configurazione automatica basata su tipo sensore
