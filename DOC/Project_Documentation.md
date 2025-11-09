# Documentazione Progetto MacroPad ESP32

Questo documento descrive l'architettura software e il funzionamento dei principali componenti del progetto MacroPad basato su ESP32.

## 1. Architettura Generale

Il software è progettato con un'architettura modulare che gira su FreeRTOS, il sistema operativo real-time dell'ESP32. La logica principale non risiede nel `loop()` di Arduino, ma in un task dedicato (`mainLoopTask`) che viene eseguito a intervalli regolari e predefiniti (attualmente ogni 5ms). Questo garantisce performance stabili e non bloccanti.

Il flusso dei dati è il seguente:
1.  Le **periferiche di input** (tastiera, rotary encoder, sensore di gesti) vengono scansionate.
2.  L'**`InputHub`** raccoglie tutti gli eventi generati e li inserisce in una coda di eventi.
3.  Il **`MacroManager`** estrae gli eventi dalla coda, interpreta le combinazioni di tasti o i gesti e identifica l'azione corrispondente.
4.  La **`CommandFactory`** crea un oggetto "comando" specifico per l'azione richiesta.
5.  Il `MacroManager` esegue il comando, che a sua volta interagisce con i **moduli di output** (es. `BLEController` per inviare un tasto, `Led` per cambiare colore, `GyroMouse` per muovere il cursore).

---

## 2. Descrizione dei Moduli (`lib`)

Ogni modulo nella directory `lib` incapsula una funzionalità specifica.

### 2.1. `main.cpp` (Entry Point)

- **Responsabilità:** Orchestrazione dell'avvio e del ciclo di vita dell'applicazione.
- **Funzionamento:**
    - La funzione `setup()` inizializza tutti i moduli in un ordine logico: prima le configurazioni, poi i servizi di base (Logger, LED), i gestori di stato (PowerManager, Scheduler), i gestori di input e macro, e infine la connettività (WiFi/BLE).
    - Avvia il `mainLoopTask`, un task FreeRTOS che contiene il ciclo operativo principale.
    - Il `mainLoopTask` viene eseguito a una frequenza fissa, garantendo reattività. Al suo interno:
        - Scansiona gli input tramite `InputHub`.
        - Processa gli eventi con `MacroManager`.
        - Aggiorna lo stato dei moduli attivi (BLE, GyroMouse, Scheduler).
        - Gestisce la logica per il cambio dinamico dei set di combinazioni.
        - Controlla l'inattività per attivare la modalità di risparmio energetico (deep sleep).

### 2.2. `InputHub`

- **Responsabilità:** Gestione centralizzata di tutti i dispositivi di input.
- **Funzionamento:**
    - Possiede e inizializza gli oggetti per `Keypad`, `RotaryEncoder`, `IRSensor` e `GestureDevice`.
    - Il metodo `scanDevices()` interroga ogni dispositivo per nuovi input.
    - Gli eventi validi vengono incapsulati in una struct `InputEvent` e messi in una coda (`std::deque`).
    - Il `MacroManager` può estrarre gli eventi da questa coda in modo ordinato tramite il metodo `poll()`.
    - Gestisce anche il `ReactiveLightingController` per l'illuminazione reattiva alla pressione dei tasti.

### 2.3. `MacroManager`

- **Responsabilità:** Cervello logico del macropad. Interpreta gli input e li traduce in azioni.
- **Funzionamento:**
    - Riceve `InputEvent` da `InputHub`.
    - Mantiene uno stato degli input attuali (es. maschera di bit dei tasti premuti).
    - Confronta lo stato corrente con le combinazioni definite nei file JSON (caricate da `CombinationManager`).
    - Supporta due modalità di rilevamento combinazioni: basata sulla maschera di bit (tasti premuti simultaneamente) o sull'ordine di pressione (`useKeyPressOrder`).
    - Quando una combinazione viene riconosciuta, utilizza la `CommandFactory` per creare il comando associato e lo esegue (`pressAction`/`releaseAction`).
    - Gestisce logiche complesse come il "combo switch" (cambio di set di macro al volo) e la modalità GyroMouse.

### 2.4. `common/Command` e `CommandFactory`

- **Responsabilità:** Implementazione del **Design Pattern Command**.
- **Funzionamento:**
    - `Command.h` definisce un'interfaccia con due metodi: `press()` e `release()`.
    - Ogni azione specifica (es. `LedCommand`, `SendIrCommand`, `GyroMouseToggleCommand`) è una classe che implementa questa interfaccia.
    - `CommandFactory` è una classe "fabbrica" che riceve una stringa (es. `"LED_RGB 255 0 0"`) e restituisce un `std::unique_ptr` al corrispondente oggetto `Command`.
    - Questo pattern disaccoppia magnificamente l'identificazione dell'azione dalla sua esecuzione, rendendo il sistema estensibile e pulito.

### 2.5. `configManager` e `combinationManager`

- **`configManager`:**
    - **Responsabilità:** Caricare e fornire accesso alla configurazione principale del dispositivo da `config.json`.
    - **Funzionamento:** All'avvio, legge `config.json` e popola delle `struct` fortemente tipizzate (definite in `configTypes.h`) per ogni sezione (keypad, led, wifi, system, etc.). Gli altri moduli possono accedere a queste configurazioni in modo sicuro.
- **`combinationManager`:**
    - **Responsabilità:** Gestire i set di combinazioni di tasti/macro.
    - **Funzionamento:** Carica i file `combo_*.json`, `my_combo_*.json` e `combo_common.json`. Può ricaricare dinamicamente un set diverso tramite `reloadCombinations()`, permettendo all'utente di cambiare layout al volo.

### 2.6. `BLEController`

- **Responsabilità:** Gestire la connettività Bluetooth Low Energy (BLE) e l'invio di comandi HID (Human Interface Device).
- **Funzionamento:**
    - Utilizza la libreria `BleCombo` per emulare una tastiera e un mouse BLE.
    - Il metodo `BLExecutor` riceve stringhe di azione e le traduce in pressioni di tasti, movimenti del mouse, etc.
    - `UnicodeHelper` è una classe ausiliaria per inviare caratteri Unicode e emoji, superando i limiti dei layout di tastiera.
    - Gestisce la connessione, il nome del dispositivo e può anche modificare il MAC address per "saltare" tra profili di accoppiamento.

### 2.7. `WIFIManager` e `configWebServer`

- **`WIFIManager`:**
    - **Responsabilità:** Gestire la connettività WiFi.
    - **Funzionamento:** Può operare in due modalità: `STA` (connessione a un router esistente) o `AP` (creazione di un proprio Access Point). La modalità viene scelta in base alla configurazione.
- **`configWebServer`:**
    - **Responsabilità:** Fornire un'interfaccia web per la configurazione del dispositivo.
    - **Funzionamento:** Quando il WiFi è attivo, avvia un server web asincrono (`ESPAsyncWebServer`). Serve file statici (HTML, CSS) dalla directory `/data` e gestisce API per leggere/scrivere configurazioni, testare funzioni (es. scansione IR) e visualizzare lo stato del dispositivo.

### 2.8. `gyroMouse` e `SensorFusion`

- **`gyroMouse`:**
    - **Responsabilità:** Implementare la funzionalità di mouse aereo.
    - **Funzionamento:** Quando attivato, legge i dati dal `GestureRead` (sensore di movimento) e li usa per calcolare il movimento del cursore.
    - Gestisce diversi livelli di sensibilità, il "recentering" della posizione neutra e una logica di "click slowdown" per migliorare la precisione durante il clic.
- **`SensorFusion`:**
    - **Responsabilità:** Fornire un orientamento 3D stabile e robusto.
    - **Funzionamento:** Implementa il filtro AHRS di Madgwick per fondere i dati dell'accelerometro e del giroscopio. Questo corregge il drift del giroscopio e le imprecisioni dell'accelerometro, producendo un `Quaternion` che rappresenta l'orientamento del dispositivo nello spazio.

### 2.9. `gesture` (Read, Analyze, etc.)

- **Responsabilità:** Rilevamento e analisi di gesti fisici.
- **Funzionamento:**
    - `GestureRead`: Interagisce direttamente con il sensore di movimento (es. MPU6050) per campionare i dati grezzi di accelerazione e rotazione. Può operare in una modalità di campionamento continuo in un task separato.
    - `GestureAnalyze`: Prende i dati campionati da `GestureRead` e li passa a un algoritmo di riconoscimento (`SimpleGestureDetector`).
    - `SimpleGestureDetector`: Implementa una logica semplice ma efficace per riconoscere gesti come "swipe" e "shake" analizzando i picchi nei dati del giroscopio o dell'accelerometro.

### 2.10. `IRManager` (Sender, Sensor, Storage)

- **Responsabilità:** Gestione completa delle funzionalità a infrarossi.
- **Funzionamento:**
    - `IRSensor`: Utilizza `IRremoteESP8266` per catturare segnali IR in arrivo.
    - `IRSender`: Utilizza la stessa libreria per inviare segnali IR.
    - `IRStorage`: Gestisce il salvataggio e il caricamento dei comandi IR appresi in un file `ir_data.json`. L'interfaccia web interagisce con questo modulo per la scansione e il salvataggio di nuovi comandi.

### 2.11. `powerManager`

- **Responsabilità:** Gestire il risparmio energetico.
- **Funzionamento:**
    - Tiene traccia dell'ultimo momento di attività (`lastActivityTime`).
    - Se non rileva attività per un periodo di tempo configurabile (`sleep_timeout_ms`), prepara il dispositivo per il deep sleep.
    - Configura i pin di wakeup (dai tasti o dall'encoder) per permettere al dispositivo di risvegliarsi.

### 2.12. `eventScheduler` e `SchedulerStorage`

- **Responsabilità:** Eseguire azioni programmate.
- **Funzionamento:**
    - `SchedulerStorage`: Carica la configurazione degli eventi dal file `scheduler.json`.
    - `EventScheduler`: Mantiene una lista di eventi programmati. Nel suo metodo `update()`, controlla se è arrivato il momento di eseguire un'azione.
    - Supporta trigger basati su:
        - Orario del giorno (es. ogni giorno alle 9:00).
        - Intervalli (es. ogni 30 minuti).
        - Tempo assoluto (una data e ora specifiche).
        - Eventi di input (es. alla pressione di un tasto specifico).
    - Può anche risvegliare il dispositivo dallo sleep per eseguire un'azione critica.

### 2.13. Altri Moduli

- **`Led`:** Un semplice singleton per controllare il LED RGB di stato.
- **`Logger`:** Un singleton per la gestione del logging. Può scrivere su Seriale e/o inviare log tramite l'interfaccia web. Ha un buffer per non bloccare il loop principale.
- **`keypad`, `rotaryEncoder`:** Driver specifici per la matrice di tasti e per l'encoder rotativo, che implementano l'interfaccia `InputDevice`.
- **`specialAction`:** Una classe che raggruppa una serie di funzioni complesse (es. `hopBleDevice`, `toggleAP`, `calibrateSensor`) che possono essere invocate tramite comandi. Agisce come un "collettore" di funzionalità di alto livello.
