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
  if (connectionLost && bluetoothEnabled)
  {
    ESP.restart();
  }
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
  return token.equalsIgnoreCase("VOL_UP") ||
         token.equalsIgnoreCase("VOL_DOWN") ||
         token.equalsIgnoreCase("NEXT_TRACK") ||
         token.equalsIgnoreCase("PREVIOUS_TRACK") ||
         token.equalsIgnoreCase("STOP") ||
         token.equalsIgnoreCase("PLAY_PAUSE") ||
         token.equalsIgnoreCase("MUTE") ||
         token.equalsIgnoreCase("WWW_HOME") ||
         token.equalsIgnoreCase("LOCAL_MACHINE_BROWSER") ||
         token.equalsIgnoreCase("CALCULATOR") ||
         token.equalsIgnoreCase("WWW_BOOKMARKS") ||
         token.equalsIgnoreCase("WWW_SEARCH") ||
         token.equalsIgnoreCase("WWW_STOP") ||
         token.equalsIgnoreCase("WWW_BACK") ||
         token.equalsIgnoreCase("CONSUMER_CONTROL_CONFIGURATION") ||
         token.equalsIgnoreCase("EMAIL_READER");
}

// Verifica se il token corrisponde a una special key
bool isSpecialKeyToken(const String &token)
{
  return token.equalsIgnoreCase("CTRL") ||
         token.equalsIgnoreCase("SHIFT") ||
         token.equalsIgnoreCase("ALT") ||
         token.equalsIgnoreCase("SUPER") ||
         token.equalsIgnoreCase("RIGHT_CTRL") ||
         token.equalsIgnoreCase("RIGHT_SHIFT") ||
         token.equalsIgnoreCase("RIGHT_ALT") ||
         token.equalsIgnoreCase("RIGHT_GUI") ||
         token.equalsIgnoreCase("UP_ARROW") ||
         token.equalsIgnoreCase("DOWN_ARROW") ||
         token.equalsIgnoreCase("LEFT_ARROW") ||
         token.equalsIgnoreCase("RIGHT_ARROW") ||
         token.equalsIgnoreCase("BACKSPACE") ||
         token.equalsIgnoreCase("TAB") ||
         token.equalsIgnoreCase("RETURN") ||
         token.equalsIgnoreCase("ESC") ||
         token.equalsIgnoreCase("INSERT") ||
         token.equalsIgnoreCase("DELETE") ||
         token.equalsIgnoreCase("PAGE_UP") ||
         token.equalsIgnoreCase("PAGE_DOWN") ||
         token.equalsIgnoreCase("HOME") ||
         token.equalsIgnoreCase("END") ||
         token.equalsIgnoreCase("CAPS_LOCK") ||
         token.equalsIgnoreCase("F1") ||
         token.equalsIgnoreCase("F2") ||
         token.equalsIgnoreCase("F3") ||
         token.equalsIgnoreCase("F4") ||
         token.equalsIgnoreCase("F5") ||
         token.equalsIgnoreCase("F6") ||
         token.equalsIgnoreCase("F7") ||
         token.equalsIgnoreCase("F8") ||
         token.equalsIgnoreCase("F9") ||
         token.equalsIgnoreCase("F10") ||
         token.equalsIgnoreCase("F11") ||
         token.equalsIgnoreCase("F12") ||
         token.equalsIgnoreCase("F13") ||
         token.equalsIgnoreCase("F14") ||
         token.equalsIgnoreCase("F15") ||
         token.equalsIgnoreCase("F16") ||
         token.equalsIgnoreCase("F17") ||
         token.equalsIgnoreCase("F18") ||
         token.equalsIgnoreCase("F19") ||
         token.equalsIgnoreCase("F20") ||
         token.equalsIgnoreCase("F21") ||
         token.equalsIgnoreCase("F22") ||
         token.equalsIgnoreCase("F23") ||
         token.equalsIgnoreCase("F24");
}

const uint8_t *getMediaKeyToken(const String &token)
{
  if (token.equalsIgnoreCase("NEXT_TRACK"))
    return KEY_MEDIA_NEXT_TRACK;
  else if (token.equalsIgnoreCase("PREVIOUS_TRACK"))
    return KEY_MEDIA_PREVIOUS_TRACK;
  else if (token.equalsIgnoreCase("STOP"))
    return KEY_MEDIA_STOP;
  else if (token.equalsIgnoreCase("PLAY_PAUSE"))
    return KEY_MEDIA_PLAY_PAUSE;
  else if (token.equalsIgnoreCase("MUTE"))
    return KEY_MEDIA_MUTE;
  else if (token.equalsIgnoreCase("VOL_UP"))
    return KEY_MEDIA_VOLUME_UP;
  else if (token.equalsIgnoreCase("VOL_DOWN"))
    return KEY_MEDIA_VOLUME_DOWN;
  else if (token.equalsIgnoreCase("WWW_HOME"))
    return KEY_MEDIA_WWW_HOME;
  else if (token.equalsIgnoreCase("LOCAL_MACHINE_BROWSER"))
    return KEY_MEDIA_LOCAL_MACHINE_BROWSER;
  else if (token.equalsIgnoreCase("CALCULATOR"))
    return KEY_MEDIA_CALCULATOR;
  else if (token.equalsIgnoreCase("WWW_BOOKMARKS"))
    return KEY_MEDIA_WWW_BOOKMARKS;
  else if (token.equalsIgnoreCase("WWW_SEARCH"))
    return KEY_MEDIA_WWW_SEARCH;
  else if (token.equalsIgnoreCase("WWW_STOP"))
    return KEY_MEDIA_WWW_STOP;
  else if (token.equalsIgnoreCase("WWW_BACK"))
    return KEY_MEDIA_WWW_BACK;
  else if (token.equalsIgnoreCase("CONSUMER_CONTROL_CONFIGURATION"))
    return KEY_MEDIA_CONSUMER_CONTROL_CONFIGURATION;
  else if (token.equalsIgnoreCase("EMAIL_READER"))
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
  if (token.equalsIgnoreCase("CTRL"))
    return KEY_LEFT_CTRL;
  else if (token.equalsIgnoreCase("SHIFT"))
    return KEY_LEFT_SHIFT;
  else if (token.equalsIgnoreCase("ALT"))
    return KEY_LEFT_ALT;
  else if (token.equalsIgnoreCase("SUPER"))
    return KEY_LEFT_GUI;
  else if (token.equalsIgnoreCase("RIGHT_CTRL"))
    return KEY_RIGHT_CTRL;
  else if (token.equalsIgnoreCase("RIGHT_SHIFT"))
    return KEY_RIGHT_SHIFT;
  else if (token.equalsIgnoreCase("RIGHT_ALT"))
    return KEY_RIGHT_ALT;
  else if (token.equalsIgnoreCase("RIGHT_GUI"))
    return KEY_RIGHT_GUI;
  else if (token.equalsIgnoreCase("UP_ARROW"))
    return KEY_UP_ARROW;
  else if (token.equalsIgnoreCase("DOWN_ARROW"))
    return KEY_DOWN_ARROW;
  else if (token.equalsIgnoreCase("LEFT_ARROW"))
    return KEY_LEFT_ARROW;
  else if (token.equalsIgnoreCase("RIGHT_ARROW"))
    return KEY_RIGHT_ARROW;
  else if (token.equalsIgnoreCase("BACKSPACE"))
    return KEY_BACKSPACE;
  else if (token.equalsIgnoreCase("TAB"))
    return KEY_TAB;
  else if (token.equalsIgnoreCase("RETURN"))
    return KEY_RETURN;
  else if (token.equalsIgnoreCase("ESC"))
    return KEY_ESC;
  else if (token.equalsIgnoreCase("INSERT"))
    return KEY_INSERT;
  else if (token.equalsIgnoreCase("DELETE"))
    return KEY_DELETE;
  else if (token.equalsIgnoreCase("PAGE_UP"))
    return KEY_PAGE_UP;
  else if (token.equalsIgnoreCase("PAGE_DOWN"))
    return KEY_PAGE_DOWN;
  else if (token.equalsIgnoreCase("HOME"))
    return KEY_HOME;
  else if (token.equalsIgnoreCase("END"))
    return KEY_END;
  else if (token.equalsIgnoreCase("CAPS_LOCK"))
    return KEY_CAPS_LOCK;
  else if (token.equalsIgnoreCase("F1"))
    return KEY_F1;
  else if (token.equalsIgnoreCase("F2"))
    return KEY_F2;
  else if (token.equalsIgnoreCase("F3"))
    return KEY_F3;
  else if (token.equalsIgnoreCase("F4"))
    return KEY_F4;
  else if (token.equalsIgnoreCase("F5"))
    return KEY_F5;
  else if (token.equalsIgnoreCase("F6"))
    return KEY_F6;
  else if (token.equalsIgnoreCase("F7"))
    return KEY_F7;
  else if (token.equalsIgnoreCase("F8"))
    return KEY_F8;
  else if (token.equalsIgnoreCase("F9"))
    return KEY_F9;
  else if (token.equalsIgnoreCase("F10"))
    return KEY_F10;
  else if (token.equalsIgnoreCase("F11"))
    return KEY_F11;
  else if (token.equalsIgnoreCase("F12"))
    return KEY_F12;
  else if (token.equalsIgnoreCase("F13"))
    return KEY_F13;
  else if (token.equalsIgnoreCase("F14"))
    return KEY_F14;
  else if (token.equalsIgnoreCase("F15"))
    return KEY_F15;
  else if (token.equalsIgnoreCase("F16"))
    return KEY_F16;
  else if (token.equalsIgnoreCase("F17"))
    return KEY_F17;
  else if (token.equalsIgnoreCase("F18"))
    return KEY_F18;
  else if (token.equalsIgnoreCase("F19"))
    return KEY_F19;
  else if (token.equalsIgnoreCase("F20"))
    return KEY_F20;
  else if (token.equalsIgnoreCase("F21"))
    return KEY_F21;
  else if (token.equalsIgnoreCase("F22"))
    return KEY_F22;
  else if (token.equalsIgnoreCase("F23"))
    return KEY_F23;
  else if (token.equalsIgnoreCase("F24"))
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
  return token.equalsIgnoreCase("MOUSE_LEFT") ||
         token.equalsIgnoreCase("MOUSE_RIGHT") ||
         token.equalsIgnoreCase("MOUSE_MIDDLE") ||
         token.equalsIgnoreCase("MOUSE_BACK") ||
         token.equalsIgnoreCase("MOUSE_FORWARD");
}

uint8_t getMouseKeyToken(String token)
{
  token.trim();
  if (token.equalsIgnoreCase("MOUSE_LEFT"))
    return MOUSE_LEFT;
  else if (token.equalsIgnoreCase("MOUSE_RIGHT"))
    return MOUSE_RIGHT;
  else if (token.equalsIgnoreCase("MOUSE_MIDDLE"))
    return MOUSE_MIDDLE;
  else if (token.equalsIgnoreCase("MOUSE_BACK"))
    return MOUSE_BACK;
  else if (token.equalsIgnoreCase("MOUSE_FORWARD"))
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

    // Split the command into groups separated by commas.
    int groupStart = 0;
    while (true)
    {
      int groupEnd = cmd.indexOf(',', groupStart);
      String group = (groupEnd == -1) ? cmd.substring(groupStart)
                                      : cmd.substring(groupStart, groupEnd);
      group.trim();

      if (group.length() > 0)
      {
        // Within each group, tokens are separated by '+'
        int tokenStart = 0;
        while (true)
        {
          int tokenEnd = group.indexOf('+', tokenStart);
          String token = (tokenEnd == -1) ? group.substring(tokenStart)
                                          : group.substring(tokenStart, tokenEnd);
          token.trim();

          if (token.length() > 0)
          {
            bool handled = false;
            delay(10);

            // mouse command debug
            //  Logger::getInstance().log("Token: " + token);
            //  Logger::getInstance().log("Group: " + group);

            // Check if the token is a mouse move command.
            if (isMouseMoveToken(token))
            {
              String command = token.substring(11); // Remove "MOUSE_MOVE_"
              int x = 0, y = 0, wheel = 0, hWheel = 0;
              int count = sscanf(command.c_str(), "%d_%d_%d_%d", &x, &y, &wheel, &hWheel);

              // Mouse command debug
              // Logger::getInstance().log("Parsed command: " + (command));
              // Logger::getInstance().log("Sscanf count: " + String(count));

              if (count == 4)
              {
                moveMouse((signed char)x, (signed char)y, (signed char)wheel, (signed char)hWheel);
                handled = true;
              }
              else
              {
                Logger::getInstance().log("Invalid MOUSE_MOVE command: " + token);
              }
            }
            // Check if the token is a mouse key command.
            else if (isMouseKeyToken(token))
            {
              uint8_t mouseButton = getMouseKeyToken(token);
              if (mouseButton != 0)
              {
                if (pressed)
                  Mouse.press(mouseButton);
                else
                  Mouse.release(mouseButton);
                handled = true;
              }
            }
            // Check if the token is a media key.
            else if (isMediaKeyToken(token))
            {
              const uint8_t *mediaKey = getMediaKeyToken(token);
              if (mediaKey != nullptr)
              {
                if (pressed)
                  Keyboard.press(mediaKey);
                else
                  Keyboard.release(mediaKey);
                handled = true;
              }
            }
            // Check if the token is a special key.
            else if (isSpecialKeyToken(token))
            {
              uint8_t keyCode = mapSpecialKey(token);
              if (keyCode != 0)
              {
                if (pressed)
                  Keyboard.press(keyCode);
                else
                  Keyboard.release(keyCode);
                handled = true;
              }
            }
            // If the token is a single character, convert it to uint8_t.
            else if (token.length() == 1)
            {
              uint8_t keyCode = (uint8_t)token.charAt(0);
              if (pressed)
                Keyboard.press(keyCode);
              else
                Keyboard.release(keyCode);
              handled = true;
            }

            // If nothing was recognized and the token is longer than one character,
            // assume it is a string and send it as text.
            // write accetta anche stringhe lunghe dovrmmo cambiarrne la logica

            if (!handled && pressed && token.length() > 0)
            {
              //   for (int i = 0; i < token.length(); i++) {
              //     delay(10);
              //     Keyboard.write(token[i]);
              //   }
              Keyboard.print(token);
            }
          }

          if (tokenEnd == -1)
            break;
          tokenStart = tokenEnd + 1;
        } // End of token loop.
      }

      if (groupEnd == -1)
        break;
      groupStart = groupEnd + 1;
    } // End of group loop.
  }
  else
  {
    // Handle other commands (e.g., AP_MODE, MEM_INFO) if necessary.
  }
}
