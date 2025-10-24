
# ESP32 MacroPad 

A versatile, customizable macro pad built on the ESP32 platform, featuring gesture recognition, programmable key combinations, and WiFi/BLE connectivity.

## 🌟 Features 
 
- **Gesture Recognition** 
  - Record and execute custom gestures using ADXL345 or MPU6050 accelerometers

  - Intuitive gesture learning system

  - Real-time gesture detection
 
- **Advanced Input Options** 
  - Programmable key combinations

  - Rotary encoder support

  - Multiple trigger macros (combining keys and rotary encoder)
 
- **Wireless Connectivity** 
  - Bluetooth Low Energy (BLE) for macro transmission

  - WiFi configuration interface

  - Supports both AP and Station modes
 
- **Easy Configuration** 
  - Web-based configuration interface

  - No serial connection required

  - Real-time logging and debugging

  - Persistent settings storage
 
  - RGB led feedback 🌈

## 🚀 Getting Started 

### Prerequisites 

- ESP32 development board (tested on lolin32_lite)

- ADXL345 or MPU6050 accelerometer

- Mechanical switches/keypad

- Rotary encoder (optional)

- Basic soldering equipment and some diodes

- Arduino IDE or PlatformIO


### Installation 

0. **build your macropad ,connect all input device you need to correct pin, led and adxl is optional**
 
1. **Clone the repository:** 

```bash
git clone https://github.com/rederyk/MacroPad_esp32.git
```
 
2. **Wait paltformio to Install the required libraries**  using the Arduino Library Manager or PlatformIO.
 
3. **Configure your hardware connections**  in the `config.json` file.
 
4. **Upload the code**  to your ESP32:

```bash
pio run --target upload && pio run --target uploadfs
```
 
5. **Note:**  The process is not entirely straightforward – further testing may be required for proper pin configuration.

## 📝 Configuration 

1. Power on your MacroPad.
 
2. Connect to the WiFi AP **"ESP32_MacroPad"**  (default password: `my_cat_name123`).
 
3. Open your web browser and navigate to `http://192.168.4.1`.

4. Enter your SSID and password, then save the configuration. The device will reboot in STA+AP mode (using only the 2.4 GHz band).

5. Reconnect to the AP to retrieve the new IP address assigned by your router, then connect to your router and access the new IP.
 
6. On the advanced settings page, you can set a new password for the AP and disable `ap_autostart`. Changing the AP password is important because if the ESP32 cannot connect to your router (or if the password is incorrect), it will open the AP with your SSID and password visible in plain text.

7. On the Combo page, you can add or edit key combinations and define their functions or outputs.
 
8. Press keys **1** , **2** , **3**  together along with the encoder button to toggle and reboot between BLE and WiFi modes, or set your own combination instead of the preset: 
  - `"1+2+3, BUTTON": ["TOGGLE_BLE_WIFI"]`
Detailed setup instructions can be found in [INSTRUCTIONS.md](INSTRUCTIONS.md) .

### Gesture Training & Execution 🤖🖐️ 
 
- **Train Your Gestures:** 
Assign the `TRAIN_GESTURE` action to the **BUTTON** from the combo page. (Switch to the Web interface’s systemAction page if you want view what you press, or new combo2 page include a serial and auto recconnect ) ,Hold the button while performing a movement, then release and press a numeric key (e.g., **1**  → registers as **G_ID:0** ). Repeat to record multiple sample of the same gesture.
 
- **Execute Gestures:** 
Switch the **BUTTON**  action to `EXECUTE_GESTURE`, then perform the gesture. The web interface’s systemAction page displays real-time matching scores, showing which gesture was recognized.
 
- **Customize Freely:** 
Any key combo works, but using the BUTTON alone is recommended for consistency. Easily verify and fine-tune your gestures via the webserver logs.

Detailed setup instructions can be found in [INSTRUCTIONS.md](INSTRUCTIONS.md) .

## 🤖 AI Development Note 

This project was developed with assistance from various AI models including GPT-4, Claude,Deepseek,Gemini,qwen-coder. While this approach enabled rapid development, the code may contain unconventional patterns or require further optimization.

## 🐛 Known Issues 

- Gesture recognition may require calibration for optimal performance.

- Multiple MAC address changes of BLE sometimes requires that the device be "forgotten" on your PC or smartphone.
 
- See [TODO.md](TODO.md)  for planned improvements.

## Tested Board 

- lolin32_lite

## 🤝 Contributing 

Contributions are welcome! Please feel free to submit a pull request or add support for another tested board.

## Motivation 

This project was born out of the desire for a custom numpad capable of performing specific functions tailored to the user's needs. Existing numpads lacked the flexibility and customization options required, which led to the creation of this DIY solution.

## Disclaimer 

Due to the development process, the code may contain errors, inefficiencies, and unconventional solutions. Use this code at your own risk.

## ✨ Acknowledgments 

- Thanks to the ESP32 and Arduino communities.

- Thanks to all the AI assistants that helped shape this project.

- Special thanks to the cats who provided moral support and occasional keyboard testing.


---


Made with ❤️ and 🤖 (and 🐱)
