
# Identified Improvements

The following improvements have been identified in the code:

1.  **String Usage:** Substitute Arduino `String` with `std::string` for better memory management and performance.
2.  **Hardcoded Numbers:** Substitute hardcoded numbers with `static const` to improve readability and maintainability.


# TODO

- memoria si riempie con le gesture , migliora la gestione del buffer senza farlo crashare

- accelerometro mpu implementato e testato ma,  necessita di aggiunta feature esclusive gyro e temp ,perche ce la temp a quanto pare, inoltre dovremmo ritestare adxl per sapere se va ancora dopo le modifiche


## impostare combo_timout dalle config[]


## decidere un modo consistente su cosa fare con comma ","e "+" nelle macro []


## problema CASE sensitive duro a morire
       /// il problema dovrebbe essere nei press multpili ,
          // quando ce un carattere CASE blecombo usa in contemporanea lo SHIFT ma,
          // quando si preme due tasti con lo shift insieme succedono guai
          // sene trova prova nei singili caratteri che se premuti molto veloce diventano lowercase, altrimenti vanno bene..
          //soluzione controlla e gestisci lo shift localamente assegnando ad ogni carattere un valore UP_CASE true o false 
          //e convertendo la stringa in minuscolo (vale anche per i simboli..:-( per poi premere la giusta combinazione
        // TIenI CONTO DEL LAYOUT E FAI DELLE PROVE CON QUELLO ITA
          // impostare anche una variabile per scegliere il layout?????? indagare se blecombo supporta i layout .... 

## Se cambi nome e resta un device abbinato con il nome vecchio causa un riavvio , bisogna riabbinare il device da capo ,.....forse e un problema solo del mio pc

## memoria limitata json per macro ,dividere il file per ongi set e usare dynamic memory,

## poca reattivita o tasti sovraposti nel tempo ,indagare
## BATTERY USAGE...provare la sleep_mode con diverse batterie 
  - lipo 3.7V 820mAh ~2 weeks (ble_mode ,sleep 50s)



## Singolo led RGB per feedback modalita....
- implementato sistema di log Led RGB con metodi di save ripristino e colori effimeri [V]
- mappato sampling xyz delle gesture al led rgb CI STA....[V]
- impostati pin e opt dal config.json [V]
* creare funzioni utili ai pattern fade blink rainbow eccetera 
* impostare colori impostabili nel config per le modalita BLE AP STA 

## cerca di usare il keypad per il wakeup meglio su pressioni ripetute...
- usare un pin di rotazione dell encoder vale usarli entrambi per via del debounce solo software (FEATURE??) 
* necessaria modifica hardware pin dedidacato con diodo collegato a tutto il keypad 

### Possibili soluzioni 

 
2. **Metodo “singolo pin” (ext0) + diodo/OR:** 
 
  - Se vuoi svegliare l’ESP32 premendo **qualsiasi**  tasto, spesso si usa un trucco hardware: si uniscono tutte le linee del keypad tramite diodi in un “OR cablato” su un singolo GPIO RTC con pull-up. Premendo *qualunque* tasto, quella linea si porta a LOW e fa scattare `esp_sleep_enable_ext0_wakeup(..., livelloBasso)`.

  - Funziona, ma serve un piccolo adattatore hardware (una serie di diodi) e una resistenza di pull-up; oppure si sfrutta il pull-up interno se la corrente di leakage è accettabile.
 
4. **
Usare ext1 con `ANY_HIGH`** 
 
  - Se le linee “in stato di riposo” possono essere tenute tutte a LOW, e premendo un tasto una di esse va a HIGH, allora puoi abilitare `esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_HIGH)`.
 
  - Devi però **invertire**  il modo di pilotare la tastiera:

    - le righe (o le colonne) che in deep sleep diventano input RTC con pull-down,

    - l’altra dimensione le lasci in output a 3.3V,

    - in modo che, quando premi un tasto, almeno una delle linee di input “sale” da 0V (pull-down) a 3.3V.
 
  - Non sempre è banale, perché devi verificare che tutti i pin coinvolti siano **RTC**  e che tu non abbia “conflitti” con altre funzioni hardware.
 
6. **Uso del co-processore ULP** 

  - L’ULP (Ultra Low Power) dell’ESP32 può rimanere attivo durante il deep sleep e fare un mini-scan della tastiera, con un consumo nettamente inferiore rispetto alla CPU principale (ma pur sempre maggiore rispetto allo standby totale).

  - In quel caso, puoi programmare l’ULP per impostare alcune linee in uscita e altre in ingresso, e se rileva un cambio di stato, può svegliare la CPU.

  - È il sistema più flessibile, ma anche il più complesso da implementare, perché devi scrivere un piccolo programma in “assembly ULP” o usare una libreria apposita.



# DOIT
* nel ble, sevono caratteri di escape ","e "+" , e i CAPS per tutti INDAGARE cilco perss per singoli caratteri stringhe
        escape funzionate con doppio carattere ++ o,, [V]


* una SPecialAction che semplicemnte aspetta sarebbe molto utile nei comandi concatenati,
  "2+5+8"["<S_B:SUPER+t><TIME_SLEEP_1000><S_B:btop><TIME_SLEEP_500><S_B:RETURN>"], [V]

  
- metti una spacial action per andare in sleep oltre al timeout..
  "1+2+3+4+5+6+7+8+9": ["ENTER_SLEEP"] [V]

- sostituiti delay  e timing con xtaskdealay [V]

* comuqnue va...

###
* special action per controllare colore e iintesita del led dal keypad con combo, esempio cw+1 rosso++ ccw+1 red-- , 3+6+button red255 green255 blue255 per la torcia

* pagina html per tutti i json

* mouse gyro mode con combo separate

