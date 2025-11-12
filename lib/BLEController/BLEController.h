/*
 * ESP32 MacroPad Project
 * Copyright (C) [2025] [Enrico Mori]
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


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
    uint8_t currentMAC[6];
    bool currentMacValid = false;
    char pressed_Char = '\0'; // null character to represent "no character pressed"

    // Mouse button state tracking
    uint8_t mouseButtonsPressed;
    unsigned long lastMouseButtonChangeTime;

    // Funzione privata per stampare il MAC in formato leggibile.
    void printMacAddress(const uint8_t *mac);

public:
    BLEController();
    void init(const String &name);
    // Costruttore: imposta il nome originale e assegna il deviceName alla tastiera.
    explicit BLEController(const String &name);

    // Salva il MAC originale all'avvio.
    void storeOriginalMAC();
    const uint8_t *getOriginalMac() const;
    const uint8_t *getCurrentMac() const;
    static String formatMac(const uint8_t *mac);

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

    // Mouse button state queries
    bool isAnyMouseButtonPressed() const { return mouseButtonsPressed != 0; }
    uint8_t getMouseButtonsPressed() const { return mouseButtonsPressed; }
    unsigned long getTimeSinceLastMouseButtonChange() const;

    // Esegue una serie di test per le funzionalità di tastiera e mouse.
    bool isBleEnabled();
    // const uint8_t* getMediaKeyToken(const String &token);
};

#endif // BLECONTROLLER_H
