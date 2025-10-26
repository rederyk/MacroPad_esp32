# MPU6050 Axis Calibration Guide

## Setup Fisico Attuale
- **Chip MPU6050**: Rivolto verso di te
- **Pin**: In alto
- **Device**: Tenuto verticalmente

## Problema Rilevato
```
Inclini verso di te → Sistema rileva "Tilt RIGHT" ❌
Dovrebbe rilevare: "Tilt FORWARD" ✅
```

---

## Test degli Assi

### Test 1: Identificare gli assi fisici

**Tieni il device fermo in posizione normale** (chip verso di te, pin in alto).

Nel log guarda i valori raw dell'accelerometro durante questi movimenti:

1. **Inclina AVANTI (verso di te)**
   - Quale asse cambia di più?
   - Aumenta o diminuisce?

2. **Inclina DESTRA**
   - Quale asse cambia di più?
   - Aumenta o diminuisce?

3. **Inclina SU (pin verso il cielo)**
   - Quale asse cambia di più?
   - Aumenta o diminuisce?

---

## Configurazioni comuni per MPU6050

### Config 1: Chip verso utente, Pin in alto (standard)
```json
"axisMap": "xyz",
"axisDir": "+++"
```

**Roll** (rotazione attorno asse X) = Destra/Sinistra laterale
**Pitch** (rotazione attorno asse Y) = Avanti/Indietro
**Yaw** (rotazione attorno asse Z) = Rotazione oraria/antioraria

### Config 2: Chip verso utente, Pin in alto (invertiti)
```json
"axisMap": "xyz",
"axisDir": "+-+"
```

### Config 3: Sensore ruotato 90° (pin a destra)
```json
"axisMap": "yxz",
"axisDir": "-++"
```

### Config 4: Tua configurazione attuale (sembra sbagliata)
```json
"axisMap": "yzx",
"axisDir": "-+-"
```

---

## Come Testare

1. **Modifica config.json** con una delle configurazioni sopra
2. **Riavvia il device**
3. **Esegui una gesture di test**: Inclina verso di te
4. **Guarda il log**:
   ```
   [OrientationFeatures] dRoll=X° dPitch=Y° dYaw=Z°
   [OrientationRecognition] Detected: Tilt ???
   ```

5. **Verifica**:
   - Inclina verso di te → Dovrebbe dire "Tilt Forward"
   - Inclina verso destra → Dovrebbe dire "Tilt Right"
   - Inclina a sinistra → Dovrebbe dire "Tilt Left"
   - Inclina indietro → Dovrebbe dire "Tilt Backward"

---

## Mapping Corretto

| Movimento Fisico | Roll | Pitch | Yaw | Nome Gesture |
|------------------|------|-------|-----|--------------|
| Inclina AVANTI (verso te) | 0° | **+30°** | 0° | Tilt Forward |
| Inclina INDIETRO | 0° | **-30°** | 0° | Tilt Backward |
| Inclina DESTRA | **+30°** | 0° | 0° | Tilt Right |
| Inclina SINISTRA | **-30°** | 0° | 0° | Tilt Left |
| Ruota ORARIA | 0° | 0° | **+90°** | Rotate 90 CW |
| Ruota ANTIORARIA | 0° | 0° | **-90°** | Rotate 90 CCW |

---

## Configurazione Probabilmente Corretta

Basandomi sul fatto che "inclina verso te" viene rilevato come "Tilt Right", sembra che:
- Il tuo **Pitch** sia mappato su **Roll**
- Serve swappare gli assi

**Prova questa configurazione:**

```json
"axisMap": "xyz",
"axisDir": "+++"
```

O se ancora sbagliato:

```json
"axisMap": "yxz",
"axisDir": "+++"
```

---

## Debug Live

Se vuoi vedere i valori raw in tempo reale, aggiungi questo nel codice:

```cpp
// In gestureOrientationFeatures.cpp, nella funzione extract():
Logger::getInstance().log("Raw angles: R=" + String(rollHistory[i] * 180/M_PI) +
                         " P=" + String(pitchHistory[i] * 180/M_PI) +
                         " Y=" + String(yawHistory[i] * 180/M_PI));
```

Questo ti mostrerà i valori Roll/Pitch/Yaw in gradi mentre muovi il device.

---

## Quick Fix

Mentre cerchi la config perfetta, puoi anche **abbassare la sensibilità** per evitare falsi positivi:

```cpp
// In GestureDevice.cpp, nella performRecognition():
analyzer.setConfidenceThreshold(0.7f);  // Aumenta da 0.5 a 0.7 (70%)
```

E nella classe OrientationFeatureExtractor:

```cpp
// Aumenta threshold per tilt (più movimento richiesto)
orientExtractor.setTiltThreshold(40.0);  // Default era 30°
```
