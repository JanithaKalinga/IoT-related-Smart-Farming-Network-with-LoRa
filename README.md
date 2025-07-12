# IoT related Smart Farming Network with LoRa
 Smart farming system designed to monitor environmental conditions in remote  agricultural fields and send data to a cloud dashboard for real-time monitoring.
1. Project Overview
The system uses sensors to collect temperature, humidity, soil moisture and rain status, and communicates using LoRa and MQTT protocols. 
 
● Remote Sensor Node 
 – Hardware: Arduino Uno + SX1278 LoRa transceiver. 
 – Sensors & Actuator: Soil moisture probe, temperature/humidity sensor, and a servo-driven 
irrigation valve. 
 – Function: Reads each sensor, formats the data as a comma-separated string (including a 
field ID), and transmits packets over LoRa to the local gateway. 
 
● Local Gateway Node 
 – Hardware: Heltec LoRa 32 module (ESP32 + LoRa) at the main farm. 
 – LoRa Integration: Continuously listens for incoming SX1278 packets, parses out sensor 
values and node ID. 
 – MQTT Bridge: Publishes parsed readings to distinct MQTT topics. 
 
● Cloud Dashboard (Node-RED) 
 – Visualization: A dedicated “Remote Field” tab displays live temperature, humidity, soil 
moisture, and valve status. 
 – Two-Way Control: Dashboard switches or sliders publish MQTT commands.

 2. System Architecture

    ● Field Node: Arduino UNO + Sensors + LoRa Transmitter 
 
    ● LoRa Gateway: Heltec LoRa ESP32 
 
    ● MQTT Broker (Mosquitto) 
 
    ● Node-RED Dashboard (subscriber & UI) 