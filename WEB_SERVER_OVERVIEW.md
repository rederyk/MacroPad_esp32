# Web Server – Panoramica Architetturale

## Struttura generale
- La classe `configWebServer` possiede un `AsyncWebServer` globale sulla porta 80 e un `AsyncEventSource` per lo streaming dei log (`lib/configWebServer/configWebServer.cpp:24-37`).
- In `begin()` monta LittleFS, abilita la modalità web del logger, registra tutte le route e avvia il server (`lib/configWebServer/configWebServer.cpp:306-323`).

## Helper filesystem
- `readConfigFile` / `writeConfigFile` gestiscono lettura e salvataggio di `/config.json`, con log delle dimensioni e fallback `{}` se il file manca (`lib/configWebServer/configWebServer.cpp:40-83`).
- `readComboFile` e `writeComboFile` aggregano le combo iterando su `/combo_*.json`, verificano il suffisso numerico e salvano il documento combinato su disco (`lib/configWebServer/configWebServer.cpp:86-252`).
- `isValidComboFilename` e `getComboFileType` centralizzano la validazione dei nomi e l’etichettatura (per la UI) (`lib/configWebServer/configWebServer.cpp:258-296`).

## Route principali
- **`/status.json`** espone informazioni runtime (WiFi, heap, versione) assemblate dai vari manager (`lib/configWebServer/configWebServer.cpp:330-482`).
- **`/config.json`** GET/POST carica o salva la configurazione persistente, con validazioni e restart opzionale (`lib/configWebServer/configWebServer.cpp:485-601`).
- **`/combo_files.json`** GET elenca tutti i file combo, li legge, aggiunge metadati e ora serializza la risposta in memoria con controlli su overflow/heap per evitare crash (`lib/configWebServer/configWebServer.cpp:623-705`).
- **`/combo_files.json`** POST accetta payload stringa o oggetto JSON, valida nome/contenuto, formatta con ArduinoJson, scrive su LittleFS e programma un restart se necessario (`lib/configWebServer/configWebServer.cpp:707-836`).
- Sezioni successive gestiscono pagine avanzate, IR recording, log stream e asset OTA.

## Pattern di gestione richieste
- Ogni handler logga gli step principali, così console seriale e UI condividono la stessa traccia (`lib/configWebServer/configWebServer.cpp:330-705`).
- Le risposte usano `request->send` per payload ridotti; lo streaming rimane solo dove serve chunking (esportazioni ampie).
- I POST bufferizzano il payload in `String` statiche, con `reserve` esplicito per limitare la frammentazione dell’heap (`lib/configWebServer/configWebServer.cpp:707-736`).

## Integrazione con altri moduli
- Interagisce con `ConfigurationManager`, `ComboManager`, `MacroManager` e `SpecialAction` per applicare le modifiche: ad esempio il salvataggio delle combo può portare a un `ESP.restart()` per coerenza (`lib/configWebServer/configWebServer.cpp:504-601`, `lib/configWebServer/configWebServer.cpp:726-836`).
- `Logger::setWebServerActive(true)` consente ai log di fluire anche nell’EventSource della UI (`lib/configWebServer/configWebServer.cpp:312-321`).

## Opportunità di miglioramento UI
1. Restituire nella GET solo metadati e caricare i contenuti dei file on-demand, riducendo il payload (~9 KB) e la pressione sull’heap.
2. Introdurre paginazione/limiti per gestire meglio eventuali espansioni future della collezione di combo.
3. Fornire errori di validazione strutturati (es. elenco di campi errati) per fornire feedback più ricco nella WebUI.
