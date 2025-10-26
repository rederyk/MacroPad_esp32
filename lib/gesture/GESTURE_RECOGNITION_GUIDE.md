# Gesture Recognition System - User Guide

## Overview

Il sistema di riconoscimento gesture del MacroPad ESP32 supporta ora **tre modalità** di riconoscimento:

1. **Legacy KNN** - Metodo originale basato su features statistiche
2. **Shape Recognition** - Riconosce forme disegnate in aria (cerchi, linee, triangoli, etc.)
3. **Orientation Recognition** - Riconosce rotazioni e inclinazioni (richiede MPU6050)

## Modalità di Riconoscimento

### 1. Legacy KNN Mode (Compatibilità)
Usa il metodo originale K-Nearest Neighbors con features statistiche dall'accelerometro.

**Pro:**
- Compatibile con codice esistente
- Funziona con qualsiasi sensore

**Contro:**
- Precisione limitata
- Richiede training preventivo

### 2. Shape Recognition Mode
Riconosce forme geometriche disegnate muovendo il device in aria.

**Forme riconosciute:**
- ⭕ **Circle** - Cerchio (orario o antiorario)
- ➖ **Line** - Linea dritta (4 direzioni)
- 🔺 **Triangle** - Triangolo (3 angoli sharp)
- ⬜ **Square** - Quadrato/Rettangolo (4 angoli)
- ⚡ **Zigzag** - Pattern a zigzag
- ∞ **Infinity** - Simbolo infinito / figura-8

**Requisiti:**
- Almeno 1-2 secondi di movimento
- Movimento chiaro e definito
- Evitare drift: completare gesture in <3 secondi

**Pro:**
- Non richiede training
- Intuitivo per l'utente
- Riconosce simboli complessi

**Contro:**
- Drift dopo 2-3 secondi
- Richiede movimento nello spazio

### 3. Orientation Recognition Mode
Riconosce rotazioni, inclinazioni e shake basati sul giroscopio.

**Gesture riconosciute:**
- 🔄 **Rotate 90° CW/CCW** - Rotazione 90° oraria/antioraria
- 🔄 **Rotate 180°** - Rotazione 180°
- 🌀 **Spin** - Rotazione continua
- 📐 **Tilt Forward/Backward** - Inclinazione avanti/indietro
- 📐 **Tilt Left/Right** - Inclinazione sinistra/destra
- 🔀 **Face Up/Down** - Device verso l'alto/basso
- ⏸️ **Shake X/Y/Z** - Vibrazione lungo asse

**Requisiti:**
- **MPU6050** (giroscopio obbligatorio)
- Movimento orientamento chiaro

**Pro:**
- **ZERO drift** (solo orientamento, no posizione)
- Precisione altissima
- Riconoscimento rapido

**Contro:**
- Richiede MPU6050

## Esempio di Utilizzo

### Auto-Mode (Raccomandato)

```cpp
#include "gestureAnalyze.h"

// Setup
GestureAnalyze analyzer(gestureSensor);
analyzer.setRecognitionMode(MODE_AUTO);  // Auto-select best mode
analyzer.setConfidenceThreshold(0.6);     // Min 60% confidence

// Durante gesture
gestureSensor.startSampling();
// ... utente esegue gesture ...
gestureSensor.stopSampling();

// Riconoscimento
GestureResult result = analyzer.recognizeGesture();

if (result.gestureID >= 0 && result.confidence >= 0.6) {
    Serial.print("Detected: ");
    Serial.println(result.getName());
    Serial.print("Mode: ");
    Serial.println(result.mode);
    Serial.print("Confidence: ");
    Serial.println(result.confidence);

    // Azioni basate su tipo
    if (result.mode == MODE_SHAPE_RECOGNITION) {
        switch (result.shapeType) {
            case SHAPE_CIRCLE:
                // Esegui azione per cerchio
                break;
            case SHAPE_LINE:
                // Esegui azione per linea
                break;
            // ...
        }
    } else if (result.mode == MODE_ORIENTATION) {
        switch (result.orientationType) {
            case ORIENT_ROTATE_90_CW:
                // Ruota display 90° CW
                break;
            case ORIENT_TILT_FORWARD:
                // Scroll down
                break;
            // ...
        }
    }
}
```

### Mode Specifico

```cpp
// Forza shape recognition
analyzer.setRecognitionMode(MODE_SHAPE_RECOGNITION);
GestureResult result = analyzer.recognizeShape();

// Forza orientation recognition
analyzer.setRecognitionMode(MODE_ORIENTATION);
GestureResult result = analyzer.recognizeOrientation();

// Usa metodo legacy
analyzer.setRecognitionMode(MODE_LEGACY_KNN);
int gestureID = analyzer.findKNNMatch(3);
```

## Gesture Set Consigliato

### Per MPU6050 (Accel + Gyro)

**Orientation-based** (zero drift, massima precisione):
1. Rotate 90° → Cambia modalità
2. Rotate 180° → Flip display
3. Tilt Forward → Scroll down
4. Tilt Backward → Scroll up
5. Face Down → Sleep/Mute
6. Shake → Undo

**Shape-based** (gesture corte, 1-2 sec):
7. Circle → Conferma
8. Line Left → Indietro
9. Line Right → Avanti
10. Zigzag → Cancella

**Totale: ~15 gesture distintive**

### Per Solo Accelerometro (ADXL345)

**Shape-based** (unico metodo disponibile):
1. Circle CW → Azione 1
2. Circle CCW → Azione 2
3. Line Up/Down/Left/Right → Navigazione
4. Triangle → Menu
5. Square → Selezione
6. Zigzag → Cancella

**Totale: ~10 gesture**

## Best Practices

### Per Shape Recognition

1. **Movimento fluido**: Disegna la forma in modo continuo
2. **Velocità moderata**: Né troppo veloce né troppo lento
3. **Durata ottimale**: 1-2 secondi (max 3 sec)
4. **Dimensione consistente**: Mantieni dimensione simile tra gesture
5. **Chiudi forme**: Per cerchi, triangoli, quadrati torna al punto iniziale

### Per Orientation Recognition

1. **Movimento deciso**: Rotazioni/inclinazioni chiare
2. **Tieni fermo**: Mantieni posizione finale per 0.5 sec
3. **Una rotazione alla volta**: Non combinare roll+pitch+yaw
4. **Shake energico**: Per shake, vibra rapidamente

## Tuning dei Parametri

### Confidence Threshold

```cpp
// Più restrittivo (meno falsi positivi, più reject)
analyzer.setConfidenceThreshold(0.8);  // 80%

// Più permissivo (più gesture accettate, più falsi positivi)
analyzer.setConfidenceThreshold(0.4);  // 40%

// Default (bilanciato)
analyzer.setConfidenceThreshold(0.5);  // 50%
```

### Shape Analysis Tuning

```cpp
ShapeAnalysis shapeAnalyzer;

// Cerchi più stretti (circolarity più alta)
shapeAnalyzer.setCircularityThreshold(0.8);  // Default 0.7

// Angoli più sharp (per triangoli/quadrati)
shapeAnalyzer.setSharpTurnAngle(45.0);  // Default 60°
```

### Orientation Tuning

```cpp
OrientationFeatureExtractor orientExtractor;

// Rotazioni più sensibili
orientExtractor.setRotationThreshold(60.0);  // Default 70°

// Tilt più sensibili
orientExtractor.setTiltThreshold(20.0);  // Default 30°

// Shake più sensibile
orientExtractor.setShakeThreshold(1.5);  // Default 2.0 rad/s
```

## Troubleshooting

### "Shape recognition drift troppo alto"

**Problema**: MotionIntegrator rileva drift eccessivo
**Soluzione**:
- Riduci durata gesture (<2 sec)
- Aumenta drift threshold: `motionIntegrator.setDriftThreshold(3.0)`
- Usa movimenti più piccoli/controllati

### "Orientation recognition non rileva rotazione"

**Problema**: Threshold troppo alto o gyro non calibrato
**Soluzione**:
- Verifica MPU6050 funzionante: `analyzer.hasGyroscope()`
- Riduci threshold: `orientExtractor.setRotationThreshold(50.0)`
- Calibra sensore prima dell'uso

### "Troppi falsi positivi"

**Problema**: Confidence threshold troppo basso
**Soluzione**:
- Aumenta threshold: `analyzer.setConfidenceThreshold(0.7)`
- Usa gesture più distintive
- Migliora qualità esecuzione gesture

### "Nessuna gesture riconosciuta"

**Problema**: Buffer vuoto o dati invalidi
**Soluzione**:
- Verifica sampling: `gestureSensor.isSampling()`
- Controlla sample count: `buffer.sampleCount > 50`
- Verifica gyro se MPU6050: `samples[i].gyroValid`

## Performance

### Memoria

- **Madgwick Filter**: ~100 bytes
- **Motion Integrator**: ~2-3 KB (array path)
- **Shape Analyzer**: ~500 bytes
- **Orientation Extractor**: ~1-2 KB (history arrays)

**Totale extra**: ~4-6 KB RAM

### CPU

- **Madgwick update**: ~200 µs per sample @ 100Hz
- **Motion integration**: ~50 µs per sample
- **Shape analysis**: ~5-10 ms per gesture
- **Orientation extraction**: ~10-15 ms per gesture

**Totale riconoscimento**: <30 ms @ 100 samples

### Batteria

- **Orientation mode**: +0% (usa già gyro attivo)
- **Shape mode**: +5-10% (integrazione extra)

## Versioning

- **v1.0** - Legacy KNN only
- **v2.0** - Added Shape Recognition + Orientation Recognition
- **v2.1** - Current (Dual-mode auto-select)

---

**Creato da**: Enrico Mori + Claude AI
**Data**: 2025-01-26
**Licenza**: GPL-3.0
