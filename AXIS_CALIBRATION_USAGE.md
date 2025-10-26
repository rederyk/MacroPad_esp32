# Calibrazione Automatica Assi Accelerometro

## Problema Risolto

Se le gesture di orientamento vengono rilevate in modo errato (es. "Tilt Right" quando inclini verso di te), significa che gli assi del sensore non sono configurati correttamente.

**La calibrazione automatica risolve questo problema!**

---

## Come Usare la Calibrazione

### Metodo 1: Tramite Codice (Automatico)

Aggiungi questa funzione nel tuo `main.cpp` o dove gestisci l'input:

```cpp
#include "GestureDevice.h"

// Dichiarazioni globali
extern GestureDevice gestureDevice;

// Funzione da chiamare una volta (es. durante setup o tramite comando)
void calibrateGestureSensor() {
    Serial.println("=== CALIBRAZIONE ASSI ACCELEROMETRO ===");
    Serial.println("Posiziona il device come segue:");
    Serial.println("- Pulsanti rivolti verso di te");
    Serial.println("- Device in verticale");
    Serial.println("- Tienilo FERMO per 3 secondi");
    Serial.println("");
    Serial.println("Premi INVIO quando pronto...");

    // Attendi input utente (opzionale)
    while (!Serial.available());
    while (Serial.available()) Serial.read();  // Svuota buffer

    // Esegui calibrazione
    bool success = gestureDevice.calibrateAxes(true);  // true = salva su config.json

    if (success) {
        Serial.println("");
        Serial.println("✓ CALIBRAZIONE RIUSCITA!");
        Serial.println("✓ Configurazione salvata in config.json");
        Serial.println("✓ RIAVVIA il device per applicare");
    } else {
        Serial.println("");
        Serial.println("✗ CALIBRAZIONE FALLITA");
        Serial.println("  Riprova tenendo il device più fermo");
    }
}
```

### Chiamata della Calibrazione

Puoi attivare la calibrazione in diversi modi:

**Opzione A: Comando Seriale**
```cpp
void loop() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        if (cmd == "calibrate") {
            calibrateGestureSensor();
        }
    }
}
```

**Opzione B: Combinazione Tasti**
```cpp
// Durante setup o quando rilevi combinazione speciale
if (button1.pressed() && button2.pressed() && button3.pressed()) {
    calibrateGestureSensor();
}
```

**Opzione C: Menu di Configurazione**
```cpp
// Nel menu web o seriale
case CONFIG_CALIBRATE_AXES:
    calibrateGestureSensor();
    break;
```

---

## Processo di Calibrazione

### 1. Preparazione
- MacroPad spento o in modalità idle
- Superficie piana disponibile (opzionale)

### 2. Posizionamento
**IMPORTANTE**: Il device deve essere nella sua **posizione di utilizzo normale**:

```
     [PIN IN ALTO]
    ┌─────────────┐
    │  MPU6050    │  ← Chip visibile rivolto verso di te
    │             │
    │  [Button]   │  ← Pulsanti verso di te
    │  [Button]   │
    │  [Button]   │
    └─────────────┘
     [VERTICALE]
```

### 3. Esecuzione
1. **Avvia** calibrazione (comando/tasto/menu)
2. **Posiziona** device come sopra
3. **Tieni FERMO** per 2-3 secondi
4. **Aspetta** il risultato

### 4. Risultato

**Se SUCCESS:**
```
[AxisCalibration] SUCCESS: axisMap="xyz", axisDir="+-+", confidence=95%
[GestureDevice] Saved! Restart device to apply.
```

**Se FAILED:**
```
[AxisCalibration] FAILED: Confidence too low (45%)
[AxisCalibration] Make sure device is held still and vertical
```

Se fallisce:
- ✓ Riprova tenendo il device PIÙ FERMO
- ✓ Usa una superficie piana
- ✓ Assicurati che il device sia VERTICALE (non inclinato)

---

## Cosa Viene Salvato

La calibrazione aggiorna automaticamente `config.json`:

```json
{
  "accelerometer": {
    "axisMap": "xyz",    // ← Aggiornato automaticamente
    "axisDir": "+-+",    // ← Aggiornato automaticamente
    ...
  }
}
```

**IMPORTANTE**: Dopo calibrazione riuscita, **RIAVVIA** il device per applicare le modifiche!

---

## Test Post-Calibrazione

Dopo riavvio, testa le gesture:

| Movimento Fisico | Gesture Rilevata | ✓/✗ |
|------------------|------------------|-----|
| Inclina verso di te | **Tilt Forward** | ✓ |
| Inclina verso destra | **Tilt Right** | ✓ |
| Inclina a sinistra | **Tilt Left** | ✓ |
| Inclina indietro | **Tilt Backward** | ✓ |
| Ruota 90° oraria | **Rotate 90 CW** | ✓ |

Se uno è sbagliato → **Ricalibra**

---

## Calibrazione Manuale (Backup)

Se la calibrazione automatica non funziona, puoi configurare manualmente guardando i log:

1. Esegui una gesture con device in posizione normale
2. Guarda nel log i valori:
   ```
   [OrientationFeatures] dRoll=-57.8° dPitch=1.5° dYaw=5.3°
   ```

3. Identifica quale valore cambia per quale movimento:
   - **Inclina AVANTI** → Dovrebbe cambiare **Pitch** (+30° circa)
   - **Inclina DESTRA** → Dovrebbe cambiare **Roll** (+30° circa)
   - **Ruota ORARIA** → Dovrebbe cambiare **Yaw** (+90° circa)

4. Se i valori sono swappati, modifica `config.json`:

**Esempi configurazioni comuni:**

```json
// Config 1: Standard
"axisMap": "xyz",
"axisDir": "+++"

// Config 2: Chip ruotato 90°
"axisMap": "yxz",
"axisDir": "-++"

// Config 3: Pin a sinistra
"axisMap": "yzx",
"axisDir": "+-+"
```

---

## Troubleshooting

### Problema: "Calibration FAILED (confidence 45%)"
**Causa**: Device non tenuto fermo o non verticale

**Soluzione**:
- Appoggia device su superficie piana
- Tienilo con entrambe le mani FERME
- Riprova calibrazione

---

### Problema: Dopo calibrazione, gesture ancora sbagliate
**Causa**: Config.json non ricaricato

**Soluzione**:
- **RIAVVIA** il device
- Verifica che config.json sia stato modificato (controlla file)
- Se necessario, ri-carica filesystem con `pio run --target uploadfs`

---

### Problema: "Cannot access motion sensor"
**Causa**: Sensore non inizializzato o non disponibile

**Soluzione**:
- Verifica che accelerometro sia configurato attivo in config.json
- Controlla connessioni I2C (SDA/SCL)
- Riavvia device

---

## API Completa

```cpp
// Header da includere
#include "GestureDevice.h"
#include "gestureAxisCalibration.h"

// Calibrazione tramite GestureDevice (consigliato)
bool success = gestureDevice.calibrateAxes(true);  // true = salva config

// Calibrazione diretta (avanzato)
MotionSensor* sensor = gestureSensor.getMotionSensor();
AxisCalibration calibrator;
AxisCalibrationResult result = calibrator.calibrate(sensor, 2000);

if (result.success) {
    Serial.println("axisMap: " + result.axisMap);
    Serial.println("axisDir: " + result.axisDir);
    Serial.println("confidence: " + String(result.confidence * 100) + "%");

    // Salva su file
    calibrator.saveToConfig(result, "/config.json");
}
```

---

## Note Tecniche

### Come Funziona

1. **Raccoglie campioni** per 2 secondi dal sensore
2. **Identifica asse gravità**: Quale asse legge ~1g (verso il basso)
3. **Determina mapping**: Quale asse fisico corrisponde a X/Y/Z logico
4. **Calcola direzioni**: Se invertire (+/-) ciascun asse
5. **Salva config**: Scrive su config.json

### Requisiti

- Sensore: **MPU6050** o **ADXL345**
- Filesystem: **LittleFS** montato
- RAM: ~500 bytes durante calibrazione

### Precisione

- **Confidence >90%**: Ottima, garantisce correttezza
- **Confidence 70-90%**: Buona, funziona bene
- **Confidence <70%**: Bassa, ripeti calibrazione

---

**Creato da**: Enrico Mori + Claude AI
**Data**: 2025-01-26
**Licenza**: GPL-3.0
