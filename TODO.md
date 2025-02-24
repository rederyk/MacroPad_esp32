
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
