# Schema Elettrico - ESP32 MacroPad

## Componenti Hardware:
*   ESP32 LOLIN32 Lite (microcontrollore principale)
*   Matrice Keypad 3x3 (9 tasti)
*   Rotary Encoder con pulsante integrato
*   LED RGB (anodo comune o catodo comune)
*   Sensore MPU6050 (accelerometro + giroscopio) - I2C
*   LED IR (trasmettitore infrarossi)
*   Ricevitore IR (sensore infrarossi)
*   Batteria LiPo 3.7V (820mAh o superiore)

## Schema dei Collegamenti:
```ascii
╔═══════════════════════════════════════════════════════════════════╗
║                     ESP32 LOLIN32 LITE                             ║
║                                                                     ║
║  3V3 ────────┬─────┬──────────────────┬────────────┐              ║
║              │     │                  │            │              ║
║              │     │                  │            │              ║
║              │   MPU6050            Resistori    Ricevitore IR   ║
║              │   (VCC)              Pull-up      (VCC)           ║
║              │                      Keypad                       ║
║              │                                                   ║
║  GND ────────┴─────┴──────────────────┴────────────┴──────┐     ║
║                                                            │     ║
║                   ┌────────────────────────────────────────┘     ║
║                   │                                              ║
║  GPIO 19 ───────── SDA (MPU6050)                                ║
║  GPIO 23 ───────── SCL (MPU6050)                                ║
║                                                                  ║
║  GPIO 0  ───────── ROW 1 (Keypad) ──────[10kΩ]──── GND         ║
║  GPIO 4  ───────── ROW 2 (Keypad) ──────[10kΩ]──── GND         ║
║  GPIO 16 ───────── ROW 3 (Keypad) ──────[10kΩ]──── GND         ║
║                                                                  ║
║  GPIO 17 ───────── COL 1 (Keypad)                               ║
║  GPIO 5  ───────── COL 2 (Keypad)                               ║
║  GPIO 18 ───────── COL 3 (Keypad)                               ║
║                                                                  ║
║  GPIO 13 ───────── Encoder CLK (Pin A)                          ║
║  GPIO 15 ───────── Encoder DT  (Pin B)                          ║
║  GPIO 2  ───────── Encoder SW  (Button) ──[10kΩ]── GND         ║
║                                                                  ║
║  GPIO 25 ───────[220Ω]───── LED RGB (Red)                       ║
║  GPIO 26 ───────[220Ω]───── LED RGB (Green)                     ║
║  GPIO 27 ───────[220Ω]───── LED RGB (Blue)                      ║
║                             LED Common ───── GND (catodo comune) ║
║                                                                  ║
║  GPIO 33 ───────[100Ω]───── LED IR (Anodo) │                   ║
║                             LED IR (Catodo) ┴─[Transistor]─ GND ║
║                                                                  ║
║  GPIO 14 ───────── IR Receiver (OUT)                            ║
║                                                                  ║
║  GPIO 32 ───────── Wakeup Pin (Encoder Button / MPU Motion)    ║
║                                                                  ║
║  BAT  ────────── LiPo 3.7V (820mAh+)                            ║
║  GND  ────────── Battery GND                                     ║
║                                                                  ║
╚═══════════════════════════════════════════════════════════════════╝
```

## Schema Dettagliato dei Componenti:

### 1. Matrice Keypad 3x3:
```
        COL1(17)  COL2(5)   COL3(18)
ROW1(0) ──┤ 1 ├────┤ 2 ├────┤ 3 ├──
          └───┘    └───┘    └───┘
          
ROW2(4) ──┤ 4 ├────┤ 5 ├────┤ 6 ├──
          └───┘    └───┘    └───┘
          
ROW3(16)──┤ 7 ├────┤ 8 ├────┤ 9 ├──
          └───┘    └───┘    └───┘
```
**Nota:** Ogni ROW ha resistore pull-down 10kΩ verso GND

### 2. Rotary Encoder:
| ESP32   | Encoder        |
|---------|----------------|
| GPIO 13 | CLK (A)        |
| GPIO 15 | DT  (B)        |
| GPIO 2  | SW  (Pulsante) ──[10kΩ]── GND |
| 3V3     | VCC            |
| GND     | GND            |

### 3. LED RGB (Catodo Comune):
| ESP32   | LED RGB        |
|---------|----------------|
| GPIO 25 | ──[220Ω]──── R (Red)   |
| GPIO 26 | ──[220Ω]──── G (Green) |
| GPIO 27 | ──[220Ω]──── B (Blue)  |
| GND     | Common (-)     |

**Nota:** Se usi anodo comune (anode_common: true):
- Common collegato a 3V3
- Logica invertita nel software

### 4. MPU6050 (Accelerometro/Giroscopio):
| ESP32   | MPU6050        |
|---------|----------------|
| GPIO 19 | SDA            |
| GPIO 23 | SCL            |
| 3V3     | VCC            |
| GND     | GND            |
|         | INT (opzionale per interrupt) |

### 5. IR LED (Trasmettitore):
| ESP32   | Circuito IR TX |
|---------|----------------|
| GPIO 33 | ──[100Ω]──┤>├── LED IR (Anodo) |
|         | │              |
|         | └──[Transistor NPN]── GND |

**Nota:** Usa transistor (es. 2N2222) per maggiore potenza. Base collegata a GPIO 33 tramite resistenza 1kΩ.

### 6. IR Receiver (TSOP38238 o simile):
| ESP32   | IR Receiver    |
|---------|----------------|
| GPIO 14 | OUT            |
| 3V3     | VCC            |
| GND     | GND            |

## Note di Progettazione:

### Alimentazione:
- Batteria LiPo 3.7V collegata al pin BAT
- LOLIN32 Lite ha circuito di ricarica integrato
- Consigliato: 820mAh+ per ~2 settimane con deep sleep

### Resistenze Pull-up/Pull-down:
- Rows del keypad: 10kΩ verso GND
- Encoder button: 10kΩ verso GND
- I2C: resistenze pull-up 4.7kΩ su SDA/SCL (spesso integrate nel modulo MPU6050)

### Protezione LED:
- LED RGB: resistenze 220Ω per limitare corrente
- IR LED: resistenza 100Ω + transistor per amplificazione

### Deep Sleep:
- GPIO 32 configurato come wakeup pin
- MPU6050 può attivare wakeup tramite motion interrupt
- Encoder button può essere usato come fallback

### Consumo Energetico:
- Operativo: ~80-150mA
- Deep Sleep: ~10µA (con motion wakeup attivo)
- Autonomia stimata: ~2 settimane con batteria 820mAh
