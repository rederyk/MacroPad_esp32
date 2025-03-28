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


#ifndef Led_h
#define Led_h

#include <Arduino.h>

class Led
{
private:
  static Led *instance;
  Led();
  ~Led();

  int redPin;
  int greenPin;
  int bluePin;
  bool commonAnode;
  // Variabili per il salvataggio del colore
  int savedRed, savedGreen, savedBlue;
  bool colorSaved = false;
  bool initialized = false;
  int redValue;
  int greenValue;
  int blueValue;

public:
  // Definisci alcuni colori
  int myColors[3][3] = {
      {255, 0, 0}, // Rosso
      {0, 255, 0}, // Verde
      {0, 0, 255}  // Blu
  };

  static Led &getInstance();
  void begin(int redPin, int greenPin, int bluePin, bool commonAnode);
  void setColor(int red, int green, int blue, bool save = true);
  void setColor(bool restore);
  void getColor(int &red, int &green, int &blue);
  String getColorLog(bool testo = true, bool emoji = true);
};

#endif