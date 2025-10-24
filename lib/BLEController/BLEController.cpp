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


#include "BLEController.h"
#include "Logger.h"

bool BLEController::isBleEnabled()
{
  return bluetoothEnabled;
}
void BLEController::printMacAddress(const uint8_t *mac)
{
  String macStr = "";
  for (int i = 0; i < 6; i++)
  {
    if (mac[i] < 16)
    {
      macStr += "0";
    }
    macStr += String(mac[i], HEX);
    if (i < 5)
    {
      macStr += ":";
    }
  }
  Logger::getInstance().log("New MAC address: " + macStr);
}

BLEController::BLEController(String name)
    : bluetoothEnabled(false),
      connectionLost(false),
      statoPrecedente(false),
      originalName(name)
{
  Keyboard.deviceName = originalName.c_str();
}

void BLEController::storeOriginalMAC()
{
  esp_efuse_mac_get_default(originalMAC);
  Logger::getInstance().log("MAC originale salvato: ", false);
  printMacAddress(originalMAC);
}

void BLEController::startBluetooth()
{
  if (!bluetoothEnabled)
  {
    Keyboard.begin();
    Mouse.begin();
    bluetoothEnabled = true;
    Logger::getInstance().log("Bluetooth started");
  }
  else
  {
    Logger::getInstance().log("Bluetooth already started");
  }
}

void BLEController::stopBluetooth()
{
  if (bluetoothEnabled)
  {
    Mouse.end();
    Keyboard.end();
    bluetoothEnabled = false;
    Logger::getInstance().log("Bluetooth stopped");
  }
  else
  {
    Logger::getInstance().log("Bluetooth not started");
  }
}

void BLEController::toggleBluetooth()
{
  if (bluetoothEnabled)
  {
    stopBluetooth();
  }
  else
  {
    startBluetooth();
  }
}

void BLEController::checkConnection()
{
  bool statoAttuale = Keyboard.isConnected();
  if (statoAttuale != statoPrecedente)
  {
    if (statoAttuale)
    {
      Logger::getInstance().log("Dispositivo BLE CONNESSO");
      connectionLost = false;
    }
    else
    {
      Logger::getInstance().log("Dispositivo BLE DISCONNESSO");
      connectionLost = true;
    }
    statoPrecedente = statoAttuale;
  }
  // if (connectionLost && bluetoothEnabled)
  // {
  //    ESP.restart();
  // }
}

void BLEController::incrementMacAddress(int increment)
{
  if (increment < 0 || increment > 9)
  {
    Logger::getInstance().log("Valore non valido. Utilizzare un valore compreso tra 0 e 9.");
    return;
  }
  uint8_t newMac[6];
  memcpy(newMac, originalMAC, sizeof(newMac));
  if (increment == 0)
  {
    // Reset al MAC originale.
    esp_base_mac_addr_set(newMac);
    Logger::getInstance().log("MAC address reimpostato al valore originale.");
  }
  else
  {
    // Incrementa l'ultimo byte.
    newMac[5] = originalMAC[5] + increment;

    esp_base_mac_addr_set(newMac);
    Logger::getInstance().log("MAC address incrementato di: " + String(increment));
    printMacAddress(newMac);
  }
}

void BLEController::incrementName(int increment)
{
  if (increment < 0 || increment > 9)
  {
    Logger::getInstance().log("Valore non valido. Utilizzare un valore compreso tra 0 e 9.");
    return;
  }
  if (increment == 0)
  {
    Keyboard.deviceName = originalName.c_str();
    Logger::getInstance().log("Nome reimpostato al valore originale.");
  }
  else
  {
    String newDeviceName = originalName + "_" + String(increment);
    Keyboard.deviceName = newDeviceName.c_str();
    Logger::getInstance().log("Device name changed to: " + newDeviceName);
  }
}

// Funzioni helper per la mappatura dei token

// Verifica se il token corrisponde a una media key
// TODO usare strighe fisse e prenderle dal json
bool isMediaKeyToken(const String &token)
{
  return token.equals("VOL_UP") ||
         token.equals("VOL_DOWN") ||
         token.equals("NEXT_TRACK") ||
         token.equals("PREVIOUS_TRACK") ||
         token.equals("STOP") ||
         token.equals("PLAY_PAUSE") ||
         token.equals("MUTE") ||
         token.equals("WWW_HOME") ||
         token.equals("LOCAL_MACHINE_BROWSER") ||
         token.equals("CALCULATOR") ||
         token.equals("WWW_BOOKMARKS") ||
         token.equals("WWW_SEARCH") ||
         token.equals("WWW_STOP") ||
         token.equals("WWW_BACK") ||
         token.equals("CONSUMER_CONTROL_CONFIGURATION") ||
         token.equals("EMAIL_READER");
}

// Verifica se il token corrisponde a una special key
bool isSpecialKeyToken(const String &token)
{
  return token.equals("CTRL") ||
         token.equals("SHIFT") ||
         token.equals("ALT") ||
         token.equals("SUPER") ||
         token.equals("RIGHT_CTRL") ||
         token.equals("RIGHT_SHIFT") ||
         token.equals("RIGHT_ALT") ||
         token.equals("RIGHT_GUI") ||
         token.equals("UP_ARROW") ||
         token.equals("DOWN_ARROW") ||
         token.equals("LEFT_ARROW") ||
         token.equals("RIGHT_ARROW") ||
         token.equals("BACKSPACE") ||
         token.equals("TAB") ||
         token.equals("RETURN") ||
         token.equals("ESC") ||
         token.equals("INSERT") ||
         token.equals("DELETE") ||
         token.equals("PAGE_UP") ||
         token.equals("PAGE_DOWN") ||
         token.equals("HOME") ||
         token.equals("END") ||
         token.equals("CAPS_LOCK") ||
         token.equals("F1") ||
         token.equals("F2") ||
         token.equals("F3") ||
         token.equals("F4") ||
         token.equals("F5") ||
         token.equals("F6") ||
         token.equals("F7") ||
         token.equals("F8") ||
         token.equals("F9") ||
         token.equals("F10") ||
         token.equals("F11") ||
         token.equals("F12") ||
         token.equals("F13") ||
         token.equals("F14") ||
         token.equals("F15") ||
         token.equals("F16") ||
         token.equals("F17") ||
         token.equals("F18") ||
         token.equals("F19") ||
         token.equals("F20") ||
         token.equals("F21") ||
         token.equals("F22") ||
         token.equals("F23") ||
         token.equals("F24");
}

const uint8_t *getMediaKeyToken(const String &token)
{
  if (token.equals("NEXT_TRACK"))
    return KEY_MEDIA_NEXT_TRACK;
  else if (token.equals("PREVIOUS_TRACK"))
    return KEY_MEDIA_PREVIOUS_TRACK;
  else if (token.equals("STOP"))
    return KEY_MEDIA_STOP;
  else if (token.equals("PLAY_PAUSE"))
    return KEY_MEDIA_PLAY_PAUSE;
  else if (token.equals("MUTE"))
    return KEY_MEDIA_MUTE;
  else if (token.equals("VOL_UP"))
    return KEY_MEDIA_VOLUME_UP;
  else if (token.equals("VOL_DOWN"))
    return KEY_MEDIA_VOLUME_DOWN;
  else if (token.equals("WWW_HOME"))
    return KEY_MEDIA_WWW_HOME;
  else if (token.equals("LOCAL_MACHINE_BROWSER"))
    return KEY_MEDIA_LOCAL_MACHINE_BROWSER;
  else if (token.equals("CALCULATOR"))
    return KEY_MEDIA_CALCULATOR;
  else if (token.equals("WWW_BOOKMARKS"))
    return KEY_MEDIA_WWW_BOOKMARKS;
  else if (token.equals("WWW_SEARCH"))
    return KEY_MEDIA_WWW_SEARCH;
  else if (token.equals("WWW_STOP"))
    return KEY_MEDIA_WWW_STOP;
  else if (token.equals("WWW_BACK"))
    return KEY_MEDIA_WWW_BACK;
  else if (token.equals("CONSUMER_CONTROL_CONFIGURATION"))
    return KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION;
  else if (token.equals("EMAIL_READER"))
    return KEY_MEDIA_EMAIL_READER;
  return nullptr;
}

void BLEController::moveMouse(signed char x, signed char y, signed char wheel, signed char hWheel)
{
  Mouse.move(x, y, wheel, hWheel);
}

// Mappa i token “speciali” ai relativi codici

uint8_t mapSpecialKey(const String &token)
{
  if (token.equals("CTRL"))
    return KEY_LEFT_CTRL;
  else if (token.equals("SHIFT"))
    return KEY_LEFT_SHIFT;
  else if (token.equals("ALT"))
    return KEY_LEFT_ALT;
  else if (token.equals("SUPER"))
    return KEY_LEFT_GUI;
  else if (token.equals("RIGHT_CTRL"))
    return KEY_RIGHT_CTRL;
  else if (token.equals("RIGHT_SHIFT"))
    return KEY_RIGHT_SHIFT;
  else if (token.equals("RIGHT_ALT"))
    return KEY_RIGHT_ALT;
  else if (token.equals("RIGHT_GUI"))
    return KEY_RIGHT_GUI;
  else if (token.equals("UP_ARROW"))
    return KEY_UP_ARROW;
  else if (token.equals("DOWN_ARROW"))
    return KEY_DOWN_ARROW;
  else if (token.equals("LEFT_ARROW"))
    return KEY_LEFT_ARROW;
  else if (token.equals("RIGHT_ARROW"))
    return KEY_RIGHT_ARROW;
  else if (token.equals("BACKSPACE"))
    return KEY_BACKSPACE;
  else if (token.equals("TAB"))
    return KEY_TAB;
  else if (token.equals("RETURN"))
    return KEY_RETURN;
  else if (token.equals("ESC"))
    return KEY_ESC;
  else if (token.equals("INSERT"))
    return KEY_INSERT;
  else if (token.equals("DELETE"))
    return KEY_DELETE;
  else if (token.equals("PAGE_UP"))
    return KEY_PAGE_UP;
  else if (token.equals("PAGE_DOWN"))
    return KEY_PAGE_DOWN;
  else if (token.equals("HOME"))
    return KEY_HOME;
  else if (token.equals("END"))
    return KEY_END;
  else if (token.equals("CAPS_LOCK"))
    return KEY_CAPS_LOCK;
  else if (token.equals("F1"))
    return KEY_F1;
  else if (token.equals("F2"))
    return KEY_F2;
  else if (token.equals("F3"))
    return KEY_F3;
  else if (token.equals("F4"))
    return KEY_F4;
  else if (token.equals("F5"))
    return KEY_F5;
  else if (token.equals("F6"))
    return KEY_F6;
  else if (token.equals("F7"))
    return KEY_F7;
  else if (token.equals("F8"))
    return KEY_F8;
  else if (token.equals("F9"))
    return KEY_F9;
  else if (token.equals("F10"))
    return KEY_F10;
  else if (token.equals("F11"))
    return KEY_F11;
  else if (token.equals("F12"))
    return KEY_F12;
  else if (token.equals("F13"))
    return KEY_F13;
  else if (token.equals("F14"))
    return KEY_F14;
  else if (token.equals("F15"))
    return KEY_F15;
  else if (token.equals("F16"))
    return KEY_F16;
  else if (token.equals("F17"))
    return KEY_F17;
  else if (token.equals("F18"))
    return KEY_F18;
  else if (token.equals("F19"))
    return KEY_F19;
  else if (token.equals("F20"))
    return KEY_F20;
  else if (token.equals("F21"))
    return KEY_F21;
  else if (token.equals("F22"))
    return KEY_F22;
  else if (token.equals("F23"))
    return KEY_F23;
  else if (token.equals("F24"))
    return KEY_F24;
  else
  {
    // Se il token è un carattere singolo, lo interpretiamo come tale
    if (token.length() == 1)
      return token[0]; // (attenzione: potrebbe essere necessario convertire nel codice HID corretto)
    // Altri casi possono essere aggiunti qui...
    return 0; // codice 0 = chiave sconosciuta
  }
}
bool isMouseKeyToken(String token)
{
  token.trim();
  return token.equals("MOUSE_LEFT") ||
         token.equals("MOUSE_RIGHT") ||
         token.equals("MOUSE_MIDDLE") ||
         token.equals("MOUSE_BACK") ||
         token.equals("MOUSE_FORWARD");
}

uint8_t getMouseKeyToken(String token)
{
  token.trim();
  if (token.equals("MOUSE_LEFT"))
    return MOUSE_LEFT;
  else if (token.equals("MOUSE_RIGHT"))
    return MOUSE_RIGHT;
  else if (token.equals("MOUSE_MIDDLE"))
    return MOUSE_MIDDLE;
  else if (token.equals("MOUSE_BACK"))
    return MOUSE_BACK;
  else if (token.equals("MOUSE_FORWARD"))
    return MOUSE_FORWARD;

  return 0; // No valid mouse token found.
}

bool isMouseMoveToken(String token)
{
  token.trim();
  return token.startsWith("MOUSE_MOVE_");
}

void BLEController::BLExecutor(String action, bool pressed)
{
  if (!Keyboard.isConnected())
    return;

  // Process only commands that start with "S_B:"
  if (action.startsWith("S_B:"))
  {
    // Remove the prefix "S_B:"
    String cmd = action.substring(4);
    cmd.trim(); // Trim any whitespace

    // Handle special cases for literal + and , characters
    if (cmd.equals("++"))
    {
      if (pressed)
        Keyboard.press('+');
      else
        Keyboard.release('+');
      return;
    }
    else if (cmd.equals(",,"))
    {
      if (pressed)
        Keyboard.press(',');
      else
        Keyboard.release(',');
      return;
    }

    // Split the command into groups separated by commas
    // Better approach: use a dedicated parsing function
    String groups[10]; // Assuming max 10 groups
    int groupCount = 0;
    int startIndex = 0;
    bool inEscape = false;

    for (int i = 0; i < cmd.length(); i++)
    {
      if (cmd[i] == ',' && !inEscape)
      {
        if (i > startIndex)
        {
          groups[groupCount++] = cmd.substring(startIndex, i);
        }
        startIndex = i + 1;
      }
      else if (cmd[i] == '+' && i + 1 < cmd.length() && cmd[i + 1] == '+')
      {
        inEscape = !inEscape;
        i++; // Skip the next +
      }
    }

    // Add the last group
    if (startIndex < cmd.length())
    {
      groups[groupCount++] = cmd.substring(startIndex);
    }

    // Process each group
    for (int g = 0; g < groupCount; g++)
    {
      String group = groups[g];

      // Split tokens by +
      String tokens[10]; // Assuming max 10 tokens per group
      int tokenCount = 0;
      startIndex = 0;
      inEscape = false;

      for (int i = 0; i < group.length(); i++)
      {
        if (group[i] == '+' && !inEscape)
        {
          if (i > startIndex)
          {
            tokens[tokenCount++] = group.substring(startIndex, i);
          }
          startIndex = i + 1;
        }
        else if (group[i] == '+' && i + 1 < group.length() && group[i + 1] == '+')
        {
          inEscape = !inEscape;
          i++; // Skip the next +
        }
      }

      // Add the last token
      if (startIndex < group.length())
      {
        tokens[tokenCount++] = group.substring(startIndex);
      }

      // Process tokens
      for (int t = 0; t < tokenCount; t++)
      {
        String token = tokens[t];
        token.trim();

        // Replace escaped characters
        token.replace("++", "+");
        token.replace(",,", ",");

        // Process the token
        if (isMouseMoveToken(token))
        {
          // Handle mouse move
          String command = token.substring(11); // Remove "MOUSE_MOVE_"
          int x = 0, y = 0, wheel = 0, hWheel = 0;
          int count = sscanf(command.c_str(), "%d_%d_%d_%d", &x, &y, &wheel, &hWheel);

          if (count == 4)
          {
            moveMouse((signed char)x, (signed char)y, (signed char)wheel, (signed char)hWheel);
            Logger::getInstance().log("Mouse moved: " + String(x) + "," + String(y));
          }
          else
          {
            Logger::getInstance().log("Invalid MOUSE_MOVE command: " + token);
          }
        }
        else if (isMouseKeyToken(token))

        {
          // Handle mouse button
          uint8_t mouseButton = getMouseKeyToken(token);
          if (mouseButton != 0)
          {
            if (pressed)
              Mouse.press(mouseButton);
            else
              Mouse.release(mouseButton);
            Logger::getInstance().log("Mouse button: " + token + (pressed ? " pressed" : " released"));
          }
        }
        else if (isMediaKeyToken(token))
        {
          // Handle media key
          const uint8_t *mediaKey = getMediaKeyToken(token);
          if (mediaKey != nullptr)
          {
            if (pressed)
              Keyboard.press(mediaKey);
            // Keyboard.write(mediaKey);
            else
              Keyboard.release(mediaKey);
            Logger::getInstance().log("Media key: " + token + (pressed ? " pressed" : " released"));
          }
        }
        else if (isSpecialKeyToken(token))
        {
          // Handle special key
          uint8_t keyCode = mapSpecialKey(token);
          if (keyCode != 0)
          {
            if (pressed)
              Keyboard.press(keyCode);
            else
              Keyboard.release(keyCode);
            Logger::getInstance().log("Special key: " + token + (pressed ? " pressed" : " released"));
          }
        }
        else if (token.length() == 1)
        {
          // Handle single character - improve mapping for non-ASCII chars
          char c = token.charAt(0);
          if (pressed)
          {
            // Map character to HID code
            Keyboard.press(c);
            Logger::getInstance().log("Character pressed: " + String(c));
          }
          else
          {
            Keyboard.release(c);
            Logger::getInstance().log("Character released: " + String(c));
          }
        }
        else if (token.length() > 1)
        {
          if (pressed)
          {
            // Handle text string - use a better approach
            Logger::getInstance().log("Printing string: " + token);
            // Use the print method which handles the mapping internally
            Keyboard.print(String(token));
            // No need for character-by-character with delays
          }
          else
          {

            // chissa che vada davverp......
            Keyboard.releaseAll();
          }
          /// il problema dovrebbe essere nei press multpili ,
          // quando ce un carattere CASE blecombo usa in contemporanea lo SHIFT ma,
          // quando si preme due tasti con lo shift insieme succedono guai
          // soluzione controlla e gestisci lo shift internamente assegnando ad ogni carattere un valore CAPS true o false
          // convertendo la stringa in minuscolo per poi premere e rilasciare nel modo corretto SHIFT
          /// altri forse
          // TIenI CONTO DEL LAYOUT E FAI DELLE PROVE CON QUELLO ITA
          // impostare anche una variabile per scegliere il layout?????? indagare se blecombo supporta i layout ....
        }
      }
    }
  }
  else
  {
    Logger::getInstance().log("No valid command found to send BLE");
    return;
  }
}