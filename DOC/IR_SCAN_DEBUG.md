# Debug IR SCAN Mode - Guida alla diagnostica

## Logging aggiunto

Ho aggiunto logging dettagliato in tutti i punti chiave del flusso IR Scan. Tutti i log usano il prefisso `[IR SCAN]` o `[IR PROCESSING]` per facilitare il filtering nella console.

## Punti di controllo

### 1. Inizializzazione (al caricamento della pagina)

**Cosa verificare nella console:**
```
[IR SCAN] Attaching event listener to startScanMode button
[IR SCAN] Attaching event listener to stopScanMode button
[IR SCAN] Attaching event listener to clearScannedCodes button
[IR SCAN] Attaching event listener to saveScannedCodes button
[IR SCAN] Attaching event listener to scannedCodesContainer
[IR SCAN] Attaching event listener to scanTargetDevice select
```

**Se vedi errori tipo:**
```
[IR SCAN] startScanMode button not found in DOM!
```
Significa che gli elementi HTML non sono stati trovati. Verifica che gli ID nel DOM corrispondano a quelli nel codice.

### 2. Apertura del tab IR Scan

**Cosa verificare nella console:**
```
[IR SCAN] IR Scan tab activated, syncing device select
[IR SCAN] syncScanDeviceSelect() called
[IR SCAN] Available devices: ["tv", "led"]
[IR SCAN] Device select synced, current value: ""
```

### 3. Click sul pulsante "Avvia acquisizione"

**Cosa verificare nella console:**
```
[IR SCAN] startScanMode() called
[IR SCAN] DOM elements check: {scanNewDeviceName: true, scanTargetDevice: true, ...}
[IR SCAN] Input values: {newDevice: "", selectedDevice: "tv"}
[IR SCAN] Using selected device: tv
[IR SCAN] Final target device: tv
[IR SCAN] State updated: {active: true, targetDevice: "tv", scannedCodes: []}
[IR SCAN] UI updated, scan mode is now active
```

**Possibili errori:**
- `[IR SCAN] No device selected or created` - Nessun dispositivo selezionato
- `[IR SCAN] Invalid device name after sanitization` - Nome dispositivo non valido

### 4. Ricezione codici IR dal log stream

**Cosa verificare nella console quando premi un tasto del telecomando:**
```
[IR PROCESSING] Potential IR message detected: {"protocol":"NEC","bits":32,"value":"f7c03f"}
[IR PROCESSING] JSON found: {"protocol":"NEC","bits":32,"value":"f7c03f"}
[IR PROCESSING] Parsed IR data: {protocol: "NEC", bits: 32, value: "f7c03f"}
[IR PROCESSING] Valid IR data, processing...
[IR SCAN] processScannedIrCodeForTab() called with: {protocol: "NEC", bits: 32, ...}
[IR SCAN] state.irScan.active: true
[IR SCAN] Created code object: {protocol: "NEC", bits: 32, name: "cmd_1", ...}
[IR SCAN] Total scanned codes: 1
[IR SCAN] IR code added to scan tab: {...}
```

**Possibili problemi:**

1. **Nessun messaggio `[IR PROCESSING]`**
   - Il firmware non sta inviando dati IR al log stream
   - L'EventSource non è connesso
   - Il formato dei messaggi IR non corrisponde al pattern atteso

2. **`[IR PROCESSING] No JSON found in message`**
   - Il messaggio contiene "IR:" o "protocol" ma non JSON valido
   - Verificare il formato dei messaggi dal firmware

3. **`[IR PROCESSING] Missing protocol or bits`**
   - Il JSON è valido ma mancano campi essenziali
   - Verificare che il firmware invii almeno `protocol` e `bits`

4. **`[IR SCAN] Scan mode not active, ignoring IR code`**
   - Il codice IR è stato ricevuto ma lo scan mode non è attivo
   - Verifica che `state.irScan.active` sia `true`

## Procedura di test

1. Apri la console del browser (F12)
2. Filtra i log per `[IR SCAN]` o `[IR PROCESSING]`
3. Ricarica la pagina e verifica i log di inizializzazione
4. Vai al tab "IR Scan"
5. Seleziona o crea un dispositivo
6. Clicca su "Avvia acquisizione"
7. Premi un tasto del telecomando
8. Verifica che il codice appaia nella lista

## Formato messaggi IR richiesto dal firmware

Il firmware DEVE inviare messaggi in uno di questi formati al log stream:

**Formato 1 (preferito):**
```
IR: {"protocol":"NEC","bits":32,"value":"f7c03f","address":247,"command":49215}
```

**Formato 2:**
```
{"protocol":"NEC","bits":32,"value":"f7c03f"}
```

I campi **obbligatori** sono:
- `protocol` (string)
- `bits` (number)

Campi opzionali ma consigliati:
- `value` (string hex senza 0x)
- `address` (number)
- `command` (number)
- `device` (number)
- `subdevice` (number)
- `raw` (array di numeri)

## Comandi console utili

Durante il debug, puoi usare questi comandi nella console:

```javascript
// Verifica stato corrente
console.log(state.irScan);

// Simula ricezione IR (per test senza hardware)
processScannedIrCodeForTab({
  protocol: "NEC",
  bits: 32,
  value: "f7c03f",
  address: 247,
  command: 49215
});

// Verifica elementi DOM
console.log(dom.startScanMode);
console.log(dom.scanTargetDevice);

// Forza avvio scan mode (per test)
state.irScan.active = true;
state.irScan.targetDevice = "test_device";
```

## Checklist troubleshooting

- [ ] Tutti gli event listener sono stati attaccati (nessun errore in console)
- [ ] Il tab IR Scan si apre correttamente
- [ ] La select dei dispositivi viene popolata
- [ ] Il pulsante "Avvia acquisizione" diventa disabilitato dopo il click
- [ ] Il badge cambia da "Inattivo" a "Attivo"
- [ ] Il firmware sta effettivamente inviando dati IR (verifica con monitor seriale)
- [ ] L'EventSource è connesso (verifica in Network tab del browser)
- [ ] I messaggi IR vengono intercettati da `processLogMessage`
- [ ] `state.irScan.active` è `true` quando premi un tasto del telecomando
- [ ] I codici vengono aggiunti a `state.irScan.scannedCodes`
- [ ] L'UI viene aggiornata con i codici acquisiti

## Prossimi passi

Una volta identificato il problema attraverso i log:

1. Se **gli elementi DOM non vengono trovati**: verifica gli ID nell'HTML
2. Se **startScanMode non viene chiamato**: verifica che l'event listener sia attaccato
3. Se **i messaggi IR non vengono intercettati**: verifica il formato dei messaggi dal firmware
4. Se **processScannedIrCodeForTab viene chiamato ma non aggiunge codici**: verifica che `state.irScan.active` sia true
5. Se **i codici vengono aggiunti ma l'UI non si aggiorna**: problema nel rendering

Esegui il test e condividi l'output completo della console per identificare il punto di failure.
