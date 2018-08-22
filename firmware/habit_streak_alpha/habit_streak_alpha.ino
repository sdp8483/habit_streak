#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <EEPROM.h>

#define BUTTON 3

#define CURRENT_STREAK_ADDRS  0
#define MAX_STREAK_ADDRS      sizeof(int)

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define OLED_REFRESH 5000 //ms

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete

int currentStreak;
int maxStreak;

void setup() {
  Serial.begin(9600);

  inputString.reserve(200);     // reserve 200 bytes for the inputString:

  pinMode(BUTTON, INPUT_PULLUP);

  // read in EEPROM data
  EEPROM.get(CURRENT_STREAK_ADDRS, currentStreak);
  EEPROM.get(MAX_STREAK_ADDRS, maxStreak);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();  // clear preloaded Adafruit splashscreen

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
} // end setup

void loop() {
  if (stringComplete) {
    Serial.println();
    Serial.println(inputString);
    inputString.toLowerCase();
    if (inputString.startsWith("?")) {
      Serial.println("");
      Serial.println("T HH:MM:SS -> To set time");
      Serial.println("now        -> To get current datetime");
      Serial.println("rstCurrent -> To reset current streak");
      Serial.println("rstMax     -> To reset max streak");
    } else if (inputString.startsWith("now")) {
      printTime();
    } else if (inputString.startsWith("t")) {
      setTime();
    } else if (inputString.startsWith("rstcurrent")) {
      currentStreak = -1;
      EEPROM.put(CURRENT_STREAK_ADDRS, currentStreak);
    } else if (inputString.startsWith("rstmax")) {
      maxStreak = 0;
      EEPROM.put(MAX_STREAK_ADDRS, maxStreak);
    }
    inputString = "";
    stringComplete = false;
  } // end stringComplete

  pollButton();
  updateOLED();
  updateStreak();

} // end loop

void updateStreak() {
  static unsigned long lastUpdate = 0;
  unsigned long nowTime = millis();

  if (nowTime - lastUpdate > 90000) {
    DateTime now = rtc.now(); // we need this

    if ((now.hour() == 0) && (now.minute() == 0)) {
      lastUpdate = nowTime;
      currentStreak++;
      EEPROM.put(CURRENT_STREAK_ADDRS, currentStreak);

      if (currentStreak > maxStreak) {
        maxStreak = currentStreak;
        EEPROM.put(MAX_STREAK_ADDRS, maxStreak);
      }
    }
  }
} // end updateStreak

void pollButton() {
  if (digitalRead(BUTTON) == LOW) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("Streak");
    display.println("Broken!");
    display.display();

    currentStreak = -1;
    EEPROM.put(CURRENT_STREAK_ADDRS, currentStreak);

    delay(5000);

  }

} // end resetStreak

void updateOLED() {
  static unsigned long last_OLED_refresh = 0;
  unsigned long nowTime = millis();

  if (nowTime - last_OLED_refresh > OLED_REFRESH) {
    last_OLED_refresh = nowTime;

    DateTime now = rtc.now(); // we need this
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(now.hour(), DEC);
    display.print(":");
    if (now.minute() < 10) {
      display.print(0, DEC);
      display.print(now.minute(), DEC);
    } else {
      display.print(now.minute(), DEC);
    }
    display.print(" ");
    display.print(now.month(), DEC);
    display.print("/");
    display.print(now.day(), DEC);
    display.print("/");
    display.println(now.year(), DEC);


    display.setTextSize(1);
    display.print("Current: ");
    display.print(currentStreak, DEC);
    display.println(" days");

    display.print("Max: ");
    display.print(maxStreak, DEC);
    display.println(" days");

    display.display();
  } // end timing loop
} // end updateOLED

void setTime() {
  DateTime now = rtc.now(); // we need this

  inputString.remove(0, 2);     // remove the T and the space
  int h = inputString.toInt();  // get hour
  inputString.remove(0, inputString.indexOf(":") + 1); // remove hour from string
  int m = inputString.toInt();  // get minute
  inputString.remove(0, inputString.indexOf(":") + 1); // remove minute from string
  int s = inputString.toInt();  // get second

  rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));
  printTime();

} // end setTime

void printTime() {
  DateTime now = rtc.now();

  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
  Serial.print(") ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
} //end printTime

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag so the main loop can
    // do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}
