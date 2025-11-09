# TODO e Roadmap - ESP32 MacroPad

## Legenda Priorità
- 🔴 **Alta** - Problemi critici o funzionalità molto richieste
- 🟠 **Media** - Miglioramenti importanti
- 🟡 **Bassa** - Nice to have, ottimizzazioni
- 🟢 **Completato** - Già implementato

---

## Problemi Noti da Risolvere

### 🔴 Alta Priorità

#### Gestione Memoria Gesti
**Problema:** La memoria si riempie con molti campioni gesture, rischio crash.

**Dettagli:**
- Ogni campione gesture occupa memoria dinamica
- Con 5-7 campioni per 5+ gesti, la RAM si esaurisce
- Possibile crash o comportamento instabile

**Soluzioni Proposte:**
- [ ] Implementare limite massimo campioni per gesto
- [ ] Compressione dati campioni (ridurre risoluzione temporale)
- [ ] Liberazione memoria campioni più vecchi
- [ ] Usare PSRAM se disponibile su board
- [ ] Migliorare algoritmo per usare meno campioni (3 invece di 7)

---

#### Case Sensitivity con Caratteri Speciali
**Problema:** Caratteri maiuscoli e simboli si comportano in modo inconsistente.

**Dettagli:**
- BLE usa SHIFT per maiuscole, ma pressioni multiple rapide causano conflitti
- Caratteri singoli premuti molto velocemente diventano lowercase
- Problemi con combinazioni multiple di caratteri uppercase
- Layout tastiera (ITA vs US) influenza simboli

**Soluzioni Proposte:**
- [ ] Gestione SHIFT locale nel codice, non delegata a BLE
- [ ] Tabella caratteri con flag `UPPERCASE: true/false`
- [ ] Conversione stringa in lowercase + gestione shift programmatica
- [ ] Supporto layout tastiera configurabile (ITA/US/DE/ecc.)
- [ ] Delay micro tra pressioni caratteri in stringhe

**Workaround Attuale:**
- Usa caratteri lowercase quando possibile
- Evita stringhe con mix maiuscole/minuscole rapide
- Usa `TOGGLE_KEY_ORDER` per migliorare timing

---

### 🟠 Media Priorità

#### Comma e Plus nelle Macro
**Problema:** Gestione inconsistente di virgola `,` e più `+` nelle stringhe.

**Dettagli:**
- `,` usato per separare azioni: `S_B:CTRL,ALT`
- `+` usato per combinazioni: `S_B:CTRL+ALT`
- Per inserire letteralmente serve doppio: `,,` o `++`
- Confusione per utenti nuovi

**Soluzioni Proposte:**
- [ ] Standardizzare sintassi: sempre `+` per modifier, `,` per sequenziale
- [ ] Parser migliorato che distingue contesto
- [ ] Documentazione chiara con esempi
- [ ] Warning in web interface se sintassi ambigua

**Workaround Attuale:**
- Usa `,,` per virgola letterale
- Usa `++` per più letterale
- Leggi documentazione COMANDI.md sezione escape

---

#### Ordine Tasti in Modalità Ordinata
**Problema:** Le combo devono essere salvate in ordine numerico in modalità ordinata.

**Dettagli:**
- Modalità ordinata richiede tasti premuti in sequenza specifica
- File JSON dovrebbe ignorare ordine, ma attualmente non sempre
- Confusione tra modalità ordinata e non-ordinata

**Soluzioni Proposte:**
- [ ] Fix parser per ignorare ordine tasti in modalità non-ordinata
- [ ] Normalizzazione automatica ordine tasti quando si salva
- [ ] UI web interface che mostra modalità attiva
- [ ] Indicatore LED per modalità corrente

**Workaround Attuale:**
- Usa `TOGGLE_KEY_ORDER` per cambiare modalità
- In modalità ordinata, salva combo in ordine crescente (es. `1+2+3` non `3+1+2`)

---

#### BLE - Cambio Nome e Riabbinamento
**Problema:** Cambiare nome BLE con dispositivo già abbinato causa riavvii continui.

**Dettagli:**
- Se cambi `BleName` ma PC/smartphone ha vecchio nome abbinato
- Il dispositivo va in loop di riavvio tentando connessione
- Richiede "dimenticare dispositivo" manuale

**Soluzioni Proposte:**
- [ ] Rilevamento automatico conflitto nome
- [ ] Clear pairing automatico su cambio nome
- [ ] Warning in web interface prima di cambiare nome
- [ ] Modalità "safe mode" per reset BLE

**Workaround Attuale:**
- Prima di cambiare nome, disconnetti e dimentica dispositivo su tutti i device
- Usa `HOP_BLE_DEVICE` invece di cambiare nome se vuoi nuovo abbinamento

---

### 🟡 Bassa Priorità

#### Reattività Tasti Sovrapposti
**Problema:** Scarsa reattività o tasti sovrapposti nel tempo.

**Possibili Cause:**
- Debouncing software troppo conservativo
- Timing scansione matrice
- Buffer eventi limitato (16 eventi max)

**Soluzioni Proposte:**
- [ ] Tuning parametri debouncing
- [ ] Aumento dimensione buffer eventi
- [ ] Ottimizzazione timing scansione (attualmente 5ms)

---

#### Memory Limitato JSON per Macro
**Problema:** Spazio limitato per combinazioni in singolo file JSON.

**Stato:** ✅ **RISOLTO** - File combo divisi (combo_0, combo_1, my_combo_0, ecc.)

**Miglioramenti Futuri:**
- [ ] Dynamic memory allocation per combo
- [ ] Caricamento lazy di combo non usate
- [ ] Compressione JSON in memoria

---

## Funzionalità da Implementare

### 🔴 Alta Priorità

#### Gestione Layout Tastiera
**Feature:** Supporto layout tastiera configurabile (ITA/US/DE/FR/ecc.)

**Benefici:**
- Caratteri speciali corretti per layout
- Simboli consistenti
- Migliore esperienza internazionale

**Implementazione:**
- [ ] Tabella mapping caratteri per layout
- [ ] Selezione layout in config.json
- [ ] Test con layout ITA, US, DE
- [ ] Documentazione layout supportati

**Priority:** Risolve molti problemi con caratteri speciali

---

### 🟠 Media Priorità

#### Pattern LED Animati
**Feature:** Modalità LED con animazioni (fade, blink, rainbow, breathing)

**Benefici:**
- Feedback visivo più ricco
- Indicatori stato personalizzati
- Eye candy

**Implementazione:**
- [ ] Classe `LEDPattern` base
- [ ] Pattern fade (transizione graduale colori)
- [ ] Pattern blink (lampeggio configurabile)
- [ ] Pattern rainbow (ciclo arcobaleno)
- [ ] Pattern breathing (respirazione)
- [ ] Trigger pattern da macro

**Esempio Utilizzo:**
```json
"1+6": ["LED_PATTERN_RAINBOW"],
"2+6": ["LED_PATTERN_BREATHING_255_0_0"]
```

---

#### Mouse Gyro Mode
**Feature:** Controllo mouse con movimento accelerometro

**Benefici:**
- Controllo mouse senza mani
- Gaming innovativo
- Accessibilità

**Implementazione:**
- [ ] Modalità dedicata "gyro mouse"
- [ ] Mappatura assi accelerometro → X/Y mouse
- [ ] Calibrazione sensibilità
- [ ] Toggle gyro mode da combo
- [ ] Smoothing movimento per precisione

**Esempio:**
```json
"4+5+6": ["TOGGLE_GYRO_MOUSE"]
```

---

#### Macro Recording On-the-Fly
**Feature:** Registra sequenza tasti in tempo reale e salva come macro

**Benefici:**
- Creazione macro rapida senza editare JSON
- Utile per workflow ripetitivi
- Accessibile a utenti non tecnici

**Implementazione:**
- [ ] Modalità "recording" attivabile da combo
- [ ] Buffer temporaneo per azioni registrate
- [ ] Salvataggio in file combo con nome
- [ ] UI web per gestire macro registrate

**Workflow:**
1. Attiva recording: `RECORD_MACRO_START`
2. Esegui azioni da registrare
3. Ferma recording: `RECORD_MACRO_STOP`
4. Assegna a combo via web interface

---

#### Profili Gaming Dedicati
**Feature:** Profili ottimizzati per gaming con latenza ridotta

**Caratteristiche:**
- [ ] Disabilitazione deep sleep
- [ ] Polling rate aumentato
- [ ] Macro anti-recoil (controverso, valutare etica)
- [ ] Combo rapid-fire
- [ ] LED feedback per cooldown abilità

---

#### Export/Import Configurazioni
**Feature:** Backup e condivisione configurazioni complete

**Implementazione:**
- [ ] Export tutti file JSON in archivio ZIP
- [ ] Import ZIP → sovrascrive configurazione
- [ ] Cloud sync (opzionale, privacy concerns)
- [ ] Preset pubblici condivisibili

---

### 🟡 Bassa Priorità

#### ULP Co-Processor per Wake
**Feature:** Usare ULP (Ultra Low Power) co-processor per wake-up

**Benefici:**
- Consumo ancora più ridotto in sleep
- Wake personalizzabile (es. sequenza tasti specifica)
- Scan keypad durante deep sleep

**Complessità:** Alta - richiede programmazione assembly ULP

**Implementazione:**
- [ ] Studio ULP ESP32
- [ ] Programma ULP per scan keypad
- [ ] Integrazione con deep sleep
- [ ] Test consumo

---

#### Display OLED
**Feature:** Display OLED per feedback visivo (128x64 o 128x32)

**Benefici:**
- Visualizzazione combo attiva
- Status sistema senza web interface
- Icons per azioni

**Implementazione:**
- [ ] Driver I2C OLED (SSD1306)
- [ ] UI minimale per status
- [ ] Animazioni feedback
- [ ] Icone per gesti/combo

---

#### App Mobile per Configurazione
**Feature:** App Android/iOS per configurazione wireless

**Benefici:**
- Configurazione senza PC
- UI nativa touch-friendly
- Notifiche push

**Complessità:** Molto alta

**Implementazione:**
- [ ] API REST completa
- [ ] App Flutter cross-platform
- [ ] Gestione combo visuale
- [ ] Gesture training guidato

---

#### Cloud Sync Configurazioni
**Feature:** Sincronizzazione configurazioni su cloud

**Privacy Concerns:** Password e dati sensibili

**Implementazione:**
- [ ] Encryption end-to-end
- [ ] Server opt-in (self-hosted)
- [ ] Sync selettivo (escludi password)

---

## Miglioramenti Hardware

### 🟠 Media Priorità

#### Wakeup Pin Keypad Dedicato
**Problema:** Attualmente wake su encoder button o motion

**Proposta:** Pin dedicato collegato a tutti i tasti con diodi OR

**Benefici:**
- Wake su qualsiasi tasto
- Nessun movimento necessario
- Più intuitivo

**Implementazione Hardware:**
- Diodi da ogni tasto → pin RTC
- Pull-up su pin
- `esp_sleep_enable_ext0_wakeup`

**Alternative:**
- 🟢 Già implementato: Wake su motion (MPU6050)
- Fallback: Wake su encoder button pin

---

#### Supporto Batterie Multiple
**Feature:** Test e documentazione batterie diverse

**Testato:**
- ✅ LiPo 3.7V 820mAh - ~2 settimane con sleep

**Da Testare:**
- [ ] LiPo 1200mAh
- [ ] LiPo 2000mAh
- [ ] 18650 Li-ion con holder

---

### 🟡 Bassa Priorità

#### Audio Feedback (Buzzer)
**Feature:** Buzzer piezo per feedback sonoro

**Benefici:**
- Conferma azioni senza guardare LED
- Accessibilità (non vedenti)
- Effetti sonori custom

**Implementazione:**
- Pin PWM per buzzer
- Toni configurabili per azioni
- Volume regolabile

---

#### Espansione Keypad oltre 3x3
**Feature:** Supporto matrici più grandi (4x4, 5x4, ecc.)

**Limitazioni:**
- Pin disponibili ESP32
- Complessità scanning

**Implementazione:**
- [ ] Parametri `rows`/`cols` dinamici
- [ ] Test matrice 4x4
- [ ] Documentazione pin mapping

---

## Ottimizzazioni Codice

### 🟠 Media Priorità

#### Sostituzione Arduino String con std::string
**Miglioramento:** Usare `std::string` invece di `String` Arduino

**Benefici:**
- Gestione memoria migliore
- Meno frammentazione heap
- Performance superiori

**Implementazione:**
- [ ] Refactor graduale modulo per modulo
- [ ] Test memory leak
- [ ] Benchmark performance

---

#### Costanti invece di Magic Numbers
**Miglioramento:** Sostituire numeri hardcoded con `static const`

**Benefici:**
- Codice più leggibile
- Manutenzione semplificata
- Self-documenting

**Esempio:**
```cpp
// Prima
if (timeout > 100) { ... }

// Dopo
static const uint32_t COMBO_TIMEOUT_MS = 100;
if (timeout > COMBO_TIMEOUT_MS) { ... }
```

**Implementazione:**
- [ ] Identificare magic numbers
- [ ] Creare file constants.h
- [ ] Refactor graduale

---

#### Gestione Errori Migliorata
**Miglioramento:** Sistema error handling consistente

**Implementazione:**
- [ ] Enum ErrorCode
- [ ] Error callback system
- [ ] Logging errori strutturato
- [ ] Recovery automatico da errori soft

---

## Investigazioni Future

### 🟡 Ricerca e Sviluppo

#### BLE + WiFi Simultaneo
**Obiettivo:** Usare BLE e WiFi insieme

**Problema:** Attualmente mutualmente esclusivi

**Investigazione:**
- [ ] Limitazioni hardware ESP32
- [ ] Dual-mode provisioning
- [ ] Profiling consumo energetico
- [ ] Possibili use case (BLE per macro, WiFi per config)

---

#### Modalità USB HID Cablata
**Obiettivo:** Funzionare come tastiera USB senza WiFi/BLE

**Benefici:**
- Zero latency
- Nessuna batteria necessaria
- Compatibilità universale

**Investigazione:**
- [ ] ESP32 USB native (ESP32-S2/S3/C3)
- [ ] TinyUSB stack
- [ ] Dual mode: USB quando collegato, BLE wireless

---

#### Expansion Port I2C/SPI
**Obiettivo:** Port espansione per periferiche esterne

**Possibilità:**
- Display OLED
- Sensori aggiuntivi
- LED matrix
- Haptic feedback

**Investigazione:**
- [ ] Pin disponibili
- [ ] Hot-plug capability
- [ ] Power budget

---

## Testing e Documentazione

### 🟠 Media Priorità

#### Board Testing Program
**Obiettivo:** Test su board ESP32 diverse

**Board da Testare:**
- ✅ LOLIN32 Lite - Testata, funzionante
- [ ] ESP32 DevKit v1
- [ ] TTGO T-Display (con schermo integrato)
- [ ] ESP32-S2/S3 (USB native)
- [ ] FireBeetle ESP32

**Processo:**
- [ ] Pin mapping per ogni board
- [ ] Test funzionalità base
- [ ] Documentazione board-specific
- [ ] Template config.json per board

---

#### Test Automatizzati
**Obiettivo:** Unit test e integration test

**Implementazione:**
- [ ] Framework testing (Unity, Google Test)
- [ ] Mock hardware I/O
- [ ] CI/CD pipeline
- [ ] Test coverage report

---

#### Video Tutorial
**Obiettivo:** Guide video per setup e uso

**Contenuti:**
- [ ] Unboxing e saldatura hardware
- [ ] Primo setup software
- [ ] Configurazione combo
- [ ] Gesture training
- [ ] Reactive lighting demo

---

### 🟡 Bassa Priorità

#### Traduzione Documentazione
**Lingue:**
- ✅ Italiano
- [ ] Inglese
- [ ] Spagnolo
- [ ] Francese
- [ ] Tedesco

---

#### Community Contributions
**Obiettivo:** Facilitare contributi esterni

**Implementazione:**
- [ ] CONTRIBUTING.md
- [ ] Code style guide
- [ ] Issue templates
- [ ] PR templates
- [ ] Good first issue labels

---

## Problemi Risolti (Completati)

### 🟢 Completati di Recente

#### ✅ Sleep Mode con Timeout Configurabile
- Implementato deep sleep
- Wake su movimento (MPU6050)
- Fallback wake su encoder button
- Timeout configurabile da config.json
- Comando `ENTER_SLEEP` manuale

#### ✅ Delay nei Comandi Concatenati
- Sintassi `<DELAY_ms>` implementata
- Supporto delay da 1ms a 60000ms
- Timing preciso con `vTaskDelay`

#### ✅ Controllo LED Avanzato
- LED RGB con PWM
- Brightness control con salvataggio
- Modalità flashlight
- Reactive lighting con colori per tasto
- Colori sistema (magenta, blu, verde, rosso)

#### ✅ Sistema IR Completo
- Scan e invio comandi
- Storage persistente ir_data.json
- Supporto protocolli multipli
- Integrazione combo

#### ✅ Sistema Combo Switching
- File combo multipli
- Switching runtime senza reboot
- Colori LED per combo (`_settings`)
- Combo common sempre caricato

#### ✅ Gesture System KNN
- Training multi-campione
- Matching KNN real-time
- Conversione JSON to Binary
- Score logging per debug

#### ✅ Web Interface Completa
- Combo editor runtime
- Gesture manager
- Config editor
- System logs
- IR database manager

---

## Note Sviluppo

### Priorità Correnti (Q1 2025)
1. 🔴 Fix gestione memoria gesti
2. 🔴 Fix case sensitivity caratteri
3. 🟠 Implementazione layout tastiera
4. 🟠 Pattern LED animati
5. 🟠 Testing su board diverse

### Contributi Benvenuti
Tutte le feature marcate con priorità media/bassa sono ottime per contributi esterni!

**Come Contribuire:**
1. Scegli un task dalla lista
2. Apri issue per discutere implementazione
3. Fork → Branch → Commit → Pull Request
4. Segui code style esistente

### Roadmap Generale
- **v2.0** - Layout tastiera, pattern LED, ottimizzazioni memoria
- **v2.5** - Mouse gyro, macro recording
- **v3.0** - Display OLED, app mobile, ULP sleep

---

**Ultima Revisione:** 2025-01
**Versione Documento:** 2.0
**Maintainer:** Enrico Mori
