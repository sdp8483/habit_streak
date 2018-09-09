#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"

#define FW_VERSION "V1.0.0"
#define HW_VERSION "V0.0.a"

// --- Pin Defines ---
#define BUTTON         3
#define OLED_RESET     4
#define AMBIENT_SENSOR A0

// --- EEPROM Addresses ---
//#define INIT_EEPROM_VALS                                  // initialize eeprom with default values if defined
#define BUTTON_TS       0                                 // last button press timestamp
#define MAX_TIMESPAN    sizeof(uint32_t)                  // max timespan address
#define MIN_TIMESPAN    MAX_TIMESPAN + sizeof(uint32_t)   // min timespan address
#define AVG_TIMESPAN    MIN_TIMESPAN + sizeof(uint32_t)   // Average timespan address
#define AMB_THRESHOLD   AVG_TIMESPAN + sizeof(uint32_t)   // ambient sensor threshold address

// --- SSD1306 128x32 i2c OLED ---
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define OLED_REFRESH 5000 // OLED refresh rate in ms
int ambient_threshold;    // ambinet sensor threshold to turn off display

// --- DS3231 Real Time Clock ---
RTC_DS3231 rtc;

// --- Serial Input Variables ---
String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete

// --- Time Variables ---
struct timedelta {
  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
};

uint32_t button_ts;
uint32_t max_ts;
uint32_t min_ts;
uint32_t avg_ts;

void setup() {
  Serial.begin(9600);
  inputString.reserve(20); // reserve 20 bytes for the inputString

  pinMode(BUTTON, INPUT_PULLUP);

  // initialize EEPROM with values if INIT_EEPROM_VALS is uncommented
#ifdef INIT_EEPROM_VALS
  EEPROM.put(BUTTON_TS, 1536425340UL);
  EEPROM.put(MAX_TIMESPAN, 216720UL);
  EEPROM.put(MIN_TIMESPAN, 13058UL);
  EEPROM.put(AVG_TIMESPAN, 114889UL);
  EEPROM.put(AMB_THRESHOLD, 5);
#endif

  // read in EEPROM data
  EEPROM.get(BUTTON_TS, button_ts);
  EEPROM.get(MAX_TIMESPAN, max_ts);
  EEPROM.get(MIN_TIMESPAN, min_ts);
  EEPROM.get(AVG_TIMESPAN, avg_ts);
  EEPROM.get(AMB_THRESHOLD, ambient_threshold);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();  // clear preloaded Adafruit splashscreen
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    display.print("Couldn't find RTC");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    display.print("RTC lost power, check the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  // print out data
  printTime();
  Serial.print("Button timestamp: ");
  Serial.println(button_ts);
  Serial.print("Max Timespan: ");
  Serial.println(max_ts);
  Serial.print("Min Timespan: ");
  Serial.println(min_ts);
  Serial.print("Average Timespan: ");
  Serial.println(avg_ts);
  Serial.print("Ambient Threshold: ");
  Serial.println(ambient_threshold);
  Serial.print("Current Ambient: ");
  Serial.println(analogRead(AMBIENT_SENSOR));
  Serial.println();
} // end setup()

void loop() {
  checkSerial();
  pollButton();
  updateOLED();
} // end loop()

void updateOLED() {
  static unsigned long last_OLED_refresh = 0;
  unsigned long nowTime = millis();

  enum statDisplay {
    MAX,
    MIN,
    AVG,
  };

  static statDisplay currentStatDisplay = MAX;

  if (nowTime - last_OLED_refresh > OLED_REFRESH) {
    last_OLED_refresh = nowTime;

    DateTime now = rtc.now(); // we need this

    timedelta nowTD = get_timedelta((now.unixtime() - button_ts));  // calculate time delta between now button presses
    timedelta maxTD = get_timedelta(max_ts);                      // calculate max time delta
    timedelta minTD = get_timedelta(min_ts);                      // calculate min time delta
    timedelta avgTD = get_timedelta(avg_ts);                      // calcualte avg time delta

    if (analogRead(AMBIENT_SENSOR) > ambient_threshold) {

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

      display.setTextSize(2);
      display.print(nowTD.days);
      display.print("d ");
      display.print(nowTD.hours);
      display.print(':');
      if (nowTD.minutes < 10) {
        display.print(0, DEC);
      }
      display.println(nowTD.minutes);
      display.setTextSize(1);
      switch (currentStatDisplay) {
        case MAX:
          display.print("Max: ");
          display.print(maxTD.days);
          display.print("d ");
          display.print(maxTD.hours);
          display.print(':');
          if (maxTD.minutes < 10) {
            display.print(0, DEC);
          }
          display.println(maxTD.minutes);
          currentStatDisplay = MIN;
          break;

        case MIN:
          display.print("Min: ");
          display.print(minTD.days);
          display.print("d ");
          display.print(minTD.hours);
          display.print(':');
          if (minTD.minutes < 10) {
            display.print(0, DEC);
          }
          display.println(minTD.minutes);
          currentStatDisplay = AVG;
          break;

        case AVG:
          display.print("Avg: ");
          display.print(avgTD.days);
          display.print("d ");
          display.print(avgTD.hours);
          display.print(':');
          if (avgTD.minutes < 10) {
            display.print(0, DEC);
          }
          display.println(avgTD.minutes);
          currentStatDisplay = MAX;
          break;

        default:
          currentStatDisplay = MAX;
      }

      display.display();
    } else {
      display.clearDisplay();
      display.display();
    } // end ambient if loop
  } // end timing loop
} // end updateOLED

void pollButton() {
  if (digitalRead(BUTTON) == LOW) {
    DateTime now = rtc.now();

    uint32_t now_timespan = now.unixtime() - button_ts;

    // is this new timespan longer then the previous maximum?
    if (now_timespan > max_ts) {
      max_ts = now.unixtime() - button_ts;
      EEPROM.put(MAX_TIMESPAN, max_ts);
    }

    // is this new timespan shorter then the previous minimum?
    if (now_timespan < min_ts) {
      min_ts = now.unixtime() - button_ts;
      EEPROM.put(MIN_TIMESPAN, min_ts);
    }

    // calculate the rolling average timespan
    avg_ts += now_timespan;
    avg_ts /= 2;
    EEPROM.put(AVG_TIMESPAN, avg_ts);

    while (digitalRead(BUTTON) == LOW) {
      delay(50); // wait for release
    } // end while button low
    button_ts = now.unixtime();
    EEPROM.put(BUTTON_TS, button_ts);
  } // end if button low
} // end pollButton

timedelta get_timedelta(uint32_t timespan_seconds) {
  timedelta td;

  td.days = uint16_t(timespan_seconds / 86400UL);
  timespan_seconds -= td.days * 86400UL;

  td.hours = uint8_t(timespan_seconds / 3600UL);
  timespan_seconds -= td.hours * 3600UL;

  td.minutes = uint8_t(timespan_seconds / 60);

  return td;
} // end get_timedelta

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
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
} //end printTime

void setAmbient() {
  inputString.remove(0, 2);     // remove the a and the space
  ambient_threshold = inputString.toInt();  // get analog threshold

  EEPROM.put(AMB_THRESHOLD, ambient_threshold);
}

void checkSerial() {
  if (stringComplete) {
    Serial.println();
    Serial.println(inputString);
    inputString.toLowerCase();
    if (inputString.startsWith("?")) {
      Serial.println("");
      Serial.println("t HH:MM:SS -> To set time");
      Serial.println("now        -> To get current datetime");
      Serial.println("a XXXX     -> To set light threshold");
    } else if (inputString.startsWith("now")) {
      printTime();
    } else if (inputString.startsWith("t")) {
      setTime();
    } else if (inputString.startsWith("a")) {
      setAmbient();
    } else {
      Serial.print(inputString);
      Serial.println(" is not a valid command!");
    }
    inputString = "";
    stringComplete = false;
  } // end stringComplete
} // end checkSerial

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
