#ifndef BLECONTROLLER_H
#define BLECONTROLLER_H

#include <Arduino.h>
#include <esp_system.h>
#include <BLEDevice.h>
#include <BleCombo.h>
#include <BLEServer.h>
#include <BleComboKeyboard.h>
#include <BleComboMouse.h>
#include <cstring>

class BLEController
{
private:
    bool bluetoothEnabled;
    bool connectionLost;
    bool statoPrecedente;
    String originalName;
    uint8_t originalMAC[6];

    // Funzione privata per stampare il MAC in formato leggibile.
    void printMacAddress(const uint8_t *mac);

public:
    // Costruttore: imposta il nome originale e assegna il deviceName alla tastiera.
    BLEController(String name = "My Custom Keyboard");

    // Salva il MAC originale all'avvio.
    void storeOriginalMAC();

    // Avvia il Bluetooth (inizializza tastiera e mouse).
    void startBluetooth();

    // Arresta il Bluetooth.
    void stopBluetooth();

    // Inverte lo stato del Bluetooth.
    void toggleBluetooth();

    // Controlla lo stato della connessione BLE e, se viene persa, riavvia il sistema.
    void checkConnection();

    // Incrementa (o resetta) l'ultimo byte del MAC address.
    void incrementMacAddress(int increment);

    // Modifica il nome del device in base all'incremento.
    void incrementName(int increment);
    void BLExecutor(String action, bool pressed);
    void moveMouse(signed char x, signed char y, signed char wheel, signed char hWheel);

    // Esegue una serie di test per le funzionalità di tastiera e mouse.
    bool isBleEnabled();
    // const uint8_t* getMediaKeyToken(const String &token);
};

#endif // BLECONTROLLER_H