
## Identified Improvements

The following improvements have been identified in the code:

1.  **String Usage:** Substitute Arduino `String` with `std::string` for better memory management and performance.
2.  **Hardcoded Numbers:** Substitute hardcoded numbers with `static const` to improve readability and maintainability.
3.  **File Handling:** Create a common method to read, edit, and save files to reduce code duplication and improve maintainability.


# TODO

* conrollare esecuzione gesture.. [V]

* impostare combo_timout dalle config

* decidere un modo consistente su cosa fare con comma ","e "+" nelle macro 

* nel ble, sevono caratteri di escape ","e "+" , e i CAPS per tutti INDAGARE cilco perss per singoli caratteri stringhe


* Se cambi nome e resta un device abbinato con il nome vecchio causa un riavvio , bisogna riabbinare il device da capo ,.....forse e un problema solo del mio pc


* BATTERY USAGE...provare la sleep_mode con diverse batterie 

* comuqnue va...
