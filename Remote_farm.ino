#include <SPI.h>
#include <LoRa.h>
#include <DHT11.h>
#include <Servo.h>

// LoRa pins (adjust if your wiring differs)
const int csPin = 10;
const int resetPin = 9;
const int irqPin = 2;

// Define the analog pin connected to the LDR
const int ldrPin = A2;

#define RAIN_SENSOR_PIN A0
#define THRESHOLD 500
const int soilMoisturePin = A1;
#define DHT11_PIN 4
#define SERVO_PIN 8

DHT11 dht11(DHT11_PIN);
Servo valveServo;

struct DHT11Data {
  int temperature;
  int humidity;
};

unsigned long lastSendTime = 0;
const unsigned long sendInterval = 25000;  // Send every 25 seconds

int flag_motor = 0;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  valveServo.attach(SERVO_PIN);
  valveServo.write(90); // Neutral servo position

  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(434E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }

  Serial.println("LoRa Transceiver Ready");
}

void loop() {
  // Check for incoming LoRa packets
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) {
      received += (char)LoRa.read();
    }
    if (received.length() > 0 && isPrintableString(received)) {
      Serial.print("Received: ");
      Serial.println(received);
      controlValve(received);
    }
  }

  // Send sensor data periodically
  unsigned long currentMillis = millis();
  if (currentMillis - lastSendTime >= sendInterval) {
    lastSendTime = currentMillis;

    bool isRaining = detectRain();
    DHT11Data sensorData = readDHT11Sensor();
    int soilMoisture = getSoilMoisturePercent(analogRead(soilMoisturePin));
    int ldrValue = analogRead(ldrPin);
    int ldrPercentage = ((long)ldrValue * 100) / 1023;
    if 

    String message = " | Rain:" + String(isRaining ? "Yes" : "No") + "\n" +
                     " | Temp:" + String(sensorData.temperature) + "C" + "\n" +
                     " | Humidity:" + String(sensorData.humidity) + "%" + "\n" +
                     " | Soil Moisture:" + String(soilMoisture) + "%" + "\n" +
                     " | Intensity:" + String(ldrPercentage) + "%";

    sendMessage(message);
  }

  // Serial command input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toLowerCase();
    controlValve(input);
  }
}

bool detectRain() {
  int sensorValue = analogRead(RAIN_SENSOR_PIN);
  Serial.print("Rain Sensor Value: ");
  Serial.println(sensorValue);
  return sensorValue < THRESHOLD;
}

DHT11Data readDHT11Sensor() {
  DHT11Data data;
  int result = dht11.readTemperatureHumidity(data.temperature, data.humidity);
  if (result != 0) {
    data.temperature = -1;
    data.humidity = -1;
  }
  return data;
}

void controlValve(String command) {
  if (command == "1" && flag_motor == 0) {
    flag_motor = 1;
    valveServo.write(60);
    delay(1000);
    valveServo.write(90);
    Serial.println("Valve opened");
    sendMessage("led:1");
  }
  else if (command == "0" && flag_motor == 1) {
    flag_motor = 0;
    valveServo.write(120);
    delay(1000);
    valveServo.write(90);
    Serial.println("Valve closed");
    sendMessage("led:0");
  }
  else {
    Serial.println("Invalid command. Type '1' or '0'");
  }
}

int getSoilMoisturePercent(int rawValue) {
  int percentage = map(rawValue, 1023, 900, 0, 100);
  return constrain(percentage, 0, 100);
}

bool isPrintableString(const String& str) {
  for (unsigned int i = 0; i < str.length(); i++) {
    if (!isPrintable(str.charAt(i))) {
      return false;
    }
  }
  return true;
}

// Send LoRa message with error handling
bool sendMessage(String message) {
  if (LoRa.beginPacket()) {
    LoRa.print(message);
    int result = LoRa.endPacket();
    if (result == 1) {
      Serial.print("Sent: ");
      Serial.println(message);
      return true;
    } else {
      Serial.println("Send failed: endPacket() returned 0");
      recoverLoRa();
      return false;
    }
  } else {
    Serial.println("Send failed: beginPacket() failed");
    recoverLoRa();
    return false;
  }
}

// Attempt to recover LoRa module
void recoverLoRa() {
  Serial.println("Recovering LoRa...");
  LoRa.end();
  delay(500);
  LoRa.begin(433E6);
  delay(500);
}
