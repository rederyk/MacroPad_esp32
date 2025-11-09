## Possibili problemi specifici per Linux Arch

Se dopo aver abilitato il BLE continui ad avere problemi su Arch Linux, potrebbe essere dovuto a:

1. **Problemi con BlueZ su Linux**: Il tuo ESP32 usa la libreria BleCombo che emula una tastiera/mouse HID. Su Linux, a volte c'è bisogno di accoppiare il dispositivo diversamente.
    
2. **Permessi Bluetooth**: Verifica che il tuo utente abbia i permessi corretti su Arch:
    
    ```bash
    sudo usermod -aG lp $USER
    ```
    
3. **Trust del dispositivo**: Dopo aver accoppiato il dispositivo, potrebbe essere necessario "fidarsi" manualmente:
    
    ```bash
    bluetoothctl
    trust <MAC_ADDRESS_DEL_TUO_ESP32>
    ```
    