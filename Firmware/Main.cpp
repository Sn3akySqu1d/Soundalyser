#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <SPI.h>
#include <SD.h>

#define OLED_RESET     -1
#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Encoder encoder(28, 29);
const int encoderBtnPin = 27;

const int micPin = 26;
const int buzzerPin = 1;
const int chipSelect = 0;

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
int prevScreen = -1;
bool screenOn = true;
bool encoderBtnPressed = false;

float soundHistory[1440];
int historyIndex = 0;

String mood = "";
String emoji = "";

bool displayFound = false;
bool sdFound = false;

void showError(const char* msg) {
  display.clearDisplay();
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
    while (1) {
      // Can't display error if display not found, so just halt here
    }
  }
  
  sdFound = SD.begin(chipSelect);
  if (!sdFound) {
    showError("SD Card Not Found");
    delay(3000);
  }

  encoder.write(0);
  attachInterrupt(digitalPinToInterrupt(encoderBtnPin), encoderPressed, FALLING);
}

void loop() {
  if (!displayFound) return;

  unsigned long now = millis();

  if (digitalRead(encoderBtnPin) == LOW && !encoderBtnPressed) {
    encoderBtnPressed = true;
    screenOn = true;
    lastInteraction = now;
    display.ssd1306_command(SSD1306_DISPLAYON);
  }

  if (encoderBtnPressed && digitalRead(encoderBtnPin) == HIGH) {
    encoderBtnPressed = false;
  }

  if (screenOn && (now - lastInteraction > oledSleepTime)) {
    screenOn = false;
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
  }

  long newPos = encoder.read() / 4;
  if (newPos != currentScreen) {
    currentScreen = newPos % 5;
    if (currentScreen < 0) currentScreen += 5;
    lastInteraction = now;
  }

  micValue = analogRead(micPin);
  soundSum += micValue;
  soundCount++;
  if (micValue > maxSound) maxSound = micValue;
  if (micValue < minSound) minSound = micValue;

  if (micValue > 900 && now - lastBeep > buzzerCooldown) {
    digitalWrite(buzzerPin, HIGH);
    delay(100);
    digitalWrite(buzzerPin, LOW);
    lastBeep = now;
  }

  if (now - lastSampleTime > 1000) {
    avgSound = soundSum / soundCount;
    soundHistory[historyIndex] = avgSound;
    historyIndex = (historyIndex + 1) % 1440;
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
      } else {
        showError("SD Write Fail");
      }
    } else {
      showError("No SD Card");
    }
    lastSDWrite = now;
  }

  if (screenOn && currentScreen != prevScreen) {
    renderScreen(currentScreen);
    prevScreen = currentScreen;
  }
}

void encoderPressed() {
  lastInteraction = millis();
  screenOn = true;
  display.ssd1306_command(SSD1306_DISPLAYON);
}

void renderScreen(int screen) {
  display.clearDisplay();
  switch (screen) {
    case 0:
      drawWaveform();
      break;
    case 1:
      drawBarChart();
      break;
    case 2:
      drawMood();
      break;
    case 3:
      draw24hHistory();
      break;
    case 4:
      drawStats();
      break;
  }
  display.display();
}

void drawWaveform() {
  display.drawLine(0, 32, 127, 32, WHITE);
  for (int x = 0; x < SCREEN_WIDTH; x++) {
    int raw = analogRead(micPin);
    int y = 32 + (raw - 512) / 8;
    display.drawPixel(x, constrain(y, 0, SCREEN_HEIGHT - 1), WHITE);
    delayMicroseconds(200);
  }
}

void drawBarChart() {
  int barHeight = map(micValue, 0, 1023, 0, SCREEN_HEIGHT);
  display.fillRect(60, SCREEN_HEIGHT - barHeight, 8, barHeight, WHITE);
}

void drawMood() {
  float avg = (maxSound + minSound) / 2;
  if (avg < 300) {
    emoji = ":)";
    mood = "Quiet";
  } else if (avg < 700) {
    emoji = ":|";
    mood = "Moderate";
  } else {
    emoji = ":(";
    mood = "Loud";
  }
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(emoji);
  display.setTextSize(1);
  display.setCursor(0, 32);
  display.print("Mood: ");
  display.println(mood);
  display.setCursor(0, 48);
  display.print("<300: Quiet | 300-700: OK | >700: Loud");
}

void draw24hHistory() {
  for (int i = 0; i < 128; i++) {
    int index = (historyIndex + i) % 1440;
    int val = map(soundHistory[index], 0, 1023, 0, SCREEN_HEIGHT);
    display.drawPixel(i, SCREEN_HEIGHT - val, WHITE);
  }
}

void drawStats() {
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("Avg: ");
  display.println(avgSound);
  display.print("Samples: ");
  display.println(soundCount);
  display.print("Max: ");
  display.println(maxSound);
  display.print("Min: ");
  display.println(minSound);
}