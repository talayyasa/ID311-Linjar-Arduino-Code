#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Fonts/FreeSansBold18pt7b.h> // Büyük font
#include <Fonts/FreeSansBold9pt7b.h>  // Küçük font

#define TFT_CS   10
#define TFT_RST  9
#define TFT_DC   8
#define BUTTON_1 2 // Birinci buton pini
#define BUTTON_2 3 // İkinci buton pini
#define BUTTON_3 4 // Üçüncü buton pini (Sayaç durdur/başlat)

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
RTC_DS3231 rtc;

bool isPomodoroMenu = false; // Hangi menüde olduğumuzu takip eder
bool isPomodoroRunning = false; // Pomodoro çalışıp çalışmadığını takip eder
bool isPomodoroPaused = false; // Pomodoro duraklatıldı mı
unsigned long timerStart = 0;
int pomodoroStep = 1;          // 1: Çalışma, 2: Mola
int remainingTime = 25 * 60;   // Başlangıç süresi (25 dakika)
int pomodoroSessionCount = 0;  // Toplam pomodoro seansı sayacı

unsigned long lastUpdate = 0;  // Son güncelleme zamanı
const long updateInterval = 1000; // 1 saniye

void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_WHITE);
  pinMode(BUTTON_1, INPUT_PULLUP); // Birinci buton için pull-up direnci
  pinMode(BUTTON_2, INPUT_PULLUP); // İkinci buton için pull-up direnci
  pinMode(BUTTON_3, INPUT_PULLUP); // Üçüncü buton için pull-up direnci

  if (!rtc.begin()) {
    Serial.println("RTC modülü bulunamadı!");
    while (1);
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  drawMainMenu(); // Başlangıç ekranı
}

void loop() {
  // Birinci butona basıldığında ana menü ile pomodoro menüsü arasında geçiş yapılır
  if (digitalRead(BUTTON_1) == LOW) { 
    delay(300); // Titreşim önleme
    if (isPomodoroRunning) { // Eğer sayaç çalışıyorsa, sıfırlanıp ana ekrana dönülür
      resetPomodoro();
    } else {
      isPomodoroMenu = !isPomodoroMenu; // Menü değiştir
      if (isPomodoroMenu) {
        drawPomodoroMenu();
      } else {
        drawMainMenu();
      }
    }
  }

  // İkinci butona basıldığında Pomodoro sayacı başlar
  if (digitalRead(BUTTON_2) == LOW && !isPomodoroRunning) { 
    delay(300); // Titreşim önleme
    isPomodoroRunning = true; // Pomodoro başlasın
    pomodoroSessionCount = 0; // Seans sayacı sıfırlanır
    remainingTime = 25 * 60; // Başlangıçta 25 dakika çalışma süresi
    timerStart = millis(); // Zaman başlatılır
    drawPomodoroMenu(); // Pomodoro ekranını çiz
  }

  // Üçüncü butona basıldığında Pomodoro sayacı duraklatılır ve başlatılır
  if (digitalRead(BUTTON_3) == LOW) {
    delay(300); // Titreşim önleme
    if (isPomodoroRunning) {
      if (isPomodoroPaused) {
        // Sayaç duraklatıldıysa, devam ettir
        timerStart = millis() - (25 * 60 - remainingTime) * 1000;
        isPomodoroPaused = false;
      } else {
        // Sayaç çalışıyorsa, duraklat
        isPomodoroPaused = true;
      }
    }
  }

  if (isPomodoroRunning && !isPomodoroPaused) {
    updatePomodoroTimer();
  }

  // Ana menüde saatin güncellenmesi (60 saniye arayla)
  if (!isPomodoroMenu) {
    if (millis() - lastUpdate >= updateInterval) {
      lastUpdate = millis();
      drawMainMenu();
    }
  }
}

void drawMainMenu() {
  DateTime now = rtc.now();

  String tarih = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());
  String saat = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

  tft.fillScreen(ILI9341_WHITE); // Arka planı temizle
  tft.drawRect(5, 5, tft.width() - 10, tft.height() - 10, ILI9341_BLACK); // Siyah çerçeve

  // Üstte "Talay Yasa - Kutay Karabey" yazısı
  tft.setFont(&FreeSansBold9pt7b); // Küçük font
  tft.setTextColor(ILI9341_BLACK);
  String isim = "Talay Yasa - Kutay Karabey";
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(isim, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, 25);
  tft.print(isim);

  // Tarih ve saat
  tft.setFont(&FreeSansBold18pt7b); // Büyük font
  tft.setTextColor(ILI9341_BLACK);

  tft.getTextBounds(tarih, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, (tft.height() / 2) - 10);
  tft.print(tarih);

  tft.getTextBounds(saat, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, (tft.height() / 2) + 60);
  tft.print(saat);
}

void drawPomodoroMenu() {
  tft.fillScreen(ILI9341_WHITE); // Arka planı temizle
  tft.drawRect(5, 5, tft.width() - 10, tft.height() - 10, ILI9341_BLACK); // Siyah çerçeve

  // Pomodoro menüsündeki "Ders Çalışma Vakti" yazısının fontunu küçültüyoruz
  tft.setFont(&FreeSansBold9pt7b); // Küçük font
  tft.setTextColor(ILI9341_BLACK);

  String statusText = (pomodoroStep == 1) ? "Ders Calisma Vakti" : "Mola Vakti";

  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(statusText, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, 50); // Yazıyı ortalıyoruz
  tft.print(statusText);

  // Sayaç göstergesi
  int minutes = remainingTime / 60;
  int seconds = remainingTime % 60;
  String timerText = String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);

  tft.setFont(&FreeSansBold18pt7b); // Sayaç kısmı büyük fontla
  tft.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, (tft.height() / 2) + 40); // Sayaç yazısını ortalıyoruz
  tft.print(timerText);
}

void updatePomodoroTimer() {
  if (millis() - timerStart >= 1000) {
    timerStart = millis();
    remainingTime--;

    if (remainingTime <= 0) {
      if (pomodoroStep == 1) { // Çalışma süresi bitti
        pomodoroStep = 2; // Mola süresine geç
        remainingTime = 5 * 60; // 5 dakika
      } else { // Mola süresi bitti
        pomodoroStep = 1; // Çalışma süresine geç
        pomodoroSessionCount++;

        if (pomodoroSessionCount >= 4) { // 4 pomodoro seansı bittiğinde
          isPomodoroRunning = false; // Pomodoro durur
          drawMainMenu(); // Ana ekrana dön
          return; // Seansı bitir
        }

        remainingTime = 25 * 60; // 25 dakika
      }
    }

    drawPomodoroMenu(); // Sayaç ekranını güncelle
  }
}

void resetPomodoro() {
  isPomodoroRunning = false;
  isPomodoroPaused = false;
  pomodoroStep = 1;
  remainingTime = 25 * 60;
  drawMainMenu(); // Ana ekrana dön
}
