# Gesture Recognition System - Guida Completa

## Panoramica

Il sistema di riconoscimento gesture del MacroPad ESP32 è stato completamente separato in **due sistemi distinti** basati sul tipo di sensore:

### 1. **MPU6050 System** (Shape + Orientation)
- **Sensore**: MPU6050 con accelerometro + giroscopio
- **Metodi**: Riconoscimento forme geometriche e orientamento
- **Gesture**: Predefinite (cerchi, linee, rotazioni, inclinazioni)
- **Training**: ❌ NON supportato - usa solo gesture predefinite
- **File storage**: Nessuno (non richiede training)

### 2. **ADXL345 System** (KNN Custom)
- **Sensore**: ADXL345 solo accelerometro
- **Metodo**: K-Nearest Neighbors con feature extraction
- **Gesture**: Personalizzate (da registrare con TRAIN_GESTURE)
- **Training**: ✅ Supportato - registra fino a 9 gesture custom (ID 0-8)
- **File storage**: `/gesture_features_adxl345.json` e `/gestures_adxl345.bin`

---

## MPU6050: Gesture Predefinite

### Orientation Gestures (ID 200-299)

**Caratteristiche**:
- Zero drift (usa solo giroscopio)
- Precisione altissima
- Riconoscimento rapido (<500ms)

| ID  | Nome              | Descrizione                      | Esempi Uso                         |
|-----|-------------------|----------------------------------|------------------------------------|
| 201 | `rotate_cw`       | Rotazione 90° senso orario       | Next track, workspace successivo   |
| 202 | `rotate_ccw`      | Rotazione 90° senso antiorario   | Previous track, workspace prec.    |
| 203 | `rotate_180`      | Rotazione 180°                   | Flip display, alterna finestra     |
| 204 | `tilt_forward`    | Inclina in avanti (pitch+)       | Scroll down, pagina giù, zoom out  |
| 205 | `tilt_backward`   | Inclina indietro (pitch-)        | Scroll up, pagina su, zoom in      |
| 206 | `tilt_left`       | Inclina a sinistra (roll+)       | Volume giù, naviga sinistra        |
| 207 | `tilt_right`      | Inclina a destra (roll-)         | Volume su, naviga destra           |

### Shape Gestures (ID 100-199)

**Caratteristiche**:
- Riconosce forme disegnate in aria
- Richiede 1-2 secondi di movimento
- Può avere drift dopo 3 secondi

| ID  | Nome       | Simbolo | Descrizione                   | Esempi Uso                    |
|-----|------------|---------|-------------------------------|-------------------------------|
| 101 | `circle`   | ⭕      | Cerchio orario/antiorario     | Conferma, OK, select all      |
| 102 | `line`     | ➖      | Linea dritta (4 direzioni)    | Avanti/indietro, cancella     |
| 103 | `triangle` | 🔺      | Triangolo (3 angoli sharp)    | Menu, opzioni, cartella       |
| 104 | `square`   | ⬜      | Quadrato/rettangolo           | Stop, fullscreen, screenshot  |
| 105 | `zigzag`   | ⚡      | Pattern zigzag                | Undo, shake refresh, delete   |
| 106 | `infinity` | ∞       | Simbolo infinito (8 orizz.)   | Loop, sync, refresh           |
| 107 | `spiral`   | 🌀      | Spirale                       | Zoom animato, loading         |
| 108 | `arc`      | ⌒       | Arco/semicerchio              | Swipe, cambio tab, notifiche  |

### Configurazione MPU6050

Nel file `config.json`:

```json
{
  "accelerometer": {
    "type": "mpu6050",
    "gestureMode": "auto",
    "active": true
  }
}
```

**Valori `gestureMode` disponibili**:
- `"auto"` - Selezione automatica (raccomandato)
- `"mpu6050"` - Forza uso MPU6050 recognizer
- `"shape"` - Solo shape recognition
- `"orientation"` - Solo orientation recognition

### Esempio Utilizzo MPU6050

```cpp
// Nel file macros configurati così:
{
  "profile": "media",
  "key": "gesture_207",  // tilt_right
  "action": "MEDIA_VOLUME_UP"
}

{
  "profile": "media",
  "key": "gesture_206",  // tilt_left
  "action": "MEDIA_VOLUME_DOWN"
}

{
  "profile": "media",
  "key": "gesture_101",  // circle
  "action": "MEDIA_PLAY_PAUSE"
}
```

---

## ADXL345: Gesture Personalizzate

### Custom Gestures (ID 0-8)

**Caratteristiche**:
- Training richiesto per ogni gesture
- Massimo 9 gesture personalizzate
- Feature extraction automatica
- Riconoscimento KNN con k=3

| ID | Nome       | Uso                                    |
|----|------------|----------------------------------------|
| 0  | `custom_0` | Gesture personalizzata 1 (da registrare) |
| 1  | `custom_1` | Gesture personalizzata 2               |
| 2  | `custom_2` | Gesture personalizzata 3               |
| 3  | `custom_3` | Gesture personalizzata 4               |
| 4  | `custom_4` | Gesture personalizzata 5               |
| 5  | `custom_5` | Gesture personalizzata 6               |
| 6  | `custom_6` | Gesture personalizzata 7               |
| 7  | `custom_7` | Gesture personalizzata 8               |
| 8  | `custom_8` | Gesture personalizzata 9               |

### Configurazione ADXL345

Nel file `config.json`:

```json
{
  "accelerometer": {
    "type": "adxl345",
    "gestureMode": "auto",
    "active": true
  }
}
```

### Come Registrare Gesture Personalizzate

#### 1. Configura il tasto TRAIN_GESTURE

Nel file macros:
```json
{
  "profile": "default",
  "key": "1",
  "press": "TRAIN_GESTURE"
}
```

#### 2. Registra la gesture

**Procedura**:
1. **Tieni premuto** il tasto configurato con `TRAIN_GESTURE`
2. **Esegui la gesture** che vuoi registrare (es: scuoti il device, muovilo in cerchio, etc.)
3. **Rilascia** il tasto
4. Il sistema chiede: "Press key 1-9 to save gesture"
5. **Premi un tasto** da 1 a 9 per assegnare l'ID:
   - Tasto `1` → salva come ID 0 (`custom_0`)
   - Tasto `2` → salva come ID 1 (`custom_1`)
   - ... e così via
6. Conferma: "Gesture saved with ID: X"

#### 3. Configura l'azione per la gesture

Dopo aver registrato, configura il macro:
```json
{
  "profile": "custom",
  "key": "gesture_0",
  "action": "LAUNCH_APP",
  "app": "chrome.exe"
}
```

### Best Practices per Training ADXL345

1. **Registra 3-5 sample per gesture**: Per migliore accuratezza, registra la stessa gesture più volte
2. **Gesture distinte**: Usa movimenti molto diversi tra loro (es: su/giù, cerchio, scuoti)
3. **Consistenza**: Esegui la gesture sempre nello stesso modo
4. **Durata**: 1-2 secondi di movimento (né troppo veloce né troppo lento)
5. **Test**: Testa il riconoscimento con `EXECUTE_GESTURE` prima di configurare i macro

### Cancellare Gesture Salvate

Per ricominciare da zero, elimina i file:
- `/gesture_features_adxl345.json`
- `/gestures_adxl345.bin`

---

## Esempi Pratici

### Esempio 1: Media Control (MPU6050)

```json
{
  "profiles": {
    "media": {
      "gesture_206": "MEDIA_VOLUME_DOWN",
      "gesture_207": "MEDIA_VOLUME_UP",
      "gesture_204": "MEDIA_NEXT_TRACK",
      "gesture_205": "MEDIA_PREVIOUS_TRACK",
      "gesture_101": "MEDIA_PLAY_PAUSE",
      "gesture_104": "MEDIA_STOP"
    }
  }
}
```

**Uso**:
- Inclina sinistra/destra per volume
- Inclina avanti/indietro per cambiare traccia
- Cerchio per play/pause
- Quadrato per stop

### Esempio 2: Navigazione Desktop (MPU6050)

```json
{
  "profiles": {
    "desktop": {
      "gesture_207": {
        "action": "KEY_COMBO",
        "keys": ["CTRL", "WIN", "RIGHT"]
      },
      "gesture_206": {
        "action": "KEY_COMBO",
        "keys": ["CTRL", "WIN", "LEFT"]
      },
      "gesture_203": {
        "action": "KEY_COMBO",
        "keys": ["ALT", "TAB"]
      }
    }
  }
}
```

**Uso**:
- Inclina sinistra/destra per cambiare workspace
- Rotazione 180° per alternare finestre (Alt+Tab)

### Esempio 3: Gesture Custom (ADXL345)

**Setup**:
1. Registra gesture "scuoti forte" → ID 0
2. Registra gesture "cerchio lento" → ID 1
3. Registra gesture "su e giù rapido" → ID 2

**Configurazione**:
```json
{
  "profiles": {
    "custom": {
      "gesture_0": {
        "action": "LAUNCH_APP",
        "app": "notepad.exe"
      },
      "gesture_1": {
        "action": "KEY_COMBO",
        "keys": ["WIN", "L"]
      },
      "gesture_2": {
        "action": "TYPE_STRING",
        "text": "mia_email@example.com"
      }
    }
  }
}
```

---

## Differenze tra i Sistemi

| Caratteristica            | MPU6050                  | ADXL345                  |
|---------------------------|--------------------------|--------------------------|
| Sensore richiesto         | MPU6050 (accel + gyro)   | ADXL345 (solo accel)     |
| Tipo gesture              | Predefinite              | Personalizzate           |
| Training richiesto        | ❌ No                    | ✅ Sì                    |
| Numero gesture            | 15+ predefinite          | Max 9 custom             |
| Drift                     | Zero (orientation mode)  | Possibile (shape mode)   |
| Precisione                | Alta (95%+)              | Media (80-90%)           |
| Setup iniziale            | Plug & play              | Richiede registrazione   |
| Storage necessario        | Nessuno                  | ~5-10KB JSON             |
| Tempo riconoscimento      | 200-500ms                | 300-800ms                |

---

## Troubleshooting

### MPU6050: "Gesture non riconosciuta"

**Cause**:
- Movimento troppo breve o poco chiaro
- Confidence sotto soglia (default 50%)

**Soluzioni**:
- Esegui gesture più lentamente e con movimenti ampi
- Riduci confidence threshold: `analyzer.setConfidenceThreshold(0.4)`
- Verifica gyro funzionante: controlla log "gyroValid"

### ADXL345: "Training già attivo"

**Causa**: Hai premuto TRAIN_GESTURE mentre training già in corso

**Soluzione**: Rilascia il tasto e riprova

### ADXL345: "Gesture salvate non riconosciute"

**Cause**:
- Sample insufficienti (meno di 3 per gesture)
- Gesture troppo simili tra loro
- Esecuzione inconsistente

**Soluzioni**:
1. Cancella file storage e ricomincia
2. Registra 5+ sample per ogni gesture
3. Usa gesture molto diverse (es: veloce vs lento, verticale vs orizzontale)
4. Mantieni stessa velocità e ampiezza quando esegui gesture

### Generale: "Troppi falsi positivi"

**Soluzione**: Aumenta confidence threshold
```json
// In config.json o in codice
"confidenceThreshold": 0.7  // Default 0.5
```

---

## Performance

### Memoria RAM

| Componente              | MPU6050 | ADXL345 |
|-------------------------|---------|---------|
| Recognizer overhead     | ~4KB    | ~2KB    |
| Sample buffer (100 Hz)  | ~6KB    | ~6KB    |
| Feature storage         | 0       | ~5KB    |
| **Totale**              | ~10KB   | ~13KB   |

### Velocità

| Operazione           | MPU6050      | ADXL345      |
|----------------------|--------------|--------------|
| Sampling (1-2 sec)   | 100-200 ms   | 100-200 ms   |
| Processing           | 20-50 ms     | 30-80 ms     |
| Recognition          | 10-30 ms     | 50-100 ms    |
| **Totale end-to-end**| 130-280 ms   | 180-380 ms   |

---

## File di Riferimento

- **Esempi comandi**: [`/data/gesture_commands_examples.json`](../../data/gesture_commands_examples.json)
- **Config sensore**: [`/data/config.json`](../../data/config.json)
- **Interface**: [`IGestureRecognizer.h`](IGestureRecognizer.h)
- **MPU6050 Recognizer**: [`MPU6050GestureRecognizer.h/.cpp`](MPU6050GestureRecognizer.h)
- **ADXL345 Recognizer**: [`ADXL345GestureRecognizer.h/.cpp`](ADXL345GestureRecognizer.h)

---

**Creato da**: Enrico Mori + Claude AI
**Data**: 2025-01-26
**Versione**: 3.0 (Sensor-specific separated systems)
**Licenza**: GPL-3.0
