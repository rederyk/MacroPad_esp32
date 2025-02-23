# ESP32 MacroPad

A versatile, customizable macro pad built on the ESP32 platform, featuring gesture recognition, programmable key combinations, and WIFI BLE connectivity.

## 🌟 Features

- **Gesture Recognition**
  - Record and execute custom gestures using ADXL345 accelerometer
  - Intuitive gesture learning system
  - Real-time gesture detection

- **Advanced Input Options**
  - Programmable key combinations
  - Rotary encoder support
  - Multi-trigger macros (keys +keys, encoder)

- **Wireless Connectivity**
  - Bluetooth Low Energy (BLE) for macro transmission
  - WiFi configuration interface
  - Supports both AP and Station modes

- **Easy Configuration**
  - Web-based configuration interface
  - No serial connection required
  - Real-time logging and debugging
  - Persistent settings storage

## 🚀 Getting Started

### Prerequisites

- ESP32 development board (only tested on lolin32_lite)
- ADXL345 accelerometer
- Mechanical switches/keypad
- Rotary encoder (optional)
- Basic soldering equipment and some diode
- Arduino IDE or PlatformIO

### Dependencies

```
- ArduinoJson
- ESPAsyncWebServer
- LittleFS
- BleCombo
```

### Installation

1. Clone this repository:
   ```bash
   git clone 
   ```

2. Install required libraries through Arduino Library Manager or PlatformIO

3. Configure your hardware connections in `config.json`

4. Upload the code to your ESP32

5. is not so easy...need more test for pin configuration

## 📝 Configuration

1. Power on your MacroPad
2. Connect to the WiFi AP "ESP32_MacroPad" (default password: "my_cat_name123")
3. Navigate to `http://192.168.4.1` in your web browser
4. Insert your SSID and password and save ,it reboot in sta+ap mode, (use only 2ghz band) 
5. reconnect to the ap to read the new ip assigned by your router from the AP ,reconnect to your router go to new ip
6. you can set a new password for the ap and disable ap_autostart in advanced setting page .. set new password for the AP is important because if the esp dont find your router or password is incorrect, it open the ap with your ssiid and password in clear so..use your cat-name....

in any case change also the ap password.

7. on the combo page you can add or edit all combinations and what they do or send ,
8. press 1+2+3 and encoder button to toggle and reboot in BLE or WIFI mode or set your combo insted of preset 
 "1+2+3,BUTTON": ["TOGGLE_BLE_WIFI"],



Detailed setup instructions can be found in [INSTRUCTIONS.md](INSTRUCTIONS.md)

## 🤖 AI Development Note

This project was developed with assistance from various AI models including GPT-4, Claude, and others. While this approach allowed for rapid development, the code may contain unconventional patterns or require optimization.

## 🐛 Known Issues

- Gesture recognition may require calibration for optimal performance
- BLE pairing sometimes requires forget from pc/smartphone
- See [TODO.md](TODO.md) for planned improvements

## Tested Board

* lolin32_lite



## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request. or add a tested Board


## Motivation

This project was born out of a desire for a custom numpad that could perform specific functions tailored to the user's needs. Existing numpads lacked the desired flexibility and customization options, leading to the creation of this DIY solution.

## Disclaimer

Due to the development process, the code may contain errors, inefficiencies, and unconventional solutions. Use this code at your own risk.

## ✨ Acknowledgments

- Thanks to the ESP32 and Arduino communities
- All the AI assistants that helped shape this project
- The cats who provided moral support and occasional keyboard testing


---
Made with ❤️ and 🤖 (and 🐱)