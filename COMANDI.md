# Documentazione Completa Comandi MacroPad ESP32

## Indice
- [Introduzione](#introduzione)
- [Sintassi delle Combinazioni](#sintassi-delle-combinazioni)
- [Comandi Bluetooth (S_B:)](#comandi-bluetooth-s_b)
  - [Tasti Modificatori](#tasti-modificatori)
  - [Tasti di Navigazione](#tasti-di-navigazione)
  - [Tasti Funzione](#tasti-funzione)
  - [Tasti Speciali](#tasti-speciali)
  - [Tasti Multimediali](#tasti-multimediali)
  - [Mouse](#mouse)
  - [Caratteri e Testo](#caratteri-e-testo)
- [Azioni di Sistema](#azioni-di-sistema)
- [Azioni LED](#azioni-led)
- [Azioni Infrarossi (IR)](#azioni-infrarossi-ir)
- [Azioni Gesti](#azioni-gesti)
- [Azioni Combo Switching](#azioni-combo-switching)
- [Comandi Concatenati](#comandi-concatenati)
- [Esempi Pratici](#esempi-pratici)

---

## Introduzione

Il MacroPad ESP32 supporta una vasta gamma di comandi configurabili attraverso combinazioni di tasti. Ogni combinazione può essere associata a uno o più comandi che vengono eseguiti in sequenza.

### Come Funziona
- I comandi sono definiti nei file JSON (`combo_0.json`, `combo_1.json`, `my_combo_0.json`, ecc.)
- Ogni combinazione di tasti può eseguire azioni Bluetooth, azioni di sistema, controlli LED, comandi IR o gesti
- I comandi possono essere concatenati usando la sintassi `<comando1><comando2>`
- È possibile aggiungere delay tra i comandi con `<DELAY_ms>`

---

## Sintassi delle Combinazioni

### Formato Base
```json
{
  "combinazione": ["azione1", "azione2", ...]
}
```

### Tipi di Combinazioni

#### 1. Tasto Singolo
```json
"1": ["S_B:a"]
"5": ["S_B:RETURN"]
```

#### 2. Tasti Multipli (Premuti Simultaneamente)
```json
"1+2": ["S_B:CTRL+c"]
"1+5+9": ["CALIBRATE_SENSOR"]
"1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"]
```
**Nota:** I tasti devono essere premuti insieme, l'ordine nella stringa non importa in modalità non-ordinata.

#### 3. Encoder Rotativo
```json
"CW": ["S_B:VOL_UP"]          // Rotazione oraria
"CCW": ["S_B:VOL_DOWN"]       // Rotazione antioraria
```

#### 4. Tasto + Encoder
```json
"1,CW": ["S_B:PAGE_UP"]       // Tasto 1 + rotazione oraria
"3,CCW": ["SEND_IR_tv_low"]   // Tasto 3 + rotazione antioraria
```

#### 5. Pulsante Encoder
```json
"BUTTON": ["EXECUTE_GESTURE"]            // Solo pulsante encoder
"1,BUTTON": ["TOGGLE_BLE_WIFI"]          // Tasto 1 + pulsante encoder
"1+2+3,BUTTON": ["RESET_ALL"]            // Tasti multipli + pulsante
```

#### 6. Gesti
```json
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
"G_ARC": ["S_B:CTRL+P"]
```

---

## Comandi Bluetooth (S_B:)

Tutti i comandi che iniziano con `S_B:` vengono inviati al dispositivo connesso via Bluetooth.

### Sintassi Generale
```
S_B:<tasto1>+<tasto2>,<tasto3>
```

**IMPORTANTE - Caratteri Speciali:**
- Per inserire una virgola letterale: `,,` (doppia virgola)
- Per inserire un più letterale: `++` (doppio più)
- Per inserire uno spazio: `S_B:SPACE` oppure `S_B: ` (spazio dopo i due punti)

**Esempi:**
```json
"3+5": ["<S_B:parola ><S_B:Virgola,, e piu ++ escape con ,,,, o ++++ >"]
```

---

### Tasti Modificatori

| Comando | Descrizione |
|---------|-------------|
| `S_B:CTRL` | Control sinistro |
| `S_B:SHIFT` | Shift sinistro |
| `S_B:ALT` | Alt sinistro |
| `S_B:SUPER` | Windows/Super (tasto Windows o Cmd su Mac) |
| `S_B:RIGHT_CTRL` | Control destro |
| `S_B:RIGHT_SHIFT` | Shift destro |
| `S_B:RIGHT_ALT` | Alt destro (AltGr) |
| `S_B:RIGHT_GUI` | Windows/Super destro |

**Esempi di Combinazioni con Modificatori:**
```json
"1": ["S_B:CTRL+c"]                    // Copia (Ctrl+C)
"2": ["S_B:CTRL+v"]                    // Incolla (Ctrl+V)
"3": ["S_B:CTRL+ALT,DELETE"]           // Ctrl+Alt+Delete
"4": ["S_B:CTRL+SUPER,LEFT_ARROW"]     // Ctrl+Super+Freccia Sinistra
"5": ["S_B:SHIFT+RIGHT_ARROW"]         // Shift+Freccia Destra (selezione)
```

---

### Tasti di Navigazione

| Comando | Descrizione |
|---------|-------------|
| `S_B:UP_ARROW` | Freccia su ↑ |
| `S_B:DOWN_ARROW` | Freccia giù ↓ |
| `S_B:LEFT_ARROW` | Freccia sinistra ← |
| `S_B:RIGHT_ARROW` | Freccia destra → |
| `S_B:PAGE_UP` | Pagina su |
| `S_B:PAGE_DOWN` | Pagina giù |
| `S_B:HOME` | Home (inizio riga) |
| `S_B:END` | End (fine riga) |

**Esempi:**
```json
"2": ["S_B:UP_ARROW"]
"8": ["S_B:DOWN_ARROW"]
"4": ["S_B:LEFT_ARROW"]
"6": ["S_B:RIGHT_ARROW"]
"CW": ["S_B:PAGE_UP"]
"CCW": ["S_B:PAGE_DOWN"]
```

---

### Tasti Funzione

| Comando | Descrizione |
|---------|-------------|
| `S_B:F1` - `S_B:F12` | Tasti funzione F1-F12 |
| `S_B:F13` - `S_B:F24` | Tasti funzione estesi F13-F24 |

**Esempi:**
```json
"1": ["S_B:F1"]
"1+2": ["S_B:F5"]         // Ricarica pagina
"1+5": ["S_B:F11"]        // Fullscreen
```

---

### Tasti Speciali

| Comando | Descrizione |
|---------|-------------|
| `S_B:RETURN` | Invio/Enter ↵ |
| `S_B:ESC` | Escape |
| `S_B:BACKSPACE` | Cancella (←) |
| `S_B:TAB` | Tabulazione |
| `S_B:SPACE` | Spazio |
| `S_B:DELETE` | Canc/Delete |
| `S_B:INSERT` | Insert |
| `S_B:CAPS_LOCK` | Blocca maiuscole |
| `S_B:PRINT_SCREEN` | Stampa schermo |
| `S_B:SCROLL_LOCK` | Blocca scorrimento |
| `S_B:PAUSE` | Pausa |
| `S_B:NUM_LOCK` | Blocco numerico |

**Esempi:**
```json
"1": ["S_B:TAB"]
"5": ["S_B:RETURN"]
"7": ["S_B:BACKSPACE"]
"9": ["S_B:SPACE"]
"1+2": ["S_B:ESC"]
```

---

### Tasti Multimediali

| Comando | Descrizione |
|---------|-------------|
| `S_B:VOL_UP` | Volume su 🔊 |
| `S_B:VOL_DOWN` | Volume giù 🔉 |
| `S_B:MUTE` | Muto 🔇 |
| `S_B:PLAY_PAUSE` | Play/Pausa ⏯ |
| `S_B:STOP` | Stop ⏹ |
| `S_B:NEXT_TRACK` | Traccia successiva ⏭ |
| `S_B:PREVIOUS_TRACK` | Traccia precedente ⏮ |
| `S_B:WWW_HOME` | Home page browser 🏠 |
| `S_B:WWW_SEARCH` | Ricerca web 🔍 |
| `S_B:WWW_BOOKMARKS` | Preferiti browser 📑 |
| `S_B:WWW_BACK` | Indietro browser ← |
| `S_B:WWW_STOP` | Stop browser |
| `S_B:CALCULATOR` | Apri calcolatrice 🧮 |
| `S_B:EMAIL_READER` | Apri client email 📧 |
| `S_B:LOCAL_MACHINE_BROWSER` | Esplora risorse 📁 |

**Esempi:**
```json
"1,CW": ["S_B:VOL_UP"]
"1,CCW": ["S_B:VOL_DOWN"]
"2": ["S_B:PLAY_PAUSE"]
"3": ["S_B:NEXT_TRACK"]
"4": ["S_B:PREVIOUS_TRACK"]
"5": ["S_B:MUTE"]
```

---

### Mouse

#### Pulsanti Mouse

| Comando | Descrizione |
|---------|-------------|
| `S_B:MOUSE_LEFT` | Click sinistro |
| `S_B:MOUSE_RIGHT` | Click destro |
| `S_B:MOUSE_MIDDLE` | Click centrale (rotella) |
| `S_B:MOUSE_BACK` | Pulsante "Indietro" |
| `S_B:MOUSE_FORWARD` | Pulsante "Avanti" |

**Esempi:**
```json
"1": ["S_B:MOUSE_LEFT"]
"3": ["S_B:MOUSE_RIGHT"]
"2": ["S_B:MOUSE_MIDDLE"]
```

#### Movimento Mouse

**Sintassi:**
```
S_B:MOUSE_MOVE_X_Y_WHEEL_HWHEEL
```

**Parametri:**
- `X`: Movimento orizzontale (-127 a +127, positivo = destra)
- `Y`: Movimento verticale (-127 a +127, positivo = giù)
- `WHEEL`: Scroll verticale (-127 a +127, positivo = su)
- `HWHEEL`: Scroll orizzontale (-127 a +127, positivo = destra)

**Esempi:**
```json
"CW": ["S_B:MOUSE_MOVE_0_0_1_0"]        // Scroll su
"CCW": ["S_B:MOUSE_MOVE_0_0_-1_0"]      // Scroll giù
"1,CW": ["S_B:MOUSE_MOVE_0_0_5_0"]      // Scroll veloce su
"1,CCW": ["S_B:MOUSE_MOVE_0_0_-5_0"]    // Scroll veloce giù
"4": ["S_B:MOUSE_MOVE_-10_0_0_0"]       // Sposta mouse a sinistra
"6": ["S_B:MOUSE_MOVE_10_0_0_0"]        // Sposta mouse a destra
"2": ["S_B:MOUSE_MOVE_0_-10_0_0"]       // Sposta mouse in alto
"8": ["S_B:MOUSE_MOVE_0_10_0_0"]        // Sposta mouse in basso
```

---

### Caratteri e Testo

Puoi inviare qualsiasi carattere singolo o stringa di testo.

**Caratteri Singoli:**
```json
"1": ["S_B:a"]
"2": ["S_B:b"]
"3": ["S_B:1"]
"4": ["S_B:@"]
```

**Stringhe di Testo:**
```json
"1+5": ["<S_B:Hello><S_B:SPACE><S_B:World>"]
"2+5": ["<S_B:username@email,,com>"]           // Nota: ,, per virgola letterale
"3+5": ["<S_B:C++>"]                           // Nota: ++ per più letterale
```

**Note Importanti:**
- Le maiuscole vengono gestite automaticamente con SHIFT
- Alcuni caratteri speciali potrebbero richiedere la sintassi di escape
- Il layout della tastiera influisce sui caratteri speciali

---

## Azioni di Sistema

### Gestione Dispositivo

| Comando | Descrizione |
|---------|-------------|
| `RESET_ALL` | Riavvia il dispositivo ESP32 |
| `TOGGLE_BLE_WIFI` | Cambia modalità tra Bluetooth e WiFi (con riavvio) |
| `HOP_BLE_DEVICE` | Cambia MAC address Bluetooth (incrementa BleMacAdd) |
| `TOGGLE_AP` | Attiva/disattiva Access Point WiFi |
| `ENTER_SLEEP` | Entra in modalità deep sleep |
| `MEM_INFO` | Mostra informazioni sulla memoria (log) |
| `PRINT_JSON` | Stampa configurazione corrente nel log |
| `TOGGLE_KEY_ORDER` | Cambia modalità rilevamento ordine tasti |

**Esempi:**
```json
"7+8+9,BUTTON": ["RESET_ALL"]
"1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"]
"1+4+7,BUTTON": ["HOP_BLE_DEVICE"]
"1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"]
"1+9": ["MEM_INFO"]
"4+5+6,BUTTON": ["TOGGLE_KEY_ORDER"]
```

**Note:**
- `TOGGLE_BLE_WIFI`: Il dispositivo si riavvia dopo il cambio modalità
- `HOP_BLE_DEVICE`: Utile se hai problemi di connessione, crea un nuovo indirizzo MAC
- `ENTER_SLEEP`: Il dispositivo si risveglia con movimento (accelerometro) o encoder
- `TOGGLE_KEY_ORDER`: Cambia tra modalità ordinata e non ordinata per i tasti

---

## Azioni LED

### Controllo Colore LED RGB

| Comando | Descrizione |
|---------|-------------|
| `LED_OFF` | Spegne il LED |
| `LED_RGB_R_G_B` | Imposta colore RGB specifico (R, G, B da 0 a 255) |
| `LED_BRIGHTNESS_PLUS` | Aumenta luminosità |
| `LED_BRIGHTNESS_MINUS` | Diminuisce luminosità |
| `FLASHLIGHT` | Modalità torcia (bianco al massimo) |
| `REACTIVE_LIGHTING` | Modalità illuminazione interattiva |
| `SAVE_INTERACTIVE_COLORS` | Salva i colori interattivi nel file combo |

**Esempi di Colori:**
```json
"3+6": ["LED_RGB_255_0_255"]        // Magenta
"1+6": ["LED_RGB_255_255_255"]      // Bianco (o usa FLASHLIGHT)
"2+6": ["LED_RGB_255_0_0"]          // Rosso
"4+6": ["LED_RGB_0_255_0"]          // Verde
"5+6": ["LED_RGB_0_0_255"]          // Blu
"6+6": ["LED_RGB_255_255_0"]        // Giallo
"7+6": ["LED_RGB_0_255_255"]        // Ciano
"8+6": ["LED_RGB_255_127_0"]        // Arancione
"4+6": ["LED_OFF"]                  // Spegni
```

**Controllo Luminosità:**
```json
"6,CW": ["LED_BRIGHTNESS_MINUS"]     // Encoder + tasto 6: diminuisci
"6,CCW": ["LED_BRIGHTNESS_PLUS"]     // Encoder + tasto 6: aumenta
```

### Modalità Illuminazione Interattiva

La modalità **REACTIVE_LIGHTING** permette di associare un colore a ogni tasto del keypad.

**Come Funziona:**
1. Attiva la modalità con `REACTIVE_LIGHTING`
2. Il LED mostra il colore del tasto premuto
3. Per modificare i colori:
   - Tieni premuto un tasto
   - Ruota l'encoder per modificare il canale corrente (R/G/B)
   - Premi il pulsante dell'encoder per cambiare canale
4. Salva i colori con `SAVE_INTERACTIVE_COLORS`

**Esempio Configurazione:**
```json
"1+2": ["REACTIVE_LIGHTING"]           // Attiva modalità interattiva
"7+8+9": ["SAVE_INTERACTIVE_COLORS"]   // Salva colori personalizzati
```

**File my_combo_0.json con Colori Personalizzati:**
```json
{
  "_settings": {
    "led_color": [255, 255, 0],
    "interactive_colors": [
      [255, 0, 0],      // Tasto 1: Rosso
      [255, 127, 0],    // Tasto 2: Arancione
      [255, 255, 0],    // Tasto 3: Giallo
      [0, 255, 0],      // Tasto 4: Verde
      [0, 255, 255],    // Tasto 5: Ciano
      [0, 0, 255],      // Tasto 6: Blu
      [127, 0, 255],    // Tasto 7: Viola
      [255, 0, 255],    // Tasto 8: Magenta
      [255, 255, 255]   // Tasto 9: Bianco
    ]
  }
}
```

---

## Azioni Infrarossi (IR)

Il MacroPad può imparare e inviare comandi infrarossi per controllare TV, condizionatori, LED strip e altri dispositivi.

### Comandi IR

| Comando | Descrizione |
|---------|-------------|
| `SCAN_IR_DEV_1` | Impara comando IR nel buffer 1 |
| `SCAN_IR_DEV_2` | Impara comando IR nel buffer 2 |
| `SEND_IR_DEV_1` | Invia comando IR dal buffer 1 |
| `SEND_IR_DEV_2` | Invia comando IR dal buffer 2 |
| `SEND_IR_<device>_<command>` | Invia comando specifico da ir_data.json |
| `IR_CHECK` | Mostra stato attuale IR (log) |

**Esempi Base:**
```json
"1+3": ["SCAN_IR_DEV_1"]         // Impara comando (punta telecomando e premi tasto)
"7+9": ["SEND_IR_DEV_1"]         // Invia comando imparato

"2+3": ["SCAN_IR_DEV_2"]         // Secondo buffer
"8+9": ["SEND_IR_DEV_2"]
```

### Comandi IR Pre-configurati

I comandi IR possono essere salvati in `ir_data.json` e richiamati con la sintassi:
```
SEND_IR_<nome_device>_<nome_comando>
```

**Esempio ir_data.json:**
```json
{
  "tv": {
    "protocol": "NEC",
    "commands": {
      "off": {"protocol": 2, "address": 0, "command": 0xBF40, "bits": 32},
      "volup": {"protocol": 2, "address": 0, "command": 0xBF00, "bits": 32},
      "low": {"protocol": 2, "address": 0, "command": 0xBF01, "bits": 32}
    }
  },
  "led": {
    "protocol": "NEC",
    "commands": {
      "on": {"protocol": 2, "address": 0, "command": 0xF7C03F, "bits": 32},
      "off": {"protocol": 2, "address": 0, "command": 0xF740BF, "bits": 32}
    }
  }
}
```

**Utilizzo nei Combo:**
```json
"3,BUTTON": ["SEND_IR_tv_off"]       // Spegni TV
"3,CW": ["SEND_IR_tv_volup"]         // Alza volume TV
"3,CCW": ["SEND_IR_tv_low"]          // Abbassa volume TV
"2+7": ["SEND_IR_led_on"]            // Accendi LED strip
"3+7": ["SEND_IR_led_off"]           // Spegni LED strip
```

**Workflow Completo:**
1. Usa `SCAN_IR_DEV_1` per imparare un comando
2. Usa `SEND_IR_DEV_1` per testarlo
3. Accedi alla web interface, sezione IR
4. Salva il comando con un nome nel file `ir_data.json`
5. Usa `SEND_IR_device_command` nei tuoi combo

---

## Azioni Gesti

Il MacroPad supporta il riconoscimento gesti personalizzati usando l'accelerometro.

### Comandi Gesti

| Comando | Descrizione |
|---------|-------------|
| `TRAIN_GESTURE` | Modalità addestramento gesto (solo recognizer legacy) |
| `EXECUTE_GESTURE` | Esegui gesto riconosciuto |
| `CALIBRATE_SENSOR` | Calibra accelerometro |
| `TOGGLE_SAMPLING` | Attiva/disattiva campionamento sensore |

**Esempi:**
```json
"BUTTON": ["TRAIN_GESTURE"]          // Addestra nuovo gesto (solo se supportato)
"BUTTON": ["EXECUTE_GESTURE"]        // Esegui gesto
"1+5+9": ["CALIBRATE_SENSOR"]        // Calibra accelerometro
```

### Come Addestrare un Gesto

> Disponibile solo se il recognizer corrente supporta il training (modalità legacy KNN). Con i recognizer predefiniti per MPU6050/ADXL345 il comando `TRAIN_GESTURE` viene ignorato.

**1. Configurazione Iniziale:**
```json
"BUTTON": ["TRAIN_GESTURE"]
```

**2. Procedura di Addestramento:**
1. Premi e **tieni premuto** il pulsante encoder
2. Esegui il movimento desiderato mentre tieni premuto
3. Rilascia il pulsante
4. Entro 5 secondi, premi un numero sul keypad (1-9)
   - Tasto `1` = Gesto ID 0 (`G_ID:0`)
   - Tasto `2` = Gesto ID 1 (`G_ID:1`)
   - ecc.
5. Ripeti il movimento 3-7 volte per lo stesso ID per migliorare il riconoscimento

**3. Conversione a Binario:**
- Accedi alla web interface
- Vai alla pagina "Gesture Features"
- Clicca "Convert JSON to Binary"

**4. Assegnazione Azione al Gesto:**
```json
"G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"]   // Workspace a destra
"G_ID:1": ["S_B:CTRL+SUPER,LEFT_ARROW"]    // Workspace a sinistra
"G_ID:2": ["S_B:SUPER+q"]                  // Chiudi applicazione
"G_ID:3": ["S_B:SUPER+l"]                  // Blocca schermo
```

**5. Esecuzione Gesto:**
```json
"BUTTON": ["EXECUTE_GESTURE"]
```
Ora premi il pulsante ed esegui il movimento: il MacroPad riconoscerà il gesto e eseguirà l'azione associata.

### Esempi di Gesti Comuni

| Gesto | Comando Suggerito |
|-------|-------------------|
| Movimento a destra | `S_B:CTRL+SUPER,RIGHT_ARROW` (desktop/workspace) |
| Movimento a sinistra | `S_B:CTRL+SUPER,LEFT_ARROW` |
| Scuotimento | `S_B:SUPER+q` (chiudi app) |
| Su e giù | `S_B:SUPER+l` (blocca schermo) |
| Rotazione | `S_B:CTRL+ALT,TAB` (cambia finestra) |

---

## Azioni Combo Switching

Puoi passare tra diversi set di combinazioni al volo.

### Comandi Switch

| Comando | Descrizione |
|---------|-------------|
| `SWITCH_COMBO_0` | Carica combo_0.json |
| `SWITCH_COMBO_1` | Carica combo_1.json |
| `SWITCH_COMBO_2` | Carica combo_2.json |
| `SWITCH_MY_COMBO_0` | Carica my_combo_0.json |
| `SWITCH_MY_COMBO_1` | Carica my_combo_1.json |

**Esempi:**
```json
"5+7+9": ["SWITCH_MY_COMBO_0"]       // Passa a my_combo_0
"2+5": ["SWITCH_COMBO_0"]            // Torna a combo_0
"3+5": ["SWITCH_MY_COMBO_1"]         // Passa a my_combo_1
```

### Configurazione LED per Combo

Ogni file combo può avere un colore LED personalizzato che viene mostrato quando si switcha:

**my_combo_0.json:**
```json
{
  "_settings": {
    "led_color": [255, 255, 0]    // Giallo
  },
  "1": ["S_B:a"],
  ...
}
```

**my_combo_1.json:**
```json
{
  "_settings": {
    "led_color": [0, 255, 255]    // Ciano
  },
  "1": ["S_B:1"],
  ...
}
```

Quando switchi combo, il LED lampeggia del colore configurato per 150ms come feedback visivo.

---

## Comandi Concatenati

Puoi eseguire più comandi in sequenza racchiudendoli tra `<` e `>`.

### Sintassi
```json
"combo": ["<comando1><comando2><comando3>"]
```

### Esempi Base

**Sequenza di Tasti:**
```json
"1+5": ["<S_B:SUPER+t><S_B:SUPER+w><S_B:SUPER+c>"]
```
Esegue: Super+T, poi Super+W, poi Super+C

**Testo con Spazi:**
```json
"1+5": ["<S_B:Hello><S_B:SPACE><S_B:World>"]
```
Scrive: "Hello World"

### Delay tra Comandi

Usa `<DELAY_milliseconds>` per inserire pause tra i comandi.

**Sintassi:**
```
<DELAY_500>    // Aspetta 500ms
<DELAY_1000>   // Aspetta 1 secondo
```

**Esempi:**
```json
"2+5+8": ["<S_B:SUPER+t><DELAY_500><S_B:btop><DELAY_500><S_B:RETURN>"]
```
1. Premi Super+T (apri terminale)
2. Aspetta 500ms
3. Scrivi "btop"
4. Aspetta 500ms
5. Premi Enter

**Altro Esempio (Login Automatico):**
```json
"9+5+7,BUTTON": ["<TOGGLE_KEY_ORDER><S_B:username><S_B:TAB><S_B:password123><S_B:RETURN>"]
```

**Esempio con IR:**
```json
"4+5+6": ["<SEND_IR_tv_off><DELAY_1000><SEND_IR_led_off><DELAY_500><LED_OFF>"]
```
Spegne TV, aspetta 1s, spegne LED strip, aspetta 500ms, spegne LED MacroPad.

---

## Esempi Pratici

### Esempio 1: Produttività Desktop (combo_0.json)

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

### Esempio 2: Numpad Multimediale (combo_1.json)

```json
{
  "1": ["S_B:1"],
  "2": ["S_B:2"],
  "3": ["S_B:3"],
  "4": ["S_B:4"],
  "5": ["S_B:5"],
  "6": ["S_B:6"],
  "7": ["S_B:7"],
  "8": ["S_B:8"],
  "9": ["S_B:9"],

  "CW": ["S_B:VOL_UP"],
  "CCW": ["S_B:VOL_DOWN"],
  "BUTTON": ["S_B:PLAY_PAUSE"],

  "1,BUTTON": ["S_B:PREVIOUS_TRACK"],
  "3,BUTTON": ["S_B:NEXT_TRACK"],
  "5,BUTTON": ["S_B:MUTE"],

  "2+5": ["SWITCH_COMBO_0"]
}
```

### Esempio 3: Combo Interattivo con Colori (my_combo_0.json)

```json
{
  "_settings": {
    "led_color": [255, 255, 0],
    "interactive_colors": [
      [255, 0, 0],
      [255, 127, 0],
      [255, 255, 0],
      [0, 255, 0],
      [0, 255, 255],
      [0, 0, 255],
      [127, 0, 255],
      [255, 0, 255],
      [255, 255, 255]
    ]
  },

  "1": ["S_B:a"],
  "2": ["S_B:b"],
  "3": ["S_B:c"],
  "4": ["S_B:d"],
  "5": ["S_B:e"],
  "6": ["S_B:f"],
  "7": ["S_B:g"],
  "8": ["S_B:h"],
  "9": ["S_B:i"],

  "1+5": ["<S_B:Hello><S_B:SPACE><S_B:World>"],
  "2+5": ["SWITCH_COMBO_0"],
  "3+5": ["SWITCH_MY_COMBO_1"],

  "6,CW": ["LED_BRIGHTNESS_MINUS"],
  "6,CCW": ["LED_BRIGHTNESS_PLUS"],
  "7+8+9": ["SAVE_INTERACTIVE_COLORS"],

  "CW": ["S_B:MOUSE_MOVE_0_0_1_0"],
  "CCW": ["S_B:MOUSE_MOVE_0_0_-1_0"]
}
```

### Esempio 4: Combo Comune con Gesti (combo_common.json)

```json
{
  "BUTTON": ["EXECUTE_GESTURE"],

  "G_ID:0": ["S_B:CTRL+SUPER,RIGHT_ARROW"],
  "G_ID:1": ["S_B:CTRL+SUPER,LEFT_ARROW"],
  "G_ID:2": ["S_B:SUPER+q"],
  "G_ID:3": ["S_B:SUPER+l"],

  "1,CW": ["S_B:VOL_UP"],
  "1,CCW": ["S_B:VOL_DOWN"],
  "1+2,CW": ["S_B:MOUSE_MOVE_0_0_5_0"],
  "1+2,CCW": ["S_B:MOUSE_MOVE_0_0_-5_0"],

  "1+5+9": ["CALIBRATE_SENSOR"],
  "1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"],
  "1+4+7,BUTTON": ["HOP_BLE_DEVICE"],
  "7+8+9,BUTTON": ["RESET_ALL"],
  "1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"]
}
```

### Esempio 5: Setup Sviluppatore

```json
{
  "1": ["S_B:CTRL+c"],
  "2": ["S_B:CTRL+v"],
  "3": ["S_B:CTRL+x"],

  "4": ["S_B:CTRL+z"],
  "5": ["<S_B:SUPER+t><DELAY_300><S_B:code SPACE .><S_B:RETURN>"],
  "6": ["S_B:CTRL+SHIFT+z"],

  "7": ["S_B:CTRL+s"],
  "8": ["<S_B:CTRL+`>"],
  "9": ["S_B:F5"],

  "1+2+3": ["<S_B:git SPACE status><S_B:RETURN>"],
  "4+5+6": ["<S_B:npm SPACE run SPACE dev><S_B:RETURN>"],
  "7+8+9": ["<S_B:git SPACE add SPACE .><S_B:RETURN>"],

  "2+5+8": ["<S_B:SUPER+t><DELAY_500><S_B:htop><DELAY_500><S_B:RETURN>"]
}
```

---

## Note Finali

### Debugging e Log

Puoi monitorare l'esecuzione dei comandi attraverso:
1. **Serial Monitor** (se `serial_enabled: true` in config.json)
2. **Web Interface** - pagina "System Actions" mostra log in tempo reale
3. **Comando `MEM_INFO`** per vedere lo stato della memoria

### Best Practices

1. **Usa combo_common.json** per comandi che vuoi sempre disponibili (gesti, sistema)
2. **Dividi i combo per contesto** (lavoro, gaming, media, ecc.)
3. **Testa i comandi concatenati** uno alla volta prima di combinarli
4. **Salva backup** dei tuoi file combo prima di modifiche importanti
5. **Usa nomi descrittivi** nei file my_combo_X.json

### Troubleshooting

**Combo non funziona:**
- Verifica sintassi JSON (virgole, parentesi)
- Controlla che i tasti siano nell'ordine corretto in modalità ordinata
- Usa `TOGGLE_KEY_ORDER` per cambiare modalità

**Caratteri sbagliati:**
- Controlla il layout tastiera del sistema
- Usa caratteri escape (`,,` per virgola, `++` per più)
- Alcuni caratteri speciali dipendono dal layout (ITA vs US)

**BLE non si connette:**
- Usa `HOP_BLE_DEVICE` per cambiare MAC
- Rimuovi dispositivi duplicati dalle impostazioni Bluetooth
- Verifica che `enable_BLE: true` in config.json

**Gesti non riconosciuti:**
- Calibra l'accelerometro con `CALIBRATE_SENSOR`
- Ripeti l'addestramento con più campioni (5-7 volte)
- Verifica che i gesti siano sufficientemente diversi tra loro
- Controlla i log per vedere gli score KNN

---

**Versione:** 1.0
**Data:** 2025-01
**Autore:** Enrico Mori
**Licenza:** GPL-3.0
