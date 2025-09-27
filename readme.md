# Pi Latency Tester

`pi-latency-tester` is a low-level tool for Raspberry Pi that measures end-to-end gaming latency. It analyzes the time from a physical button press to the on-screen result, helping users optimize their setups.

## Features

* **Multi-Stage Analysis**: Breaks down latency into `Controller + Kernel` and `Software + Core` components.
* **Universal Controller Support**: Tests both GPIO and standard USB gamepads.
* **Display Lag Measurement**: Measures display latency by comparing against a CRT baseline.
* **High Precision**: Uses a low-level C program for accurate timing.
* **Auto-Detection**: Finds the controller and Pi model for consistent logging.

## Future Enhancements
* **Potential Refactor**: Changing the GPIO driver from polling to an interrupt-based system could offer a marginal increase in precision.

***
## Hardware Required

| Component | Purpose | Specification |
| :--- | :--- | :--- |
| **Raspberry Pi** | Host system | Any model |
| **Phototransistor**| Screen sensor | 5mm, NPN, Visible Light |
| **10kΩ Resistor** | Pull-down circuit| Brown-Black-Orange |
| **Jumper Wires** | Connections | Male-to-Female |
| **Breadboard** | Sensor circuit | Solderless, mini or half-size|
| **Controller**| Device under test| GPIO or USB |
| **CRT Monitor**| **(Optional)** For baseline | Any functional CRT |

***
## 1. Software Setup

* Install [`gpio-joystick-rpi`](https://github.com/forkymcforkface/gpio-joystick-rpi) and verify your GPIO pinout.
 
```bash
# Clone tester
git clone https://github.com/forkymcforkface/RPi-Latency-tester
cd RPi-Latency-tester
```
#### 2. Compile
```bash
# Install libgpiod
sudo apt-get install libgpiod-dev
# Compile the tester
gcc -Wall -o pi-latency pi-latency.c -lgpiod
```

#### 3. Run a Test
Load the GPIO driver, then run the latency tester. You must provide a name for the core you're testing.
```bash
# Load the Driver:
sudo insmod gpio-joystick-rpi.ko
# Run the Tester:
sudo ./pi-latency snes9x
```

Now, follow the on-screen instructions and press the test button on your controller. The results will appear in the terminal.

```
--- Test 2 Started for snes9x ---
T0 (Physical Press): Captured
T1 (OS Event):       Captured
T2 (Photon Output):  Captured

--- Results for Current Test ---
Controller + Kernel Latency: 1.213 ms
Software + Core Latency:     8.199 ms
Total End-to-End Latency:    9.412 ms

--- Session Statistics (Total Tests: 2) ---
Category                Min             Max             Average
------------------------------------------------------------------
Controller+Kernel       1.158 ms        1.213 ms        1.185 ms
Software+Core           8.199 ms        8.215 ms        8.207 ms
Total End-to-End        9.373 ms        9.412 ms        9.392 ms
------------------------------------------------------------------
```
