#include <M5StickCPlus.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "...";
const char* password = "...";

// ThingsBoard configuration
const char* thingsboardServer = "demo.thingsboard.io";
const char* deviceToken = "...";

// Metrics measurements
int sendCount = 0;
int successCount = 0;
int failCount = 0;
unsigned long totalLatency = 0;

const int SMOKE_SENSOR_PIN = 33;

// Function to send data to ThingsBoard
void sendToThingsBoard(int sensorValue) {
  HTTPClient http;
  String url = "http://" + String(thingsboardServer) + "/api/v1/" + String(deviceToken) + "/telemetry";

  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["co2_level"] = sensorValue;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  unsigned long startTime = millis();
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Send the POST request
  int httpResponseCode = http.POST(jsonPayload);
  unsigned long endTime = millis();

  // Check for success
  if (httpResponseCode >= 200 && httpResponseCode < 300) {
    unsigned long latency = endTime - startTime;
    M5.Lcd.printf("HTTP Response: %d\n", httpResponseCode);
    M5.Lcd.printf("Latency (ms): %d\n", latency);
    successCount++;
    totalLatency = totalLatency + latency;
  } else if (httpResponseCode >= 400 && httpResponseCode < 500) {
    M5.Lcd.printf("Client Error: %d\n", httpResponseCode);
    failCount++;
  } else if (httpResponseCode >= 500 && httpResponseCode < 600) {
    M5.Lcd.printf("Server Error: %d\n", httpResponseCode);
    failCount++;
  } else {
    M5.Lcd.printf("Error: %d\n", httpResponseCode);
    failCount++;
  }

  http.end();
}

void setup() {
  // Initialize M5StickC Plus
  M5.begin();
  M5.Lcd.setRotation(3);  // Landscape orientation
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Smoke Sensor");
  M5.Lcd.println("CO2 Meter");

  Serial.begin(9600);

  // Connect to WiFi
  M5.Lcd.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }

  M5.Lcd.println("\nWiFi connected!");
  M5.Lcd.printf("IP: %s\n", WiFi.localIP().toString().c_str());
  delay(2000);

  M5.Lcd.fillScreen(BLACK);
}

void loop() {
  M5.update();  // Update button state

  // Read sensor
  int sensorValue = analogRead(SMOKE_SENSOR_PIN);

  // Update display
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("CO2 Level:");
  M5.Lcd.setTextSize(3);
  M5.Lcd.printf("%d ppm\n", sensorValue);

  if (WiFi.status() == WL_CONNECTED) {
    sendToThingsBoard(sensorValue);
    sendCount++;
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("Data sent to ThingsBoard");
  } else {
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("WiFi disconnected!");

    // Attempt to reconnect
    WiFi.begin(ssid, password);
  }

  // Check metrics every 10 data sent
  if (sendCount % 10 == 0) {
    float packetLoss = (float)failCount / 10 * 100;
    Serial.println("Packet Loss: ");
    Serial.print(packetLoss);
    Serial.println("%");

    Serial.println("Average Latency: ");
    Serial.print(totalLatency / 10);
    Serial.println("ms");

    totalLatency = 0; // Reset
  }

  delay(3000); // Read every 3 seconds
}