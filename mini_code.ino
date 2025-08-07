#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <ESP8266WiFi.h>

// ThingSpeak settings
String apiKey = "D64QYC4DTTTMKOOW"; // Replace with your Write API key
const char *ssid = "Galaxy A120939"; // Replace with your Wi-Fi SSID
const char *pass = "auaz3575";       // Replace with your Wi-Fi password
const char *server = "api.thingspeak.com";

// BMP280 sensor configuration
Adafruit_BMP280 bmp; // Create an instance of the BMP280 sensor

// DHT11 sensor configuration
#define DHTPIN D5          // Pin where the DHT11 is connected
DHT dht(DHTPIN, DHT11);

// Rain sensor configuration
#define POWER_PIN D7 // Pin providing power to the rain sensor
#define AO_PIN A0    // Analog pin connected to the rain sensor
const int RAIN_THRESHOLD = 700; // Adjust based on your sensor calibration

WiFiClient client;

void setup() {
  Serial.begin(9600);
  delay(10);

  // Initialize sensors
  dht.begin();
  pinMode(POWER_PIN, OUTPUT); // Configure the power pin for the rain sensor as OUTPUT

  // Connect to Wi-Fi
  Serial.println("Connecting to Wi-Fi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi connected.");

  // Initialize BMP280 sensor
  if (!bmp.begin(0x76)) { // Initialize BMP280 with default I2C address 0x76
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }
}

void loop() {
  // Read data from DHT11
  float humidity = dht.readHumidity();
  float temperatureDHT = dht.readTemperature();

  if (isnan(humidity) || isnan(temperatureDHT)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // Read data from BMP280
  float temperatureBMP = bmp.readTemperature();
  float pressure = bmp.readPressure() / 100.0F; // Convert pressure to hPa

  if (isnan(temperatureBMP) || isnan(pressure)) {
    Serial.println("Failed to read from BMP280 sensor!");
    return;
  }

  // Power the rain sensor and read its value
  digitalWrite(POWER_PIN, HIGH); // Turn the sensor ON
  delay(10);                     // Allow time to stabilize
  int rainValue = analogRead(AO_PIN);
  digitalWrite(POWER_PIN, LOW);  // Turn the sensor OFF

  // Determine rain status
  bool isRaining = rainValue < RAIN_THRESHOLD; // True if raining, false otherwise

  // Log data to the Serial Monitor
  Serial.print("DHT Temperature: ");
  Serial.print(temperatureDHT);
  Serial.print(" °C, DHT Humidity: ");
  Serial.print(humidity);
  //Serial.print(" %, BMP Temperature: ");
 // Serial.print(temperatureBMP);
  Serial.print("Pressure: ");
  Serial.print(pressure);
  Serial.print(" hPa, Rain Status: ");
  Serial.println(isRaining ? "Raining" : "Not Raining");

  // Send data to ThingSpeak
  if (client.connect(server, 80)) {
    String postStr = "api_key=" + apiKey;
    postStr += "&field1=";
    postStr += String(temperatureDHT); // Send DHT temperature
    postStr += "&field2=";
    postStr += String(humidity); // Send DHT humidity
    postStr += "&field3=";
    postStr += String(isRaining); // Send rain status
    postStr += "&field5=";
    postStr += String(temperatureBMP); // Send BMP temperature
    postStr += "&field4=";
    postStr += String(pressure); // Send pressure
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);

    // Check for response from ThingSpeak
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("Timeout while waiting for response.");
        client.stop();
        return;
      }
    }

    // Read the response from ThingSpeak
    String response = "";
    while (client.available()) {
      response = client.readString();
    }

    Serial.println("Data sent to ThingSpeak.");
    Serial.println("Response: " + response);
  } else {
    Serial.println("Failed to connect to ThingSpeak.");
  }

  client.stop();
  Serial.println("Waiting...");

  // ThingSpeak requires at least 15 seconds between updates
  delay(15000);
}