
## Identified Improvements

The following improvements have been identified in the code:

1.  **String Usage:** Substitute Arduino `String` with `std::string` for better memory management and performance.
2.  **Hardcoded Numbers:** Substitute hardcoded numbers with `static const` to improve readability and maintainability.
3.  **File Handling:** Create a common method to read, edit, and save files to reduce code duplication and improve maintainability.


# TODO

* conrollare esecuzione gesture

* impostare combo_timout dalle config

* decidere un modo consistente su cosa fare con comma "," nelle macro

* Se cambi nome e resta un device abbinato con il nome vecchio causa un riavvio , bisogna riabbinare il device da capo ,.....forse e un problema solo del mio pc

* Crea una nuova pagina html molto pcarina per impostare le combo in modo visivo e  guidato, la pagina deve Leggere advanced.json  e combination.json repire la mappa del keypad row col e tasti da advanced.json per disegnare un keypad in css/js,  da usare come interfaccia per combiantion.json cosi da poter formare e selzionare la combo (key del json),

*  per il comando da eseguire.(la value della key del json).disporre un menu sottostsnte con tutte le systemAction disponibili,e una tastiera virtuale, con la possibilita di formare tutti i comandi inviabili dal bluethoot con la flag S_B: in automatico ,  non ti soffermare tanto sui nomi dei comandi bluethoot e delle systemaction le inseriremo 
corrette dopo la logica

* il mouse non usa x e y esempio..
```
    "1+4,CW": ["MOUSE_MOVE_0_5_0_0"],
    "1+4,CCW": ["MOUSE_MOVE_0_-5_0_0"],
    "4+7,CW": ["MOUSE_MOVE_-5_0_0_0"],
    "4+7,CCW": ["MOUSE_MOVE_5_0_0_0"],
    NON VA....
```
* BATTERY USAGE... 

* comuqnue va...
