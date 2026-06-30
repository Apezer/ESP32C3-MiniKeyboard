#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED 引脚与地址
#define OLED_SDA 6
#define OLED_SCL 7
#define OLED_ADDR 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

// 按键数量
#define NUM_KEYS 5

// 波形显示参数
#define SAMPLE_COUNT 128
#define WAVE_TOP 10
#define WAVE_BOTTOM 30

// 去抖时间（毫秒）
#define DEBOUNCE_MS 10

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 按键引脚定义
const uint8_t KEY_PINS[NUM_KEYS] = {2, 3, 4, 5, 21};
bool keyState[NUM_KEYS] = {false};
bool bleConnected = false;

// 按键计数器
unsigned long rawPressCount = 0;     // 未去抖，每次下降沿算一次按下
unsigned long stablePressCount = 0;  // 去抖后，实际按下次数

// 波形显示相关变量
uint8_t waveBuf[SAMPLE_COUNT];       // 波形采样缓冲区
int wavePos = 0;                     // 当前写入位置
uint8_t prevRaw = HIGH;              // 上一次原始采样值，用于边沿检测
uint8_t debouncedState = HIGH;       // 去抖后的稳定状态
unsigned long stateChangeTime = 0;   // 原始电平最后一次跳变的时间戳
int edgeCount = 0;                   // 当前按下期间的跳变次数
int lastEdgeCount = 0;               // 上一次按下的跳变次数
bool pressActive = false;            // 是否处于按下状态

// 开屏画面
void showSplash() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("MiniKey");

    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println("ESP32-C3  v0.1");

    display.display();
}

// 主界面
void drawDisplay() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // 第一行：标题 + 上次抖动跳变次数
    display.setCursor(0, 0);
    display.print("== MiniKey v0.1 ==");
    if (lastEdgeCount > 0) {
        display.setCursor(104, 0);
        display.print("B:");
        display.print(lastEdgeCount);
    }

    // 第二行：未去抖次数 vs 去抖后次数
    display.setCursor(0, 8);
    display.print("Raw:");
    display.print(rawPressCount);
    display.setCursor(64, 8);
    display.print("OK:");
    display.print(stablePressCount);

    // 第三行：各按键状态
    display.setCursor(0, 16);
    display.print("KEY:");
    for (int i = 0; i < NUM_KEYS; i++) {
        display.print(" ");
        display.print(i + 1);
        display.print(keyState[i] ? "ON" : "--");
    }

    // 第四行：蓝牙状态 + 运行时间
    display.setCursor(0, 24);
    display.print(bleConnected ? "BLE:ON " : "BLE:OFF");

    unsigned long sec = millis() / 1000;
    unsigned long min = sec / 60;
    display.setCursor(64, 24);
    display.print("UP:");
    display.print(min);
    display.print("m");
    display.print(sec % 60);
    display.print("s");

    display.display();
}

// 抖动波形显示界面
void drawBounceViewer() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    // 顶部：当前电平状态 + 跳变次数
    display.setCursor(0, 0);
    display.print("KEY1:");
    display.print(prevRaw ? "HIGH" : "LOW ");
    display.setCursor(60, 0);
    display.print("Edges:");
    display.print(edgeCount);

    // 绘制波形，HIGH 显示在下方，LOW 显示在上方（内部上拉，按下为 LOW）
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        int idx = (wavePos + i) % SAMPLE_COUNT;
        int y = waveBuf[idx] ? WAVE_BOTTOM : WAVE_TOP;
        display.drawPixel(i, y, SSD1306_WHITE);

        // 相邻采样点之间画竖线连接，使跳变更清晰
        if (i > 0) {
            int prevIdx = (wavePos + i - 1) % SAMPLE_COUNT;
            int prevY = waveBuf[prevIdx] ? WAVE_BOTTOM : WAVE_TOP;
            if (prevY != y) {
                int yMin = min(prevY, y);
                int yMax = max(prevY, y);
                for (int j = yMin + 1; j < yMax; j++) {
                    display.drawPixel(i, j, SSD1306_WHITE);
                }
            }
        }
    }

    // 画参考线
    display.drawFastHLine(0, WAVE_TOP, SCREEN_WIDTH, SSD1306_WHITE);
    display.drawFastHLine(0, WAVE_BOTTOM, SCREEN_WIDTH, SSD1306_WHITE);

    // 参考线标签
    display.setCursor(SAMPLE_COUNT + 2, WAVE_TOP - 3);
    display.print("L");
    display.setCursor(SAMPLE_COUNT + 2, WAVE_BOTTOM - 3);
    display.print("H");

    display.display();
}

void setup() {
    Serial.begin(115200);

    // 初始化 I2C
    Wire.begin(OLED_SDA, OLED_SCL);

    // 初始化 OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("SSD1306 init failed");
        for (;;)
            ;
    }

    // 初始化按键引脚（内部上拉）
    for (int i = 0; i < NUM_KEYS; i++) {
        pinMode(KEY_PINS[i], INPUT_PULLUP);
    }

    // 初始化波形缓冲区为 HIGH
    for (int i = 0; i < SAMPLE_COUNT; i++) {
        waveBuf[i] = HIGH;
    }
    prevRaw = digitalRead(KEY_PINS[0]);
    debouncedState = prevRaw;

    showSplash();
    delay(1000);
    drawDisplay();
}

void loop() {
    unsigned long now = millis();

    // 采样 KEY1 原始电平
    uint8_t raw = digitalRead(KEY_PINS[0]);

    // 每检测到一个下降沿（HIGH->LOW），未去抖计数 +1
    if (prevRaw == HIGH && raw == LOW) {
        rawPressCount++;
    }

    // 检测所有电平跳变，记录时间戳
    if (raw != prevRaw) {
        edgeCount++;
        stateChangeTime = now;
    }
    prevRaw = raw;

    // 存入波形缓冲区
    waveBuf[wavePos] = raw;
    wavePos = (wavePos + 1) % SAMPLE_COUNT;

    // 去抖逻辑：原始电平与去抖状态不同时，等待稳定超过 DEBOUNCE_MS 才更新
    if (raw != debouncedState) {
        if ((now - stateChangeTime) >= DEBOUNCE_MS) {
            debouncedState = raw;
            // 去抖后的下降沿才算一次真实按下
            if (debouncedState == LOW) {
                stablePressCount++;
            }
        }
    }

    // 读取所有按键状态用于显示
    for (int i = 0; i < NUM_KEYS; i++) {
        keyState[i] = (digitalRead(KEY_PINS[i]) == LOW);
    }

    // KEY1 按下时显示波形，松开时显示主界面
    if (raw == LOW) {
        if (!pressActive) {
            pressActive = true;
            edgeCount = 1;  // 首条下降沿已计入
        }
        drawBounceViewer();
    } else {
        if (pressActive) {
            pressActive = false;
            lastEdgeCount = edgeCount;  // 保存本次跳变次数
        }
        drawDisplay();
    }
}
