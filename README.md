# air-home-mqtt
## Description

Home air quality station project

## Components used

#### Board
NodeMCU v1.0 (ESP-12E)
#### Sensors

|       Name     |      Metrics collected                        
|----------------|-------------------------------
|BME280          |`Temperature, Humidity, Pressure`            
|Senseair S8     |`Carbon Dioxide (CO2)`            
|PMS5003         |`Particles in air (Dust PM)`

## Software

Developed in VScode + PlatformIO
But can be also run on ArduinoIDE

# RESULT
For data visualisation I'm using this stack: https://github.com/spookyrecharge/mosquitto-telegraf-influxdb-grafana
![image](https://user-images.githubusercontent.com/5503131/168440375-db141415-25a8-4ed2-b14d-5ce38c3f3a5a.png)

Details, pinout, etc: **LATER**

# Pinout schema:
![image](https://user-images.githubusercontent.com/5503131/168439977-e20a8898-139d-4627-a25d-f504d8045e54.png)

