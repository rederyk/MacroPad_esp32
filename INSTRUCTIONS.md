# Guida Completa Setup e Configurazione ESP32 MacroPad

## Indice
- [Introduzione](#introduzione)
- [Prerequisiti](#prerequisiti)
- [Installazione Hardware](#installazione-hardware)
- [Installazione Software](#installazione-software)
- [Configurazione config.json](#configurazione-configjson)
- [Setup WiFi e BLE](#setup-wifi-e-ble)
- [Configurazione Combinazioni](#configurazione-combinazioni)
- [Addestramento Gesti](#addestramento-gesti)
- [Configurazione IR](#configurazione-ir)
- [Gestione LED](#gestione-led)
- [Power Management](#power-management)
- [Web Interface](#web-interface)
- [Troubleshooting](#troubleshooting)

---

## Introduzione

Questa guida ti accompagnerà passo passo nella configurazione completa del tuo MacroPad ESP32, dalla saldatura dei componenti alla configurazione dei gesti personalizzati.

### Cosa Puoi Fare con il MacroPad

- **Controllo Desktop:** Scorciatoie, navigazione workspace, controllo applicazioni
- **Multimedia:** Volume, play/pause, cambio traccia
- **Smart Home:** Controllo dispositivi IR (TV, LED strip, condizionatori)
- **Produttività:** Macro per sviluppatori, automazione task ripetitivi
- **Gaming:** Macro veloci, cambio setup al volo
- **Gesti:** Controllo con movimenti fisici del dispositivo

---

## Prerequisiti

### Hardware Necessario

**Componenti Base (Obbligatori):**
- 1x ESP32 Board (LOLIN32 Lite consigliata, testata)
- 9x Switch Meccanici (Cherry MX compatibili o simili)
- 9x Diodi 1N4148 (o simili diodi switching veloci)
- 1x Encoder Rotativo con pulsante integrato
- Cavi jumper o filo per collegamenti
- Saldatore e stagno
- (Opzionale) Keycaps per switch

**Componenti Opzionali:**
- 1x Accelerometro MPU6050 o ADXL345 (per gesti)
- 1x LED RGB catodo comune (per feedback visivo)
- 1x LED IR 940nm (per trasmissione IR)
- 1x Ricevitore IR TSOP38238 (per ricezione IR)
- 1x Batteria LiPo 3.7V (consigliata 820mAh o superiore)
- 1x Interruttore on/off
- Resistenze appropriate per LED (se necessario)
- Case/enclosure (stampato 3D o custom)

### Software Necessario

- **PlatformIO** - IDE per sviluppo ESP32
  - Installabile come estensione VSCode
  - Oppure PlatformIO Core (CLI)
- **Git** - Per clonare il repository
- **Driver USB** - CP2102 o CH340 (dipende dalla tua board ESP32)

**Installazione PlatformIO (VSCode):**
1. Installa Visual Studio Code
2. Vai su Extensions (Ctrl+Shift+X)
3. Cerca "PlatformIO IDE"
4. Clicca Install
5. Riavvia VSCode

---

## Installazione Hardware

### Schema Keypad 3x3

Il keypad è organizzato in una matrice 3x3 con diodi per prevenire ghosting.

**Layout Fisico:**
```
1  2  3
4  5  6
7  8  9
```

**Schema Elettrico:**
```
        COL0 (17)    COL1 (5)     COL2 (18)
           │            │            │
ROW0 (0)───┤────────────┤────────────┤
           │ [1]        │ [2]        │ [3]
           ▼|           ▼|           ▼|
           ─┘           ─┘           ─┘
           │            │            │
ROW1 (4)───┤────────────┤────────────┤
           │ [4]        │ [5]        │ [6]
           ▼|           ▼|           ▼|
           ─┘           ─┘           ─┘
           │            │            │
ROW2 (16)──┤────────────┤────────────┤
           │ [7]        │ [8]        │ [9]
           ▼|           ▼|           ▼|
           ─┘           ─┘           ─┘

Legenda:
▼| = Diodo (catodo verso righe)
─┘ = Switch
```

**Direzione Diodi:**
- Catodo (banda) → verso le RIGHE (row pins)
- Anodo → verso le COLONNE (col pins)
- Se `invertDirection: true` in config.json, inverti

**Procedura Saldatura Keypad:**
1. Salda i diodi su ogni switch (rispetta polarità!)
2. Collega tutti gli switch di una RIGA insieme
3. Collega tutti gli switch di una COLONNA insieme
4. Collega i fili alle righe → GPIO 0, 4, 16
5. Collega i fili alle colonne → GPIO 17, 5, 18

### Schema Encoder Rotativo

**Connessioni:**
```
Encoder Pin    →  ESP32 GPIO
─────────────────────────────
CLK (A)        →  13
DT (B)         →  15
SW (Button)    →  2
GND            →  GND
+ (VCC)        →  3.3V (se richiesto)
```

**Note:**
- Alcuni encoder hanno pull-up integrati
- Se hai rimbalzi (bouncing), aggiungi condensatori 100nF tra pin e GND
- GPIO 2 viene anche usato come wakeup pin per deep sleep

### Schema LED RGB

**LED RGB Catodo Comune (configurazione di default):**
```
LED Pin    →  ESP32 GPIO    →  Note
───────────────────────────────────────
Rosso      →  25             →  (opzionale resistenza 220Ω)
Verde      →  26             →  (opzionale resistenza 220Ω)
Blu        →  27             →  (opzionale resistenza 220Ø)
Catodo(-)  →  GND            →  Pin comune a massa
```

**LED RGB Anodo Comune:**
Se hai un LED anodo comune, imposta `"anode_common": true` in config.json.

**Note:**
- I GPIO ESP32 forniscono ~40mA max, alcuni LED ad alta luminosità potrebbero richiedere transistor
- Resistenze da 220Ω-330Ω consigliate per protezione
- Il software usa PWM per controllo luminosità e colore

### Schema Accelerometro I2C

**MPU6050 o ADXL345:**
```
Accelerometro    →  ESP32 GPIO
─────────────────────────────
VCC              →  3.3V
GND              →  GND
SDA              →  19
SCL              →  23
INT (opzionale)  →  32 (per motion wake)
```

**Note:**
- Usa sempre 3.3V, NON 5V!
- Alcuni moduli hanno pull-up integrati su SDA/SCL
- Se hai più dispositivi I2C, assicurati che non ci siano conflitti di indirizzo

### Schema IR (Infrarossi)

**IR LED Trasmettitore:**
```
IR LED Pin    →  Connessione
──────────────────────────────
Anodo (+)     →  GPIO 33
Catodo (-)    →  GND (tramite resistenza 100Ω)
```

**Configurazione `anodeGpio: true`:**
Se `anodeGpio: true`, il LED è pilotato direttamente dal GPIO (configurazione di default).

**IR Receiver (TSOP38238):**
```
TSOP Pin     →  ESP32 GPIO
──────────────────────────────
OUT          →  14
GND          →  GND
VCC          →  3.3V
```

**Note:**
- TSOP38238 funziona a 38kHz (standard NEC, Sony, RC5)
- IR LED 940nm consigliato per massima compatibilità
- Portata tipica: 5-10 metri

### Pin Layout Completo LOLIN32 Lite

```
Funzione              GPIO    Note
────────────────────────────────────────────────
Keypad Row 0          0
Keypad Row 1          4
Keypad Row 2          16
Keypad Col 0          17
Keypad Col 1          5
Keypad Col 2          18
Encoder A (CLK)       13
Encoder B (DT)        15
Encoder Button        2       Anche wakeup pin
LED Rosso             25      PWM
LED Verde             26      PWM
LED Blu               27      PWM
IR TX                 33
IR RX                 14
I2C SDA               19      Accelerometro
I2C SCL               23      Accelerometro
Motion Wakeup         32      Opzionale per interrupt accelerometro
GND                   GND     Multipli
3.3V                  3.3V    Multipli
```

**Alimentazione:**
- USB 5V (via porta USB) → Ricarica batteria LiPo automaticamente
- Batteria LiPo 3.7V → Collegata a BAT+/BAT- su LOLIN32 Lite
- Consumo tipico: ~80mA attivo, ~10µA in deep sleep

---

## Installazione Software

### 1. Clona Repository

```bash
# Clona il progetto
git clone https://github.com/rederyk/MacroPad_esp32.git
cd MacroPad_esp32
```

### 2. Apri in PlatformIO

**VSCode + PlatformIO:**
1. Apri VSCode
2. File → Open Folder → Seleziona cartella `MacroPad_esp32`
3. PlatformIO rileverà automaticamente il progetto
4. Attendi installazione dipendenze (prima volta può richiedere 5-10 minuti)

**PlatformIO Core (CLI):**
```bash
cd MacroPad_esp32
pio lib install  # Installa librerie
```

### 3. Verifica platformio.ini

Il file `platformio.ini` dovrebbe contenere:

```ini
[env:lolin32_lite]
platform = espressif32
board = lolin32_lite
framework = arduino
monitor_speed = 115200
board_build.partitions = partitions.csv

lib_deps =
    bblanchon/ArduinoJson@^6.21.3
    IRremoteESP8266@^2.8.6
    adafruit/Adafruit MPU6050@^2.2.4
    ESP32-BLE-Combo
    me-no-dev/AsyncTCP@^1.1.1
    me-no-dev/ESP Async WebServer@^1.2.3
    arduino-libraries/Arduino_ADXL345@^1.0.0

build_flags =
    -DCORE_DEBUG_LEVEL=3
```

### 4. Compila il Progetto

```bash
# Compila
pio run

# Oppure in VSCode: PlatformIO → Build
```

Se la compilazione ha successo, sei pronto per l'upload!

---

## Configurazione config.json

Il file `data/config.json` contiene tutta la configurazione hardware e sistema.

### File Completo con Commenti

```json
{
  "wifi": {
    "ap_ssid": "ESP32_MacroPad",           // Nome rete Access Point
    "ap_password": "my_cat_name123",       // Password AP (CAMBIALA!)
    "router_ssid": "TuoRouterSSID",        // Nome tuo WiFi
    "router_password": "TuaPasswordWiFi"   // Password tuo WiFi
  },

  "system": {
    "ap_autostart": false,                 // Avvia AP automaticamente?
    "router_autostart": true,              // Connetti a router automaticamente?
    "enable_BLE": false,                   // Modalità BLE (true) o WiFi (false)
    "serial_enabled": true,                // Abilita output seriale
    "BleMacAdd": 0,                        // Incremento MAC BLE (0-255)
    "combo_timeout": 100,                  // Timeout combo in ms
    "BleName": "Macropad_esp32",           // Nome dispositivo BLE
    "sleep_enabled": true,                 // Abilita deep sleep
    "sleep_timeout_ms": 50000,             // Timeout sleep (50s)
    "wakeup_pin": 32                       // Pin GPIO per wakeup
  },

  "keypad": {
    "rows": 3,                             // Numero righe
    "cols": 3,                             // Numero colonne
    "rowPins": [0, 4, 16],                 // GPIO righe
    "colPins": [17, 5, 18],                // GPIO colonne
    "keys": [                              // Mappatura tasti
      ["1", "2", "3"],
      ["4", "5", "6"],
      ["7", "8", "9"]
    ],
    "invertDirection": true                // Inverti direzione scansione
  },

  "encoder": {
    "pinA": 13,                            // Pin CLK (A)
    "pinB": 15,                            // Pin DT (B)
    "buttonPin": 2,                        // Pin pulsante
    "stepValue": 1                         // Step per rotazione
  },

  "led": {
    "pinRed": 25,                          // GPIO LED rosso
    "pinGreen": 26,                        // GPIO LED verde
    "pinBlue": 27,                         // GPIO LED blu
    "anode_common": false,                 // LED catodo comune (false) o anodo comune (true)
    "active": true                         // Abilita LED
  },

  "irLed": {
    "pin": 33,                             // GPIO IR TX
    "anodeGpio": true,                     // Pilotaggio diretto GPIO
    "active": true                         // Abilita IR TX
  },

  "irSensor": {
    "pin": 14,                             // GPIO IR RX
    "active": true                         // Abilita IR RX
  },

  "accelerometer": {
    "sdaPin": 19,                          // GPIO I2C SDA
    "sclPin": 23,                          // GPIO I2C SCL
    "sensitivity": 2.0,                    // Sensibilità (2g, 4g, 8g, 16g)
    "sampleRate": 100,                     // Frequenza campionamento (Hz)
    "threshold": 500,                      // Soglia rilevamento movimento
    "axisMap": "yzx",                      // Mappatura assi (xyz, yzx, zxy, ecc.)
    "axisDir": "++-",                      // Direzione assi (+/-)
    "active": true,                        // Abilita accelerometro
    "type": "mpu6050",                     // Tipo: "mpu6050" o "adxl345"
    "address": 104,                        // Indirizzo I2C (104 = 0x68 per MPU6050)

    "motionWakeEnabled": true,             // Abilita wakeup su movimento
    "motionWakeThreshold": 5,              // Soglia movimento per wake
    "motionWakeDuration": 1,               // Durata movimento per wake
    "motionWakeHighPass": 5,               // Filtro high-pass
    "motionWakeCycleRate": 1,              // Frequenza ciclo wake

    "autoCalibrate": true,                 // Auto-calibrazione all'avvio
    "autoCalibrateGyroThreshold": 0.12,    // Soglia gyro per calibrazione
    "autoCalibrateStableSamples": 0,       // Campioni stabili richiesti
    "autoCalibrateSmoothing": 0.05         // Fattore smoothing
  }
}
```

### Sezione `gyromouse`

È stato introdotto un nuovo blocco di configurazione dedicato alla modalità mouse giroscopico:

```json
"gyromouse": {
  "enabled": true,          // Abilita (true) o disabilita (false) la modalità gyromouse
  "smoothing": 0.25,        // Fattore di smoothing esponenziale (0.0–1.0)
  "invertX": false,         // Inverte l'asse X
  "invertY": false,         // Inverte l'asse Y
  "swapAxes": false,        // Scambia X/Y
  "defaultSensitivity": 1,  // Indice della sensibilità di default
  "sensitivities": [        // Elenco profili sensibilità disponibili
    { "name": "Slow",   "scale": 0.6, "deadzone": 1.5 },
    { "name": "Medium", "scale": 1.0, "deadzone": 1.2 },
    { "name": "Fast",   "scale": 1.4, "deadzone": 1.0 }
  ]
}
```

> **Suggerimento:** lascia almeno un profilo nelle `sensitivities` oppure l'inizializzazione verrà ignorata.

### Modalità GyroMouse

- Premi `4+7` per attivare/disattivare la modalità (combinazione definita in `combo_common.json`).
- All'attivazione vengono caricati i bindings dal file `gyromouse_combo_0.json` (tasti 1/2/3/4/6 per i pulsanti del mouse, encoder per lo scroll, tasto `5` per uscire, pulsante encoder per cambiare sensibilità).
- Alla disattivazione il sistema ripristina automaticamente il set di combo precedente.
- Per funzionare correttamente richiede **BLE attivo** e l'accelerometro inizializzato; se BLE è spento viene stampato un warning e la modalità non parte.
- Puoi personalizzare sensibilità, deadzone e mapping modificando sia la sezione `gyromouse` in `config.json` sia il file `data/gyromouse_combo_0.json`.

### Parametri Critici da Verificare

**1. Pin Hardware:**
- Verifica che `rowPins` e `colPins` corrispondano ai tuoi collegamenti
- Controlla `encoder` pins
- Verifica `led` pins se usi LED RGB

**2. WiFi Credentials:**
- **IMPORTANTE:** Cambia `ap_password` per sicurezza!
- Inserisci `router_ssid` e `router_password` corretti

**3. Modalità Avvio:**
- `enable_BLE: false` → Avvia in modalità WiFi
- `enable_BLE: true` → Avvia in modalità Bluetooth
- Puoi cambiare modalità con `TOGGLE_BLE_WIFI` senza modificare file

**4. Accelerometro:**
- Imposta `type` corretto: `"mpu6050"` o `"adxl345"`
- Indirizzo I2C: 104 (0x68) per MPU6050, 83 (0x53) per ADXL345
- Se non usi accelerometro: `"active": false`

**5. Deep Sleep:**
- `sleep_timeout_ms: 50000` → 50 secondi di inattività prima sleep
- `motionWakeEnabled: true` → Risveglio su movimento
- Fallback wakeup su `encoder.buttonPin` (GPIO 2)

---

## Upload Codice e Filesystem

### 1. Upload Codice Principale

```bash
# Connetti ESP32 via USB
# Identifica porta COM (es. /dev/ttyUSB0 su Linux, COM3 su Windows)

# Upload firmware
pio run --target upload

# Oppure in VSCode: PlatformIO → Upload
```

**Primo Upload:**
- Potrebbe essere necessario tenere premuto il pulsante BOOT durante upload
- Su LOLIN32 Lite, di solito il reset è automatico

### 2. Upload Filesystem (IMPORTANTE!)

Il filesystem contiene i file JSON di configurazione e la web interface.

```bash
# Upload filesystem
pio run --target uploadfs

# Oppure in VSCode: PlatformIO → Upload Filesystem Image
```

**Questo comando carica:**
- `data/config.json` → Configurazione sistema
- `data/combo_*.json` → File combinazioni
- `data/ir_data.json` → Database IR
- `data/*.html` → Pagine web interface

**Quando ri-uploadare filesystem:**
- Prima installazione (obbligatorio)
- Dopo modifiche a file in `data/`
- Dopo aggiunta nuovi file HTML

**ATTENZIONE:** Upload filesystem sovrascrive TUTTI i file, inclusi quelli modificati via web interface!

### 3. Monitor Seriale

```bash
# Apri monitor seriale
pio device monitor

# Oppure in VSCode: PlatformIO → Serial Monitor
```

Dovresti vedere:
```
🔹 Logger avviato correttamente!
✓ Configurazione caricata
✓ WiFi AP avviato: ESP32_MacroPad
✓ Indirizzo IP: 192.168.4.1
```

---

## Setup WiFi e BLE

### Modalità WiFi (Default)

**Primo Avvio in WiFi:**

1. **Alimenta il dispositivo** - LED diventa verde (WiFi mode)

2. **Connetti all'Access Point**
   - SSID: `ESP32_MacroPad`
   - Password: `my_cat_name123` (o quella che hai impostato)
   - IP MacroPad: `192.168.4.1`

3. **Accedi alla Web Interface**
   - Apri browser: `http://192.168.4.1`
   - Dovresti vedere la pagina principale

4. **Configura Router WiFi**
   - Vai su "Config" → sezione WiFi
   - Inserisci SSID del tuo router
   - Inserisci password del tuo router
   - Clicca "Save Configuration"
   - Il dispositivo si riavvia

5. **Ottieni Nuovo IP**
   - Riconnettiti all'AP `ESP32_MacroPad`
   - Vai su `http://192.168.4.1`
   - Nella pagina principale, leggi il nuovo IP assegnato dal router
   - Es. "Router IP: 192.168.1.145"

6. **Connetti al Router**
   - Connetti il tuo PC/smartphone al router (WiFi casa)
   - Accedi al MacroPad tramite il nuovo IP: `http://192.168.1.145`

7. **Disabilita AP (Opzionale)**
   - Su "Advanced Settings" → `ap_autostart: false`
   - Salva
   - Il MacroPad si connetterà solo al router all'avvio

### Modalità BLE (Bluetooth)

**Passare a BLE:**

**Metodo 1: Comando da Combo**
```json
"1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"]
```
Premi tasti 1+2+3 + pulsante encoder → Riavvio in BLE

**Metodo 2: Web Interface**
- Advanced Settings → `enable_BLE: true`
- Save → Riavvio automatico

**Metodo 3: File config.json**
- Modifica `data/config.json` → `"enable_BLE": true`
- Upload filesystem: `pio run --target uploadfs`
- Riavvia dispositivo

**Accoppiamento BLE:**

1. **LED diventa BLU** (modalità BLE attiva)

2. **Su Windows:**
   - Settings → Bluetooth & devices
   - Add device → Bluetooth
   - Seleziona "Macropad_esp32"
   - Clicca "Connect"

3. **Su Linux:**
   ```bash
   bluetoothctl
   scan on
   # Attendi "Macropad_esp32"
   pair <MAC_ADDRESS>
   connect <MAC_ADDRESS>
   trust <MAC_ADDRESS>
   ```

4. **Su Android/iOS:**
   - Impostazioni → Bluetooth
   - Cerca dispositivi
   - Tap su "Macropad_esp32"

**Testare BLE:**
- Premi un tasto configurato (es. "1": ["S_B:a"])
- Dovresti vedere il carattere apparire

**Problemi BLE:**
- Se non si connette, usa `HOP_BLE_DEVICE` per cambiare MAC
- Rimuovi dispositivi duplicati da impostazioni Bluetooth
- Alcuni sistemi richiedono "dimenticare dispositivo" dopo cambi MAC

---

## Configurazione Combinazioni

### Struttura File Combo

I file combo sono in `data/`:
- `combo_0.json` - Combo principale
- `combo_1.json` - Secondo set (es. numpad)
- `combo_common.json` - Sempre caricato (gesti, sistema)
- `my_combo_0.json` - Custom con colori interattivi
- `my_combo_1.json` - Secondo custom

### Sintassi Combinazioni

**Tasto Singolo:**
```json
"1": ["S_B:a"]
```

**Tasti Multipli:**
```json
"1+2": ["S_B:CTRL+c"],
"1+5+9": ["CALIBRATE_SENSOR"]
```

**Encoder:**
```json
"CW": ["S_B:VOL_UP"],
"CCW": ["S_B:VOL_DOWN"],
"BUTTON": ["EXECUTE_GESTURE"]
```

**Tasto + Encoder:**
```json
"1,CW": ["S_B:PAGE_UP"],
"3,BUTTON": ["SEND_IR_tv_off"]
```

**Comandi Concatenati:**
```json
"1+5": ["<S_B:SUPER+t><DELAY_500><S_B:htop><DELAY_500><S_B:RETURN>"]
```

**Gesti:**
```json
"G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"]
```

### Modificare Combo via Web Interface

**Metodo Consigliato (Runtime):**

1. Accedi a web interface
2. Vai su "Combo Editor"
3. Seleziona file combo da modificare
4. Clicca su combinazione esistente per modificare
5. Oppure "Add New Combination"
6. Inserisci:
   - **Keys:** es. `1+2` o `3,CW`
   - **Actions:** es. `["S_B:CTRL+c"]`
7. Clicca "Save"
8. Le modifiche sono **immediate**, nessun riavvio necessario!

**Metodo File (Richiede Upload):**

1. Modifica `data/combo_0.json` localmente
2. Upload filesystem: `pio run --target uploadfs`
3. Riavvia dispositivo

### Esempi Combo Pratici

**File combo_0.json completo:**
```json
{
  "1": ["S_B:TAB"],
  "2": ["S_B:UP_ARROW"],
  "3": ["S_B:SUPER"],
  "4": ["S_B:LEFT_ARROW"],
  "5": ["S_B:RETURN"],
  "6": ["S_B:RIGHT_ARROW"],
  "7": ["S_B:BACKSPACE"],
  "8": ["S_B:DOWN_ARROW"],
  "9": ["S_B:SPACE"],

  "1+5": ["<S_B:SUPER+t><S_B:SUPER+w><S_B:SUPER+c>"],
  "2+5": ["S_B:SUPER+q"],
  "4+5": ["S_B:CTRL+SUPER,LEFT_ARROW"],
  "5+6": ["S_B:CTRL+SUPER,RIGHT_ARROW"],

  "CW": ["S_B:MOUSE_MOVE_0_0_1_0"],
  "CCW": ["S_B:MOUSE_MOVE_0_0_-1_0"],
  "1,CW": ["S_B:VOL_UP"],
  "1,CCW": ["S_B:VOL_DOWN"],

  "3,BUTTON": ["SEND_IR_tv_off"],
  "3,CW": ["SEND_IR_tv_volup"],
  "3,CCW": ["SEND_IR_tv_low"],

  "1+6": ["FLASHLIGHT"],
  "6,CW": ["LED_BRIGHTNESS_MINUS"],
  "6,CCW": ["LED_BRIGHTNESS_PLUS"],

  "5+7+9": ["SWITCH_MY_COMBO_0"]
}
```

**File combo_common.json (sempre attivo):**
```json
{
  "BUTTON": ["EXECUTE_GESTURE"],
  "G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"],
  "G_ID:1": ["S_B:CTRL+SUPER,LEFT_ARROW"],
  "G_ID:2": ["S_B:SUPER+q"],

  "1,CW": ["S_B:VOL_UP"],
  "1,CCW": ["S_B:VOL_DOWN"],

  "1+5+9": ["CALIBRATE_SENSOR"],
  "1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"],
  "1+4+7,BUTTON": ["HOP_BLE_DEVICE"],
  "7+8+9,BUTTON": ["RESET_ALL"],
  "1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"]
}
```

### Switching tra Combo Sets

**Comandi Switch:**
- `SWITCH_COMBO_0` → Carica combo_0.json
- `SWITCH_COMBO_1` → Carica combo_1.json
- `SWITCH_MY_COMBO_0` → Carica my_combo_0.json
- `SWITCH_MY_COMBO_1` → Carica my_combo_1.json

**Esempio Utilizzo:**
```json
{
  "5+7+9": ["SWITCH_MY_COMBO_0"],   // Passa a combo interattivo
  "2+5": ["SWITCH_COMBO_0"]          // Torna a combo principale
}
```

**Feedback Visivo:**
Ogni combo può avere il suo colore LED che lampeggia quando switchi:
```json
{
  "_settings": {
    "led_color": [255, 255, 0]  // Giallo
  },
  ...
}
```

---

## Addestramento Gesti

### Setup Iniziale Gesti

**1. Rimuovi Gesti di Test**

I gesti precaricati vanno rimossi prima di addestrare i tuoi:

- Accedi a web interface
- Vai su "Gesture Features"
- Clicca "Clear All Gestures" (o rimuovi singolarmente)

**2. Configura Pulsante Training**

In `combo_common.json`:
```json
"BUTTON": ["TRAIN_GESTURE"]
```

### Procedura Addestramento

**3. Addestra Primo Gesto**

1. **Premi e TIENI PREMUTO** il pulsante encoder
2. **Esegui il movimento** desiderato mentre tieni premuto
   - Es. Muovi il dispositivo rapidamente a destra
3. **Rilascia** il pulsante quando finito movimento
4. **Entro 5 secondi**, premi un tasto numerico:
   - Tasto **1** → Salva come **G_ID:0**
   - Tasto **2** → Salva come **G_ID:1**
   - ...
   - Tasto **9** → Salva come **G_ID:8**

**4. Ripeti lo Stesso Gesto**

Per migliorare il riconoscimento, ripeti il movimento 3-7 volte:

1. Premi BUTTON
2. Esegui STESSO movimento
3. Rilascia BUTTON
4. Premi STESSO numero (es. 1 per G_ID:0)

Ogni ripetizione aggiunge un "campione" che migliora l'accuratezza.

**5. Addestra Altri Gesti**

Ripeti procedura usando numeri diversi:
- Movimento a sinistra → Tasto 2 (G_ID:1)
- Scuotimento → Tasto 3 (G_ID:2)
- ecc.

**6. Converti JSON a Binario**

**FONDAMENTALE prima di eseguire gesti:**

- Web interface → "Gesture Features"
- Clicca "Convert JSON to Binary"
- Attendere conferma "Conversion completed"

Questo processo crea il file binario usato per matching veloce.

### Assegnazione Azioni ai Gesti

**7. Modifica combo_common.json**

Assegna un'azione a ogni G_ID:

```json
{
  "G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"],  // Swipe destra
  "G_ID:1": ["S_B:CTRL+SUPER,LEFT_ARROW"],   // Swipe sinistra
  "G_ID:2": ["S_B:SUPER+q"],                 // Shake
  "G_ID:3": ["S_B:SUPER+l"]                  // Twist
}
```

**8. Cambia Modalità a Esecuzione**

In `combo_common.json`:
```json
"BUTTON": ["EXECUTE_GESTURE"]
```

(Cambia da `TRAIN_GESTURE` a `EXECUTE_GESTURE`)

### Utilizzo Gesti

**Eseguire un Gesto:**
1. Premi BUTTON encoder
2. Esegui movimento
3. Il sistema riconosce il gesto ed esegue l'azione

**Monitorare Riconoscimento:**
- Web interface → "System Actions"
- Nella sezione log vedrai:
  ```
  Gesture recognized: G_ID:0 (score: 0.85)
  ```

**Score KNN:**
- Score vicino a 0 = Match perfetto
- Score < 1.0 = Ottimo
- Score 1.0-2.0 = Buono
- Score > 2.0 = Potrebbe non riconoscere

### Suggerimenti per Gesti Migliori

**Movimenti Efficaci:**
- **Swipe Rapido** (destra/sinistra/su/giù)
- **Scuotimento** (shake)
- **Rotazione** (twist)
- **Cerchio** (circular)

**Best Practices:**
- Rendi i gesti **ben distinti** tra loro
- Movimento **deciso e veloce** (non lento)
- Addestra **5-7 campioni** per gesto
- Mantieni **orientamento consistente** del dispositivo
- Evita gesti troppo **simili** (es. swipe-destra vs swipe-destra-su)

**Troubleshooting Gesti:**

**Gesto non riconosciuto:**
1. Calibra accelerometro: `CALIBRATE_SENSOR`
2. Aggiungi più campioni (3-7 volte)
3. Rendi movimento più "estremo"

**Riconosce gesto sbagliato:**
1. Verifica che i gesti siano abbastanza diversi
2. Controlla log per vedere score degli altri gesti
3. Ricalibrare o rimuovere campioni ambigui

**Orientamento sbagliato:**
1. Modifica `axisMap` e `axisDir` in config.json
2. Es. `"axisMap": "xyz"` → `"axisMap": "yzx"`
3. Es. `"axisDir": "+++"` → `"axisDir": "++-"`

---

## Configurazione IR

### Setup Hardware IR

**1. Verifica Connessioni:**
- IR LED → GPIO 33
- IR Receiver → GPIO 14

**2. Abilita IR in config.json:**
```json
{
  "irLed": { "active": true, "pin": 33 },
  "irSensor": { "active": true, "pin": 14 }
}
```

### Apprendere Comandi IR

**Metodo Buffer Temporaneo:**

1. **Configura combo per scan:**
```json
"1+3": ["SCAN_IR_DEV_1"],  // Impara in buffer 1
"7+9": ["SEND_IR_DEV_1"]   // Invia da buffer 1
```

2. **Procedura Apprendimento:**
- Premi combo scan (es. 1+3)
- LED lampeggia (modalità scan attiva)
- Punta telecomando verso IR receiver
- Premi tasto sul telecomando
- LED conferma ricezione
- Comando salvato in buffer

3. **Test Comando:**
- Premi combo send (es. 7+9)
- Il comando viene inviato
- Dispositivo dovrebbe reagire

**Buffer Disponibili:**
- `SCAN_IR_DEV_1` / `SEND_IR_DEV_1` → Buffer 1
- `SCAN_IR_DEV_2` / `SEND_IR_DEV_2` → Buffer 2

### Salvare Comandi IR Permanenti

**1. Accedi Web Interface**
- Vai su "IR Manager" (o pagina system actions)

**2. Visualizza Ultimo Comando Scansionato**
Il log mostrerà:
```
IR Received: Protocol=NEC, Address=0x0, Command=0xBF40, Bits=32
```

**3. Aggiungi a ir_data.json**

Via web interface o manualmente in `data/ir_data.json`:

```json
{
  "tv": {
    "protocol": "NEC",
    "commands": {
      "off": {
        "protocol": 2,
        "address": 0,
        "command": 47936,
        "bits": 32,
        "repeat": 0
      },
      "volup": {
        "protocol": 2,
        "address": 0,
        "command": 47872,
        "bits": 32,
        "repeat": 0
      }
    }
  },
  "led_strip": {
    "protocol": "NEC",
    "commands": {
      "on": {
        "protocol": 2,
        "address": 0,
        "command": 16203711,
        "bits": 32,
        "repeat": 0
      },
      "off": {
        "protocol": 2,
        "address": 0,
        "command": 16187455,
        "bits": 32,
        "repeat": 0
      }
    }
  }
}
```

**Mappatura Protocol:**
- 0 = UNKNOWN
- 1 = SONY
- 2 = NEC
- 3 = RC5
- 4 = RC6
- (vedi IRremoteESP8266 per lista completa)

**4. Usa Comandi in Combo**

```json
{
  "3,BUTTON": ["SEND_IR_tv_off"],
  "3,CW": ["SEND_IR_tv_volup"],
  "2+7": ["SEND_IR_led_strip_on"],
  "3+7": ["SEND_IR_led_strip_off"]
}
```

Sintassi: `SEND_IR_<device>_<command>`

### Esempi IR Avanzati

**Sequenza Spegnimento Tutto:**
```json
"4+5+6": ["<SEND_IR_tv_off><DELAY_1000><SEND_IR_led_strip_off><DELAY_500><LED_OFF>"]
```

**Toggle con Encoder:**
```json
"9,CW": ["SEND_IR_tv_volup"],
"9,CCW": ["SEND_IR_tv_voldown"],
"9,BUTTON": ["SEND_IR_tv_mute"]
```

---

## Gestione LED

### Controllo Base LED

**Comandi Disponibili:**
```json
"4+6": ["LED_OFF"],                    // Spegni
"3+6": ["LED_RGB_255_0_255"],         // Magenta
"1+6": ["FLASHLIGHT"],                 // Torcia (bianco max)
"6,CW": ["LED_BRIGHTNESS_MINUS"],      // Luminosità -
"6,CCW": ["LED_BRIGHTNESS_PLUS"]       // Luminosità +
```

**Sintassi Colore Custom:**
```
LED_RGB_R_G_B
```
Dove R, G, B sono valori 0-255.

**Esempi Colori:**
```json
"1+6": ["LED_RGB_255_0_0"],       // Rosso
"2+6": ["LED_RGB_0_255_0"],       // Verde
"3+6": ["LED_RGB_0_0_255"],       // Blu
"4+6": ["LED_RGB_255_255_0"],     // Giallo
"5+6": ["LED_RGB_0_255_255"],     // Ciano
"6+6": ["LED_RGB_255_0_255"],     // Magenta
"7+6": ["LED_RGB_255_127_0"],     // Arancione
"8+6": ["LED_RGB_127_0_255"],     // Viola
"9+6": ["LED_RGB_255_255_255"]    // Bianco
```

### Modalità Reactive Lighting

**Cos'è:**
Modalità interattiva dove ogni tasto ha il suo colore. Premendo un tasto, il LED mostra quel colore.

**Attivazione:**
```json
"1+2": ["REACTIVE_LIGHTING"]
```

**Configurazione Colori Iniziale:**

In `my_combo_0.json`:
```json
{
  "_settings": {
    "led_color": [255, 255, 0],      // Colore alla selezione combo
    "interactive_colors": [
      [255, 0, 0],      // Tasto 1
      [255, 127, 0],    // Tasto 2
      [255, 255, 0],    // Tasto 3
      [0, 255, 0],      // Tasto 4
      [0, 255, 255],    // Tasto 5
      [0, 0, 255],      // Tasto 6
      [127, 0, 255],    // Tasto 7
      [255, 0, 255],    // Tasto 8
      [255, 255, 255]   // Tasto 9
    ]
  }
}
```

**Modificare Colori Interattivi:**

1. Attiva modalità reactive: premi 1+2
2. **Tieni premuto** il tasto di cui vuoi cambiare colore (es. tasto 5)
3. **Ruota encoder** per modificare canale corrente (R, G o B)
4. **Premi pulsante encoder** per cambiare canale
   - Prima pressione: modifica Rosso
   - Seconda pressione: modifica Verde
   - Terza pressione: modifica Blu
   - Quarta pressione: torna a Rosso
5. **Rilascia il tasto** quando soddisfatto del colore
6. Ripeti per altri tasti

**Salvare Colori:**
```json
"7+8+9": ["SAVE_INTERACTIVE_COLORS"]
```

Questo salva i colori modificati in `my_combo_0.json`.

### Colori Sistema

Il LED mostra automaticamente colori di sistema:
- **Magenta** → Avvio/inizializzazione
- **Blu** → Modalità BLE attiva
- **Verde** → Modalità WiFi attiva
- **Rosso** → Errore

Questi colori sono temporanei (150-300ms) e poi tornano al colore normale.

---

## Power Management

### Configurazione Deep Sleep

**Parametri in config.json:**
```json
{
  "system": {
    "sleep_enabled": true,          // Abilita sleep
    "sleep_timeout_ms": 50000,      // 50 secondi inattività
    "wakeup_pin": 32                // GPIO per wakeup alternativo
  },
  "accelerometer": {
    "motionWakeEnabled": true,      // Wake su movimento
    "motionWakeThreshold": 5,       // Sensibilità movimento
    "motionWakeDuration": 1         // Durata movimento per wake
  }
}
```

### Entrare in Deep Sleep

**Automatico:**
- Dopo `sleep_timeout_ms` senza input → deep sleep automatico

**Manuale:**
```json
"1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"]
```

### Wake-up da Deep Sleep

**Metodi Disponibili:**

1. **Motion Wake (MPU6050):**
   - Muovi/scuoti il dispositivo
   - Accelerometro rileva movimento e sveglia ESP32

2. **Encoder Button (Fallback):**
   - Se motion wake non disponibile
   - Premi pulsante encoder (GPIO 2)

3. **GPIO Esterno:**
   - Puoi usare `wakeup_pin` per wake manuale
   - Collega pulsante a `wakeup_pin` e GND

**Consumo Energetico:**
- Attivo WiFi: ~80-100mA
- Attivo BLE: ~50-70mA
- Deep Sleep: ~10µA (con motion wake: ~200µA)

**Autonomia Batteria 820mAh:**
- WiFi attivo continuo: ~8-10 ore
- BLE attivo continuo: ~12-16 ore
- Deep sleep con wake: ~2-3 settimane

---

## Web Interface

### Pagine Disponibili

**Pagina Principale (index.html):**
- Status sistema
- IP addresses
- Modalità attiva (BLE/WiFi)
- Link alle altre pagine

**Combo Editor (combo.html):**
- Visualizza combinazioni correnti
- Aggiungi nuove combinazioni
- Modifica combinazioni esistenti
- Elimina combinazioni
- Cambia file combo attivo

**System Actions (special_actions.html):**
- Log live sistema
- Test azioni speciali
- Monitoraggio gesti (score KNN)
- Status componenti

**Gesture Features:** non più configurabili – i recognizer utilizzano set predefiniti.

**Config Editor (config.html):**
- Modifica config.json
- Salva configurazione
- Riavvia dispositivo

**Advanced Settings (advanced.html):**
- Parametri sistema avanzati
- Modalità debug
- Gestione file

**IR Manager:**
- Database comandi IR
- Aggiungi/rimuovi comandi
- Test invio comandi

### Accesso Web Interface

**Modalità WiFi:**
1. Connetti a router → `http://<IP_ASSEGNATO>`
2. Oppure ad AP → `http://192.168.4.1`

**Modalità BLE:**
Non disponibile (BLE non ha networking IP)
Passa a WiFi per usare web interface.

---

## Troubleshooting

### Problemi Compilazione

**Errore: Library not found**
```bash
pio lib install
```

**Errore: Partition table**
Verifica che `board_build.partitions = partitions.csv` sia in platformio.ini

### Problemi Upload

**Errore: Failed to connect**
- Tieni premuto BOOT durante upload
- Verifica porta COM corretta
- Controlla driver USB installati

**Errore: Timeout**
- Riduci baud rate in platformio.ini: `upload_speed = 115200`
- Verifica cavo USB (dati, non solo alimentazione)

### Problemi Hardware

**Tasti non rispondono:**
- Verifica saldature keypad
- Controlla direzione diodi
- Verifica pin in config.json corrispondano a collegamenti

**Encoder non funziona:**
- Verifica pin A, B, Button
- Controlla se serve pull-up esterno
- Testa con `IR_CHECK` o log seriale

**LED non si accende:**
- Verifica `"active": true`
- Controlla `anode_common` true/false
- Verifica GPIO corretti
- Testa con `FLASHLIGHT`

**Accelerometro non rilevato:**
- Verifica I2C address (104 per MPU6050, 83 per ADXL345)
- Controlla saldature SDA/SCL
- Testa con `CALIBRATE_SENSOR`
- Verifica 3.3V, NON 5V!

**IR non funziona:**
- Verifica polarità LED IR
- Controlla TSOP riceve 3.3V
- Testa con `IR_CHECK`
- Distanza massima receiver: 5-10m

### Problemi Connettività

**WiFi non si connette al router:**
- Verifica SSID e password in config.json
- Controlla che router sia 2.4GHz (non 5GHz)
- Verifica segnale WiFi sufficiente
- Prova a disabilitare firewall temporaneamente

**BLE non accoppia:**
- Rimuovi dispositivi duplicati da settings Bluetooth
- Usa `HOP_BLE_DEVICE` per nuovo MAC
- Riavvia Bluetooth su PC/smartphone
- Verifica che `enable_BLE: true`

### Problemi Funzionali

**Combo non funziona:**
- Verifica sintassi JSON
- Controlla che i tasti premuti siano nell'ordine corretto (se modalità ordinata)
- Usa `TOGGLE_KEY_ORDER` per cambiare modalità
- Monitora log in web interface per vedere eventi

**Gesti non riconosciuti:**
- Calibra: `CALIBRATE_SENSOR`
- Aggiungi più campioni (5-7)
- Controlla score KNN in log
- Verifica `axisMap` e `axisDir`

**Deep sleep non funziona:**
- Verifica `sleep_enabled: true`
- Controlla timeout non sia troppo lungo
- Se motion wake non funziona, usa encoder button

**Caratteri sbagliati in BLE:**
- Problema layout tastiera (ITA vs US)
- Usa escape: `,,` per virgola, `++` per più
- Alcuni caratteri speciali dipendono da layout

### Reset Completo

**Se tutto va male:**

1. **Erase Flash Completo:**
```bash
pio run --target erase
```

2. **Re-upload Tutto:**
```bash
pio run --target upload
pio run --target uploadfs
```

3. **Reset Configurazione:**
- Modifica `data/config.json` ai valori di default
- Re-upload filesystem

---

## Prossimi Passi

Dopo aver completato setup:

1. **Personalizza le Combo** per il tuo workflow
2. **Addestra Gesti** per azioni frequenti
3. **Configura IR** per dispositivi smart home
4. **Crea Profili Multipli** con combo switching
5. **Sperimenta LED Interattivi** con colori personalizzati

**Risorse:**
- [COMANDI.md](COMANDI.md) - Riferimento completo comandi
- [README.md](README.md) - Panoramica progetto
- [COMBO_SETTINGS.md](COMBO_SETTINGS.md) - Configurazione combo avanzata

---

**Buon divertimento con il tuo MacroPad ESP32!** 🎮🌈

Se hai problemi, apri una issue su GitHub: https://github.com/rederyk/MacroPad_esp32/issues
