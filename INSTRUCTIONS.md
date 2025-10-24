
# ESP32 MacroPad Guide

## Introduction

This document provides a comprehensive guide on how to set up and use the ESP32 MacroPad. It covers device actions, Bluetooth actions, and how to configure them.

## Setup

* write hardware setup instruction (WIP)





### Prerequisites

*   ESP32 development board (lolin32_lite)
*   some diode , switch and keycaps
*   Platformio IDE
* Rotary Encoder (no capacitor needed)
* ADXL345 
* Battery lipo (LOLIN32 have built-in charger) 
* Wiring and solder or your pcb
* some enclosure (stl file soon)

### Installation

1.  Clone the repository:

    ```bash
    git clone <repository_url>
    ```
2.  Open the project in Platformio IDE.
3.  Wait the Install of required libraries.
4.  Configure the `config.json` file. th current pin config is for lolin32_lite
5.  Upload the code to the ESP32 board.

## Configuration

The `config.json` file is used to configure the MacroPad. It includes settings for Wi-Fi, system, keypad, encoder, accelerometer, and key combinations.

### Wi-Fi Configuration

```json
{
  "wifi": {
    "ap_ssid": "ESP32_MacroPad",
    "ap_password": "my_cat_name123",
    "router_ssid": "ROUTER_SSID",
    "router_password": "SSID_PASSWORD"
  }
}
```

*   `ap_ssid`: SSID for the access point mode.
*   `ap_password`: Password for the access point mode.
*   `router_ssid`: SSID for the Wi-Fi router.
*   `router_password`: Password for the Wi-Fi router.

### System Configuration

```json
{
  "system": {
    "ap_autostart": false,
    "router_autostart": false,
    "enable_BLE": true,
    "serial_enabled": true,
    "BleMacAdd":0,
    "BleName":"Macropad_esp32" // attualmente harcoded
  }
}
```

*   `ap_autostart`: Enable or disable the access point mode on startup.
*   `router_autostart`: Enable or disable the Wi-Fi router mode on startup.
*   `enable_BLE`: Enable or disable Bluetooth.
*   `serial_enabled`: Enable or disable serial communication.
*   `BleMacAdd`: increment mac addres for ble
*   `BleName`: name of the device

### Keypad Configuration

* the deafult config is for lolin32_lite with 9 key, need more test for other boards and layout

```json
{
  "keypad": {
    "rows": 3,
    "cols": 3,
    "rowPins": [0, 4, 16],
    "colPins": [17, 5, 18],
    "keys": [
      ["1", "2", "3"],
      ["4", "5", "6"],
      ["7", "8", "9"]
    ],
    "invertDirection": true
  }
}
```

*   `rows`: Number of rows in the keypad.
*   `cols`: Number of columns in the keypad.
*   `rowPins`: Array of pins connected to the rows.
*   `colPins`: Array of pins connected to the columns.
*   `keys`: 2D array representing the keys on the keypad.
*   `invertDirection`: Invert the direction of the diode.

### Encoder Configuration

```json
{
  "encoder": {
    "pinA": 13,
    "pinB": 15,
    "buttonPin": 2,
    "stepValue": 1
  }
}
```

*   `pinA`: Pin connected to the A channel of the rotary encoder.
*   `pinB`: Pin connected to the B channel of the rotary encoder.
*   `buttonPin`: Pin connected to the button of the rotary encoder.
*   `stepValue`: Step value for each encoder increment/decrement.

### Accelerometer Configuration

```json
{
  "accelerometer": {
    "sdaPin": 19,
    "sclPin": 23,
    "sensitivity": 2.0,
    "sampleRate": 100,
    "threshold": 500,
    "axisMap": "zyx",
    "active": true,
    "type": "mpu6050",
    "address": 104
  }
}
```

*   `sdaPin`: SDA pin for I2C communication.
*   `sclPin`: SCL pin for I2C communication.
*   `sensitivity`: Sensitivity of the accelerometer.
*   `sampleRate`: Sample rate of the accelerometer.
*   `threshold`: Threshold for gesture detection.
*   `axisMap`: Axis mapping for the accelerometer.
*   `active`: Enables (`true`) or disables (`false`) the accelerometer logic.
*   `type`: Sensor driver to use. Supported values: `adxl345`, `mpu6050`.
*   `address`: I2C address in decimal form (e.g., 104 for `0x68`). Leave `0` to use the library default.

### Combinations Configuration 

```json
{
  "combinations": {
    "1": ["S_B:1"],
    "2": ["S_B:2"],
    "3": ["S_B:3"],
    "4": ["S_B:4"],
    "5": ["S_B:5"],
    "6": ["S_B:6"],
    "7": ["S_B:7"],
    "8": ["S_B:8"],
    "9": ["S_B:9"],
    "5+4": ["S_B:CTRL+SUPER,Left_arrow"],
    "5+6": ["S_B:CTRL+SUPER,Right_arrow"],
    "6+9": ["CONVERT_JSON_BINARY"],
    "8+9": ["CLEAR_A_GESTURE"],
    "CW": ["S_B:MOUSE_MOVE:0,0,1,0"],
    "CCW": ["S_B:MOUSE_MOVE:0,0,-1,0"],
    "BUTTON": ["TRAIN_GESTURE"],
    "G_ID:1": ["S_B:7"],
    "G_ID:2": ["S_B:CTRL+SUPER,Left_arrow"],
    "G_ID:0": ["S_B:CTRL+SUPER,Right_arrow"],

    "1,CW": ["S_B:VOL_UP"],
    "1+9": ["MEM_INFO"],
    "1,CCW": ["S_B:VOL_DOWN"],
    "1+2,CW": ["S_B:FAST_VOL_UP"],
    "1+2,CCW": ["S_B:FAST_VOL_DOWN"],
    "1+3,CW": ["S_B:NEXT_PLAYLIST"],
    "1+3": ["S_B:PREV_PLAYLIST"],
    "1+4": ["CALIBRATE_SENSOR"],
    "1+2+3,CW": ["TOGGLE_BLE_WIFI"],
    "7+8+9,BUTTON": ["RESET_ALL"]
  }
}
```

*   Key combinations are mapped to actions.
*   Actions can be device actions or Bluetooth actions.
*   there is a "bug" in macromanager we need to map combo in order like 1+9 , 

## Device Actions

Device actions are actions that are performed by the MacroPad itself. These actions include:

*   `RESET_ALL`: Resets the device.
*   `EXECUTE_GESTURE`: Executes a gesture.
*   `CALIBRATE_SENSOR`: Calibrates the accelerometer sensor.
*   `TOGGLE_SAMPLING`: Toggles the sampling of the accelerometer sensor.
*   `SAVE_GESTURE:id`: Saves a gesture with the specified ID (0-8).
*   `CONVERT_JSON_BINARY`: Converts the `gesture.json` file to a binary format.to be used for compare
*   `TRAIN_GESTURE`: Toggle the Train gesture recognition model and save a sample in an IDnum with keypad or arg.
*   `MEM_INFO`: Log memory information.
*   `PRINT_JSON`: Prints the `config.json` file to Logger.
*   `TOGGLE_BLE_WIFI`: Toggles between Bluetooth and Wi-Fi. and reboot device ,im not able to start in same time...

## Bluetooth Actions

Bluetooth actions are actions that are sent to a connected Bluetooth device. These actions are prefixed with `S_B:`.


### Syntax

```
S_B:<action1>+<action2>,<action3>
```
* 
* sorry + and , need more attention , try empirical method
* 
### Available Actions

*   **Keyboard Keys:**
    *   `CTRL`, `SHIFT`, `ALT`, `SUPER` (Windows key), `RIGHT_CTRL`, `RIGHT_SHIFT`, `RIGHT_ALT`, `RIGHT_GUI`
    *   `UP_ARROW`, `DOWN_ARROW`, `LEFT_ARROW`, `RIGHT_ARROW`
    *   `BACKSPACE`, `TAB`, `RETURN`, `ESC`, `INSERT`, `DELETE`, `PAGE_UP`, `PAGE_DOWN`, `HOME`, `END`, `CAPS_LOCK`
    *   `F1` - `F24`
    *   Any single character (e.g., `a`, `b`, `1`, `2`)
*   **Media Keys:**
    *   `VOL_UP`, `VOL_DOWN`, `NEXT_TRACK`, `PREVIOUS_TRACK`, `STOP`, `PLAY_PAUSE`, `MUTE`, `WWW_HOME`, `LOCAL_MACHINE_BROWSER`, `CALCULATOR`, `WWW_BOOKMARKS`, `WWW_SEARCH`, `WWW_STOP`, `WWW_BACK`, `CONSUMER_CONTROL_CONFIGURATION`, `EMAIL_READER`
*   **Mouse Keys:**
    *   `MOUSE_LEFT`, `MOUSE_RIGHT`, `MOUSE_MIDDLE`, `MOUSE_BACK`, `MOUSE_FORWARD`
*   **Mouse Movement:**
    *   `MOUSE_MOVE:<x>,<y>,<wheel>,<hWheel>` (e.g., `MOUSE_MOVE:10,0,0,0` moves the mouse 10 pixels to the right, `MOUSE_MOVE:0,0,1,0` scrolls up)

### Examples

*   `S_B:CTRL+ALT+DELETE`: Sends the Ctrl+Alt+Delete key combination.
*   `S_B:VOL_UP`: Increases the volume.
*   `S_B:MOUSE_MOVE:10,0,0,0`: Moves the mouse 10 pixels to the right.
*   `S_B:a`: Sends the 'a' key.
*   `S_B:CTRL+SUPER,Left_arrow`: Sends the "CTRL+SUPER,Left_arrow" key combination.
*   `S_B:MOUSE_MOVE:0,0,1,0`: Scrolls up.
*   `S_B:MOUSE_MOVE:0,0,-1,0`: Scrolls down.

## Gestures

The MacroPad supports gesture recognition. To train a gesture, use the `TRAIN_GESTURE` action. To execute a gesture, use the `EXECUTE_GESTURE` action.


## Gesture Training and Execution 

The ESP32 MacroPad supports custom gesture recognition. Follow these steps to train and execute your gestures:

### 1. Initial Cleanup 
 
- **Remove Pre-Registered Gestures:** 
The device comes preloaded with three test gestures. Before training your own, use the Web UI to delete these test gestures to avoid conflicts.

### 2. Training a New Gesture 
 
- **Set the Training Button:** 
It is best to dedicate the **BUTTON**  for both training and executing gestures. Update your `config.json` accordingly (e.g., `"BUTTON": ["TRAIN_GESTURE"]`).
 
- **Training Process:**  
  1. **Training an ID:**  Press and hold the BUTTON and Execute the desired movement while holding the button.
  
  2. **Assign a Gesture ID:**  Once you release the button, you have a 5-second window during which the keypad serves as a numeric input. Press a number (e.g., `1` assigns the gesture as `G_ID:0`). Repeat the movement 3–7 times assigning same id to ensure reliable recognition.
  3. 
  4. Repeat this for any Gesture ID you want record

### 3. Converting Gestures to Binary 
Before moving on to gesture execution, press the **Convert JSON to Binary**  button on the Web UI. This action reads your updated `gesture.json` file and converts it into a binary file that the device uses for real-time gesture matching.

### 4.Backup or Sharing Gestures Across Devices 
 
- **Using the Gesture Features Page:** 
The Web UI includes a **Manage Gesture Features**  page where you can copy and paste your custom gestures. This allows you to easily transfer your gesture configurations to multiple devices.
 
- **Reconversion Step:** 
After importing gestures on a new device, remember to convert the JSON configuration to binary again using the UI.

### 5. Executing Trained Gestures 
 
- **Switch to Execution Mode:** 
Reconfigure the BUTTON to trigger `EXECUTE_GESTURE` (e.g., `"BUTTON": ["EXECUTE_GESTURE"]`).
 
- **Test and Verify:** 
Hold the BUTTON and perform your gesture. The log panel on the System Actions page will display the executed gesture along with KNN feature scores, indicating which registered gesture was the closest match.

##  **FAQ**
### **connection problem**
 - attenzione a rimuovere i device duplicati da windows ,esempio esp_macropad_1 non si connettera se windows ha gia assiociato esp_macropad_0 o altri,
