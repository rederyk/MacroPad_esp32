# Combo Settings Documentation

## Overview
Ogni file combo JSON può ora includere un campo `_settings` che permette di personalizzare il comportamento del combo set, come il colore del LED quando si switcha a quel combo.

## Struttura del campo _settings

Il campo `_settings` è un oggetto JSON speciale che inizia con underscore per evitare conflitti con le combinazioni di tasti normali.

### Formato Base

```json
{
  "_settings": {
    "led_color": [R, G, B]
  },
  "1": ["S_B:a"],
  "2": ["S_B:b"],
  ...
}
```

## Parametri Disponibili

### led_color
- **Tipo**: Array di 3 numeri interi
- **Range**: 0-255 per ogni componente RGB
- **Descrizione**: Imposta il colore del LED che verrà mostrato quando si switcha a questo combo file
- **Default**: Verde (0, 255, 0) se non specificato

#### Esempi di Colori Comuni

```json
"led_color": [255, 0, 0]      // Rosso
"led_color": [0, 255, 0]      // Verde
"led_color": [0, 0, 255]      // Blu
"led_color": [255, 255, 0]    // Giallo
"led_color": [255, 0, 255]    // Magenta
"led_color": [0, 255, 255]    // Cyan
"led_color": [255, 255, 255]  // Bianco
"led_color": [255, 128, 0]    // Arancione
```

## Esempi Completi

### Example 1: Combo con LED Giallo
```json
{
  "_settings": {
    "led_color": [255, 255, 0]
  },
  "1": ["S_B:a"],
  "2": ["S_B:b"],
  "3": ["S_B:c"],
  "1+5": ["<S_B:Hello><S_B:SPACE><S_B:World>"],
  "2+5": ["SWITCH_COMBO_0"]
}
```

### Example 2: Combo con LED Cyan
```json
{
  "_settings": {
    "led_color": [0, 255, 255]
  },
  "1": ["S_B:1"],
  "2": ["S_B:2"],
  "3": ["S_B:3"],
  "CW": ["S_B:PAGE_UP"],
  "CCW": ["S_B:PAGE_DOWN"]
}
```

### Example 3: Combo senza Settings (usa colore default)
```json
{
  "1": ["S_B:TAB"],
  "2": ["S_B:UP_ARROW"],
  "3": ["S_B:SUPER"]
}
```

## Note Importanti

1. **Il campo `_settings` è opzionale**: Se non specificato, verrà usato il colore verde di default
2. **Non crea conflitti**: Il nome `_settings` inizia con underscore e non può essere una combinazione valida di tasti
3. **Viene filtrato dalle combo**: Il sistema automaticamente salta `_settings` quando carica le combinazioni di tasti
4. **Modalità BLE rispettata**: Anche con il colore personalizzato, il sistema continua a rispettare lo stato BLE/WiFi per le notifiche di sistema

## Comportamento del Sistema

Quando fai lo switch a un nuovo combo file:
1. Il sistema carica il file JSON
2. Legge il campo `_settings` (se presente)
3. Estrae il colore LED personalizzato
4. Carica tutte le combinazioni (escludendo `_settings`)
5. Mostra il LED con il colore personalizzato per 150ms come feedback visivo
6. Il colore del LED torna poi al colore di sistema appropriato (blu per BLE, ecc.)

## Troubleshooting

### Il colore non cambia
- Verifica che il campo sia scritto correttamente: `"_settings"` (con underscore)
- Controlla che `led_color` sia un array di 3 numeri
- Verifica che i valori RGB siano tra 0 e 255

### Il combo non si carica
- Verifica la sintassi JSON (virgole, parentesi)
- Assicurati che `_settings` sia un oggetto valido
- Controlla i log seriali per messaggi di errore

## Files di Esempio

I file `my_combo_0.json` e `my_combo_1.json` nella cartella `data/` contengono esempi completi di utilizzo del campo `_settings`.
