# ESP32 MacroPad

Un MacroPad versatile e completamente personalizzabile basato su ESP32, con riconoscimento gesti, combinazioni di tasti programmabili, controllo IR, illuminazione RGB interattiva e connettività WiFi/BLE.

## Indice

- [Documentazione Completa](#documentazione-completa)
- [Contributi](#contributi)
- [Licenza](#licenza)

---

## Documentazione Completa

Per una guida dettagliata su tutte le funzionalità, l'installazione, la configurazione e la risoluzione dei problemi, consulta i file nella cartella `DOC/`.

- **[INSTRUCTIONS.md](DOC/INSTRUCTIONS.md)** - Guida completa al setup e configurazione hardware/software.
- **[COMANDI.md](DOC/COMANDI.md)** - Riferimento dettagliato di tutti i comandi e la loro sintassi.
- **[COMBO_SETTINGS.md](DOC/COMBO_SETTINGS.md)** - Documentazione sui settaggi avanzati per i file combo.
- **[GESTURE_SYSTEMS_README.md](DOC/GESTURE_SYSTEMS_README.md)** - Panoramica tecnica sul sistema di riconoscimento gesti.
- **[GYROMOUSE_REPORT.md](DOC/GYROMOUSE_REPORT.md)** - Report tecnico sull'implementazione del GyroMouse.
- **[IR_SCAN_DEBUG.md](DOC/IR_SCAN_DEBUG.md)** - Guida alla diagnostica per la modalità IR Scan.
- **[IR_SCAN_IDENTIFICATION_INTEGRATION.md](DOC/IR_SCAN_IDENTIFICATION_INTEGRATION.md)** - Dettagli sull'integrazione dell'identificazione IRDB.
- **[IR_SCAN_IMPLEMENTATION.md](DOC/IR_SCAN_IMPLEMENTATION.md)** - Riepilogo completo dell'implementazione della modalità IR Scan.
- **[SCHEDULER_EDITOR_GUIDE.md](DOC/SCHEDULER_EDITOR_GUIDE.md)** - Guida all'editor dello scheduler.
- **[TODO.md](DOC/TODO.md)** - Roadmap, problemi noti e miglioramenti pianificati.
- **[WEB_SERVER_OVERVIEW.md](DOC/WEB_SERVER_OVERVIEW.md)** - Panoramica architetturale del server web.
- **[DOC_OVERVIEW.md](DOC/DOC_OVERVIEW.md)** - Note specifiche per Linux Arch.

---

## Contributi

Contributi benvenuti! Per contribuire:

1. **Fork** il repository
2. **Crea branch** per la tua feature (`git checkout -b feature/AmazingFeature`)
3. **Commit** le modifiche (`git commit -m 'Add some AmazingFeature'`)
4. **Push** al branch (`git push origin feature/AmazingFeature`)
5. **Apri Pull Request**

**Aree in cui aiutare:**
- Supporto altri board ESP32
- Ottimizzazione memoria gesti
- Nuovi protocolli IR
- Pattern LED avanzati
- Documentazione e traduzioni
- Test su diversi sistemi operativi

---

## Licenza

Questo progetto è rilasciato sotto **GNU General Public License v3.0**.

```
ESP32 MacroPad Project
Copyright (C) 2025 Enrico Mori

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
```

---

## Disclaimer

A causa del processo di sviluppo, il codice può contenere errori, inefficienze e soluzioni non convenzionali. **Usa questo codice a tuo rischio.**

**Importante:**
- Testa le modifiche su hardware non critico
- Fai backup delle configurazioni prima di aggiornamenti
- Alcuni comandi possono interagire con il sistema in modi imprevisti
- La gestione password WiFi in chiaro richiede precauzioni (cambia password AP!)

---

## Ringraziamenti

- **Community ESP32 e Arduino** - Per le eccellenti librerie e documentazione
- **Tutti gli assistenti AI** - GPT-4, Claude, Deepseek, Gemini, Qwen-Coder
- **Contributors** - A tutti coloro che contribuiranno al progetto
- **I gatti** - Per il supporto morale e il testing involontario della tastiera 🐱

---

## Link Utili

- **Repository:** [github.com/rederyk/MacroPad_esp32](https://github.com/rederyk/MacroPad_esp32)
- **Issues:** [Report bug o richiedi feature](https://github.com/rederyk/MacroPad_esp32/issues)
- **Discussions:** [Discussioni e Q&A](https://github.com/rederyk/MacroPad_esp32/discussions)

---

## Contatti

**Autore:** Enrico Mori
**Anno:** 2025
**Versione Documentazione:** 2.0

---

Made with ❤️, 🤖, and 🐱

**Se questo progetto ti è stato utile, lascia una ⭐ su GitHub!**
