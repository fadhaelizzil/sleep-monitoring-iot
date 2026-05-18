# SiPETIR: IoT-Based Wireless Sleep Monitoring System for Boarding Schools

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-NodeMCU%20%7C%20ESP8266-blue?style=for-the-badge&logo=espressif" alt="Hardware">
  <img src="https://img.shields.io/badge/Software-Arduino%20IDE%20%7C%20Blynk%20App-orange?style=for-the-badge&logo=arduino" alt="Software">
  <img src="https://img.shields.io/badge/Research-OPSI%202025%20Submission-purple?style=for-the-badge" alt="Research">
  <img src="https://img.shields.io/badge/Cost-~Rp200k%20($13%20USD)-green?style=for-the-badge" alt="Cost">
  <img src="https://img.shields.io/badge/SDG-No.%203%20Good%20Health%20%26%20Well--being-green?style=for-the-badge" alt="SDG">
</p>

## 🏆 Project Standings & Roles
* **Research Submission:** National Science Olympiad for Research (OPSI) 2025 - Applied Sciences (Engineering).
* **Project Roles:** Project Leader, Lead Hardware Engineer, & Lead Coder (Muhammad Fadhael Izzil Haq).
* **Research Partner:** Samir Affan Wiguna.
* **Affiliation:** MAN Insan Cendekia Serpong, South Tangerang, Banten, Indonesia.

## 📝 Overview

**SiPETIR** (*Sistem Pemantauan Tidur*) is an innovative, **ultra-low-cost (~Rp200,000 / $13 USD)** IoT-based sleep monitoring system designed specifically for the unique environment of boarding schools. Developed using 8 strain-gauge load cells, an HX711 amplifier module, and a NodeMCU ESP8266 microcontroller, the system automates curfew enforcement and student safety tracking by constantly auditing bed occupancy and load variations after hours. Real-time data and automated push notifications are seamlessly routed to the hostel warden's (*wali asrama*) smartphone via the Blynk application dashboard, avoiding intrusive late-night physical checks.

> 📢 **Portfolio Note:** This project was developed in **November 2024** driven by empathy for my peers' well-being and a personal commitment to assist our school administration and hostel wardens. I am publishing this open-source repository as core documentation for my college application portfolio. 

---

## 💡 The Story Behind: A Real-World Problem

### The Incident (October 2024)
Living in the dormitories of MAN Insan Cendekia Serpong, students are strictly required to sleep by 23:00 to sustain their intensive academic performance. However, structural late-night stayovers and students crowding into single dorm beds were common infractions. 

The turning point occurred in **October 2024**: four students were hanging out on a top bunk bed while another student was asleep directly underneath. The high-altitude bunk frame structurally collapsed, falling flat on the student below. Beyond messing up sleep schedules, it was a critical safety hazard that endangered student life and cost the school expensive facility maintenance repairs.

### Turning Care into Solutions
Witnessing this firsthand, I took the initiative as the **Project Leader and Lead Hardware Engineer/Coder** to design an operational countermeasure. Manual rounds by a warden typically waste over 10 minutes just scanning 15 rooms—proving incredibly inefficient for facilities hosting hundreds of students, while fundamentally disrupting student privacy. Market solutions like smartwatches or commercial smart mattresses are either restrictive for mass school deployment or prohibitively expensive.

SiPETIR was born out of a real desire to help our dormitory parents gain visibility without intrusion, ensuring student safety, protecting school properties, and preserving essential sleep health.

🚀 **From Prototype to Impact:** The success, engineering frameworks, and foundational telemetry models built during this exact project catalyzed the birth of **Pulas.io**—a youth-led social enterprise dedicated to systematically addressing sleep deprivation, curfew balance, and structural well-being among Indonesian students.

---

## 🛠️ System Architecture & Logic

The firmware maintains a continuous automated telemetry loop starting exactly at 23:00. The device executes tare calibration upon initialization and evaluates the current state using a specialized dynamic buffer zone ($\pm5\text{ kg}$) to compensate for natural tossing and turning.

| Scenario | Load Cell Weight Status | System Status | Action / Warden Notification Output |
| :--- | :---: | :---: | :--- |
| **1. Student Safely in Bed** | ✅ Within Calibrated Target | **SAFE** | System loops every second. Dashboard reads normal. |
| **2. Empty Bed / Out-of-bounds** | ❌ Below Lower Threshold | **⚠️ ABSENT** | Starts an internal timer. If absent for **>15 minutes**, Blynk dispatches an instant push notification: *"Student is missing from bed!"* |
| **3. Bed Overcrowding / Danger** | ❌ Above Upper Threshold | **🚨 ANOMALY** | Instantly catches illegal bunk stayovers. Dispatches instant push warning: *"Overload detected! Potential structural safety hazard."* |

---

### System Flowchart

```text
     [ START: 23:00 Curfew ]
                │
        [ Initialization ] ────> Connect to Local Dorm Wi-Fi
                │
      [ Read 8x Load Cells ] ──> Convert Analog Signals to Digital via HX711
                │
    ┌───────────┴───────────┐
    ▼                       ▼
[ Weight OK? ]       [ Overload Detected? (>62kg)* ]
    │                       │
    │ (Yes)                 │ (Yes)
    ▼                       ▼
 [ Loop ]             [ INSTANT BLYNK ALERT: Overload Danger ]
    │
    └─> [ Absent / Bed Empty? (<52kg)* ]
                │
                │ (Yes)
                ▼
      [ Start 15-Minute Timer ]
                │
        [ Still Empty > 15m? ] ──(Yes)──> [ BLYNK ALERT: Student Missing ]

*Note: Threshold examples based on a calibrated baseline test student weighing 57 kg.
```
---

## 🔌 Hardware Configuration & Bill of Materials (BOM)

To keep implementation highly scalable for standard Indonesian public schools, the total build cost was constrained underneath Rp200,000 (~$13 USD):
* **Core Processor:** NodeMCU ESP8266 (Onboard Wi-Fi Module for server telemetry syncing).
* **Weight Infrastructure:**
  * 8x Strain-Gauge Load Cells (Affordable sensors balanced arrayed beneath bed structures).
  * 3x HX711 Amplifier AD Modules (24-bit high-precision ADC to digitize signal inputs).
* **Power & Portability:** 3.7V Lithium Battery coupled with a generic Battery Shield for offline grid backup.

---

## 📈 Performance & Empirical Results
Extensive iterative R&D engineering loops delivered highly reliable metrics during field testing at the MAN IC Serpong dormitories:
* **Accuracy Metrics:** Evaluated across 90 structural stress tests, achieving an **average tracking accuracy of 97.78%.** Early sensor signal noise spikes triggered during fast bed entries were engineered out by modifying the firmware code to only accept stable weight averages after a 5-second buffer.
* **Network Latency:** Telemetry sync speed via the Wi-Fi stack to the remote Blynk application dashboard is exceptionally agile:
  * **Bed Absent Alerts:** Avg. Latency of **1.21 seconds.**
  * **Overload Safety Warnings:** Avg. Latency of **1.55 seconds.**

 ---

 ## 🚀 Getting Started
 **Prerequisites**
 1. Open the Arduino IDE (v2.0+ recommended).
 2. Install the ESP8266 Board Core via the Preferences Board Manager URL.
 3. Import the required project libraries through the Library Manager:
    * HX711 (By Bogdan Necula)
    * Blynk (By Volodymyr Shymanskyy)

**Installation & Deployment**
1. Clone this repository to your local directory:
   git clone [https://github.com/fadhaelizzil/sipetir-sleep-monitoring-iot.git](https://github.com/fadhaelizzil/sipetir-sleep-monitoring-iot.git)
2. Open /src/sipetir_main.ino within your Arduino IDE.
3. Navigate to the credential lines and alter placeholder details with your dedicated values:
   * char auth[] = "YOUR_BLYNK_AUTH_TOKEN";
   * char ssid[] = "YOUR_DORM_WIFI_SSID";
   * char pass[] = "YOUR_DORM_WIFI_PASSWORD";
4. Flash the custom sketch over Micro-USB into your NodeMCU ESP8266 board.
5. Calibrate your physical sensor array using the /src/calibration_helper.ino sketch with standard reference weights before permanent hardware fitting under the bed frames.
