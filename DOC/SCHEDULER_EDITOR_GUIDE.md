# Guida all'Editor dello Scheduler

## Panoramica

L'Editor dello Scheduler è un'interfaccia grafica interattiva che permette di creare, modificare e gestire eventi programmati per il MacroPad ESP32 senza dover modificare manualmente il file JSON.

## Accesso

- **URL**: `/scheduler_editor.html`
- **Link disponibili in**:
  - [scheduler.html](scheduler.html) - Pulsante "Editor Eventi"
  - [config.html](config.html) - Sezione "Azioni rapide"
  - [special_actions.html](special_actions.html) - Tab di navigazione

## Funzionalità Principali

### 1. Interfaccia a Blocchi

Ogni evento è rappresentato da un **blocco interattivo** che contiene:

- **Header**: ID evento (modificabile), pulsanti di controllo
- **Opzioni**: Checkbox per configurazioni rapide
- **Trigger**: Selezione del tipo di attivazione
- **Campi specifici**: Input dinamici basati sul tipo di trigger
- **Azione**: Configurazione dell'azione da eseguire

### 2. Catalogo Azioni e Suggerimenti

La sezione **"Azione da eseguire"** include ora due suggeritori:

- **Catalogo combo**: importa con un click le sequenze definite nei file `combo_*.json`, `my_combo_*.json` e `combo_common.json`. Ogni voce mostra il nome del file, la combinazione e il numero di azioni BLE/di sistema incluse.
- **Catalogo IR**: elenca automaticamente tutti i comandi presenti in `ir_data.json`, permettendo di autopopolare l'azione `send_ir_command` senza conoscere sintassi o parametri.

Entrambi i cataloghi possono essere richiamati in qualsiasi momento: scegli un suggerimento e l'editor imposterà automaticamente il tipo di azione corretto (macro o special action) e i relativi parametri JSON.

### 3. Tipi di Trigger Disponibili

#### 🕒 Orario (TIME_OF_DAY)
Esegue l'evento a orari specifici della giornata.

**Campi**:
- **Ora**: 0-23
- **Minuto**: 0-59
- **Secondo**: 0-59
- **Giorni della settimana**: Selezione visuale (Dom-Sab)
- **Usa UTC**: Ignora il timezone offset

**Esempio d'uso**: Eseguire un'azione ogni giorno alle 08:00

```json
{
  "type": "time_of_day",
  "hour": 8,
  "minute": 0,
  "second": 0,
  "days_mask": 127,
  "use_utc": false
}
```

#### 🔁 Intervallo (INTERVAL)
Esegue l'evento a intervalli regolari.

**Campi**:
- **Intervallo (ms)**: Tempo tra le esecuzioni in millisecondi
- **Jitter (ms)**: Variazione casuale opzionale

**Esempio d'uso**: Eseguire un'azione ogni 10 minuti

```json
{
  "type": "interval",
  "interval_ms": 600000,
  "jitter_ms": 5000
}
```

#### 📅 Assoluto (ABSOLUTE_TIME)
Esegue l'evento una volta in un momento specifico.

**Campi**:
- **Epoch seconds**: Timestamp Unix

**Esempio d'uso**: Eseguire un'azione il 31/12/2025 alle 23:59:59

```json
{
  "type": "absolute",
  "absolute_epoch": 1767225599
}
```

#### 🖱️ Input (INPUT_EVENT)
Esegue l'evento in risposta a input specifici.

**Campi**:
- **Sorgente Input**: es. "keypad", "encoder"
- **Tipo Input**: es. "key", "rotation", "button"
- **Valore**: Valore specifico (-1 = ignora)
- **Stato**: -1 = ignora, 0 = false, 1 = true
- **Testo**: Match opzionale su testo

**Esempio d'uso**: Eseguire un'azione quando viene premuto il tasto "5"

```json
{
  "type": "input",
  "input_source": "keypad",
  "input_type": "key",
  "input_value": 5,
  "input_state": 1
}
```

### 4. Opzioni dell'Evento

Ogni evento può avere le seguenti opzioni configurabili tramite checkbox:

- **Abilitato**: Attiva/disattiva l'evento
- **Previeni Sleep**: Impedisce il deep sleep quando l'evento è in attesa
- **Risveglio**: Può risvegliare il dispositivo dal deep sleep
- **Esegui all'avvio**: Esegue l'evento al boot del dispositivo
- **Una sola volta**: L'evento viene disabilitato dopo la prima esecuzione

### 5. Tipi di Azione

#### Macro / Combo
Esegue una sequenza di azioni (BLE, IR, LED, special action) riutilizzando esattamente gli stessi comandi salvati nelle combo.

**Campi**:
- **Etichetta**: nome descrittivo mostrato nella UI.
- **Parametri (JSON)**:
  - `actions`: array di stringhe da eseguire in ordine (es. `["S_B:CTRL+c","SEND_IR_tv_off"]`).
  - `release_delay_ms`: ritardo tra press e release simulato per ogni comando (default 30 ms).
  - Metadati opzionali (`source`, `combo`, `label`) per ricordare la provenienza della sequenza.

> Suggerimento: usa il catalogo combo per riempire automaticamente questi campi.

#### Special Action
Esegue una special action registrata nel sistema.

**Campi**:
- **Action ID**: Nome della special action (es. "print_memory_info")
- **Parametri (JSON)**: Parametri opzionali in formato JSON

#### Log Message
Scrive un messaggio nei log.

**Campi**:
- **Messaggio**: Testo da loggare

#### Enter Sleep
Forza il dispositivo in deep sleep.

**Campi**:
- **Valore**: "enter" per confermare

## Operazioni Disponibili

### Creare un Nuovo Evento

1. Clicca sul blocco **"Nuovo Evento"** nella palette laterale
2. Un nuovo blocco evento apparirà nel canvas
3. Modifica il nome dell'evento cliccando sul campo in alto
4. Configura le opzioni, il trigger e l'azione

### Duplicare un Evento

1. Clicca sull'icona **"Duplica"** (📋) nell'header del blocco
2. Verrà creata una copia completa dell'evento

### Eliminare un Evento

1. Clicca sull'icona **"Elimina"** (🗑️) nell'header del blocco
2. Conferma la cancellazione nel dialog

### Comprimere/Espandere un Blocco

1. Clicca sull'icona **"Comprimi"** (⬆️/⬇️) nell'header del blocco
2. Il contenuto verrà nascosto/mostrato

### Esportare la Configurazione

1. Clicca su **"Esporta JSON"** nella toolbar
2. Verrà scaricato un file `scheduler_config.json` con la configurazione completa

### Importare una Configurazione

1. Clicca su **"Importa JSON"** nella toolbar
2. Seleziona un file JSON precedentemente esportato
3. Gli eventi verranno caricati nel canvas

### Salvare sul Dispositivo

1. Clicca su **"Salva configurazione"** nella toolbar
2. La configurazione verrà inviata al dispositivo (endpoint `/scheduler.json`)
3. Il dispositivo si riavvierà automaticamente per applicare le modifiche

> Nota: lo scheduler ora è salvato in un file dedicato (`/scheduler.json`) separato da `config.json`. Questo permette di eseguire backup o modifiche rapide senza toccare il resto della configurazione. L'editor carica e salva direttamente quel file e il firmware lo importa automaticamente all'avvio.

## Struttura JSON Generata

L'editor genera una configurazione completa dello scheduler nel formato richiesto dal backend:

```json
{
  "enabled": false,
  "prevent_sleep_if_pending": true,
  "sleep_guard_seconds": 45,
  "wake_ahead_seconds": 900,
  "timezone_minutes": 0,
  "poll_interval_ms": 250,
  "events": [
    {
      "id": "event_1234567890_abc123",
      "description": "Esempio evento",
      "enabled": true,
      "prevent_sleep": false,
      "wake_from_sleep": false,
      "run_on_boot": false,
      "one_shot": false,
      "trigger": {
        "type": "interval",
        "interval_ms": 60000,
        "jitter_ms": 0
      },
      "action": {
        "type": "macro",
        "id": "combo_common · 1+2",
        "params": {
          "actions": ["S_B:CTRL+c", "S_B:CTRL+v"],
          "release_delay_ms": 30,
          "source": "combo_common.json",
          "combo": "1+2"
        }
      }
    }
  ]
}
```

## Esempi di Configurazione

### Esempio 1: Backup giornaliero

Esegue un backup ogni giorno alle 3:00 AM:

```json
{
  "id": "daily_backup",
  "description": "Backup giornaliero automatico",
  "enabled": true,
  "prevent_sleep": true,
  "wake_from_sleep": true,
  "run_on_boot": false,
  "one_shot": false,
  "trigger": {
    "type": "time_of_day",
    "hour": 3,
    "minute": 0,
    "second": 0,
    "days_mask": 127,
    "use_utc": false
  },
  "action": {
    "type": "special_action",
    "id": "create_backup",
    "params": "{\"compress\": true}"
  }
}
```

### Esempio 2: Monitoraggio periodico

Controlla la memoria libera ogni 5 minuti:

```json
{
  "id": "memory_monitor",
  "description": "Controllo memoria ogni 5 minuti",
  "enabled": true,
  "prevent_sleep": false,
  "wake_from_sleep": false,
  "run_on_boot": true,
  "one_shot": false,
  "trigger": {
    "type": "interval",
    "interval_ms": 300000,
    "jitter_ms": 10000
  },
  "action": {
    "type": "special_action",
    "id": "print_memory_info",
    "params": ""
  }
}
```

### Esempio 3: Azione su input

Esegue un'azione quando viene premuto il pulsante dell'encoder:

```json
{
  "id": "encoder_button_action",
  "description": "Azione su pulsante encoder",
  "enabled": true,
  "prevent_sleep": false,
  "wake_from_sleep": true,
  "run_on_boot": false,
  "one_shot": false,
  "trigger": {
    "type": "input",
    "input_source": "encoder",
    "input_type": "button",
    "input_value": -1,
    "input_state": 1,
    "input_text": ""
  },
  "action": {
    "type": "special_action",
    "id": "toggle_mode",
    "params": ""
  }
}
```

## Note Tecniche

### Validazione

L'editor valida automaticamente:
- Formati numerici (intervalli, ore, minuti)
- Sintassi JSON nei parametri delle azioni
- Presenza di campi obbligatori

### Compatibilità

La configurazione generata è compatibile con:
- EventScheduler backend (C++)
- Config Manager per la persistenza
- Tutte le special actions registrate nel sistema

### Limitazioni

- La pagina carica la configurazione esistente al primo accesso
- Il salvataggio sovrascrive completamente la sezione scheduler in config.json
- Non è possibile modificare le configurazioni globali dello scheduler (timezone, poll_interval, ecc.) dall'editor visuale

## Integrazione con il Sistema

L'editor interagisce con i seguenti endpoint:

- **GET `/config.json`**: Carica la configurazione esistente
- **POST `/config.json`**: Salva la nuova configurazione (richiede riavvio)

## Troubleshooting

### L'editor non carica gli eventi esistenti

1. Verifica che il file `/config.json` contenga una sezione `scheduler`
2. Controlla la console del browser per errori di parsing
3. Verifica che il formato JSON sia valido

### Il salvataggio fallisce

1. Verifica che il dispositivo sia raggiungibile
2. Controlla che non ci siano errori di validazione nei campi
3. Verifica che i parametri JSON siano validi

### Gli eventi non vengono eseguiti

1. Verifica che lo scheduler sia abilitato in `config.json` (`scheduler.enabled = true`)
2. Controlla che l'evento sia abilitato
3. Verifica che il trigger sia configurato correttamente
4. Consulta i log del dispositivo per errori di esecuzione

## Riferimenti

- **Backend**: [EventScheduler.cpp](../lib/eventScheduler/EventScheduler.cpp)
- **Tipi**: [configTypes.h](../lib/configManager/configTypes.h)
- **Stato Scheduler**: [scheduler.html](scheduler.html)
- **Special Actions**: [SpecialActionRouter.h](../lib/common/SpecialActionRouter.h)
