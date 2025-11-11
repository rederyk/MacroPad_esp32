# Guida alla Sintassi dei Comandi

## Nuova Sintassi Semplificata (Array-based)

### Formato Base

```json
{
  "combo_name": [
    "command1",
    "command2",
    "command3"
  ]
}
```

### Regole Fondamentali

1. **Array JSON**: Ogni elemento dell'array è un comando eseguito **sequenzialmente** (uno dopo l'altro con auto-release)
2. **Separatore `+`**: All'interno di un comando `S_B:`, il `+` indica tasti premuti **simultaneamente**
3. **Escape `\+`**: Usa `\+` solo per il carattere `+` letterale nel testo (raro)
4. **Niente più virgola**: Il separatore `,` NON serve più! Usa array JSON per comandi sequenziali

---

## Esempi Pratici

### Comando Singolo
```json
"2+8": [
  "S_B:SUPER+q"
]
```
**Risultato**: Premi SUPER e 'q' insieme

---

### Comandi Sequenziali
```json
"2+5+8": [
  "S_B:SUPER+t",
  "DELAY_500",
  "S_B:btop",
  "DELAY_500",
  "S_B:RETURN"
]
```
**Risultato**:
1. Premi SUPER+t (apre terminale)
2. Aspetta 500ms
3. Scrive "btop"
4. Aspetta 500ms
5. Premi ENTER

---

### Tasti Combinati
```json
"1+5": [
  "S_B:SUPER+t",
  "S_B:SUPER+w",
  "S_B:SUPER+c"
]
```
**Risultato**: Esegue 3 combo separate in sequenza

---

### Testo con Caratteri Speciali
```json
"3+5": [
  "S_B:Hello World!",
  "S_B:2+2=4",
  "S_B:path,file",
  "S_B:C++ programming"
]
```
**Risultato**:
1. Scrive "Hello World!"
2. Scrive "2+2=4" (+ nel testo non serve escape!)
3. Scrive "path,file" (, nel testo non serve escape!)
4. Scrive "C++ programming"

---

### Escape Solo Quando Necessario
```json
"test": [
  "S_B:CTRL+ALT",       // + separa tasti = combo
  "S_B:2\\+2",          // \+ letterale = scrive "2+2"
  "S_B:formula: a\\+b"  // \+ nel testo = scrive "formula: a+b"
]
```

⚠️ **Nota**: L'escape `\+` serve SOLO se hai un `+` in una posizione ambigua (es. dopo un token speciale). Nella maggior parte dei casi il testo normale NON richiede escape!

---

### Comandi Sequenziali (Prima con `,` - Ora con Array)
**VECCHIO (deprecato)**:
```json
"combo": [
  "S_B:CTRL+c,CTRL+v"   // ❌ Sintassi vecchia
]
```

**NUOVO (raccomandato)**:
```json
"combo": [
  "S_B:CTRL+c",         // ✅ Premi CTRL+c
  "S_B:CTRL+v"          // ✅ Poi premi CTRL+v
]
```
**Risultato**: Esegue CTRL+c, rilascia, poi CTRL+v con delay automatico tra i comandi.

---

## Tipi di Comandi

### 1. BLE Keyboard (S_B:)
```json
"S_B:comando"
```
- **Tasti speciali**: CTRL, SHIFT, ALT, SUPER, F1-F24, UP_ARROW, DOWN_ARROW, LEFT_ARROW, RIGHT_ARROW, etc.
- **Combo di tasti**: Usa `+` per premere tasti simultaneamente (es. `CTRL+c`)
- **Testo normale**: Scrivi direttamente, i caratteri `+` e `,` nel testo sono automaticamente riconosciuti
- **Escape raro**: `\+` solo se hai ambiguità (es. dopo un token speciale)

### 2. Delay
```json
"DELAY_500"        // 500ms
"DELAY_1000"       // 1 secondo
```

### 3. Altri Comandi
```json
"GYROMOUSE_TOGGLE"
"RESET_ALL"
"LED_RGB_255_0_0"
"SWITCH_COMBO_2"
```

---

## Migrazione dalla Vecchia Sintassi

### 1. Da `<>` brackets ad Array JSON
**PRIMA (deprecato)**:
```json
"2+5+8": [
  "<S_B:SUPER+t><DELAY_500><S_B:btop><DELAY_500><S_B:RETURN>"
]
```

**DOPO**:
```json
"2+5+8": [
  "S_B:SUPER+t",
  "DELAY_500",
  "S_B:btop",
  "DELAY_500",
  "S_B:RETURN"
]
```

### 2. Da separatore `,` ad Array JSON
**PRIMA (deprecato)**:
```json
"combo": [
  "S_B:CTRL+SUPER,LEFT_ARROW"
]
```

**DOPO**:
```json
"combo": [
  "S_B:CTRL+SUPER",
  "S_B:LEFT_ARROW"
]
```

### 3. Escape semplificato
**PRIMA (deprecato)**:
```json
"test": [
  "S_B:testo con ,, e ++ caratteri"  // ,, = virgola, ++ = plus
]
```

**DOPO**:
```json
"test": [
  "S_B:testo con , e + caratteri"  // Nessun escape necessario!
]
```

Solo se hai ambiguità:
```json
"test": [
  "S_B:CTRL\\+ALT letterale"  // Se vuoi scrivere "CTRL+ALT" come testo
]
```

---

## Vantaggi della Nuova Sintassi

✅ **Più chiara**: Ogni comando su una riga separata
✅ **Più semplice**: Niente più separatore `,` dentro S_B:
✅ **Meno escape**: Il 99% del testo non richiede escape
✅ **Più facile da editare**: Facile aggiungere/rimuovere comandi
✅ **Niente parsing complesso**: Rimosso tutto il codice legacy
✅ **JSON nativo**: Standard, pulito, manutenibile

---

## Note Tecniche

### Esecuzione
- **Array con 1 elemento**: Eseguito immediatamente
- **Array con >1 elementi**: Accodati ed eseguiti in sequenza con delay automatico (200ms default)
- **Auto-release**: Tra un comando e l'altro viene fatto automaticamente release

### Parsing
- **Split per `+`**: Solo all'interno di `S_B:` per combo di tasti
- **Escape `\+`**: Processato DOPO lo split, solo quando serve
- **Niente `,`**: Rimosso completamente il parsing dei gruppi con virgola

### Compatibilità
- ❌ **Sintassi `<>` rimossa**: Non più supportata
- ❌ **Escape `,,` e `++` rimosso**: Non più necessario
- ✅ **Solo `\+` quando serve**: Escape minimale e chiaro
