#include "Led.h"

Led *Led::instance = nullptr;

Led::Led() : redValue(0), greenValue(0), blueValue(0) {}

Led::~Led() {}

Led &Led::getInstance()
{
  if (!instance)
  {
    instance = new Led();
  }
  return *instance;
}

void Led::begin(int redPin, int greenPin, int bluePin, bool commonAnode)
{
  this->redPin = redPin;
  this->greenPin = greenPin;
  this->bluePin = bluePin;
  this->commonAnode = commonAnode;

  redValue = 0;
  greenValue = 0;
  blueValue = 0;

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  // All'avvio, spegne tutti i colori
  // Per LED ad anodo comune, spegnere significa mandare il pin a HIGH (255)
  // Per LED a catodo comune, spegnere significa mandare il pin a LOW (0)
  if (commonAnode)
  {
    analogWrite(redPin, 255);
    analogWrite(greenPin, 255);
    analogWrite(bluePin, 255);
  }
  else
  {
    analogWrite(redPin, 0);
    analogWrite(greenPin, 0);
    analogWrite(bluePin, 0);
  }
}
  // Overload 1: Imposta il colore con i valori RGB.
  // Il parametro 'save' (default false) specifica se salvare il colore.
  void Led::setColor(int red, int green, int blue, bool save) {
    if (save) {
      savedRed = red;
      savedGreen = green;
      savedBlue = blue;
      colorSaved = true;
    }
    
    redValue = red;
    greenValue = green;
    blueValue = blue;
    
    if (commonAnode) {
      analogWrite(redPin, 255 - red);
      analogWrite(greenPin, 255 - green);
      analogWrite(bluePin, 255 - blue);
    } else {
      analogWrite(redPin, red);
      analogWrite(greenPin, green);
      analogWrite(bluePin, blue);
    }
  }

  // Overload 2: Se chiamato con un solo booleano, se è true e un colore è stato salvato, lo ripristina
  void Led::setColor(bool restore) {
    if (restore && colorSaved) {
      // Chiamando l'overload precedente con save=false per evitare di sovrascrivere il valore salvato
      setColor(savedRed, savedGreen, savedBlue, false);
    }
  }

void Led::getColor(int &red, int &green, int &blue)
{
  red = redValue;
  green = greenValue;
  blue = blueValue;
}
String Led::getColorLog(bool testo, bool emoji)
{
  // Default parameters
  if (testo == false && emoji == false)
  {
    testo = true;
    emoji = true;
  }

  String result = "";
  String colorName = "";
  String colorEmoji = "";

  // Basic colors
  if (redValue == 255 && greenValue == 0 && blueValue == 0)
  {
    colorName = "ROSSO";
    colorEmoji = "🔴";
  }
  else if (redValue == 0 && greenValue == 255 && blueValue == 0)
  {
    colorName = "VERDE";
    colorEmoji = "🟢";
  }
  else if (redValue == 0 && greenValue == 0 && blueValue == 255)
  {
    colorName = "BLU";
    colorEmoji = "🔵";
  }
  else if (redValue == 255 && greenValue == 255 && blueValue == 0)
  {
    colorName = "GIALLO";
    colorEmoji = "🟡";
  }
  else if (redValue == 255 && greenValue == 0 && blueValue == 255)
  {
    colorName = "MAGENTA";
    colorEmoji = "🟣";
  }
  else if (redValue == 0 && greenValue == 255 && blueValue == 255)
  {
    colorName = "CIANO";
    colorEmoji = "🔷";
  }
  else if (redValue == 255 && greenValue == 255 && blueValue == 255)
  {
    colorName = "BIANCO";
    colorEmoji = "⚪";
  }
  else if (redValue == 0 && greenValue == 0 && blueValue == 0)
  {
    colorName = "SPENTO";
    colorEmoji = "⚫";
  }
  // Extended colors
  else if (redValue > 200 && greenValue > 100 && greenValue < 180 && blueValue < 100)
  {
    colorName = "ARANCIONE";
    colorEmoji = "🟠";
  }
  else if (redValue > 120 && redValue < 200 && greenValue > 50 && greenValue < 100 && blueValue < 50)
  {
    colorName = "MARRONE";
    colorEmoji = "🟤";
  }
  else if (redValue > 150 && greenValue > 150 && blueValue < 150)
  {
    colorName = "GIALLO-CHIARO";
    colorEmoji = "💛";
  }
  else if (redValue < 100 && greenValue < 100 && blueValue > 100)
  {
    colorName = "BLU-SCURO";
    colorEmoji = "🌑";
  }
  else if (redValue > 200 && greenValue < 150 && blueValue > 150)
  {
    colorName = "ROSA";
    colorEmoji = "🌸";
  }
  else if (redValue > 180 && greenValue < 100 && blueValue > 180)
  {
    colorName = "VIOLA";
    colorEmoji = "💜";
  }
  else if (redValue < 100 && greenValue > 100 && blueValue < 100)
  {
    colorName = "VERDE-SCURO";
    colorEmoji = "🌲";
  }
  else if (redValue > 150 && greenValue > 150 && blueValue > 150)
  {
    colorName = "GRIGIO-CHIARO";
    colorEmoji = "⚪";
  }
  else if (redValue < 100 && greenValue < 100 && blueValue < 100 && (redValue > 0 || greenValue > 0 || blueValue > 0))
  {
    colorName = "GRIGIO-SCURO";
    colorEmoji = "⚫";
  }
  else if (redValue > 150 && greenValue > 100 && blueValue > 200)
  {
    colorName = "LAVANDA";
    colorEmoji = "🔮";
  }
  else if (redValue < 100 && greenValue > 150 && blueValue > 150)
  {
    colorName = "ACQUAMARINA";
    colorEmoji = "💦";
  }
  else if (redValue > 230 && greenValue > 190 && blueValue > 100)
  {
    colorName = "CREMA";
    colorEmoji = "🍦";
  }
  else if (redValue > 100 && greenValue > 230 && blueValue < 100)
  {
    colorName = "LIME";
    colorEmoji = "🍏";
  }
  else if (redValue > 200 && greenValue < 100 && blueValue < 100)
  {
    colorName = "ROSSO-SCURO";
    colorEmoji = "🍎";
  }
  else if (redValue > 230 && greenValue > 100 && blueValue < 150)
  {
    colorName = "CORALLO";
    colorEmoji = "🍑";
  }
  else if (redValue < 50 && greenValue < 50 && blueValue > 100)
  {
    colorName = "NAVY";
    colorEmoji = "🌃";
  }
  else
  {
    // Color not specifically defined, create a generic name
    if (redValue > greenValue && redValue > blueValue)
    {
      colorName = "ROSSASTRO";
      colorEmoji = "🟥";
    }
    else if (greenValue > redValue && greenValue > blueValue)
    {
      colorName = "VERDASTRO";
      colorEmoji = "🟩";
    }
    else if (blueValue > redValue && blueValue > greenValue)
    {
      colorName = "BLUASTRO";
      colorEmoji = "🟦";
    }
    else
    {
      colorName = "MISTO";
      colorEmoji = "🎨";
    }
  }

  // Build the result string based on requested format
  if (testo && emoji)
  {
    result = colorName + " " + colorEmoji;
  }
  else if (testo)
  {
    result = colorName;
  }
  else if (emoji)
  {
    result = colorEmoji;
  }

  return result;
}
