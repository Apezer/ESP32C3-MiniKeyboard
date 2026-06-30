#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BleKeyboard.h>

// OLED 引脚与地址
#define OLED_SDA 6
#define OLED_SCL 7
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// 按键数量与去抖时间
#define NUM_KEYS 5
#define DEBOUNCE_MS 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 按键引脚定义
const uint8_t KEY_PINS[NUM_KEYS] = {2, 3, 4, 5, 8};

// 按键对应的字符
const char KEY_CHARS[NUM_KEYS] = {'z', 'x', 'c', 'v', 'b'};

// 按键状态与去抖
bool keyState[NUM_KEYS] = {false};
bool lastReportedState[NUM_KEYS] = {false};
unsigned long lastChangeTime[NUM_KEYS] = {0};

// 蓝牙图标字模（外部定义）
extern const uint8_t BT_ICON[];

// BLE 键盘实例
BleKeyboard bleKeyboard("MiniKey", "Apezer", 100);

// 开屏画面
void showSplash() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("MiniKey");

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("ESP32-C3 BLE v0.1");

    display.display();
}

// 主界面
void drawDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // 第一行：标题
    display.setCursor(0, 0);
    display.print("== MiniKey v0.1 ==");

    // 第二行：按键映射
    display.setCursor(0, 8);
    display.print("MAP:");
    for (int i = 0; i < NUM_KEYS; i++) {
        display.print("  ");
        display.print(KEY_CHARS[i]);
    }

    // 第三行：蓝牙图标 + 连接状态
    display.drawBitmap(0, 16, BT_ICON, 8, 8, SSD1306_WHITE);
    display.setCursor(10, 16);
    if (bleKeyboard.isConnected()) {
        display.print("Connected");
    } else {
        display.print("Waiting...");
    }

    display.display();
}

void setup() {
    Serial.begin(115200);

    // 初始化 BLE 键盘
    bleKeyboard.begin();

    // 初始化 I2C 和 OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("SSD1306 init failed");
        for (;;)
            ;
    }

    // 初始化按键引脚（内部上拉）
    for (int i = 0; i < NUM_KEYS; i++) {
        pinMode(KEY_PINS[i], INPUT_PULLUP);
    }

    showSplash();
    delay(1000);
    drawDisplay();
}

void loop() {
    // BLE 未连接时只刷新显示
    if (!bleKeyboard.isConnected()) {
        static unsigned long lastDraw = 0;
        if (millis() - lastDraw > 500) {
            lastDraw = millis();
            drawDisplay();
        }
        return;
    }

    unsigned long now = millis();
    bool changed = false;

    // 扫描所有按键，去抖处理
    for (int i = 0; i < NUM_KEYS; i++) {
        bool raw = (digitalRead(KEY_PINS[i]) == LOW);

        if (raw != keyState[i]) {
            if ((now - lastChangeTime[i]) >= DEBOUNCE_MS) {
                keyState[i] = raw;
                lastChangeTime[i] = now;
                changed = true;
            }
        }
    }

    // 状态变化时发送 BLE HID 报告
    if (changed) {
        for (int i = 0; i < NUM_KEYS; i++) {
            if (keyState[i] && !lastReportedState[i]) {
                bleKeyboard.press(KEY_CHARS[i]);
            } else if (!keyState[i] && lastReportedState[i]) {
                bleKeyboard.release(KEY_CHARS[i]);
            }
            lastReportedState[i] = keyState[i];
        }
        drawDisplay();
    }
}
