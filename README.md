# ESP32 MacroPad

A powerful, customizable macro pad built on the ESP32 platform, featuring gesture recognition, programmable key combinations, IR universal remote, gyroscopic mouse control, event scheduling, and wireless connectivity.

## 🎥 See It In Action

<p align="center">
  <video src="DOC/images/gyromouse.webm" width="360" autoplay loop muted playsinline></video>
  <video src="DOC/images/IRremote.webm" width="360" autoplay loop muted playsinline></video>
</p>

<p align="center">
  <b>🖱️ GyroMouse Control</b><br/>
  Air mouse with gesture-based movement
  &nbsp;&nbsp;&nbsp;&nbsp;
  <b>📡 IR Universal Remote</b><br/>
  Learn and send IR commands
</p>


> 🖨️ **Want to build your own?** Check out the [3D printable case on Thingiverse](https://www.thingiverse.com/thing:7188015)!

## 🌟 Features

### 🎯 Core Functionality

- **Advanced Macro System**
  - Programmable key combinations with simplified array-based syntax
  - Multiple combo profiles with instant switching (no reboot required!)
  - Rotary encoder support with configurable actions
  - Smart command factory architecture for extensible actions
  - Delay support for precise timing control

- **Gesture Recognition** 🤖
  - Dual-sensor support: **MPU6050** (gyro-based, faster) or **ADXL345** (accel-based)
  - Real-time gesture detection: shake, swipe left/right
  - Intelligent learning system with multi-sample training
  - Motion-based wake from deep sleep (MPU6050 only)
  - Madgwick AHRS fusion for enhanced orientation tracking

- **GyroMouse Control** 🖱️
  - Transform your MacroPad into an air mouse!
  - Multiple sensitivity presets (Precision, Normal, Fast)
  - Advanced tilt control with configurable deadzone
  - Click slowdown for precise targeting
  - On-the-fly recenter and sensitivity adjustment

- **IR Universal Remote** 📡
  - Learn IR signals from any remote control
  - Transmit IR commands via programmed combos
  - Multiple protocol support with device organization
  - Persistent storage of IR database
  - Web-based IR scanning and testing

- **Event Scheduler** ⏰
  - Time-based automation (daily at specific times)
  - Interval-based triggers (every N milliseconds)
  - Absolute timestamp execution (one-shot events)
  - Input event triggers (on specific key combinations)
  - Smart sleep management with wake-ahead capability

- **Reactive Lighting System** 🌈
  - Per-key RGB color customization
  - Interactive mode: press button → LED shows assigned color
  - Profile-based color schemes
  - Status indicators: Magenta (startup), Blue (BLE), Green (WiFi), Red (AP/Error)
  - Brightness control with persistent settings

- **Wireless Connectivity**
  - Bluetooth Low Energy (BLE) for HID keyboard + mouse
  - WiFi configuration interface (AP + Station modes)
  - Multi-device BLE pairing with MAC address hopping
  - Async web server for non-blocking configuration
  - Real-time logging and debugging via web interface

- **Power Management** 🔋
  - Deep sleep mode with configurable timeout
  - Motion-based wake (MPU6050) or button-based wake
  - Scheduler-aware sleep prevention
  - LiPo battery support with weeks of standby time

## 🚀 Getting Started

### Prerequisites

#### Hardware Components
- **ESP32 development board** (tested on **LOLIN32 Lite**)
- **Accelerometer/Gyroscope**: MPU6050 (recommended) or ADXL345 optional
- **XxX Keypad** (X*X mechanical switches with diodes) (optional, needed for an input but can use the webui)
- **Rotary encoder** (optional but recommended) 
- **RGB LED**  optional
- **IR Transmitter LED** (with transistor amplification circuit) optional
- **IR Receiver** (TSOP38238 or similar) optional
- **LiPo Battery** 3.7V (optional, but recommended for portable use)
- Basic soldering equipment

#### Software
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- Git for cloning repository

### Installation

**1. Build your MacroPad hardware**
   - Connect all input devices to correct pins (see [Hardware_Schema.md](DOC/Hardware_Schema.md))
   - LED and accelerometer are optional but recommended for full functionality

**2. Clone the repository:**

```bash
git clone https://github.com/rederyk/MacroPad_esp32.git
cd MacroPad_esp32
```

**3. Install dependencies**
   - PlatformIO will automatically install required libraries on first build

**4. Configure hardware connections**
   - Edit [config.json](data/config.json) to match your pin configuration
   - Set accelerometer type: `"mpu6050"` or `"adxl345"`
   - Configure keypad matrix rows/columns pins

**5. Upload firmware and filesystem:**

```bash
pio run --target upload && pio run --target uploadfs
```

**6. First-time setup:**
   - See [Quick Start Guide](DOC/quick_start.md) for detailed WiFi configuration
   - See [Hardware Schema](DOC/Hardware_Schema.md) for pin connections

## 📝 Configuration

### Initial WiFi Setup

1. **Power on** your MacroPad
2. **Connect** to WiFi AP: `ESP32_MacroPad` (password: `my_cat_name123`)
3. **Navigate** to `http://192.168.4.1` in your browser
4. **Configure** your WiFi credentials and save
5. **Reboot** - device will enter STA+AP mode (2.4 GHz only)
6. **Reconnect** to AP to get new router-assigned IP address
7. **Access** web interface via new IP from your router

⚠️ **Security Note:** Change the AP password in advanced settings! If ESP32 cannot connect to your router, it falls back to AP mode with your SSID and password visible in plain text.

### Web Interface Overview

The web interface provides complete control over your MacroPad:

- **Config** (`config.html`) - System settings, WiFi, pin mappings, accelerometer configuration
- **Combinations** (`combo.html`) - Macro editor with live testing and profile management
- **Special Actions** (`special_actions.html`) - IR management, gyromouse settings, advanced actions
- **Advanced** (`advanced.html`) - Sensor calibration, scheduler configuration, system diagnostics

### Combo Syntax - New Simplified System 🎨

The MacroPad uses a modern **array-based JSON syntax** for defining macros:

#### Basic Example
```json
{
  "2+8": [
    "S_B:SUPER+q"
  ]
}
```
**Result:** Presses SUPER and 'q' simultaneously

#### Sequential Commands
```json
{
  "2+5+8": [
    "S_B:SUPER+t",
    "DELAY_500",
    "S_B:btop",
    "DELAY_500",
    "S_B:RETURN"
  ]
}
```
**Result:** Opens terminal (SUPER+t) → waits 500ms → types "btop" → waits 500ms → presses ENTER

#### Key Syntax Rules

✅ **Array-based**: Each element executes sequentially with automatic key release
✅ **Plus (`+`) for simultaneous keys**: Within `S_B:` commands only
✅ **No escape needed**: Text with `+` and `,` doesn't need escaping!
✅ **Minimal escape**: Only use `\+` for literal plus in ambiguous contexts

**Examples:**
```json
{
  "text_example": [
    "S_B:Hello World!",
    "S_B:2+2=4",
    "S_B:C++ programming"
  ]
}
```

#### Available Command Types

**BLE Keyboard (`S_B:`)**
- Special keys: CTRL, SHIFT, ALT, SUPER, F1-F24, Arrow keys, etc.
- Key combinations: `S_B:CTRL+c`
- Text input: `S_B:Your text here`
- Unicode support: `S_B:😀🎉`

**Timing**
- `DELAY_500` - 500ms pause

**Gesture Actions**
- `G_SHAKE` - Execute on shake motion
- `G_SWIPE_RIGHT` - Execute on right swipe
- `G_SWIPE_LEFT` - Execute on left swipe

**Special Actions** (40+ commands available!)
- `GYROMOUSE_TOGGLE` - Enable/disable gyro mouse
- `GYROMOUSE_CYCLE_SENSITIVITY` - Switch sensitivity preset
- `GYROMOUSE_RECENTER` - Reset neutral position
- `SWITCH_COMBO_1` - Load combo profile 1
- `TOGGLE_BLE_WIFI` - Switch between BLE and WiFi modes
- `TOGGLE_REACTIVE_LIGHTING` - Enable/disable per-key colors
- `SEND_IR_device_name_power` - Send saved IR command
- `LED_RGB_255_0_0` - Set LED color (Red in this case)
- `LED_BRIGHTNESS_128` - Set LED brightness (0-255)
- `ENTER_SLEEP` - Enter deep sleep mode manually
- `CALIBRATE_SENSOR` - Recalibrate accelerometer
- `RESET_ALL` - Factory reset
- And many more...

📖 **Full Syntax Guide:** See [SYNTAX_GUIDE.md](SYNTAX_GUIDE.md) for complete documentation

### Gesture Training & Execution 🤖🖐️

#### Training Your Gestures

1. **Assign action** to a key combo in web interface:
   ```json
   "BUTTON": ["TRAIN_GESTURE"]
   ```

2. **Record gesture:**
   - Hold the assigned button while performing a movement
   - Release button
   - Press a number key (1-9) to assign gesture ID
   - Example: Press `1` → saves as `G_ID:0`

3. **Repeat** for multiple samples of the same gesture (3-5 recommended)

#### Executing Gestures

1. **Change action** to execute mode:
   ```json
   "BUTTON": ["EXECUTE_GESTURE"]
   ```

2. **Perform gesture** - system will recognize and execute matched gesture

3. **View real-time scores** on web interface systemAction page

**Tip:** Use the web interface to see live matching scores and fine-tune your gestures!

### IR Remote Control 📡

#### Learning IR Commands

1. Navigate to **Special Actions** page
2. Click **IR Scan Mode**
3. Point remote at IR receiver and press button
4. System captures and displays protocol/data
5. Save with device name and button label

#### Sending IR Commands

Assign saved IR commands to combos:
```json
{
  "7+8+9": [
    "SEND_IR_tv_power"
  ]
}
```

### Event Scheduler ⏰

Configure automated actions in `scheduler.json`:

```json
{
  "events": [
    {
      "type": "time_of_day",
      "hour": 8,
      "minute": 0,
      "action": "LED_RGB_0_255_0",
      "enabled": true
    },
    {
      "type": "interval",
      "interval_ms": 3600000,
      "action": "MEM_INFO",
      "enabled": true
    }
  ]
}
```

Enable scheduler in `config.json`:
```json
"scheduler": {
  "enabled": true,
  "prevent_sleep_if_pending": true,
  "wake_ahead_seconds": 900
}
```

## 🎮 Usage Examples

### Toggle BLE/WiFi Mode

**Default combo** (can be customized):
- Press keys **1 + 2 + 3 + ENCODER_BUTTON** together
- Device reboots in opposite mode (BLE ↔ WiFi)

### GyroMouse Control

1. **Enable:** Assign `GYROMOUSE_TOGGLE` to a combo
2. **Use:** Tilt MacroPad to move cursor
3. **Adjust sensitivity:** Use `GYROMOUSE_CYCLE_SENSITIVITY`
4. **Recenter:** Use `GYROMOUSE_RECENTER` if drift occurs

### Profile Switching

Switch between different combo sets on-the-fly:
```json
{
  "1+4+7": ["SWITCH_COMBO_0"],
  "2+5+8": ["SWITCH_COMBO_1"],
  "3+6+9": ["SWITCH_COMBO_2"]
}
```

Each profile can have different LED colors configured in `_settings` section!

## 🏗️ Architecture Highlights

### FreeRTOS-Based Design
- **Main Loop Task** - 5ms polling cycle for optimal responsiveness
- **Gesture Sampling Task** - Continuous accelerometer data collection
- **Async Web Server** - Non-blocking configuration interface
- **Event Queue** - Buffered input processing (16 events max)

### Design Patterns
- **Command Pattern** - Actions as first-class objects via CommandFactory
- **Factory Pattern** - Dynamic command instantiation
- **Observer Pattern** - Event-driven gesture handling
- **Singleton Pattern** - Global managers (Logger, LED, Config)

### Memory Management
- LittleFS for configuration storage
- Bluetooth Classic release frees ~30KB when using BLE
- Smart buffer management to prevent fragmentation

## 🤖 AI Development Note

This project was developed with assistance from various AI models including GPT-4, Claude, Deepseek, Gemini, and Qwen-Coder. While this enabled rapid prototyping and feature development, the codebase may contain unconventional patterns. We're continuously refactoring towards best practices!


See [TODO.md](DOC/TODO.md) for detailed roadmap and planned improvements.

## 📊 Tested Hardware

### Confirmed Working
- ✅ **LOLIN32 Lite** - Full testing, all features working
- ✅ **MPU6050** - Recommended accelerometer (gyro + motion wake)
- ✅ **ADXL345** - Fallback accelerometer (accel only)
- ✅ **TSOP38238** IR receiver
- ✅ LiPo 3.7V 820mAh (~2 weeks standby with sleep)

### Seeking Community Testing
- ESP32 DevKit v1
- TTGO T-Display
- ESP32-S2/S3 (USB native HID potential!)
- FireBeetle ESP32

**Have a different board?** Please test and contribute pin mappings!

## 🤝 Contributing

Contributions are welcome! Areas where we'd love help:

- 🧪 Testing on different ESP32 boards
- 🌐 Translation (docs currently in Italian/English mix)
- 🎨 LED animation patterns
- 🔧 Memory optimization for gestures
- 📱 Mobile app development (Flutter?)
- 🎮 Gaming profiles with reduced latency

See [TODO.md](DOC/TODO.md) for full list of planned features.

## 📚 Documentation

- [Quick Start Guide](DOC/quick_start.md) - First-time setup walkthrough
- [Syntax Guide](SYNTAX_GUIDE.md) - Complete macro syntax reference
- [Hardware Schema](DOC/Hardware_Schema.md) - Pin connections and circuit diagrams
- [TODO & Roadmap](DOC/TODO.md) - Planned features and known issues

## 🎯 Motivation

This project was born from the desire for a truly customizable macro pad that could:
- Execute complex multi-step macros
- Act as a universal remote (IR)
- Function as an air mouse (gyro)
- Automate tasks with scheduling
- Respond to physical gestures
- Switch between multiple profiles instantly

Existing macro pads lacked this level of flexibility and extensibility, so we built our own!

## ⚠️ Disclaimer

This is a hobby project developed with AI assistance. The code may contain errors, inefficiencies, or unconventional solutions. Use at your own risk and always review configurations before deploying to production workflows.

## ✨ Acknowledgments

- 🙏 ESP32 and Arduino communities for amazing libraries
- 🤖 AI assistants (GPT-4, Claude, Deepseek, Gemini, Qwen) that helped shape this project
- 🐱 The cats who provided moral support and occasional "keyboard testing"
- 🌟 All contributors and testers who help improve the MacroPad

## 📜 License

GNU GENERAL PUBLIC LICENSE Version 3

## 🔗 Links

- **GitHub Repository:** [rederyk/MacroPad_esp32](https://github.com/rederyk/MacroPad_esp32)
- **Issue Tracker:** Report bugs and request features
- **Discussions:** Share your builds and configurations!

---

**Current Version:** 2.0
**Last Updated:** January 2025
**Maintainer:** Enrico Mori

Made with ❤️ and 🤖 (and 🐱)
