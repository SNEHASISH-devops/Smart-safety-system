#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// SENSOR PINS
#define DHTPIN 4
#define DHTTYPE DHT22
#define GAS_PIN 34
#define BUZZER 25
#define LED 26

// THRESHOLDS
#define TEMP_LIMIT 40.0
#define GAS_LIMIT 2000

// WIFI
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// TELEGRAM: ENTER YOUR OWN DETAILS
String botToken = "9214003668";
String chatID = "@SnehaSafety_Bot";

// MQTT
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* topic = "snehasish/smart-safety-2026/data";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

DHT dht(DHTPIN, DHTTYPE);
Adafruit_SSD1306 display(128, 64, &Wire, -1);

bool previousAlert = false;
unsigned long lastPublish = 0;

String urlEncode(String input) {
  String output = "";
  char hex[] = "0123456789ABCDEF";

  for (int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      output += c;
    } else {
      output += '%';
      output += hex[(c >> 4) & 0x0F];
      output += hex[c & 0x0F];
    }
  }
  return output;
}

void sendTelegram(String message) {
  if (botToken == "9214003668" ||
      chatID == "@SnehaSafety_Bot") return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" +
               botToken + "/sendMessage";

  if (http.begin(client, url)) {
    http.addHeader("Content-Type",
                   "application/x-www-form-urlencoded");

    String body = "chat_id=" + chatID +
                  "&text=" + urlEncode(message);

    int response = http.POST(body);
    Serial.print("Telegram HTTP response: ");
    Serial.println(response);
    http.end();
  }
}

void connectMQTT() {
  while (!mqtt.connected()) {
    String clientID = "ESP32Safety-" +
                      String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientID.c_str())) {
      Serial.println("MQTT connected");
    } else {
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  dht.begin();

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SMART SAFETY IoT");
  display.println("Starting...");
  display.display();

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");

  mqtt.setServer(mqttServer, mqttPort);
}

void loop() {
  if (!mqtt.connected()) {
    connectMQTT();
  }

  mqtt.loop();

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int gasValue = analogRead(GAS_PIN);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("DHT sensor error");
    delay(1000);
    return;
  }

  bool alert = (temperature >= TEMP_LIMIT ||
                gasValue >= GAS_LIMIT);

  digitalWrite(LED, alert ? HIGH : LOW);

  if (alert) {
    tone(BUZZER, 2000);
  } else {
    noTone(BUZZER);
  }

  String status = alert ? "DANGER" : "SAFE";

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("SMART SAFETY IoT");
  display.println("----------------");
  display.print("Temp: ");
  display.print(temperature);
  display.println(" C");
  display.print("Humidity: ");
  display.print(humidity);
  display.println("%");
  display.print("Gas ADC: ");
  display.println(gasValue);
  display.print("Status: ");
  display.println(status);
  display.display();

  Serial.printf(
    "Temperature: %.1f C | Humidity: %.1f%% | Gas: %d | %s\n",
    temperature, humidity, gasValue, status.c_str()
  );

  if (alert && !previousAlert) {
    String message = "SMART SAFETY ALERT!\n";
    message += "Temperature: " + String(temperature) + " C\n";
    message += "Gas ADC: " + String(gasValue) + "\n";
    message += "Danger detected!";

    sendTelegram(message);
  }

  previousAlert = alert;

  if (millis() - lastPublish >= 2000) {
    lastPublish = millis();

    String payload = "{";
    payload += "\"temperature\":" + String(temperature, 1) + ",";
    payload += "\"humidity\":" + String(humidity, 1) + ",";
    payload += "\"gas\":" + String(gasValue) + ",";
    payload += "\"status\":\"" + status + "\"";
    payload += "}";

    mqtt.publish(topic, payload.c_str());
  }

  delay(1000);
}