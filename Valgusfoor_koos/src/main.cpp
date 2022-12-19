#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <time.h>

#define P_SWITCH D6

#define LED_RC D0
#define LED_YC D1
#define LED_GC D2

#define LED_RP D5
#define LED_GP D3

#define WIFI_SSID "#Telia-89E1CC"
#define WIFI_PASS "nzuZ9Eyx7wfzhmeY"

// const uint8_t fingerprintFb[20] = {0x5a, 0x12, 0xca, 0xb5, 0x35, 0x69, 0x04, 0x81, 0xe6, 0x1f, 0x8a, 0x3d, 0xba, 0xf1, 0x87, 0x1a, 0x24, 0xa5, 0x40, 0x64};
const uint8_t fingerprintDo[20] = {0xAA, 0x97, 0x76, 0x9D, 0xD6, 0xB8, 0x8B, 0xAA, 0xA0, 0x3C, 0xB5, 0x7B, 0xDE, 0x76, 0x62, 0x84, 0xC0, 0x19, 0xEC, 0xA0};
// Kui Digitalocean fingerprinti kasutada, siis saab katte, aga siis annab firebase 403

ESP8266WiFiMulti WiFiMulti;

enum CycleType {
  NIGHT = 0,
  REGULAR = 1
};

enum CycleState {
  START,
  RED,
  YELLOW,
  GREEN,
  END
};

int trafficLightId = 0;

int cycle_length = 20 * 1000;
int cycle_type = CycleType::REGULAR;

int red_length = cycle_length * 0.5;
int yellow_length = cycle_length * 0.05;
int green_length = cycle_length * 0.25;
int green_blink_length = cycle_length * 0.033;

int cycle_start = millis();
unsigned long offsetTime;
unsigned long startMillis;
unsigned long startMillisBuf;
unsigned long timeNow;
CycleState state;

// Set your Static IP address
IPAddress local_IP(192, 168, 1, 33);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   //optional
IPAddress secondaryDNS(8, 8, 4, 4); //optional

const uint16_t traffic_light_id = 1;
bool firstRequest = true;

const char* cycle_length_url = "https://orca-app-tlr83.ondigitalocean.app/get_cycle_length";
const char* yellow_toggle_url = "https://orca-app-tlr83.ondigitalocean.app/get_yellow_state/0";
const char* start_time_url = "https://orca-app-tlr83.ondigitalocean.app/get_arduino_start/0";

void calculateLightLengths() {
  red_length = cycle_length * 0.5;
  yellow_length = cycle_length * 0.05;
  green_length = cycle_length * 0.25;
  green_blink_length = cycle_length * 0.033;
}

void sendStartTime() {
  const char* send_start_url = "https://orca-app-tlr83.ondigitalocean.app/arduino_start/0";
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprintDo);
    HTTPClient https;

    if (https.begin(*client, send_start_url)) {
      int httpCode = https.GET();
      Serial.println("Sending start time....");
      Serial.print("Status code: ");
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);
      } else {
        Serial.println("ERROR: " + httpCode);
      }
    }
  }  
}

void getStartTime(BearSSL::WiFiClientSecure client) {
  const char* start_time_url = "https://orca-app-tlr83.ondigitalocean.app/get_arduino_start/0";
  HTTPClient https;
  Serial.println("Getting start time for Arduino in function....");
  if (https.begin(client, start_time_url)) {
    int httpCode = https.GET();
    Serial.print("Status code: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      StaticJsonDocument<200> doc;
      String payload = https.getString();
      DeserializationError error = deserializeJson(doc, payload);
      String startTime = doc["start"];
      Serial.println("firstReq: " + firstRequest);
      if (firstRequest) {
        startMillis = startTime.toInt() - 1365;
      } else {
        startMillisBuf = startTime.toInt() - 1365;
      }
    }
  }
}

void setup() {
  pinMode(P_SWITCH, INPUT);

  pinMode(LED_RC, OUTPUT);
  pinMode(LED_YC, OUTPUT);
  pinMode(LED_GC, OUTPUT);

  pinMode(LED_RP, OUTPUT);
  pinMode(LED_GP, OUTPUT);

  Serial.begin(115200);
  state = CycleState::START;

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.println(".");
  }

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setFingerprint(fingerprintDo);

  sendStartTime();
  getStartTime(*client);
  firstRequest = false;
}

void resetLights() {
  digitalWrite(LED_RC, LOW);
  digitalWrite(LED_YC, LOW);
  digitalWrite(LED_GC, LOW);

  digitalWrite(LED_RP, LOW);
  digitalWrite(LED_GP, LOW);
}

void yellowCycle() {
  digitalWrite(LED_YC, HIGH);
  delay(1000);
  digitalWrite(LED_YC, LOW);
  delay(1000);
}

int d_green;
int d_green_blinks[3] = {0};
int d_yellow;
int d_yellow_ag;
int d_red;

int pedestrian_input = 0;

void pedestrianLight(int pedestrian_input, CycleState cycle_state) {
  if (pedestrian_input) {
    switch (cycle_state) {
      case START:
        digitalWrite(LED_GP, HIGH);
      break;
      case YELLOW:
        digitalWrite(LED_GP, LOW);
        digitalWrite(LED_RP, HIGH);
      break;
      case END:
        digitalWrite(LED_RP, LOW);
      break;
    }
  } else {
    digitalWrite(LED_RP, HIGH);
    digitalWrite(LED_GP, LOW);
  }
}


void regularCycle(int current_millis, BearSSL::WiFiClientSecure client) {
  if (state == START) {
    cycle_start = millis();

    d_red = cycle_start + red_length;
    d_yellow = d_red + yellow_length;
    d_green = d_yellow + green_length;

    d_green_blinks[2] = d_green + green_blink_length;
    d_green_blinks[1] = d_green_blinks[2] + (green_blink_length * 2);
    d_green_blinks[0] = d_green_blinks[1] + (green_blink_length * 3);
    d_yellow_ag = yellow_length + d_green_blinks[0];

    pedestrianLight(pedestrian_input, state);
    pedestrian_input = 0;

    digitalWrite(LED_YC, LOW);
    digitalWrite(LED_RC, HIGH);
    state = RED;
  }
  if (current_millis > d_red && state == CycleState::RED) {
    digitalWrite(LED_RC, LOW);
    digitalWrite(LED_YC, HIGH);
    pedestrianLight(pedestrian_input, state);
    state = CycleState::YELLOW;
  } else if (current_millis > d_yellow && state == CycleState::YELLOW) {
    digitalWrite(LED_YC, LOW);
    digitalWrite(LED_GC, HIGH);

    pedestrianLight(pedestrian_input, state);

    state = GREEN;
    getStartTime(client);
  } else if (current_millis > (d_green - d_green_blinks[0]) && state == CycleState::GREEN) {
    if (current_millis > d_green_blinks[0]) {
      digitalWrite(LED_GC, LOW);
      digitalWrite(LED_YC, HIGH);
      pedestrianLight(pedestrian_input, state);
      state = START;
      state = END;
    } else if (current_millis > (d_green_blinks[0] - (green_blink_length / 2))) {
      digitalWrite(LED_GC, HIGH);
    } else if (current_millis > d_green_blinks[1]) {
      digitalWrite(LED_GC, LOW);
    } else if (current_millis > (d_green_blinks[1] - (green_blink_length / 2))) {
      digitalWrite(LED_GC, HIGH);
    } else if (current_millis > d_green_blinks[2]) {
      digitalWrite(LED_GC, LOW);
    } else if (current_millis > (d_green_blinks[2] - (green_blink_length / 2))) {
      digitalWrite(LED_GC, HIGH);
    }
  } else if (current_millis > d_yellow_ag && state == CycleState::END) {
      digitalWrite(LED_YC, LOW);
      pedestrianLight(pedestrian_input, state);
      startMillis = startMillisBuf - 1365;
      state = START;
  }
}


void loop() {
  unsigned long tempOffsetTime = 0;
  timeNow = millis();
  Serial.print("start: ");
  Serial.print(timeNow);
  Serial.print("offset: ");
  Serial.print(startMillis);
  Serial.print("dyellowg: ");
  Serial.print(d_yellow_ag);
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprintDo);
    HTTPClient https;
    // getStartTime(*client);

    if (timeNow >= startMillis) {
      if (https.begin(*client, yellow_toggle_url)) {
        int httpCode = https.GET();
        Serial.print("Status code: ");
        Serial.println(httpCode);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          StaticJsonDocument<200> doc;
          Serial.print("Payload: ");
          String payload = https.getString();
          DeserializationError error = deserializeJson(doc, payload);
          String state = doc["state"];
          if (state == "1" && cycle_type == CycleType::REGULAR) {
            resetLights();
            cycle_type = CycleType::NIGHT;
          }
          if (state == "0" && cycle_type == CycleType::NIGHT) {
            state = START;
            cycle_type = CycleType::REGULAR;
          }
        }
      }

      Serial.println("Starting GET req for traffic_lights_url");
      if (https.begin(*client, cycle_length_url)) {
        int httpCode = https.GET();
        Serial.print("Status code: ");
        Serial.println(httpCode);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          StaticJsonDocument<200> doc;
          Serial.print("Payload: ");
          String payload = https.getString();
          DeserializationError error = deserializeJson(doc, payload);
          Serial.println(payload);
          String cycLength = doc["cycleLength"];
          int payload_i = cycLength.toInt();
          if (cycle_length != payload_i) {
            cycle_length = payload_i;
            calculateLightLengths();
          }
        }
      }
      int pedestrian_btn = digitalRead(P_SWITCH);
      if (pedestrian_btn) {
        pedestrian_input = pedestrian_btn;
      }

      switch (cycle_type) {
        case CycleType::REGULAR:
          regularCycle(timeNow, *client);
        break;
        case CycleType::NIGHT:
          yellowCycle();
        break;
      }
    }
  } else {
    Serial.println("Wifi not connected");
  }
}
