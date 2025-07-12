#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFiManager.h>

//── LoRa pins ──
#define SS      18
#define RST     14
#define DIO0    26

//── OLED settings ──
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET     16
#define OLED_SDA       4
#define OLED_SCL       15
TwoWire I2COLED = TwoWire(1);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2COLED, OLED_RESET);

//── On-board LED ──
#define LED_PIN 25

//── MQTT Broker ──
const char* mqtt_server = "broker.mqtt.cool";
WiFiClient   espClient;
PubSubClient client(espClient);

//── Polling ──
unsigned long lastRequestTime = 0;
const long requestInterval = 16000;  // 16 s

//── Last sensor values ──
String lastRain = "-", lastTemp = "-", lastHumidity = "-", lastSoil = "-", lastIntensity = "-", lastMotor = "-";

//── Task Handles ──
TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;

//───────────────────────────────────────
void connectToWifi() {
  WiFiManager wm;
  if (!wm.autoConnect("JSK_AgroSense_ConfigAP")) {
    Serial.println("Wi-Fi failed, restarting");
    delay(3000);
    ESP.restart();
  }
  Serial.println("Wi-Fi connected:");
  Serial.println("  SSID: " + WiFi.SSID());
  Serial.println("  IP:   " + WiFi.localIP().toString());
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("MQTT connecting… ");
    if (client.connect("ESP32_LoRa_Gateway")) {
      Serial.println("OK");
      client.subscribe("JSK/m");
    } else {
      Serial.print("fail, rc=");
      Serial.print(client.state());
      Serial.println("; retry in 2 s");
      delay(2000);
    }
  }
}

String extractValue(const String &data, const String &key, const String &endChar) {
  int s = data.indexOf(key);
  if (s < 0) return "";
  s += key.length();
  int e = data.indexOf(endChar, s);
  if (e < 0) e = data.length();
  return data.substring(s, e);
}

void publishSensors(const String &msg) {
  String rain   = extractValue(msg, "R:", ",");
  String temp   = extractValue(msg, "T:", ",");
  String hum    = extractValue(msg, "H:", ",");
  String soil   = extractValue(msg, "S:", ",");
  String ldr    = extractValue(msg, "L:", ",");
  String motor  = extractValue(msg, "V:", " ");

  lastRain = rain;
  lastTemp = temp;
  lastHumidity = hum;
  lastSoil = soil;
  lastIntensity = ldr;
  lastMotor = motor;

  updateDisplay();

  client.publish("JSK/sensor/rain",         rain.c_str(),  true);
  client.publish("JSK/sensor/temp",         temp.c_str(),  true);
  client.publish("JSK/sensor/humidity",     hum.c_str(),   true);
  client.publish("JSK/sensor/soilmoisture", soil.c_str(),  true);
  client.publish("JSK/sensor/ldr",          ldr.c_str(),   true);
  client.publish("JSK/led",                 motor.c_str(), true);
}

void updateDisplay() {
  String rain_status = "";
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  if (lastRain == "1"){
    rain_status = "Yes";
  }
  else{
    rain_status = "No";
  }
  display.println("Latest Sensor Readings:");
  display.println("---------------------");
  display.println("Rain:     " + rain_status      + ".");
  display.println("Temp:     " + lastTemp      + "C");
  display.println("Humidity: " + lastHumidity  + "%");
  display.println("Soil:     " + lastSoil      + "%");
  display.println("LDR:      " + lastIntensity + "%");
  display.println("Motor:    " + lastMotor     + "%");

  display.display();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received [");
  Serial.print(topic);
  Serial.print("]: ");

  char payloadChar[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    payloadChar[i] = (char)payload[i];
  }
  payloadChar[length] = '\0';
  Serial.println();

  if (strcmp(topic, "JSK/m") == 0) {
    if (payloadChar[0] == '1' || payloadChar[0] == '0') {
      LoRa.beginPacket();
      LoRa.print(payloadChar[0]);
      LoRa.endPacket();
      Serial.println("→ Sent motor command: " + String(payloadChar[0]));
    }
  }
}

//───────────────────────────
// LoRa task
void loraTask(void *parameter) {
  for (;;) {
    if (millis() - lastRequestTime >= requestInterval) {
      lastRequestTime = millis();
      LoRa.beginPacket();
      LoRa.print("REQ");
      LoRa.endPacket();
      Serial.println("→ LoRa SENT: REQ");
    }

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String message;
      while (LoRa.available()) message += char(LoRa.read());
      message.trim();
      Serial.println("← LoRa RECV: " + message);

      if (message.startsWith("ACK :")) {
        publishSensors(message);
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        digitalWrite(LED_PIN, LOW);
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// MQTT task
void mqttTask(void *parameter) {
  for (;;) {
    if (!client.connected()) reconnectMQTT();
    client.loop();
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

//───────────────────────────
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // OLED init
  I2COLED.begin(OLED_SDA, OLED_SCL);
  pinMode(OLED_RESET, OUTPUT);
  digitalWrite(OLED_RESET, LOW); delay(20); digitalWrite(OLED_RESET, HIGH);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 failed")); while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Welcome To");
  display.println("JSK AgroSense");
  display.println("Farming");
  display.println("----------------");
  display.println("LoRa Transceiver");
  display.println("Receive: Waiting...");
  display.display();

  connectToWifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // LoRa init
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(875.5E6)) {
    Serial.println("LoRa init failed"); while (true);
  }
  LoRa.setSyncWord(0xA5);
  LoRa.enableCrc();
  LoRa.setTxPower(18);
  LoRa.receive();
  Serial.println("LoRa Receiver Initialized");

  // Create tasks
  xTaskCreatePinnedToCore(mqttTask, "MQTT Task", 8192, NULL, 1, &mqttTaskHandle, 0);
  xTaskCreatePinnedToCore(loraTask, "LoRa Task", 8192, NULL, 1, &loraTaskHandle, 1);
}

void loop() {
  // Empty: everything runs in tasks now
}
