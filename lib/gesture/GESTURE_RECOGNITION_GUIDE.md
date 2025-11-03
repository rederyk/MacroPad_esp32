# Guida al Sistema Gesture (Swipe + Shake)

## 1. Panoramica

- Il sistema gesture ora supporta **solo tre eventi** condivisi da entrambi i sensori:
  - `G_SWIPE_RIGHT` (ID 201)
  - `G_SWIPE_LEFT`  (ID 202)
  - `G_SHAKE`       (ID 203)
- **MPU6050** utilizza accelerometro + giroscopio per migliorare la direzione dello swipe.
- **ADXL345** usa esclusivamente l'accelerometro.
- Tutti i vecchi riconoscitori di forme/orientamento sono stati rimossi.

## 2. Flusso dei Dati

1. `GestureRead`
   - Campiona i dati grezzi dal sensore (`MotionSensor`).
   - Applica offset di calibrazione e riempie il buffer circolare.

2. `GestureAnalyze`
   - Seleziona il recognizer corretto in base al tipo di sensore (`mpu6050` o `adxl345`).
   - Tutte le modalità (`auto`, `shape`, `orientation`, `simple`, `swipe`) vengono mappate alla pipeline Swipe+Shake.

3. `SimpleGestureDetector`
   - Analizza il buffer di campioni.
   - Calcola:
     - Asse dominante (esclude l'asse della gravità).
     - Range min/max dell'accelerazione (e del gyro, se disponibile).
     - Magnitudo massima.
   - Classifica in swipe o shake e restituisce `GestureRecognitionResult`.

## 3. Soglie Principali

| Parametro                    | Valore default | Note                                               |
|-----------------------------|----------------|----------------------------------------------------|
| `swipeAccelThreshold`       | 0.6 g          | Range minimo per riconoscere swipe                 |
| `shakeBidirectionalMin/Max` | ±0.7 g         | Minimo picco in entrambe le direzioni per shake    |
| `shakeRangeThreshold`       | 1.8 g          | Range minimo complessivo per classificare shake    |

Le soglie sono condivise da entrambi i sensori; per il MPU6050 il giroscopio aiuta a disambiguare la direzione dello swipe.

## 4. Log Diagnostici

Ogni riconoscimento produce log simili:

- `MPU6050: Gravity on X (gyro=1)`
- `MPU6050: accelRange X=... Y=... Z=...`
- `MPU6050: gyroRange X=... Y=... Z=... maxMag=...`
- `MPU6050: G_SWIPE_RIGHT detected ... (conf: 0.85)`

Per ADXL345 i log sono equivalenti ma senza i dati del gyro.  
`GestureDevice` riporta sempre la modalità `"Swipe+Shake (Accel+Gyro)"` oppure `"Swipe+Shake (Accel only)"`.

## 5. Confidenza e Filtri

- La confidenza restituita varia da **0.5 a 1.0**.
- `GestureAnalyze` applica la soglia globale (`_confidenceThreshold`, default 0.5).
- In caso di confidenza bassa, il recognizer scarta direttamente la gesture e logga il motivo.

## 6. Consigli di Utilizzo

- **Calibrazione**: eseguire la calibrazione da menu con il dispositivo nella posizione d'uso.
- **Swipe**: movimenti rapidi e direzionali su assi non dominati dalla gravità.
- **Shake**: scuotere con forza in entrambe le direzioni sullo stesso asse.
- **Rumore / drift**: se l'ADXL345 è troppo rumoroso, ridurre la sensibilità via configurazione del sensore (`motionWakeThreshold`, `sampleRate`).

## 7. Estensioni Future

Il nuovo design separa chiaramente lo strato di cattura (`GestureRead`), l'analisi (`SimpleGestureDetector`) e le soglie.  
Per aggiungere gesture supplementari basterà estendere il detector o introdurre un nuovo recognizer specializzato.
