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

const uint8_t fingerprint[20] = {0x5a, 0x12, 0xca, 0xb5, 0x35, 0x69, 0x04, 0x81, 0xe6, 0x1f, 0x8a, 0x3d, 0xba, 0xf1, 0x87, 0x1a, 0x24, 0xa5, 0x40, 0x64};
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

int cycle_length = 10 * 1000;
int cycle_type = CycleType::REGULAR;

int red_length = cycle_length * 0.5;
int yellow_length = cycle_length * 0.05;
int green_length = cycle_length * 0.3;

int green_blink_length = cycle_length * 0.033;

int cycle_start = millis();
unsigned long offsetTime;
unsigned long startMillis;
CycleState state;

void calculateLightLengths() {
  red_length = cycle_length * 0.5;
  yellow_length = cycle_length * 0.05;
  green_length = cycle_length * 0.3;
  green_blink_length = cycle_length * 0.033;
}

// Siit ka url ara muuta
void getStartTime() {
  const char* traffic_lights_url = "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app/traffic_lights/states/0.json";
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);
    HTTPClient https;

    if (https.begin(*client, traffic_lights_url)) {
      int httpCode = https.GET();
      Serial.print("Status code: ");
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        Serial.println(payload);
        if (payload == "1") {
          state = START;
        }
      }
    }
  } 
}

void sendStartTime() {
  // const char* traffic_lights_url = "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app/traffic_lights/arduinoStarts/1.json";
  const char* send_start_url = "https://nice-cyan-goose-hat.cyclic.app/arduino_start/0";
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);
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

void setup() {
  pinMode(P_SWITCH, INPUT);

  pinMode(LED_RC, OUTPUT);
  pinMode(LED_YC, OUTPUT);
  pinMode(LED_GC, OUTPUT);

  pinMode(LED_RP, OUTPUT);
  pinMode(LED_GP, OUTPUT);

  Serial.begin(115200);
  state = CycleState::START;

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP("TLU", "");

  delay(2000);
  sendStartTime();
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


void regularCycle(int current_millis) {


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
  } else if (current_millis > (d_green - d_green_blinks[0]) && state == CycleState::GREEN) {
    if (current_millis > d_green_blinks[0]) {
      digitalWrite(LED_GC, LOW);
      digitalWrite(LED_YC, HIGH);
      //pedestrianLight(pedestrian_input, state);
      //state = START;
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
      state = START;
  }
}

const uint16_t traffic_light_id = 1;

const char* cycle_length_url = "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app/traffic_lights/cycleLength.json";
const char* yellow_toggle_url = "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app/traffic_light/yellow_mode/.json";
const char* start_time_url = "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app/traffic_lights/arduinoNextOffsets/1.json";

void loop() {
  time_t time = now();
  startMillis = millis();
  Serial.print("Cycle_start: ");
  Serial.println(startMillis);
  if (WiFiMulti.run() == WL_CONNECTED) {
    std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
    client->setFingerprint(fingerprint);
    HTTPClient https;

    Serial.println("Getting start time for Arduino....");
    if (https.begin(*client, start_time_url)) {
      int httpCode = https.GET();
      Serial.print("Status code: ");
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        Serial.print("Payload: ");
        String payload = https.getString();
        Serial.println(payload);
        offsetTime = payload.toInt();
      }
    }

    Serial.print("StartMillis: ");
    Serial.print(startMillis);
    Serial.print("offsetTime: ");
    Serial.print(offsetTime);

    if (startMillis > offsetTime) {
      Serial.println("Starting GET req for yellow_toggle_url");
      if (https.begin(*client, yellow_toggle_url)) {
        int httpCode = https.GET();
        Serial.print("Status code: ");
        Serial.println(httpCode);

        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          Serial.print("Payload: ");
          String payload = https.getString();
          Serial.println(payload);
          if (payload == "1" && cycle_type == CycleType::REGULAR) {
            resetLights();
            cycle_type = CycleType::NIGHT;
          }
          if (payload == "0" && cycle_type == CycleType::NIGHT) {
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
          Serial.print("Payload: ");
          String payload = https.getString();
          Serial.println(payload);

          int payload_i = payload.toInt();
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
          regularCycle(startMillis);
        break;
        case CycleType::NIGHT:
          yellowCycle();
        break;
      }
    }
  }
}

// nice-cyan-goose-hat.cyclic.app
// Muidu tootab, aga jalakaija roheline ei lahe polema