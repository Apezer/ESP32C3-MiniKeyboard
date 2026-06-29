#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_SDA 6
#define OLED_SCL 7
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define NUM_KEYS 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const uint8_t KEY_PINS[NUM_KEYS] = {2, 3, 4, 5, 21};
bool keyState[NUM_KEYS] = {false};
bool bleConnected = false;

void drawDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Line 1: Title
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("== MiniKey v0.1 ==");

    // Line 2: Key status
    display.setCursor(0, 8);
    display.print("KEY:");
    for (int i = 0; i < NUM_KEYS; i++) {
        display.print(" ");
        display.print(i + 1);
        display.print(keyState[i] ? "ON" : "--");
    }

    // Line 3: BLE status
    display.setCursor(0, 16);
    display.print("BLE:");
    display.println(bleConnected ? "Connected" : "Disconnected");

    // Line 4: Uptime
    display.setCursor(0, 24);
    display.print("UP: ");
    unsigned long sec = millis() / 1000;
    unsigned long min = sec / 60;
    unsigned long hr = min / 60;
    if (hr > 0) {
        display.print(hr);
        display.print("h");
    }
    display.print(min % 60);
    display.print("m");
    display.print(sec % 60);
    display.print("s");

    display.display();
}

void setup() {
    Serial.begin(115200);

    Wire.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("SSD1306 init failed");
        for (;;)
            ;
    }

    for (int i = 0; i < NUM_KEYS; i++) {
        pinMode(KEY_PINS[i], INPUT_PULLUP);
    }

    drawDisplay();
}

void loop() {
    // Read keys
    for (int i = 0; i < NUM_KEYS; i++) {
        keyState[i] = (digitalRead(KEY_PINS[i]) == LOW);
    }

    drawDisplay();
    delay(100);
}
