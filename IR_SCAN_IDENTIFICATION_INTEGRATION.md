# Integrazione Identificazione IRDB nel Tab IR Scan

## Modifiche Implementate

Ho spostato e integrato la funzionalità di identificazione automatica IRDB dal tab "Gestione IR" al tab "IR Scan", creando un **flusso unificato** per acquisizione + identificazione.

## Problema Risolto

**Prima**:
- Card "Identificazione automatica" nel tab IR con due metodi:
  - "Scansiona nuovi codici" → **NON funzionava** (aspettava codici che non arrivavano)
  - "Usa comandi esistenti" → Funzionava ma su dispositivi già configurati

**Dopo**:
- Card "Identificazione automatica IRDB" integrata nel tab "IR Scan"
- "Usa codici acquisiti" → Usa i codici già scansionati nella lista sopra
- "Usa comandi salvati" → Come prima, per dispositivi esistenti
- Flusso naturale: **Scan → Identifica → Importa**

## File Modificati

### `/data/special_actions.html`

#### 1. Rimossa card identificazione dal tab IR (riga 503)
```html
<!-- Card identificazione rimossa - spostata nel tab IR Scan -->
```

#### 2. Aggiunta card identificazione nel tab IR Scan (righe 668-700)
```html
<article class="card" id="irIdentificationCard">
  <div class="card-header">
    <div>
      <h3 class="card-title">
        <i class="fas fa-fingerprint"></i>Identificazione automatica IRDB
      </h3>
      <p class="card-subtitle">
        Trova automaticamente il dataset IRDB migliore per i tuoi codici
      </p>
    </div>
    <span class="badge info" id="identificationBadge">Pronto</span>
  </div>

  <div class="form-group">
    <label>Come identificare il dispositivo</label>
    <div class="flex-row">
      <button class="secondary-btn" id="identifyFromScan" title="Usa i codici acquisiti sopra per trovare il dataset">
        <i class="fas fa-radar"></i>Usa codici acquisiti
      </button>
      <button class="secondary-btn" id="identifyFromProfile" title="Usa i comandi già salvati per identificare il dataset">
        <i class="fas fa-chart-bar"></i>Usa comandi salvati
      </button>
    </div>
  </div>

  <div id="identificationStatus" class="muted">
    Acquisisci almeno 2-3 codici IR e poi clicca "Usa codici acquisiti" per identificare automaticamente il dataset IRDB migliore.
  </div>

  <div id="identificationResults">
    <!-- Popolato dinamicamente con i risultati -->
  </div>
</article>
```

#### 3. Riscritta funzione `identifyDeviceFromScan()` (righe 2507-2609)

**PRIMA** (non funzionante):
```javascript
async function identifyDeviceFromScan() {
  showToast("Modalità scansione: premi 2-3 tasti del telecomando...", "info");
  state.irFingerprinting.scanning = true;
  state.irFingerprinting.scannedCodes = [];
  updateIdentificationBadge("In attesa codici...", "info");
  updateIdentificationStatus("Premi 2-3 tasti sul telecomando...");
  // NON faceva nulla - aspettava codici che non arrivavano
}
```

**DOPO** (funzionante):
```javascript
async function identifyDeviceFromScan() {
  // Usa i codici già acquisiti nel tab IR Scan
  if (!state.irScan.scannedCodes || state.irScan.scannedCodes.length < 2) {
    showToast("Acquisisci almeno 2-3 codici IR prima di identificare.", "error");
    return;
  }

  // Crea fingerprints dai codici acquisiti
  const fingerprints = state.irScan.scannedCodes.map((code) =>
    generateFingerprint(code, { type: "scan" })
  );

  state.irFingerprinting.identifying = true;
  updateIdentificationBadge("Identificazione...", "info");
  updateIdentificationStatus(`Analizzando ${fingerprints.length} codici acquisiti...`);

  await loadIrdbIndex();

  if (!state.irdb.index.length) {
    showToast("Indice IRDB non disponibile. Caricalo prima dal tab IR.", "error");
    state.irFingerprinting.identifying = false;
    updateIdentificationBadge("Errore", "error");
    return;
  }

  // Determina protocollo dominante
  const protocolCounts = {};
  fingerprints.forEach((fp) => {
    protocolCounts[fp.protocol] = (protocolCounts[fp.protocol] || 0) + 1;
  });

  const dominantProtocol = Object.keys(protocolCounts).sort(
    (a, b) => protocolCounts[b] - protocolCounts[a]
  )[0];

  // Filtra candidati IRDB per categoria/protocollo
  const categoryHints = {
    NEC: ["tv", "audio", "video", "projector"],
    SAMSUNG: ["tv", "audio", "video"],
    SONY: ["tv", "audio", "video", "camera"],
    RC5: ["tv", "audio", "video"],
    RC6: ["tv", "audio", "video"],
  };

  let candidateEntries = state.irdb.index.filter((entry) => {
    const hints = categoryHints[dominantProtocol] || [];
    return hints.some((cat) =>
      entry.category?.toLowerCase().includes(cat.toLowerCase())
    );
  });

  // Fallback se nessun candidato
  if (candidateEntries.length === 0) {
    candidateEntries = state.irdb.index.filter(
      (entry) =>
        entry.category?.toLowerCase().includes("tv") ||
        entry.category?.toLowerCase().includes("audio") ||
        entry.category?.toLowerCase().includes("video")
    );
  }

  const testLimit = Math.min(50, candidateEntries.length);
  const matches = [];

  // Testa ogni dataset candidato
  for (let i = 0; i < testLimit; i++) {
    const entry = candidateEntries[i];
    const dataset = await loadIrdbDatasetQuiet(entry);
    const matchScore = scoreDatasetMatch(fingerprints, dataset);

    if (matchScore.matchCount >= 1) {
      matches.push({ entry, ...matchScore });
    }

    if (i % 10 === 0) {
      updateIdentificationStatus(`Testati ${i + 1}/${testLimit} dataset...`);
    }
  }

  state.irFingerprinting.identifying = false;
  matches.sort((a, b) => b.confidence - a.confidence || b.score - a.score);
  state.irFingerprinting.matches = matches.slice(0, 5);

  if (matches.length > 0) {
    updateIdentificationBadge(`${matches.length} trovati`, "success");
    updateIdentificationStatus(
      `Identificazione completata! Trovati ${matches.length} dataset compatibili.`
    );
    showToast(`Trovati ${matches.length} dataset compatibili!`, "success");
  } else {
    updateIdentificationBadge("Nessun match", "error");
    updateIdentificationStatus("Nessun dataset compatibile trovato.");
    showToast("Nessun dataset IRDB compatibile trovato.", "error");
  }

  renderIdentificationResults();
}
```

#### 4. Abilitazione dinamica pulsante identificazione

**In `processScannedIrCodeForTab()`** (righe 3073-3081):
```javascript
// Abilita identificazione se abbiamo almeno 2 codici
if (state.irScan.scannedCodes.length >= 2) {
  if (dom.identifyFromScan) {
    dom.identifyFromScan.disabled = false;
  }
  updateIdentificationStatus(
    `${state.irScan.scannedCodes.length} codici acquisiti. Clicca "Usa codici acquisiti" per identificare il dataset IRDB.`
  );
}
```

**In `clearScannedCodes()`** (righe 2954-2957):
```javascript
if (dom.identifyFromScan) dom.identifyFromScan.disabled = true;
updateIdentificationStatus(
  "Acquisisci almeno 2-3 codici IR e poi clicca 'Usa codici acquisiti' per identificare automaticamente il dataset IRDB migliore."
);
```

**In `handleScannedCodeAction()` - delete** (righe 3039-3045):
```javascript
if (state.irScan.scannedCodes.length === 0) {
  if (dom.saveScannedCodes) dom.saveScannedCodes.disabled = true;
  if (dom.identifyFromScan) dom.identifyFromScan.disabled = true;
}
if (state.irScan.scannedCodes.length < 2 && dom.identifyFromScan) {
  dom.identifyFromScan.disabled = true;
}
```

## Nuovo Flusso Utente

### 1. Acquisizione Codici
```
1. User apre tab "IR Scan"
2. Seleziona dispositivo destinazione (o crea nuovo)
3. Clicca "Avvia acquisizione"
   ↓
4. LED lampeggia rosso
5. User preme tasti telecomando
   ↓
6. Ogni codice acquisito appare nella lista
7. Status update: "2 codici acquisiti"
   ↓
8. Pulsante "Usa codici acquisiti" si ABILITA automaticamente ✅
```

### 2. Identificazione Automatica
```
9. User clicca "Usa codici acquisiti"
   ↓
10. Sistema analizza i codici acquisiti
11. Badge: "Identificazione..." (blu)
12. Status: "Analizzando 3 codici acquisiti..."
    ↓
13. Sistema testa ~50 dataset IRDB
14. Progress: "Testati 10/50 dataset..."
    ↓
15. Risultati mostrati con:
    - Manufacturer/Device
    - Confidence %
    - Match count
    - Pulsante "Importa dataset"
```

### 3. Importazione Dataset
```
16. User clicca "Importa dataset" su risultato migliore
    ↓
17. Sistema apre il dataset nel tab IR
18. User può selezionare quali comandi importare
19. Clicca "Importa selezionati"
    ↓
20. Comandi aggiunti al dispositivo ✅
```

## Vantaggi dell'Integrazione

### ✅ Flusso Unificato
- **Prima**: Scan e identificazione erano separati, confusione su come farli comunicare
- **Dopo**: Tutto in un unico tab, flusso lineare dall'alto verso il basso

### ✅ Feedback Visivo
- Pulsante identificazione disabilitato fino a quando non hai 2+ codici
- Status dinamico che guida l'utente ("Acquisisci almeno 2-3 codici...")
- Progress durante identificazione

### ✅ Nessun Codice Duplicato
- Usa la stessa lista `state.irScan.scannedCodes`
- Nessuna sincronizzazione manuale necessaria

### ✅ UI Più Pulita
- Tab IR meno affollato
- Funzionalità correlate raggruppate logicamente

## Layout Tab IR Scan (dall'alto verso il basso)

```
┌─────────────────────────────────────────────┐
│ 📡 Modalità acquisizione                    │
│ ├─ Seleziona dispositivo                    │
│ ├─ [Avvia acquisizione] [Ferma]             │
│ └─ Status: "In ascolto..." / LED rosso      │
└─────────────────────────────────────────────┘
           ↓ Codici acquisiti
┌─────────────────────────────────────────────┐
│ 📋 Codici acquisiti (3 codici)              │
│ ├─ cmd_1: NEC 32bit 0xf7c03f               │
│ ├─ cmd_2: NEC 32bit 0xf740bf               │
│ ├─ cmd_3: NEC 32bit 0xf7807f               │
│ └─ [Svuota lista] [Salva tutti]             │
└─────────────────────────────────────────────┘
           ↓ Identifica
┌─────────────────────────────────────────────┐
│ 🔍 Identificazione automatica IRDB          │
│ ├─ [Usa codici acquisiti] ✅ ABILITATO     │
│ ├─ [Usa comandi salvati]                    │
│ ├─ Status: "3 codici. Clicca per ID..."     │
│ └─ Risultati:                                │
│    ├─ Samsung/BN59-01199F (95% confidence)  │
│    ├─ Samsung/BN59-01247A (87% confidence)  │
│    └─ ...                                    │
└─────────────────────────────────────────────┘
```

## Test Suggeriti

### Test 1: Acquisizione + Identificazione
1. Apri tab IR Scan
2. Seleziona/crea dispositivo "test_tv"
3. Avvia acquisizione
4. Premi 3-4 tasti di un telecomando Samsung
5. Verifica che i codici appaiano nella lista
6. Verifica che "Usa codici acquisiti" si abiliti dopo 2 codici
7. Clicca "Usa codici acquisiti"
8. Verifica che trovi dataset Samsung con alta confidence

### Test 2: Svuota Lista
1. Acquisisci alcuni codici
2. Clicca "Svuota lista"
3. Verifica che "Usa codici acquisiti" si disabiliti
4. Verifica status torna a "Acquisisci almeno 2-3 codici..."

### Test 3: Elimina Codici Singoli
1. Acquisisci 3 codici
2. Elimina 1 codice → pulsante rimane abilitato (hai ancora 2)
3. Elimina 1 codice → pulsante si disabilita (rimane solo 1)

### Test 4: Usa Comandi Salvati
1. Nel tab IR, crea dispositivo con 3+ comandi
2. Nel tab IR Scan, clicca "Usa comandi salvati"
3. Verifica che funzioni come prima (non modificato)

## Log Console Attesi

```javascript
// Acquisizione
[IR SCAN] Firmware scan mode activated successfully
[IR PROCESSING] Valid IR data, processing...
[IR SCAN] processScannedIrCodeForTab() called with: {protocol: "NEC", bits: 32, ...}
[IR SCAN] Created code object: {name: "cmd_1", ...}
[IR SCAN] Total scanned codes: 1

[IR SCAN] Total scanned codes: 2
// Pulsante si abilita qui

// Identificazione
Dominant protocol from scan: NEC
Testing 45 candidate datasets
Scan dataset Samsung/BN59-01199F: 3 matches, confidence 95%
Scan dataset Samsung/BN59-01247A: 2 matches, confidence 87%
Testati 10/50 dataset...
Testati 20/50 dataset...
```

## Compilazione

```
RAM:   [==        ]  22.7% (used 74460 bytes from 327680 bytes)
Flash: [======    ]  64.4% (used 2027105 bytes from 3145728 bytes)
========================= [SUCCESS] Took 35.39 seconds =========================
```

**Impact**: Nessun overhead significativo - il codice era già presente, solo spostato e migliorato.

## Conclusioni

✅ **Card identificazione rimossa dal tab IR**
✅ **Card identificazione aggiunta al tab IR Scan**
✅ **`identifyFromScan()` riscritta per usare codici già acquisiti**
✅ **Abilitazione dinamica pulsante basata su numero codici**
✅ **Flusso unificato Scan → Identifica → Importa**
✅ **Compilazione riuscita senza errori**

L'utente ora ha un'esperienza molto più intuitiva:
1. **Scansiona** codici con feedback visivo chiaro
2. **Identifica** automaticamente con un click
3. **Importa** i comandi dal dataset trovato

Tutto in un unico posto, con guidance chiara ad ogni step! 🎯
