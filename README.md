
# Intelligent Electrical Monitoring and Anomaly Detection

## Project Overview

This project focuses on the design and implementation of an intelligent electrical current monitoring system and anomaly detection using STM32F407, ACS712, NanoEdge AI, ESP8266 and ThingSpeak.

## Project Architecture

- **STM32F407:** Main microcontroller for data acquisition and processing.
- **ACS712:** Electrical current measurement.
- **DHT22:** Temperature and humidity measurement.
- **NanoEdge AI:** Anomaly detection based on current and temperature data.
- **ESP8266:** Wi-Fi communication.
- **ThingSpeak:** Remote monitoring and visualization.

## Repository Structure

```text
Smart_Electrical_Monitoring/
├── STM32/
├── ESP8266/
├── Simulation/
├── README.md
└── .gitignore
```

## Current Project Status

- STM32 firmware developed and compiled.
- NanoEdge AI library integrated.
- ESP8266 firmware developed and compiled.
- Python simulation sends synthetic data to ThingSpeak.
- Physical hardware validation is not yet performed.

## Technologies

- STM32CubeIDE
- Arduino IDE
- NanoEdge AI Studio
- Python
- ThingSpeak
- Git and GitHub

## Security

Wi-Fi credentials and ThingSpeak API keys must not be committed to this repository.

## Author

University Internship Project
