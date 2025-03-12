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