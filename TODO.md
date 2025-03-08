
## Identified Improvements

The following improvements have been identified in the code:

1.  **String Usage:** Substitute Arduino `String` with `std::string` for better memory management and performance.
2.  **Hardcoded Numbers:** Substitute hardcoded numbers with `static const` to improve readability and maintainability.
3.  **File Handling:** Create a common method to read, edit, and save files to reduce code duplication and improve maintainability.


# TODO

* conrollare esecuzione gesture.. [V]

* impostare combo_timout dalle config[]

* decidere un modo consistente su cosa fare con comma ","e "+" nelle macro []

* nel ble, sevono caratteri di escape ","e "+" , e i CAPS per tutti INDAGARE cilco perss per singoli caratteri stringhe
        escape funzionate con doppio carattere ++ o,, [V]

* problema CASE sensitive duro a morire       /// il problema dovrebbe essere nei press multpili ,
          // quando ce un carattere CASE blecombo usa in contemporanea lo SHIFT ma,
          // quando si preme due tasti con lo shift insieme succedono guai
          // sene trova prova nei singili caratteri che se premuti molto veloce diventano lowercase, altrimenti vanno bene..
          //soluzione controlla e gestisci lo shift localamente assegnando ad ogni carattere un valore UP_CASE true o false 
          //e convertendo la stringa in minuscolo (vale anche per i simboli..:-( per poi premere la giusta combinazione
        // TIenI CONTO DEL LAYOUT E FAI DELLE PROVE CON QUELLO ITA
          // impostare anche una variabile per scegliere il layout?????? indagare se blecombo supporta i layout .... 



* Se cambi nome e resta un device abbinato con il nome vecchio causa un riavvio , bisogna riabbinare il device da capo ,.....forse e un problema solo del mio pc


* BATTERY USAGE...provare la sleep_mode con diverse batterie 
    - lipo 3.7V 820mAh >1Giorno
- metti una spacial action per andare in sleep oltre al timeout..
- cerca di usare il keypad per il wakeup meglio su pressioni ripetute...

* una SPecialAction che semplicemnte aspetta sarebbe molto utile nei comandi concatenati,
  "2+5+8"["<S_B:SUPER+t><TIME_SLEEP_1000><S_B:btop><TIME_SLEEP_500><S_B:RETURN>"],

  


* Singolo led RGB per feedback modalita....

* comuqnue va...
