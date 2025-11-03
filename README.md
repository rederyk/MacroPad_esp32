# ESP32 MacroPad

Un MacroPad versatile e completamente personalizzabile basato su ESP32, con riconoscimento gesti, combinazioni di tasti programmabili, controllo IR, illuminazione RGB interattiva e connettività WiFi/BLE.

## Indice

- [Caratteristiche Principali](#caratteristiche-principali)
- [Novità e Funzionalità Recenti](#novità-e-funzionalità-recenti)
- [Hardware Necessario](#hardware-necessario)
- [Installazione Rapida](#installazione-rapida)
- [Configurazione Iniziale](#configurazione-iniziale)
- [Documentazione](#documentazione)
- [Esempi d'Uso](#esempi-duso)
- [Contributi](#contributi)
- [Licenza](#licenza)

---

## Caratteristiche Principali

### Input e Controlli
- **Keypad 3x3 Programmabile** - 9 tasti configurabili con supporto combinazioni multiple
- **Encoder Rotativo** - Input incrementale con pulsante integrato
- **Riconoscimento Gesti** - Sistema avanzato con accelerometro ADXL345 o MPU6050
  - Addestramento personalizzato multi-gesto
  - Algoritmo KNN per matching in tempo reale
  - Wake-up da deep sleep su movimento

### Connettività
- **Bluetooth Low Energy (BLE)** - Emula tastiera/mouse HID
- **WiFi** - Modalità Access Point e Station
- **Web Interface Completa** - Configurazione senza cavo seriale
  - Editor combinazioni in tempo reale
  - Gestione gesti e IR
  - Log sistema e debugging
  - Gestione file JSON

### Illuminazione LED RGB
- **Feedback Visivo Intelligente** - Indicatori di stato colorati
- **Modalità Interattiva** - Un colore diverso per ogni tasto
- **Editor Colori Integrato** - Modifica colori RGB con encoder
- **Luminosità Persistente** - Salvata automaticamente
- **Colori per Combo** - Ogni set di combinazioni può avere il suo colore

### Controllo Infrarossi
- **Apprendimento IR** - Impara comandi da qualsiasi telecomando
- **Database Comandi** - Salva e organizza comandi per dispositivo
- **Integrazione Combo** - Controlla TV, LED, condizionatori dalle combinazioni

### Gestione Energia
- **Deep Sleep Mode** - Consumo ultra-ridotto (~2 settimane con batteria 820mAh)
- **Motion Wake** - Risveglio automatico su movimento accelerometro
- **Timeout Configurabile** - Sleep automatico dopo inattività
- **Fallback Pin** - Wake su encoder se accelerometro non disponibile

### Sistema Macro Avanzato
- **Comandi Concatenati** - Esegui sequenze di azioni
- **Delay Configurabili** - Timing preciso tra comandi
- **Switching Combo** - Cambia set di combinazioni al volo
- **Combo Common** - Azioni sempre disponibili (gesti, sistema)

---

## Novità e Funzionalità Recenti

### Illuminazione Interattiva (Reactive Lighting)
Modalità LED innovativa che permette di associare colori personalizzati a ogni tasto:
- Premi un tasto e il LED mostra il suo colore
- Modifica i colori tenendo premuto il tasto e ruotando l'encoder
- Cambia canale RGB con il pulsante encoder
- Salva configurazione nel file combo

**Esempio:**
```json
"1+2": ["REACTIVE_LIGHTING"],
"7+8+9": ["SAVE_INTERACTIVE_COLORS"]
```

### Settings per Combo File
Ogni file combo può ora includere configurazioni personalizzate:
```json
{
  "_settings": {
    "led_color": [255, 255, 0],
    "interactive_colors": [
      [255, 0, 0], [255, 127, 0], [255, 255, 0],
      [0, 255, 0], [0, 255, 255], [0, 0, 255],
      [127, 0, 255], [255, 0, 255], [255, 255, 255]
    ]
  }
}
```

### Sistema di Gestione Colori
- Colori di notifica sistema (magenta=avvio, blu=BLE, verde=WiFi, rosso=errore)
- Modalità torcia (flashlight) a intensità massima
- Controllo luminosità con salvataggio automatico
- Scaling globale brightness con persistenza

### Gestione Energia Migliorata
- Wake-up su movimento con MPU6050
- Auto-calibrazione giroscopio all'avvio
- Configurazione motion threshold da config.json
- Timeout sleep configurabile

### Sistema IR Completo
- Supporto protocolli multipli (NEC, Sony, RC5, ecc.)
- Storage persistente comandi in ir_data.json
- Scan e invio comandi da combo
- Organizzazione per dispositivo

---

## Hardware Necessario

### Componenti Base
- **ESP32** - Board LOLIN32 Lite (testata) o compatibile
- **Switch Meccanici** - 9 switch per keypad 3x3
- **Diodi** - 9 diodi 1N4148 o simili per matrice keypad
- **Encoder Rotativo** - Con pulsante integrato
- **Keycaps** - 9 keycaps (opzionali ma consigliati)

### Componenti Opzionali
- **Accelerometro** - ADXL345 o MPU6050 (per gesti e motion wake)
- **LED RGB** - LED RGB catodo comune (feedback visivo)
- **IR LED** - LED infrarossi 940nm (trasmissione IR)
- **IR Receiver** - TSOP38238 o compatibile (ricezione IR)
- **Batteria LiPo** - 3.7V (LOLIN32 ha caricatore integrato)
- **Case/Enclosure** - Stampato 3D o custom

### Pin Layout Predefinito (LOLIN32 Lite)

| Componente | Pin GPIO | Note |
|------------|----------|------|
| **Keypad Righe** | 0, 4, 16 | Con diodi |
| **Keypad Colonne** | 17, 5, 18 | |
| **Encoder A** | 13 | |
| **Encoder B** | 15 | |
| **Encoder Button** | 2 | Anche wakeup pin |
| **LED Rosso** | 25 | |
| **LED Verde** | 26 | |
| **LED Blu** | 27 | |
| **IR TX** | 33 | |
| **IR RX** | 14 | |
| **I2C SDA** | 19 | Accelerometro |
| **I2C SCL** | 23 | Accelerometro |
| **Wakeup Pin** | 32 | Motion wake (opzionale) |

**Layout Keypad:**
```
1  2  3
4  5  6
7  8  9
```

---

## Installazione Rapida

### 1. Preparazione Software
```bash
# Clona repository
git clone https://github.com/rederyk/MacroPad_esp32.git
cd MacroPad_esp32

# PlatformIO installerà automaticamente le dipendenze
```

### 2. Configurazione Hardware
Modifica `data/config.json` per adattarlo al tuo hardware:
```json
{
  "keypad": {
    "rowPins": [0, 4, 16],
    "colPins": [17, 5, 18]
  },
  "encoder": {
    "pinA": 13,
    "pinB": 15,
    "buttonPin": 2
  },
  "led": {
    "active": true,
    "pinRed": 25,
    "pinGreen": 26,
    "pinBlue": 27
  },
  "accelerometer": {
    "active": true,
    "type": "mpu6050",
    "sdaPin": 19,
    "sclPin": 23
  }
}
```

### 3. Upload
```bash
# Upload codice e filesystem
pio run --target upload && pio run --target uploadfs
```

---

## Configurazione Iniziale

### Primo Avvio

1. **Alimenta il MacroPad** - Il LED diventerà magenta (avvio), poi blu (BLE) o verde (WiFi)

2. **Connetti al WiFi AP**
   - SSID: `ESP32_MacroPad`
   - Password: `my_cat_name123`
   - IP: `192.168.4.1`

3. **Configura il Router WiFi**
   - Apri browser su `http://192.168.4.1`
   - Vai su "Config" → "WiFi Settings"
   - Inserisci SSID e password del tuo router
   - Salva (il dispositivo si riavvierà)

4. **Riconnetti all'AP** per vedere il nuovo IP assegnato dal router

5. **Connetti al Router** usando il nuovo IP

6. **Configura Modalità** (opzionale)
   - Su "Advanced Settings" puoi:
     - Cambiare password AP (importante per sicurezza!)
     - Disabilitare `ap_autostart` se vuoi solo WiFi
     - Configurare timeout sleep
     - Abilitare/disabilitare componenti

### Passare a Modalità BLE

Premi contemporaneamente i tasti **1+2+3** e il **pulsante encoder** (o configura la tua combo):
```json
"1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"]
```

Il dispositivo si riavvierà in modalità Bluetooth. Cerca "Macropad_esp32" nelle impostazioni Bluetooth del tuo PC/smartphone.

---

## Documentazione

### File Documentazione Completa

- **[COMANDI.md](COMANDI.md)** - Riferimento completo di tutti i comandi disponibili
  - Sintassi combinazioni
  - Comandi Bluetooth (tastiera/mouse)
  - Azioni di sistema
  - Controllo LED RGB
  - Comandi IR
  - Sistema gesti
  - Esempi pratici

- **[INSTRUCTIONS.md](INSTRUCTIONS.md)** - Guida setup e configurazione
  - Installazione hardware
  - Configurazione config.json
  - Setup WiFi e BLE
  - Procedura addestramento gesti

- **[COMBO_SETTINGS.md](COMBO_SETTINGS.md)** - Documentazione settings combo
  - Campo `_settings`
  - Configurazione LED per combo
  - Colori interattivi

- **[TODO.md](TODO.md)** - Roadmap e miglioramenti pianificati

### Web Interface

Accedi alla web interface per:
- **Combo Editor** - Modifica combinazioni in tempo reale
- **System Actions** - Log live e debug
- **Gesture Management** - Visualizza e modifica gesti
- **IR Manager** - Database comandi infrarossi
- **Config Editor** - Modifica configurazione sistema
- **Advanced Settings** - Opzioni avanzate

---

## Esempi d'Uso

### Esempio 1: Produttività Desktop

**Navigazione base + scorciatoie:**
```json
{
  "1": ["S_B:TAB"],
  "4": ["S_B:LEFT_ARROW"],
  "5": ["S_B:RETURN"],
  "6": ["S_B:RIGHT_ARROW"],
  "7": ["S_B:BACKSPACE"],

  "4+5": ["S_B:CTRL+SUPER,LEFT_ARROW"],   // Workspace sinistra
  "5+6": ["S_B:CTRL+SUPER,RIGHT_ARROW"],  // Workspace destra
  "2+5": ["S_B:SUPER+q"],                 // Chiudi app

  "CW": ["S_B:MOUSE_MOVE_0_0_1_0"],       // Scroll su
  "CCW": ["S_B:MOUSE_MOVE_0_0_-1_0"],     // Scroll giù
  "1,CW": ["S_B:VOL_UP"],
  "1,CCW": ["S_B:VOL_DOWN"]
}
```

### Esempio 2: Controllo Multimediale + Smart Home

**TV e dispositivi IR:**
```json
{
  "3,BUTTON": ["SEND_IR_tv_off"],         // Spegni TV
  "3,CW": ["SEND_IR_tv_volup"],          // Volume TV +
  "3,CCW": ["SEND_IR_tv_low"],           // Volume TV -

  "2+7": ["SEND_IR_led_on"],             // LED strip ON
  "3+7": ["SEND_IR_led_off"],            // LED strip OFF

  "BUTTON": ["S_B:PLAY_PAUSE"],
  "1,BUTTON": ["S_B:PREVIOUS_TRACK"],
  "2,BUTTON": ["S_B:NEXT_TRACK"],

  "4+5+6": ["<SEND_IR_tv_off><DELAY_1000><SEND_IR_led_off><DELAY_500><LED_OFF>"]
}
```
L'ultimo comando spegne tutto: TV → aspetta 1s → LED strip → aspetta 500ms → LED MacroPad

### Esempio 3: Sviluppatore Workflow

**Terminal + Git + IDE:**
```json
{
  "1": ["S_B:CTRL+c"],
  "2": ["S_B:CTRL+v"],
  "3": ["S_B:CTRL+s"],

  "1+5": ["<S_B:SUPER+t><DELAY_300><S_B:code SPACE .><S_B:RETURN>"],
  "2+5+8": ["<S_B:SUPER+t><DELAY_500><S_B:htop><DELAY_500><S_B:RETURN>"],

  "1+2+3": ["<S_B:git SPACE status><S_B:RETURN>"],
  "4+5+6": ["<S_B:npm SPACE run SPACE dev><S_B:RETURN>"],
  "7+8+9": ["<S_B:git SPACE add SPACE .><S_B:RETURN>"]
}
```

### Esempio 4: Controllo Gesti Desktop

**Gesti per workspace e app:**
```json
{
  "BUTTON": ["EXECUTE_GESTURE"],

  "G_ROTATE_90_CW": ["S_B:CTRL+ALT,RIGHT_ARROW"],
  "G_ROTATE_90_CCW": ["S_B:CTRL+ALT,LEFT_ARROW"],
  "G_ROTATE_180": ["S_B:CTRL+ALT,DOWN_ARROW"],
  "G_FACE_UP": ["S_B:HOME"],
  "G_FACE_DOWN": ["S_B:END"],
  "G_SPIN": ["S_B:SUPER+l"],
  "G_SHAKE_X_POS": ["S_B:SUPER+q"],
  "G_SHAKE_X_NEG": ["S_B:SUPER+q"],
  "G_SHAKE_Y_POS": ["S_B:CTRL+SUPER,UP_ARROW"],
  "G_SHAKE_Y_NEG": ["S_B:CTRL+SUPER,UP_ARROW"],
  "G_SHAKE_Z_POS": ["S_B:CTRL+SUPER,DOWN_ARROW"],
  "G_SHAKE_Z_NEG": ["S_B:CTRL+SUPER,DOWN_ARROW"],

  "G_LINE": ["S_B:CTRL+SHIFT,LEFT_ARROW"],
  "G_CIRCLE": ["S_B:CTRL+A"],
  "G_TRIANGLE": ["S_B:CTRL+SHIFT,S"],
  "G_SQUARE": ["S_B:CTRL+S"],
  "G_ZIGZAG": ["S_B:CTRL+Z"],
  "G_INFINITY": ["S_B:CTRL+SHIFT,Z"],
  "G_SPIRAL": ["S_B:CTRL+Y"],
  "G_ARC": ["S_B:CTRL+P"],

  "1+5+9": ["CALIBRATE_SENSOR"]
}
```

### Esempio 5: LED Interattivo con Colori Arcobaleno

**my_combo_0.json:**
```json
{
  "_settings": {
    "led_color": [255, 255, 0],
    "interactive_colors": [
      [255, 0, 0],      // 1: Rosso
      [255, 127, 0],    // 2: Arancione
      [255, 255, 0],    // 3: Giallo
      [0, 255, 0],      // 4: Verde
      [0, 255, 255],    // 5: Ciano
      [0, 0, 255],      // 6: Blu
      [127, 0, 255],    // 7: Viola
      [255, 0, 255],    // 8: Magenta
      [255, 255, 255]   // 9: Bianco
    ]
  },

  "1+2": ["REACTIVE_LIGHTING"],
  "6,CW": ["LED_BRIGHTNESS_MINUS"],
  "6,CCW": ["LED_BRIGHTNESS_PLUS"],
  "7+8+9": ["SAVE_INTERACTIVE_COLORS"],

  "2+5": ["SWITCH_COMBO_0"]
}
```

Per modificare i colori:
1. Attiva `REACTIVE_LIGHTING` (1+2)
2. Tieni premuto un tasto (es. tasto 5)
3. Ruota encoder per modificare canale corrente (R/G/B)
4. Premi pulsante encoder per cambiare canale
5. Rilascia il tasto quando soddisfatto
6. Salva con `SAVE_INTERACTIVE_COLORS` (7+8+9)

---

## Gestione Gesti - Tutorial Completo

### Setup Iniziale

**1. Rimuovi gesti di test precaricati** (tramite web interface)

**2. Configura pulsante per training:**
```json
"BUTTON": ["TRAIN_GESTURE"]
```

### Procedura Addestramento

**3. Addestra un nuovo gesto:**
1. Premi e **tieni premuto** il pulsante encoder
2. Esegui il movimento desiderato (es. swipe a destra)
3. Rilascia il pulsante
4. Entro 5 secondi, premi un numero (1-9):
   - Tasto 1 → G_ID:0
   - Tasto 2 → G_ID:1
   - ...
5. Ripeti lo stesso movimento 3-7 volte premendo lo stesso numero

**4. Ripeti per altri gesti** usando numeri diversi

**5. Converti a binario** (web interface → Gesture Features → "Convert JSON to Binary")

### Assegnazione Azioni

**6. Assegna azioni ai gesti in combo_common.json:**
```json
{
  "G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"],
  "G_ID:1": ["S_B:CTRL+SUPER,LEFT_ARROW"],
  "G_ID:2": ["S_B:SUPER+q"]
}
```

### Esecuzione

**7. Cambia modalità a esecuzione:**
```json
"BUTTON": ["EXECUTE_GESTURE"]
```

**8. Usa i gesti:** Premi pulsante ed esegui movimento

### Suggerimenti Gesti

| Tipo Movimento | Azione Suggerita |
|---------------|------------------|
| Swipe destra | Workspace/Desktop successivo |
| Swipe sinistra | Workspace/Desktop precedente |
| Scuoti (shake) | Chiudi applicazione |
| Su/Giù | Luminosità/Volume |
| Twist/Rotazione | Blocca schermo |

---

## Problemi Comuni e Soluzioni

### BLE non si connette

**Problema:** Windows/smartphone non vede il dispositivo o fallisce accoppiamento

**Soluzioni:**
1. Rimuovi dispositivi Bluetooth duplicati (es. Macropad_esp32_0, _1, ecc.)
2. Usa comando `HOP_BLE_DEVICE` per cambiare MAC address
3. Verifica `enable_BLE: true` in config.json
4. Riavvia il MacroPad con `RESET_ALL`

### Caratteri sbagliati in BLE

**Problema:** Simboli o lettere appaiono diversi da quanto configurato

**Soluzioni:**
1. Verifica layout tastiera sistema (ITA vs US)
2. Usa caratteri escape: `,,` per virgola, `++` per più
3. Cambia modalità ordine tasti con `TOGGLE_KEY_ORDER`

### Gesti non riconosciuti

**Problema:** Il gesto non viene riconosciuto o riconosce quello sbagliato

**Soluzioni:**
1. Calibra accelerometro: `CALIBRATE_SENSOR`
2. Addestra con più campioni (5-7 volte)
3. Rendi i gesti più "estremi" e diversi tra loro
4. Controlla log per vedere gli score KNN
5. Verifica orientamento accelerometro (axisMap/axisDir in config.json)

### Combo non funziona

**Problema:** Premo i tasti ma non succede nulla

**Soluzioni:**
1. Verifica sintassi JSON (virgole, parentesi quadre)
2. Controlla ordine tasti in modalità ordinata
3. Usa `TOGGLE_KEY_ORDER` per cambiare modalità
4. Controlla timeout combo in config.json (`combo_timeout`)
5. Verifica nel log web interface che i tasti vengano rilevati

### LED non funziona

**Problema:** LED non si accende o colori sbagliati

**Soluzioni:**
1. Verifica `"active": true` in `"led"` config.json
2. Controlla pin GPIO corretti
3. Verifica `anode_common` (true/false) in base al tuo LED
4. Testa con `FLASHLIGHT` per vedere se è solo un problema di luminosità

### Deep Sleep non funziona

**Problema:** Dispositivo non va in sleep o non si risveglia

**Soluzioni:**
1. Verifica `sleep_enabled: true` in config.json
2. Controlla `sleep_timeout_ms` (default 50000 = 50s)
3. Se motion wake non funziona, il fallback è il pin encoder (GPIO 2)
4. Verifica accelerometro configurato e `motionWakeEnabled: true`

---

## Specifiche Tecniche

### Software
- **Framework:** Arduino/PlatformIO con FreeRTOS
- **Linguaggio:** C++11
- **File System:** LittleFS (partizione dati)

### Librerie Principali
- ArduinoJson 6.21.3 (configurazione e combo)
- ESP32-BLE-Combo (tastiera/mouse BLE)
- IRremoteESP8266 2.8.6 (controllo infrarossi)
- Adafruit MPU6050 / Arduino-ADXL345 (gesti)
- ESPAsyncWebServer (web interface)
- AsyncTCP (server asincrono)

### Prestazioni
- **Frequenza Scan Keypad:** 5ms (200Hz)
- **Frequenza Campionamento Gesti:** 100Hz (configurabile)
- **Latenza BLE:** ~10-30ms
- **Autonomia Batteria (820mAh LiPo):** ~2 settimane in BLE con sleep
- **Memoria Flash:** ~1.5MB codice + 256KB filesystem
- **RAM Utilizzata:** ~60-80KB (dipende da gesti e combo attivi)

### Limiti
- **Max Combinazioni per file:** ~100-150 (limite JSON in RAM)
- **Gesture supportate:** Swipe sinistra/destra + Shake (MPU6050 usa anche il gyro)
- **Campioni gesture:** ~3s buffer circolare (configurabile)
- **Max Lunghezza Comando Concatenato:** ~500 caratteri
- **WiFi:** Solo 2.4 GHz (limitazione ESP32)

---

## Sviluppo e Contributi

### Struttura Progetto

```
MacroPad_esp32/
├── src/
│   └── main.cpp                    # Punto d'ingresso
├── lib/                            # Librerie custom
│   ├── BLEController/              # Gestione Bluetooth
│   ├── combinationManager/         # Manager combinazioni
│   ├── configManager/              # Caricamento config
│   ├── configWebServer/            # Server web
│   ├── gesture/                    # Sistema riconoscimento gesti
│   │   ├── gestureRead.cpp
│   │   ├── gestureAnalyze.cpp
│   │   ├── SimpleGestureDetector.cpp
│   │   ├── MotionSensor.cpp
│   │   └── ...
│   ├── inputDevice/                # Astrazione input
│   ├── IRManager/                  # Gestione infrarossi
│   │   ├── IRSensor.cpp
│   │   ├── IRSender.cpp
│   │   └── IRStorage.cpp
│   ├── keypad/                     # Scansione matrice tasti
│   ├── Led/                        # Controllo LED RGB
│   ├── Logger/                     # Sistema logging asincrono
│   ├── macroManager/               # Esecuzione comandi
│   ├── powerManager/               # Gestione sleep/wake
│   ├── rotaryEncoder/              # Encoder rotativo
│   ├── specialAction/              # Azioni speciali
│   └── WIFIManager/                # Gestione WiFi
├── data/                           # Filesystem (upload con uploadfs)
│   ├── config.json
│   ├── combo_0.json
│   ├── combo_1.json
│   ├── combo_common.json
│   ├── my_combo_0.json
│   ├── my_combo_1.json
│   ├── ir_data.json
│   ├── gesture.json
│   └── [file HTML web interface]
├── platformio.ini                  # Configurazione build
├── partitions.csv                  # Partizioni flash
└── [documentazione]
```

### Contribuire

Contributi benvenuti! Per contribuire:

1. **Fork** il repository
2. **Crea branch** per la tua feature (`git checkout -b feature/AmazingFeature`)
3. **Commit** le modifiche (`git commit -m 'Add some AmazingFeature'`)
4. **Push** al branch (`git push origin feature/AmazingFeature`)
5. **Apri Pull Request**

**Aree in cui aiutare:**
- Supporto altri board ESP32
- Ottimizzazione memoria gesti
- Nuovi protocolli IR
- Pattern LED avanzati
- Documentazione e traduzioni
- Test su diversi sistemi operativi

### Board Testate

- ✅ LOLIN32 Lite
- ❓ Altre board ESP32 (contribuisci con il tuo test!)

Se testi su altre board, apri una issue/PR con:
- Nome board
- Pin mapping utilizzato
- Eventuali modifiche necessarie
- Risultati test funzionalità

---

## Motivazione

Questo progetto nasce dalla necessità di un numpad personalizzato capace di eseguire funzioni specifiche e complesse. I numpad commerciali mancano della flessibilità e delle opzioni di personalizzazione richieste, portando alla creazione di questa soluzione DIY.

Nel corso dello sviluppo, il progetto si è evoluto ben oltre un semplice numpad, incorporando:
- Riconoscimento gesti con machine learning
- Controllo IR universale
- Sistema di illuminazione interattiva
- Gestione energia intelligente
- Web interface completa

---

## Note Sviluppo AI

Questo progetto è stato sviluppato con l'assistenza di vari modelli AI tra cui GPT-4, Claude, Deepseek, Gemini e Qwen-Coder. Questo approccio ha permesso uno sviluppo rapido e l'implementazione di funzionalità complesse, ma il codice potrebbe contenere pattern non convenzionali o necessitare di ulteriore ottimizzazione.

**Considerazioni:**
- Il codice è funzionante e testato su hardware reale
- Alcune soluzioni potrebbero non seguire best practices standard
- Miglioramenti e refactoring sono sempre benvenuti
- La documentazione è stata curata per facilitare comprensione e modifiche

---

## Problemi Noti

Vedi [TODO.md](TODO.md) per lista completa di problemi e miglioramenti pianificati.

**Principali:**
- Gestione memoria gesti può migliorare (rischio crash con molti campioni)
- Case sensitivity con caratteri speciali in alcune lingue
- Conflitti Shift+carattere con pressioni multiple veloci
- BLE richiede "dimentica dispositivo" dopo cambi MAC multipli
- Alcuni caratteri speciali dipendono dal layout tastiera

---

## Roadmap Futura

**In Sviluppo:**
- [ ] Gestione layout tastiera (ITA/US/DE/ecc.)
- [ ] Pattern LED animati (fade, blink, rainbow)
- [ ] Mouse gyro mode (controllo mouse con accelerometro)
- [ ] Modalità gaming con profili dedicati
- [ ] Esportazione/importazione configurazioni

**Pianificato:**
- [ ] ULP co-processor per wake ultra-low power
- [ ] Display OLED per feedback visivo
- [ ] Supporto macro recording on-the-fly
- [ ] App mobile per configurazione
- [ ] Cloud sync configurazioni

**Da Investigare:**
- [ ] Supporto BLE + WiFi simultaneo
- [ ] Audio feedback con buzzer
- [ ] Espansione a più di 9 tasti
- [ ] Modalità USB HID cablata

---

## Licenza

Questo progetto è rilasciato sotto **GNU General Public License v3.0**.

```
ESP32 MacroPad Project
Copyright (C) 2025 Enrico Mori

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

---

## Disclaimer

A causa del processo di sviluppo, il codice può contenere errori, inefficienze e soluzioni non convenzionali. **Usa questo codice a tuo rischio.**

**Importante:**
- Testa le modifiche su hardware non critico
- Fai backup delle configurazioni prima di aggiornamenti
- Alcuni comandi possono interagire con il sistema in modi imprevisti
- La gestione password WiFi in chiaro richiede precauzioni (cambia password AP!)

---

## Ringraziamenti

- **Community ESP32 e Arduino** - Per le eccellenti librerie e documentazione
- **Tutti gli assistenti AI** - GPT-4, Claude, Deepseek, Gemini, Qwen-Coder
- **Contributors** - A tutti coloro che contribuiranno al progetto
- **I gatti** - Per il supporto morale e il testing involontario della tastiera 🐱

---

## Link Utili

- **Repository:** [github.com/rederyk/MacroPad_esp32](https://github.com/rederyk/MacroPad_esp32)
- **Issues:** [Report bug o richiedi feature](https://github.com/rederyk/MacroPad_esp32/issues)
- **Discussions:** [Discussioni e Q&A](https://github.com/rederyk/MacroPad_esp32/discussions)

---

## Contatti

**Autore:** Enrico Mori
**Anno:** 2025
**Versione Documentazione:** 2.0

---

Made with ❤️, 🤖, and 🐱

**Se questo progetto ti è stato utile, lascia una ⭐ su GitHub!**
