# SegreGreen
## Smart Waste-to-WiFi Vending Machine

**Version:** 2.0  
**Last Updated:** February 2026

---

## 📋 Project Overview

**SegreGreen** is an IoT-based smart waste segregation vending machine that rewards users with WiFi access in exchange for properly sorted recyclable waste. Using AI-powered waste detection and a multi-controller architecture, the system promotes environmental responsibility while providing a self-sustaining community internet service.

### Core Concept
- **Deposit Waste → Get WiFi Time**
- **Paper** → 15 minutes WiFi
- **Styrofoam** → 20 minutes WiFi
- **Plastic Bottle** → 30 minutes WiFi

### Sustainability
Collected recyclables are sold to fund the monthly 4G internet plan, making the system self-sustaining.

---

## 🏗️ System Architecture

### Star Topology with ESP-NOW Broadcast

```
                    ┌─────────────────┐
                    │   ESP32-CAM     │
                    │  (AI Detection) │
                    │                 │
                    │ • IR Sensor     │
                    │ • Camera        │
                    │ • TFLite Model  │
                    └────────┬────────┘
                             │
                    ESP-NOW BROADCAST
              "PAPER" / "STYRO" / "PLASTIC_BOTTLE"
                             │
              ┌──────────────┴──────────────┐
              │                             │
              ▼                             ▼
    ┌─────────────────┐           ┌─────────────────┐
    │  ESP32-SLAVE 1  │           │  ESP32-SLAVE 3  │
    │  (Servo Control)│           │  (Token + TFT)  │
    │                 │           │                 │
    │ • Servo Paper   │           │ • Token Gen     │
    │ • Servo Styro   │           │ • TFT ST7735    │
    │ • Servo Plastic │           │ • Serial to Pi  │
    └─────────────────┘           └────────┬────────┘
                                           │
                                    SERIAL (UART)
                                           │
                                           ▼
                                 ┌─────────────────┐
                                 │  ORANGE PI      │
                                 │  ZERO 3         │
                                 │                 │
                                 │ • WiFi Hotspot  │
                                 │ • Captive Portal│
                                 │ • SIM7600 LTE   │
                                 └─────────────────┘

    ┌─────────────────┐
    │  ESP32-SLAVE 2  │
    │  (Bin Monitor)  │
    │                 │
    │ • 3 Ultrasonic  │
    │ • SIM800L SMS   │
    │ • Gate Servo    │
    └─────────────────┘
```

### Module Roles Summary

| Module | Role | Input | Output |
|--------|------|-------|--------|
| **ESP32-CAM** | AI Waste Detection | IR sensor + Camera | ESP-NOW broadcast |
| **ESP32-SLAVE 1** | Servo Control | ESP-NOW from CAM | 3 sorting servos |
| **ESP32-SLAVE 2** | Bin Monitoring | 3x Ultrasonic sensors | SMS alerts + Gate servo |
| **ESP32-SLAVE 3** | Token Generation | ESP-NOW from CAM | TFT display + Serial to Pi |
| **Orange Pi Zero 3** | WiFi Hotspot | Serial from Slave 3 | WiFi + Captive portal |

---

## 🔧 Hardware Components

### ESP32-CAM (Detection Unit)

| Component | Specification | Purpose |
|-----------|--------------|---------|
| ESP32-CAM | OV2640 Camera | Visual waste detection |
| IR Sensor | HC-SR501 | Trigger camera capture |
| Power Supply | 5V 1A | Module power |
| FTDI Programmer | 3.3V/5V | Code uploading |

---

### ESP32-SLAVE 1 (Servo Control Unit)

| Component | Specification | Purpose |
|-----------|--------------|---------|
| ESP32 Dev Board | 38-pin | Controller |
| Servo Motor 1 | SG90/MG996R | Paper chute |
| Servo Motor 2 | SG90/MG996R | Styro chute |
| Servo Motor 3 | SG90/MG996R | Plastic chute |
| Power Supply | 5V 2A (external) | Servo power |

**Wiring:**
```
Servo Paper   → GPIO23
Servo Styro   → GPIO22
Servo Plastic → GPIO21
All VCC       → External 5V
All GND       → Common GND
```

---

### ESP32-SLAVE 2 (Bin Monitoring Unit)

| Component | Specification | Purpose |
|-----------|--------------|---------|
| ESP32 Dev Board | 38-pin | Controller |
| Ultrasonic 1 | HC-SR04 | Paper bin level |
| Ultrasonic 2 | HC-SR04 | Styro bin level |
| Ultrasonic 3 | HC-SR04 | Plastic bin level |
| SIM800L | GSM Module | SMS maintenance alert |
| Servo Motor | SG90 | Entry gate (close when bin full) |
| Logic Level Converter | 4-channel | 5V to 3.3V conversion |
| Power Supply | 5V 2A | System power |

**Wiring:**
```
Ultrasonic 1 (Paper):   Trig→GPIO18, Echo→GPIO5
Ultrasonic 2 (Styro):   Trig→GPIO17, Echo→GPIO16
Ultrasonic 3 (Plastic): Trig→GPIO4,  Echo→GPIO0

SIM800L: TX→GPIO27, RX→GPIO26, PWRKEY→GPIO25

Gate Servo: Signal→GPIO19, VCC→5V, GND→Common
```

---

### ESP32-SLAVE 3 (Token Display Unit)

| Component | Specification | Purpose |
|-----------|--------------|---------|
| ESP32 Dev Board | 38-pin | Controller |
| TFT Display | 1.77" ST7735 (128x160) | Token display |
| Serial Cable | 3-wire UART | To Orange Pi |

**Wiring:**
```
TFT ST7735:
  SCLK → GPIO18
  MOSI → GPIO23
  CS   → GPIO15
  DC   → GPIO5
  RST  → GPIO4
  LED  → GPIO2
  VCC  → 3.3V
  GND  → GND

Serial to Orange Pi:
  ESP32 GPIO17 (TX2) → Orange Pi GPIO15 (RXD)
  ESP32 GPIO16 (RX2) → Orange Pi GPIO14 (TXD)
  ESP32 GND          → Orange Pi GND
```

---

### Orange Pi Zero 3 (WiFi Hotspot + Internet)

| Component | Specification | Purpose |
|-----------|--------------|---------|
| Orange Pi Zero 3 | 1GB/2GB RAM | Main server |
| SIM7600 LTE Module | USB mode | Internet backhaul |
| 4G Antenna | External SMA | Signal boost |
| microSD Card | 16GB+ | OS storage |
| Power Supply | 5V 3A | System power |

**Connections:**
```
UART:
  GPIO15 (RXD) ← ESP32-SLAVE 3 TX
  GPIO14 (TXD) → ESP32-SLAVE 3 RX
  GND          → Common GND

USB:
  USB Port → SIM7600 (USB mode)
```

---

## 📡 Communication Flow

```
┌──────────────────────────────────────────────────────────┐
│                   COMPLETE DATA FLOW                      │
├──────────────────────────────────────────────────────────┤
│                                                           │
│  1. User deposits trash                                   │
│          ↓                                                │
│  2. IR Sensor triggers ESP32-CAM                          │
│          ↓                                                │
│  3. AI classifies waste type                              │
│          ↓                                                │
│  4. ESP-NOW BROADCAST                                     │
│     ├──────────────────────────────────┐                  │
│     ↓                                  ↓                  │
│  ESP32-SLAVE 1                   ESP32-SLAVE 3            │
│  • Opens appropriate servo       • Generates token        │
│  • Closes after 1s               • Displays on TFT        │
│                                  • Sends to Orange Pi     │
│                                         ↓                 │
│                                  ORANGE PI ZERO 3         │
│                                  • Stores token           │
│                                  • Serves captive portal  │
│                                  • Grants WiFi access     │
│                                                           │
│  Meanwhile (independent):                                 │
│  ESP32-SLAVE 2 monitors bin levels                        │
│  • Ultrasonic every 2s                                    │
│  • If bin full → SMS alert via SIM800L                    │
│  • If any bin full → close entry gate                     │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

---

## 📊 Waste-to-Time Mapping

| Waste Type | Time Reward | Servo (Slave 1) | Bin Sensor (Slave 2) |
|------------|-------------|-----------------|----------------------|
| Paper | 15 minutes | Servo 1 (GPIO23) | Ultrasonic 1 (Paper bin) |
| Styrofoam | 20 minutes | Servo 2 (GPIO22) | Ultrasonic 2 (Styro bin) |
| Plastic Bottle | 30 minutes | Servo 3 (GPIO21) | Ultrasonic 3 (Plastic bin) |

---

## 📱 User Interface (TFT Display on Slave 3)

### Screen Dimensions: 128x160 (Landscape)

#### Idle Screen
```
┌────────────────────┐
│   TRASH FOR WIFI   │
│                    │
│      Deposit:      │
│  PAPER - 15 min    │
│  STYRO - 20 min    │
│ PLASTIC - 30 min   │
│                    │
│      Ready...      │
│         ●          │
└────────────────────┘
```

#### Token Display Screen
```
┌────────────────────┐
│   TRASH FOR WIFI   │
│ Item: PLASTIC      │
│                    │
│      TOKEN:        │
│                    │
│      X8Y9Z2        │
│                    │
│    30 MINUTES      │
│ WiFi: SegreGreen   │
│   Enter in browser │
│      29:45         │
└────────────────────┘
```

#### Token Active Screen
```
┌────────────────────┐
│   TOKEN ACTIVE     │
│                    │
│      X8Y9Z2        │
│                    │
│    WiFi in use     │
│  Time remaining:   │
│      25:30         │
└────────────────────┘
```

---

## 💻 Software Architecture

### ESP32-CAM (Detection)
- Captures image on IR trigger
- Runs TensorFlow Lite model
- Broadcasts waste type via ESP-NOW
- Low power, event-driven

### ESP32-SLAVE 1 (Servo Control)
- Listens for ESP-NOW broadcasts
- Controls 3 servos based on waste type
- Simple, fast response

### ESP32-SLAVE 2 (Bin Monitor)
- Monitors 3 ultrasonic sensors every 2 seconds
- Compares distances to full threshold (10cm)
- Sends SMS via SIM800L when bin full
- Closes entry gate when any bin full

### ESP32-SLAVE 3 (Token Display)
- Listens for ESP-NOW broadcasts
- Generates 6-character tokens
- Displays token on TFT with countdown
- Sends token + duration + waste type to Orange Pi via Serial

### Orange Pi Zero 3 (WiFi Server)
- **OS**: Armbian or Debian
- **Python HTTP server** for captive portal
- **hostapd + dnsmasq** for WiFi hotspot
- **SIM7600** via USB for 4G internet
- **Token validation** with expiry timers
- **iptables** for user access control

---

## 🔌 Complete Wiring Summary

### ESP32-CAM
```
IR Sensor OUT → GPIO13
IR Sensor VCC → 5V
IR Sensor GND → GND
Camera → Built-in connector
```

### ESP32-SLAVE 1 (Servo)
```
Servo 1 (Paper)   → GPIO23
Servo 2 (Styro)   → GPIO22
Servo 3 (Plastic) → GPIO21
External 5V       → All Servo VCC
Common GND        → All Servo GND
```

### ESP32-SLAVE 2 (Bin + SMS)
```
Ultrasonic 1: Trig→GPIO18, Echo→GPIO5
Ultrasonic 2: Trig→GPIO17, Echo→GPIO16
Ultrasonic 3: Trig→GPIO4,  Echo→GPIO0

SIM800L: TX→GPIO27, RX→GPIO26, PWRKEY→GPIO25

Gate Servo: Signal→GPIO19
```

### ESP32-SLAVE 3 (Token + TFT)
```
TFT ST7735: SCLK→GPIO18, MOSI→GPIO23, CS→GPIO15,
            DC→GPIO5, RST→GPIO4, LED→GPIO2, VCC→3.3V

Serial to Pi:
  TX2 (GPIO17) → Pi GPIO15 (RXD)
  RX2 (GPIO16) → Pi GPIO14 (TXD)
  GND          → Pi GND
```

### Orange Pi Zero 3
```
UART: GPIO15 ← ESP32-SLAVE 3 TX
      GPIO14 → ESP32-SLAVE 3 RX
      GND    → Common GND
USB: SIM7600 LTE (USB mode)
Power: 5V 3A
```

---

## 📡 ESP-NOW Broadcast Protocol

### Message Structure
```c
typedef struct {
    char wasteType[20];    // "PAPER", "STYRO", "PLASTIC_BOTTLE"
    uint8_t mac[6];        // Sender MAC
    int confidence;        // 0-100
} espnow_message;
```

### Broadcast Behavior
- ESP32-CAM sends to **broadcast MAC** `FF:FF:FF:FF:FF:FF`
- **Slave 1 and Slave 3** receive simultaneously
- Each slave acts independently based on waste type
- **Slave 2** does not participate (no ESP-NOW needed)

---

## 🔐 Token System

### Token Format
- **Length**: 6 characters
- **Character set**: `ABCDEFGHJKLMNPQRSTUVWXYZ23456789`
- **Excluded**: 0/O, 1/I (to avoid confusion)
- **Example**: `X8Y9Z2`, `A7B9C2`

### Token Lifecycle
1. Generated on Slave 3 upon waste detection
2. Displayed on TFT
3. Sent to Orange Pi via Serial
4. Stored with 5-minute validity for user to enter
5. Used once → marked as active
6. WiFi access granted for duration (15/20/30 min)
7. Auto-disconnect after time expires

---

## 🌐 Network Configuration

### WiFi Hotspot (Orange Pi)
- **SSID**: `SegreGreen`
- **Password**: `Recycle2024`
- **Security**: WPA2-PSK
- **Captive Portal**: `http://192.168.4.1`
- **Max Users**: 20+ (Orange Pi)

### 4G LTE (SIM7600)
- **Network**: Smart/Globe (Philippines)
- **Data Plan**: 8GB/month recommended
- **Fallback**: SMS for maintenance alerts

---

## 💰 Business Model

### Revenue (Recyclables)
- PET bottles: ₱15-20/kg
- Paper: ₱5-10/kg
- Styrofoam: ₱10-15/kg

### Estimated Monthly Revenue (100 users/day)
| Waste Type | Kg/month | Price/kg | Revenue |
|------------|----------|----------|---------|
| PET | 90 | ₱18 | ₱1,620 |
| Paper | 30 | ₱8 | ₱240 |
| Styro | 15 | ₱12 | ₱180 |
| **Total** | | | **₱2,040** |

### Monthly Costs
| Item | Cost |
|------|------|
| 4G Data Plan | ₱599 |
| Power | ₱300 |
| Maintenance | ₱200 |
| **Total** | **₱1,099** |

### Net Profit: **~₱941/month** ✅

---

## 📲 SMS Alert System (Slave 2)

### SIM800L Configuration
```
Maintenance Number: +639XXXXXXXXX
Alert Trigger: Bin full (ultrasonic < 10cm)

Message Format:
  "ALERT: [Bin Type] bin is FULL. Please empty immediately."
```

### Entry Gate Logic
- **Gate Open**: All bins below threshold
- **Gate Closed**: Any bin at or above threshold

---

## 🛠️ Testing Procedure

### Phase 1: Individual Module Testing
| Module | Test |
|--------|------|
| ESP32-CAM | Camera capture + TFLite classification |
| ESP32-SLAVE 1 | 3 servo movements |
| ESP32-SLAVE 2 | Ultrasonic readings + SMS + gate |
| ESP32-SLAVE 3 | TFT display + Serial output |
| Orange Pi | WiFi hotspot + captive portal + internet |

### Phase 2: ESP-NOW Communication
1. Get MAC addresses of ESP32-CAM, SLAVE 1, SLAVE 3
2. Configure peers
3. Test broadcast reception
4. Verify both slaves receive simultaneously

### Phase 3: Integration
1. Full flow: Trash → Detection → Servo + Token + Display
2. Serial from Slave 3 to Orange Pi
3. Captive portal validation
4. WiFi access grant
5. Token expiry
6. Bin full → SMS alert → Gate close

---

## 📦 Libraries Required

### ESP32-CAM
```
TensorFlowLite_ESP32
EloquentTinyML
esp_camera
esp_now
WiFi
```

### ESP32-SLAVE 1
```
Servo
esp_now
WiFi
```

### ESP32-SLAVE 2
```
Servo
SoftwareSerial
```

### ESP32-SLAVE 3
```
TFT_eSPI
esp_now
WiFi
```

### Orange Pi Zero 3
```
Python 3.9+
pyserial
hostapd
dnsmasq
iptables
```

---

## 🚀 Future Enhancements

- [ ] QR code for instant connection
- [ ] Mobile app for token management
- [ ] Solar power + battery backup
- [ ] Cloud dashboard for monitoring
- [ ] Weight sensor for accurate sorting
- [ ] Receipt printer for tokens
- [ ] GPS tracking for maintenance scheduling
- [ ] Multi-language support

---

## 📊 System Specifications

| Parameter | Value |
|-----------|-------|
| **Controllers** | 4x ESP32 + 1x Orange Pi Zero 3 |
| **Communication** | ESP-NOW (2.4GHz) + Serial UART |
| **WiFi Standard** | 802.11 b/g/n |
| **Max Users** | 20+ (Orange Pi) |
| **Internet** | 4G LTE (SIM7600) |
| **Display** | 1.77" ST7735 TFT (128x160) |
| **Power Input** | 5V 3A |
| **Power Consumption** | ~1.2A peak |

---

## 📁 Project File Structure

```
SegreGreen/
├── esp32_cam/
│   └── esp32_cam.ino
├── esp32_slave1_servo/
│   └── esp32_slave1_servo.ino
├── esp32_slave2_bin/
│   └── esp32_slave2_bin.ino
├── esp32_slave3_token/
│   └── esp32_slave3_token.ino
├── orange_pi/
│   ├── wifi_server.py
│   ├── hostapd.conf
│   ├── dnsmasq.conf
│   └── setup.sh
├── User_Setup.h
├── wiring_diagrams/
│   ├── esp32_cam.png
│   ├── esp32_slave1_servo.png
│   ├── esp32_slave2_bin.png
│   ├── esp32_slave3_token.png
│   └── orange_pi.png
└── README.md
```

---

## 👥 License

MIT License - Free for educational and commercial use

---

**SegreGreen** - *Turning Trash into Connection, One Bottle at a Time* 🌱📶

*End of Document*
