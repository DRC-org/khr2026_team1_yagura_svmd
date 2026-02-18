#include <Arduino.h>
#include <FastLED_NeoPixel.h>
#include <SPI.h>
#include <Servo.h>
#include <mcp_can.h>

#define PGOOD 2
#define INT 3
#define SV0 4
#define SV1 5
#define SV2 6
#define SV3 7
#define LED_PIN 8
#define RGB 9
#define CS 10
#define MOSI 11
#define MISO 12
#define SCLK 13

Servo servo0;
Servo servo1;
Servo servo2;
Servo servo3;
Servo servo4;

FastLED_NeoPixel<1, RGB, NEO_GRB> strip;  // RGBLED 制御用

MCP_CAN CAN(CS);

const float SERVO_SPEED = 10.0f;  // 度/秒

Servo* servos[2] = {&servo0, &servo1};
float currentAngles[2] = {80.0f, 80.0f};
float targetAngles[2] = {80.0f, 80.0f};
unsigned long lastUpdateMs = 0;

void updateServos() {
  unsigned long now = millis();
  float dt = (now - lastUpdateMs) / 1000.0f;
  lastUpdateMs = now;

  for (int i = 0; i < 2; i++) {
    float diff = targetAngles[i] - currentAngles[i];
    if (fabsf(diff) < 0.5f) {
      currentAngles[i] = targetAngles[i];
    } else {
      float step = constrain(diff, -SERVO_SPEED * dt, SERVO_SPEED * dt);
      currentAngles[i] += step;
    }
    servos[i]->write((int)roundf(currentAngles[i]));
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(PGOOD, INPUT);
  pinMode(INT, INPUT);
  pinMode(SV0, OUTPUT);
  pinMode(SV1, OUTPUT);
  // pinMode(SV2, OUTPUT);
  // pinMode(SV3, OUTPUT);
  // pinMode(LED_PIN, OUTPUT);
  pinMode(RGB, OUTPUT);
  pinMode(CS, OUTPUT);
  pinMode(MOSI, OUTPUT);
  pinMode(MISO, INPUT);
  pinMode(SCLK, OUTPUT);

  strip.begin();
  strip.setBrightness(100);

  strip.setPixelColor(0, strip.Color(0, 255, 0));
  strip.show();

  servo0.attach(SV0);
  servo1.attach(SV1);
  // servo2.attach(SV2);
  // servo3.attach(SV3);

  servo0.write(80);
  servo1.write(80);

  delay(50);

  // TODO: ホーミング

  lastUpdateMs = millis();

  if (CAN.begin(MCP_ANY, CAN_1000KBPS, MCP_16MHZ) != CAN_OK) {
    Serial.println("CAN.begin(...) failed.");
    while (1) {
      strip.setPixelColor(0, strip.Color(255, 0, 0));
      strip.show();
      delay(100);
      strip.setPixelColor(0, strip.Color(0, 0, 0));
      strip.show();
      delay(100);
    }
  }

  CAN.setMode(MCP_NORMAL);
}

void loop() {
  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    unsigned long rxId;
    unsigned char len;
    unsigned char buf[8];
    CAN.readMsgBuf(&rxId, &len, buf);  // 受信したCANの読み出し

    if (rxId == 0x400) {
      unsigned char command = buf[0];

      if (command == 0x00) {  // ハンド 1 を閉じる
        targetAngles[0] = 170.0f;
      } else if (command == 0x01) {  // ハンド 1 を開く
        targetAngles[0] = 80.0f;
      } else if (command == 0x02) {  // ハンド 1 を停止
        targetAngles[0] = currentAngles[0];
      }

      if (command == 0x10) {  // ハンド 2 を閉じる
        targetAngles[1] = 170.0f;
      } else if (command == 0x11) {  // ハンド 2 を開く
        targetAngles[1] = 80.0f;
      } else if (command == 0x12) {  // ハンド 2 を停止
        targetAngles[1] = currentAngles[1];
      }
    }
  }

  updateServos();
}
