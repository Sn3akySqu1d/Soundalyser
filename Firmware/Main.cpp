#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#define OLED_RESET -1
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RotaryEncoder encoder(28, 29, RotaryEncoder::LatchMode::TWO03);

const int encoderBtnPin = 27;
const int micPin = 26;
const int buzzerPin = 1;
const int chipSelect = 0;

const uint32_t sampleIntervalMs = 1000;
const int totalScreens = 5;
const int historySlots = 1440;
const int waveformSize = 128;

float micValue = 0;
float maxSound = 0;
float minSound = 1024;
float avgSound = 0;
float soundSum = 0;
int soundCount = 0;

unsigned long lastSampleTime = 0;
unsigned long lastSDWrite = 0;
unsigned long lastInteraction = 0;
unsigned long lastBeep = 0;
unsigned long oledSleepTime = 60000;
unsigned long buzzerCooldown = 5000;

int currentScreen = 0;
int lastEncoderPos = 0;
bool screenOn = true;
bool encoderBtnPressed = false;

float soundHistory[historySlots];
int historyIndex = 0;

int waveform[waveformSize];
int waveformIndex = 0;

String mood = "";
String emoji = "";

bool displayFound = false;
bool sdFound = false;

void showError(const char* msg) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Error:");
  display.setCursor(0, 16);
  display.println(msg);
  display.display();
}

void setup() {
  pinMode(encoderBtnPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  Wire.setClock(400000);
  displayFound = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  if (!displayFound) {
    while (1);
  }
  sdFound = SD.begin(chipSelect);
  if (!sdFound) {
    showError("SD Card Not Found");
    delay(3000);
  }
  encoder.setPosition(0);
  lastEncoderPos = 0;
}

void loop() {
  unsigned long now = millis();

  encoder.tick();
  int pos = encoder.getPosition();
  if (pos != lastEncoderPos) {
    int diff = pos - lastEncoderPos;
    if (abs(diff) == 1) {
      currentScreen += diff;
      if (currentScreen < 0) currentScreen = totalScreens - 1;
      if (currentScreen >= totalScreens) currentScreen = 0;
      lastInteraction = now;
      lastEncoderPos = pos;
    } else {
      encoder.setPosition(lastEncoderPos);
    }
  }

  if (digitalRead(encoderBtnPin) == LOW && !encoderBtnPressed) {
    encoderBtnPressed = true;
    screenOn = true;
    lastInteraction = now;
    display.ssd1306_command(SSD1306_DISPLAYON);
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
  }
  if (encoderBtnPressed && digitalRead(encoderBtnPin) == HIGH) {
    encoderBtnPressed = false;
  }

  if (micValue > 700 && now - lastBeep > buzzerCooldown) {
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    lastBeep = now;
  }

  if (screenOn && (now - lastInteraction > oledSleepTime)) {
    screenOn = false;
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  micValue = analogRead(micPin);
  waveform[waveformIndex] = micValue;
  waveformIndex = (waveformIndex + 1) % waveformSize;

  soundSum += micValue;
  soundCount++;
  if (micValue > maxSound) maxSound = micValue;
  if (micValue < minSound) minSound = micValue;

  if (now - lastSampleTime >= sampleIntervalMs) {
    avgSound = soundSum / soundCount;
    soundHistory[historyIndex] = avgSound;
    historyIndex = (historyIndex + 1) % historySlots;
    soundSum = 0;
    soundCount = 0;
    lastSampleTime = now;
  }

  if (now - lastSDWrite > 60000) {
    if (sdFound) {
      File file = SD.open("/soundlog.csv", FILE_WRITE);
      if (file) {
        file.print(now / 1000);
        file.print(",");
        file.println(micValue);
        file.close();
      }
    }
    lastSDWrite = now;
  }

  if (screenOn) {
    renderScreen(currentScreen);
  }

  delay(10);
}

void renderScreen(int screen) {
  display.clearDisplay();
  switch (screen) {
    case 0: drawWaveform(); break;
    case 1: drawBarChart(); break;
    case 2: drawMood(); break;
    case 3: drawStats(); break;
    case 4: drawSDStatus(); break;
  }
  drawSoundValue();
  drawScreenNumber(screen);
  display.display();
}

void drawScreenNumber(int screen) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(SCREEN_WIDTH - 24, SCREEN_HEIGHT - 8);
  display.print(screen + 1);
  display.print("/");
  display.print(totalScreens);
}

void drawSoundValue() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(SCREEN_WIDTH - 60, 0);
  display.print("Snd:");
  display.print((int)micValue);
}

void drawWaveform() {
  display.drawLine(0, 32, SCREEN_WIDTH - 1, 32, WHITE);
  for (int i = 0; i < waveformSize - 1; i++) {
    int idx1 = (waveformIndex + i) % waveformSize;
    int idx2 = (waveformIndex + i + 1) % waveformSize;
    int x1 = i;
    int x2 = i + 1;
    int y1 = 32 + (waveform[idx1] - 512) / 8;
    int y2 = 32 + (waveform[idx2] - 512) / 8;
    y1 = constrain(y1, 0, SCREEN_HEIGHT - 1);
    y2 = constrain(y2, 0, SCREEN_HEIGHT - 1);
    display.drawLine(x1, y1, x2, y2, WHITE);
  }
  unsigned long remainingSlots = (historySlots - historyIndex) % historySlots;
  unsigned long remainingMs = remainingSlots * sampleIntervalMs;
  unsigned long sec = remainingMs / 1000;
  unsigned long minPart = sec / 60;
  unsigned long secPart = sec % 60;
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  if (minPart > 0) {
    display.print(minPart);
    display.print("m");
    display.print(secPart);
    display.print("s");
  } else {
    display.print(secPart);
    display.print("s");
  }
}

void drawBarChart() {
  int barHeight = map((int)micValue, 0, 1023, 0, SCREEN_HEIGHT);
  display.fillRect(60, SCREEN_HEIGHT - barHeight, 8, barHeight, WHITE);
}

void drawMood() {
  float avg = (maxSound + minSound) / 2;
  if (avg < 300) { emoji = ":)"; mood = "Quiet"; }
  else if (avg < 700) { emoji = ":|"; mood = "Moderate"; }
  else { emoji = ":("; mood = "Loud"; }
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(emoji);
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Mood: ");
  display.println(mood);
  display.setCursor(0, 48);
  display.print("<300 Quiet 300-700 OK >700 Loud");
}

void drawStats() {
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print("Avg: ");
  display.println(avgSound);
  display.print("Max: ");
  display.println(maxSound);
  display.print("Min: ");
  display.println(minSound);
  display.print("Last: ");
  display.println((int)micValue);
}

void drawSDStatus() {
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  if (sdFound) {
    display.println("SD CARD");
    display.println("CONNECTED");
  } else {
    display.println("SD CARD");
    display.println("NOT FOUND");
  }
}
