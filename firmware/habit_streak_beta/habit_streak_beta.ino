#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"
#include <EEPROM.h>

//#define WRITE_INIT_DATA  // uncomment to write initial data to eeprom for new devices

#define BUTTON 3

#define FIRST_TS_ADDRESS   0                                      // eeprom address of newest button press time stamp
#define SECOND_TS_ADDRESS  sizeof(uint32_t)                       // eeprom address of second oldest button press time stamp
#define THIRD_TS_ADDRESS   SECOND_TS_ADDRESS + sizeof(uint32_t)   // eeprom address of oldest button press time stamp

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define OLED_REFRESH 5000 //ms

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

RTC_DS3231 rtc;

String inputString = "";         // a String to hold incoming data
boolean stringComplete = false;  // whether the string is complete

uint32_t firstTS;  // nesets time stamp of when button was pressed
uint32_t secondTS; // second oldest time stamp of when button was pressed
uint32_t thirdTS;  // oldest time stamp of when button was pressed

struct TimeDelta {
  uint16_t days;
  uint8_t hours;
  uint8_t minutes;
};

void setup() {
  Serial.begin(9600);

  inputString.reserve(200);     // reserve 200 bytes for the inputString:

  pinMode(BUTTON, INPUT_PULLUP);

#ifdef WRITE_INIT_DATA
  //EEPROM.put(FIRST_TS_ADDRESS, 1535223600UL);
  EEPROM.put(SECOND_TS_ADDRESS, 1535223600UL);
  EEPROM.put(THIRD_TS_ADDRESS,  1535103000UL);
#endif

  // read in EEPROM data
  EEPROM.get(FIRST_TS_ADDRESS, firstTS);
  EEPROM.get(SECOND_TS_ADDRESS, secondTS);
  EEPROM.get(THIRD_TS_ADDRESS, thirdTS);

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
  checkSerial();
  pollButton();
  updateOLED();

} // end loop

void checkSerial() {
  if (stringComplete) {
    Serial.println();
    Serial.println(inputString);
    inputString.toLowerCase();
    if (inputString.startsWith("?")) {
      Serial.println("");
      Serial.println("T HH:MM:SS -> To set time");
      Serial.println("now        -> To get current datetime");
    } else if (inputString.startsWith("now")) {
      printTime();
    } else if (inputString.startsWith("t")) {
      setTime();
    }
    inputString = "";
    stringComplete = false;
  } // end stringComplete
} // end checkSerial

void pollButton() {
  if (digitalRead(BUTTON) == LOW) {
    DateTime now = rtc.now();

    if ((now.unixtime() - firstTS) > (secondTS - thirdTS)) {
      thirdTS = firstTS;
      secondTS = now.unixtime();
    }

    firstTS = now.unixtime();

    EEPROM.put(FIRST_TS_ADDRESS, firstTS);
    EEPROM.put(SECOND_TS_ADDRESS, secondTS);
    EEPROM.put(THIRD_TS_ADDRESS, thirdTS);

    while (digitalRead(BUTTON) == LOW) {
      delay(50); // wait for release
    }
  }
} // end pollButton

void updateOLED() {
  static unsigned long last_OLED_refresh = 0;
  unsigned long nowTime = millis();

  if (nowTime - last_OLED_refresh > OLED_REFRESH) {
    last_OLED_refresh = nowTime;

    DateTime now = rtc.now(); // we need this

    TimeDelta nowTD = calcTimeDelta(now.unixtime(), firstTS); // calculate time delta between now and newest button press
    TimeDelta pastTD = calcTimeDelta(secondTS, thirdTS); // calculate previous time delta

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
    display.print("Now: ");
    display.print(nowTD.days);
    display.print(' ');
    display.print(nowTD.hours);
    display.print(':');
    if (nowTD.minutes < 10) {
      display.print(0, DEC);
    }
    display.println(nowTD.minutes);

    display.print("Max: ");
    display.print(pastTD.days);
    display.print(' ');
    display.print(pastTD.hours);
    display.print(':');
    if (pastTD.minutes < 10) {
      display.print(0, DEC);
    }
    display.println(pastTD.minutes);

    display.display();
  } // end timing loop
} // end updateOLED

TimeDelta calcTimeDelta(uint32_t firstUnixTS, uint32_t secondUnixTS) {
  TimeDelta td;
  uint32_t totalSeconds = firstUnixTS - secondUnixTS;

  td.days = uint16_t(totalSeconds / 86400UL);
  totalSeconds -= td.days * 86400UL;

  td.hours = uint8_t(totalSeconds / 3600UL);
  totalSeconds -= td.hours * 3600UL;

  td.minutes = uint8_t(totalSeconds / 60);

  return td;
}

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
