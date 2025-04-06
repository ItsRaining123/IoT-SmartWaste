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

// Ultrasonic sensor pins
const int trigPin = 32;
const int echoPin = 33;

// Time interval between measurements (in milliseconds)
const unsigned long sendInterval = 3000;
unsigned long previousMillis = 0;

// Metrics measurements
int sendCount = 0;
int successCount = 0;
int failCount = 0;
unsigned long totalLatency = 0;

// Function to measure distance using ultrasonic sensor
float measureDistance() {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  // Set the trigger pin HIGH for 10 microseconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read the echo pin, return the sound wave travel time in microseconds
  long duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  // Speed of sound is 343 m/s = 0.0343 cm/microsecond
  // Distance = (Time x Speed) / 2 (division by 2 because sound travels to object and back)
  float distance = (duration * 0.0343) / 2;

  return distance;
}

// Function to send data to ThingsBoard
void sendToThingsBoard(float distance) {
  HTTPClient http;
  String url = "http://" + String(thingsboardServer) + "/api/v1/" + String(deviceToken) + "/telemetry";

  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["distance"] = distance;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  unsigned long startTime = millis();
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Send the POST request
  int httpResponseCode = http.POST(jsonPayload);
  unsigned long endTime = millis();
  String response = http.getString();

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
  M5.Lcd.println("Ultrasonic");
  M5.Lcd.println("Distance Meter");

  Serial.begin(9600);

  // Initialize ultrasonic sensor pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

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

  unsigned long currentMillis = millis();

  // Check if it's time to send data
  if (currentMillis - previousMillis >= sendInterval) {
    previousMillis = currentMillis;

    float distance = measureDistance();

    // Update display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Distance:");
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("%.2f cm\n", distance);

    if (WiFi.status() == WL_CONNECTED) {
      sendToThingsBoard(distance);
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
  }

  delay(100);
}