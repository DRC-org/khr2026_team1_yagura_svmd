#include <Arduino.h>
#include <Arduino_CAN.h>
#include <FastLED_NeoPixel.h>
#include <Servo.h>

// void readSwitch();
void updateServos();

// サーボ関係
#define SV0 3
#define SV1 6
#define SV2 10
#define SV3 9
// LED 関係
#define LED_PIN 13
#define RGB 8
// 通信関係
#define CAN_RX 5
#define CAN_TX 4
// 入力関係
#define SW0 8
#define SW1 9
#define SW2 1
#define SW3 0

Servo servo0;
Servo servo1;

FastLED_NeoPixel<1, RGB, NEO_GRB> strip;  // RGBLED 制御用

unsigned int CAN_ID = 0x400;  // CANのID（サーボモータドライバは 0x400 番台）

const float SERVO_SPEED = 5.0f;

Servo* servos[2] = {&servo0, &servo1};
float currentAngles[2] = {80.0f, 80.0f};
float targetAngles[2] = {80.0f, 80.0f};
int8_t states[2] = {2, 2};  // OPEN=0,CLOSE=1,STOPPED=2,OPEN_DONE=3,CLOSE_DONE=4
unsigned long lastUpdateMs = 0;

void setup() {
  Serial.begin(115200);

  pinMode(SV0, OUTPUT);
  pinMode(SV1, OUTPUT);
  // pinMode(SV2, OUTPUT);
  // pinMode(SV3, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RGB, OUTPUT);
  pinMode(SW0, INPUT_PULLUP);
  pinMode(SW1, INPUT_PULLUP);
  pinMode(SW2, INPUT_PULLUP);
  pinMode(SW3, INPUT_PULLUP);

  strip.begin();
  strip.setBrightness(100);

  strip.setPixelColor(0, strip.Color(0, 255, 0));
  strip.show();

  servo0.attach(SV0);
  servo1.attach(SV1);

  Serial.println("Setup done.");

  servo0.write(0);
  servo1.write(0);

  Serial.println("Waiting for power...");

  delay(50);

  // TODO: ホーミング

  lastUpdateMs = millis();

  if (!CAN.begin(CanBitRate::BR_1000k)) {
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

  // readSwitch();  // DIP スイッチの読み出して CAN_ID を指定
}

void loop() {
  if (CAN.available()) {
    CanMsg const msg = CAN.read();  // 受信した CAN の読み出し
    unsigned int rxId = msg.id;

    if (rxId == CAN_ID) {
      unsigned char command = msg.data[0];

      if (command == 0x00) {  // ハンド 1 を閉じる
        targetAngles[0] = 50.0f;
        servo0.write(50);
        states[0] = 1;               // CLOSE
      } else if (command == 0x01) {  // ハンド 1 を開く
        targetAngles[0] = 0.0f;
        states[0] = 0;               // OPEN
      } else if (command == 0x02) {  // ハンド 1 を停止
        Serial.println("STOP HAND 1");
        targetAngles[0] = currentAngles[0];
        states[0] = 2;  // STOPPED
      }

      if (command == 0x10) {  // ハンド 2 を閉じる
        targetAngles[1] = 80.0f;
        states[1] = 1;               // CLOSE
      } else if (command == 0x11) {  // ハンド 2 を開く
        targetAngles[1] = 0.0f;
        states[1] = 0;               // OPEN
      } else if (command == 0x12) {  // ハンド 2 を停止
        targetAngles[1] = currentAngles[1];
        states[1] = 2;  // STOPPED
      }
    }

    updateServos();
  }
}

// DIPスイッチを読み取ってCAN_IDを指定する
// void readSwitch() {
//   unsigned int readNumber = !digitalRead(SW0) + 2 * !digitalRead(SW1) +
//                             4 * !digitalRead(SW2) + 8 * !digitalRead(SW3);
//   switch (readNumber) {
//     case 0:
//       strip.setPixelColor(0, strip.Color(255, 100, 0));
//       break;
//     case 1:
//       strip.setPixelColor(0, strip.Color(255, 255, 0));
//       break;
//     case 2:
//       strip.setPixelColor(0, strip.Color(0, 255, 0));
//       break;
//     case 3:
//       strip.setPixelColor(0, strip.Color(0, 0, 255));
//       break;
//   }
//   strip.show();
//   CAN_ID = 0x300 + readNumber;
//   Serial.print("CAN_ID: ");
//   Serial.println(CAN_ID);
// }

void updateServos() {
  for (int i = 0; i < 2; i++) {
    if (states[i] == 2 || states[i] == 3 || states[i] == 4) {
      continue;
    }

    float diff = targetAngles[i] - currentAngles[i];
    if (fabsf(diff) < 0.5f) {
      currentAngles[i] = targetAngles[i];
      if (states[i] == 0) {
        states[i] = 3;  // OPEN_DONE
      } else if (states[i] == 1) {
        states[i] = 4;  // CLOSE_DONE
      }

      CanMsg msg;
      msg.id = 0x000;
      msg.data_length = 8;

      if (i == 0) {
        msg.data[0] = 0x40;
      } else if (i == 1) {
        msg.data[0] = 0x41;
      }
      msg.data[1] = states[i] == 3 ? 0x01 : 0x00;

      CAN.write(msg);
    } else {
      float step = constrain(diff, -SERVO_SPEED, SERVO_SPEED);
      currentAngles[i] += step;
    }
    servos[i]->write((int)roundf(currentAngles[i]));
  }
}
