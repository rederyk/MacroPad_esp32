# Guida Rapida per ESP32 MacroPad

Questa guida ti aiuterà a configurare e utilizzare il tuo ESP32 MacroPad.

## 1. Configurazione Iniziale (Wi-Fi)

Alla prima accensione, il MacroPad tenterà di connettersi a una rete Wi-Fi pre-configurata. Se non ci riesce, o se nessuna rete è configurata, creerà un proprio Access Point (AP) per permetterti di accedere al pannello di configurazione.

1.  **Alimenta il MacroPad**: Connetti il dispositivo a una fonte di alimentazione USB.
2.  **Cerca la Rete Wi-Fi**: Utilizzando il tuo computer o smartphone, cerca le reti Wi-Fi disponibili.
3.  **Connettiti all'Access Point**:
    *   **Nome Rete (SSID)**: `ESP32_MacroPad`
    *   **Password**: `my_cat_name123`
4.  **Accedi al Pannello Web**:
    *   Apri un browser web.
    *   Naviga all'indirizzo: `http://192.168.4.1`

Una volta connesso, vedrai il pannello di configurazione del MacroPad, da cui potrai gestire tutte le sue funzionalità.

**Nota**: Dal pannello di configurazione, puoi connettere il MacroPad alla tua rete Wi-Fi domestica (modalità STA). Una volta connesso, potrai accedere al pannello tramite l'indirizzo IP assegnato dal tuo router.

## 2. Panoramica dell'Interfaccia Web

L'interfaccia web è suddivisa in diverse sezioni per una facile configurazione.

*   **Config (`config.html`)**: Pagina principale dove puoi configurare le impostazioni di sistema, Wi-Fi, pin del keypad, LED e altri parametri hardware.
*   **Combinations (`combo.html`)**: Qui puoi definire le macro e le azioni associate a ogni tasto del pad. Puoi creare e gestire diversi set di combinazioni (es. `combo_0.json`, `combo_1.json`).
*   **Special Actions (`special_actions.html`)**: Permette di configurare azioni speciali non legate a semplici pressioni di tasti, come il controllo del mouse giroscopico, l'invio di segnali IR o l'attivazione di modalità specifiche.
*   **Advanced (`advanced.html`)**: Contiene impostazioni avanzate, probabilmente relative a sensori, gesti e altre funzionalità complesse.

## 3. Funzionalità Principali

### Combinazioni di Tasti e Macro

Il cuore del MacroPad è la sua capacità di eseguire macro complesse.

*   Vai alla sezione **Combinations** nel pannello web.
*   Puoi assegnare a ogni tasto una sequenza di azioni, che possono includere:
    *   Pressioni di tasti (es. `CTRL+C`).
    *   Testo da digitare.
    *   Azioni speciali (vedi sotto).
*   Puoi salvare e caricare diversi "profili" di combinazioni, rendendo il pad versatile per diverse applicazioni (es. un profilo per il video editing, uno per la programmazione).

### Modalità BLE (Bluetooth Low Energy)

Il MacroPad può funzionare come un dispositivo di input wireless (tastiera, mouse) per computer, tablet e smartphone.

*   Per attivare la modalità BLE, è necessario disabilitare il Wi-Fi. Questo può essere fatto tramite un'azione speciale assegnata a un tasto o tramite le impostazioni di sistema nel file `config.json` (`"enable_BLE": true`).
*   Quando il BLE è attivo, il Wi-Fi viene disattivato per liberare risorse.
*   Il dispositivo sarà visibile come `Macropad_esp32` (o il nome configurato) durante la ricerca di dispositivi Bluetooth.

### Gyro Mouse

Utilizzando l'accelerometro e il giroscopio integrati, il MacroPad può funzionare come un mouse aereo.

*   Puoi attivare/disattivare il Gyro Mouse tramite un'azione speciale.
*   Altre azioni includono la recentratura del cursore e il cambio di sensibilità al volo.
*   La configurazione dettagliata (inversione assi, sensibilità) si trova nel pannello web.

### Controllo IR

Il dispositivo può imparare e trasmettere segnali a infrarossi, trasformandolo in un telecomando universale.

*   **Scansione**: Dalla pagina "Special Actions", puoi mettere il dispositivo in modalità di ascolto per catturare un segnale da un telecomando esistente.
*   **Invio**: Assegna l'azione di invio del segnale IR catturato a un tasto del pad.

### Gestures (Gesti)

Il sensore di movimento permette di eseguire azioni tramite gesti fisici (es. scuotendo o inclinando il dispositivo).

*   I gesti possono essere configurati per eseguire azioni specifiche, come cambiare profilo di combinazioni o attivare il Gyro Mouse.
*   La calibrazione e la configurazione dei gesti si trovano nella sezione "Advanced" del pannello web.

## 4. Indicatore LED di Stato

Il LED RGB integrato fornisce un feedback visivo sullo stato del dispositivo:

*   **Magenta**: Avvio del sistema.
*   **Blu**: Modalità BLE attiva.
*   **Verde**: Connesso alla rete Wi-Fi (modalità STA).
*   **Rosso**: Modalità Access Point (AP) attiva o errore di connessione Wi-Fi.
*   **Colori Personalizzati**: Quando si cambia profilo di combinazioni, il LED può assumere un colore specifico per quel profilo, se configurato.

## 5. Gestione Energetica

*   Il MacroPad è dotato di una modalità di **deep sleep** per risparmiare energia.
*   Entra in sleep automaticamente dopo un periodo di inattività (configurabile).
*   Per risvegliarlo, è sufficiente premere un tasto qualsiasi o, se configurato, muovere il dispositivo.
