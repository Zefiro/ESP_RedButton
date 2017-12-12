/**
 * Arduino IDE Hardware Settings:
 * - Generic ESP8266
 * - Reset Mode: nodemcu
 * - Upload Speed: 256000
*/
#include <Arduino.h>

// #include <ESP8266WiFi.h>
// #include <ESP8266WiFiMulti.h>
// #include <ESP8266HTTPClient.h>
// ESP8266WiFiMulti WiFiMulti;
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
WiFiMulti WiFiMulti;


#include <Adafruit_NeoPixel.h>

#define NEOPIXELS_NUM 2
#define NEOPIXEL_PIN 5
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NEOPIXELS_NUM, NEOPIXEL_PIN, NEO_RGB + NEO_KHZ800);

#include "Bounce2.h"
Bounce bouncerButtonA = Bounce();
Bounce bouncerButtonB = Bounce();

#define BUTTON_A_PIN 4
#define BUTTON_B_PIN 0


#define STATE_INIT 0
#define STATE_CONNECTED 1
#define STATE_DISCONNECTED 2
#define STATE_SENDING 3
#define STATE_SEND_ERROR 4
byte state = STATE_INIT;

struct config_T {
  char* wifi_ssid;
  char* wifi_pass;
  String baseurl;
  String cmd_RedButton;
  String cmd_HiddenButton;
  String cmd_Ping;
} *config;

#include "config.h"

void setup() {
  initConfig();
  config = &configArray[0];
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON_A_PIN, INPUT_PULLUP);
  pinMode(BUTTON_B_PIN, INPUT_PULLUP);
  digitalWrite(LED_BUILTIN, HIGH); // LED ON

  // Initialisierung der Debouncer-Objekte zu den Pins
  bouncerButtonA.attach(BUTTON_A_PIN);
  bouncerButtonB.attach(BUTTON_B_PIN);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();


  Serial.printf("RedButton v1.0 (c) 2017 Zefiro\n");
  Serial.flush();

  pixels.begin();
  pixels.setPixelColor(0, pixels.Color(10, 10, 10));
  pixels.setPixelColor(1, pixels.Color(0, 1, 0));
  pixels.show();

  WiFiMulti.addAP(config->wifi_ssid, config->wifi_pass);
  Serial.printf("Wifi: connecting to ", config->wifi_ssid);
  WiFi.mode(WIFI_STA);
//    WiFi.begin("ssid","password");
  pixels.setPixelColor(0, pixels.Color(0, 10, 0));
  pixels.show();
}

#define MAX_TIMER 10
long timer[MAX_TIMER];

void startTimer(byte idx) {
  timer[idx] = millis();
}

bool timerPassed(byte idx, int ms) {
  return millis() - timer[idx] >= ms;
}

void doWifi(String param) {
  // wait for WiFi connection
  if (WiFiMulti.run() == WL_CONNECTED) {
    long startMillis = millis();
    HTTPClient http;

//    String url = "http://raspi.cave.zefiro.de/scenario/" + param;
//    String url = "http://192.168.4.1:80/" + param;
    String url = config->baseurl + param;
    http.begin(url);

    /*
      // or
      http.begin("http://192.168.1.12/test.html");
      http.setAuthorization("user", "password");

      // or
      http.begin("http://192.168.1.12/test.html");
      http.setAuthorization("dXNlcjpwYXN3b3Jk");
    */

    Serial.println("URL: " + url);
    Serial.print("[HTTP] GET ");
    Serial.print(param);
    // start connection and send HTTP header
    int httpCode = http.GET();
    long duration = millis() - startMillis;

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf(" -> HTTP %d (took %d ms)\n", httpCode, duration);

      // file found at server
      if (false && httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf(" -> failed, error: %d=%s (took %d ms)\n", httpCode, http.errorToString(httpCode).c_str(), duration);
      state = STATE_SEND_ERROR;
    }

    http.end();
  }
}

void loop() {
  bouncerButtonA.update();
  bouncerButtonB.update();

  // Button pressed
  if (bouncerButtonA.rose()) {
    Serial.printf("RED BUTTON!!\n");
    digitalWrite(LED_BUILTIN, LOW); // LED ON
    startTimer(1);
    state = STATE_SENDING;
    // TODO change color first, then start blocking doWifi()
    doWifi(config->cmd_RedButton);
  }
  // Button released
  if (bouncerButtonA.fell()) {
    Serial.printf("Button released\n");
    digitalWrite(LED_BUILTIN, HIGH); // LED ON
  }

  // Button pressed
  if (bouncerButtonB.fell()) {
    pixels.setPixelColor(1, pixels.Color(0, 20, 0));
    Serial.printf("Small Button\n");
    doWifi(config->cmd_HiddenButton);
  }
  // Button released
  if (bouncerButtonB.rose()) {
    pixels.setPixelColor(1, pixels.Color(0, 1, 0));
    Serial.printf("Small Button released\n");
  }

  if (timerPassed(2, 30 * 1000)) {
    startTimer(2);
    startTimer(1);
    Serial.println("Sending Ping");
    Serial.print("My IP: ");
    Serial.println(WiFi.localIP());
    doWifi(config->cmd_Ping);
    state = STATE_SENDING;
  }
  
  if ((state == STATE_INIT || state == STATE_DISCONNECTED) && WiFi.status() == WL_CONNECTED) {
    state = STATE_CONNECTED;
    Serial.print("Wifi connected, IP: ");
    Serial.print(WiFi.localIP());
    Serial.print("\n");
  }

  if (state == STATE_CONNECTED && WiFi.status() != WL_CONNECTED) {
    state = STATE_DISCONNECTED;
    Serial.println("Wifi lost");
  }

  switch (state) {
    case STATE_INIT: { // white sawtooth
      byte c = (millis() >> 4) % 63;
      pixels.setPixelColor(0, pixels.Color(c, c, c));
      break; }
    case STATE_CONNECTED: // green
      pixels.setPixelColor(0, pixels.Color(0, 20, 0));
      break;
    case STATE_DISCONNECTED: // 2 Hz blue blinking
      pixels.setPixelColor(0, millis() % 500 < 250 ? pixels.Color(0, 0, 20) : pixels.Color(0, 0, 2));
      break;
    case STATE_SENDING: // greenish, some blue flickering
      pixels.setPixelColor(0, pixels.Color(0, 20, millis() % 32));
      if (timerPassed(1, 500)) {
        state = STATE_CONNECTED;
      }
      break;
    case STATE_SEND_ERROR: // red blinking
      pixels.setPixelColor(0, millis() % 300 < 150 ? pixels.Color(50, 0, 0) : pixels.Color(10, 0, 0));
      break;
  }

  pixels.show();
  delay(1);
}


