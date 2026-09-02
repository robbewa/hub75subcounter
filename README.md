# ESP32 YouTube Subscriber Counter & HUB75 Matrix Display

An open-source, Wi-Fi-connected YouTube subscriber counter and multi-functional clock powered by an ESP32 and a 128x64 HUB75 LED matrix panel. 

It fetches real-time channel statistics via the YouTube Data API, displays a custom channel logo, includes an active YouTube-themed NTP clock, and triggers animated fireworks upon hitting subscriber milestones!

![https://www.youtube.com/watch?v=sCw4Do6QDgE](banner.jpg)

---

## Features

- **Live YouTube Stats:** Fetches and displays real-time subscriber counts using the YouTube Data API v3.
- **Custom Logo Integration:** Display your own 128x64 channel logo or graphic.
- **NTP Time Sync & YouTube Progress Bar:** Digital clock synced via NTP with a custom progress bar simulating a YouTube video player.
- **Milestone Animations:** Custom animated fireworks show whenever a new subscriber joins or a milestone is reached.
- **Automated Activity Scheduler:** Screen automatically turns on at 09:00 and off at 22:00 to save power and extend LED lifespan.
- **Display Rotation Support:** Software-configurable screen orientation (0°, 90°, 180°, 270°).
- **Custom 3D Printed Stand:** Dedicated magnetic enclosure with integrated USB-C power delivery.

---

## Hardware Requirements

- **Microcontroller:** ESP32 (e.g., ESP32-WROOM-32 / ESP32-S3)
- **Display Panel:** HUB75 LED Matrix Panel (128x64 pixels)
- **Power Supply:** 5V Power Supply / USB-C (ensure sufficient current capacity for the LED matrix)
- **Connector:** HUB75 ribbon cable (soldered directly to ESP32 for signal stability)
- **Enclosure:** Custom 3D-printed magnetic stand (STL files provided below)

---

## Pinout & Wiring Guide

>  **Important:** Standard jumper wires can cause severe signal interference on HUB75 panels. For optimal stability, it is strongly recommended to cut a HUB75 ribbon cable and solder the wires directly to the ESP32 GPIO pins.

| HUB75 Cable Wire | Function | ESP32 GPIO Pin |
| :--- | :--- | :--- |
| Wire 1 | R1 (Red 1) | GPIO 25 |
| Wire 2 | G1 (Green 1) | GPIO 26 |
| Wire 3 | B1 (Blue 1) | GPIO 27 |
| Wire 4 | GND | GND |
| Wire 5 | R2 (Red 2) | GPIO 14 |
| Wire 6 | G2 (Green 2) | GPIO 12 |
| Wire 7 | B2 (Blue 2) | GPIO 13 |
| Wire 8 | GND | GND |
| Wire 9 | A (Row Address) | GPIO 33 |
| Wire 10 | B (Row Address) | GPIO 32 |
| Wire 11 | C (Row Address) | GPIO 22 |
| Wire 12 | D (Row Address) | GPIO 19 |
| Wire 13 | CLK (Clock) | GPIO 4 |
| Wire 14 | LAT / LATCH | GPIO 2 |
| Wire 15 | OE (Output Enable) | GPIO 15 |
| Wire 16 | GND | GND |

*Note: Ensure the ESP32 ground (GND) and the HUB75 5V external power supply ground are connected to a common ground.*

---

## Quick Start & Setup

### 1. Prerequisites
- Install [Arduino IDE](https://www.arduino.cc/en/software).
- Add ESP32 Board Support to your Arduino IDE.
- Required Libraries: Install `ESP32-HUB75-MatrixPanel-I2S-DMA`, `ArduinoJson`, and `NTPClient` via the Library Manager.
- Obtain a YouTube Data API Key via the [Google Cloud Console](https://console.cloud.google.com/).

### 2. Preparing Image Assets
1. Create or resize your image to **128x64 pixels** in an image editor.
2. Convert the image using [image2cpp](https://endroad.net/env/image2cpp/).
3. Export the file as a `.h` header array and place it in your project directory.

### 3. Configuration & Flash
1. Clone or download this repository:
   ```bash
   git clone https://github.com/your-username/esp32-youtube-counter.git
   ```

2. Open the project in Arduino IDE and select your ESP32 board.
3. Update your credentials in the main sketch / configuration file:
```cpp
const char* ssid       = "YOUR_WIFI_SSID";
const char* password   = "YOUR_WIFI_PASSWORD";
const char* apiKey     = "YOUR_YOUTUBE_API_KEY";
const char* channelId  = "YOUR_YOUTUBE_CHANNEL_ID";
```

4. Set your preferred display orientation (`0`, `1`, `2`, or `3`).
5. Hold the **BOOT** button on your ESP32 (if required) and upload the code.

---

## 3D Printed Stand (Enclosure)

The repository includes printable STL files for a custom rear-mounting stand featuring:

* Recessed slots for M3 heat-set inserts / bolts.
* Magnet mounts for quick snap-on backplate removal.
* Integrated cutout for a panel-mount USB-C extension port.

 Find all model files in the `/3D_Files` directory.

---

## Video & Build Guide

Want to see how this project was built step-by-step or see it in action? Check out the full video tutorial on YouTube:

---

## License & Terms of Use

This project is open-source and shared strictly for personal and educational use.

This work is licensed under a **[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (CC BY-NC-SA 4.0)](https://creativecommons.org/licenses/by-nc-sa/4.0/)**.

```
    -  Attribution Required: You must give appropriate credit to Robbe Wauters and link back to this repository or the original YouTube channel.
    -  No Commercial Use: You may NOT sell, commercialize, package, or monetize this code, project files, or physical derivatives in any form.
    -  ShareAlike: If you remix, transform, or build upon the material, you must distribute your contributions under the same license as the original.

```

---

##  Credits & Author

Designed and developed by **Robbe Wauters**.

If you build your own version of this subscriber counter, feel free to share your build or tag me on social media! If this project helped you, consider subscribing to the channel or dropping a star on this repository.

```

```
