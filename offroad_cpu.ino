#include <Wire.h>
#include <LiquidCrystal.h>
#include <EEPROMEx.h>
#include <I2Cdev.h>
#include <BMP085.h>
#include <ADXL345.h>
#include <HMC5883L.h>


const uint8_t VERSION = 2;

unsigned long millisCounter = 0;
unsigned long minMaxAltitudeMillsCounter = 0;

// Menu and display modes
const String menuText[][2] = {{"Incline        ","               "},
                              {"Altitude       ","               "},
                              {"Compass        ","               "},
                              {"Multi          ","               "},
                              {"Temperature    ","               "},
                              {"Track          ","Altitude       "},
                              {"Min/Max        ","Altitude       "},
                              {"Calibrate      ","Altitude       "},
                              {"Calibrate      ","Inclinometer   "},
                              {"Set            ","Brightness     "},
                              {"Factory        ","Reset          "}};
const uint8_t INCLINE = 0;
const uint8_t ALTITUDE = 1;
const uint8_t COMPASS = 2;
const uint8_t MULTI = 3;
const uint8_t TEMPERATURE = 4;
const uint8_t TRACK = 5;
const uint8_t MINMAX = 6;
const uint8_t CALIBRATE_ALT = 7;
const uint8_t CALIBRATE_INC = 8;
const uint8_t BRIGHTNESS = 9;
const uint8_t RESET = 10;
const uint8_t MENU = 11;
const uint8_t MENU_LENGTH = 11;

// Keep track of where we are
uint8_t mode;
int8_t displayMenuItem;
uint8_t lastMode;

// Display in (m)etric or (i)mperial
char unit;

// Display character for brightness meter
const int BLOCK_CHAR = 255;
const int MAX_BRIGHTNESS = 255;
const int MIN_BRIGHTNESS = 1;
const int BRIGHTNESS_INCREMENT = 32;
int brightness;


/////////////////////
// Three-way button
/////////////////////
const uint8_t UP = 13;
const uint8_t PUSH = 12;
const uint8_t DOWN = 11;

uint8_t lastState = 0;
uint8_t buttonState = 0;


//////////////////////
// Accelerometer
//////////////////////

ADXL345 accel;

// Extents of accelerometer
int xMin, xMax, yMin, yMax, zMin, zMax;
int xMinCal = 0;
int xMaxCal = 0;
int yMinCal = 0;
int yMaxCal = 0;
int zMinCal = 0;
int zMaxCal = 0;

// So that we can zero out the inclinometer readings
int pitchOffset, rollOffset;


//////////////////////
// Barometer
//////////////////////

BMP085 barometer;
float lastAltitude;

// User-defined offset based some something like GPS readings
float calibrateAltitudeOffset = 0;
float calibrateAltitudeDisplay;

// When user wants to track altitude change we start with the current altitude
float trackingAltitudeOffset = 0;

// Keep track of minimum and maximum altitude
float minAltitude;
float maxAltitude;

// convert from meters to feet
float metersToFeet = 3.28084;



////////////////////
// Magnetometer
////////////////////
HMC5883L mag;

float compassOffset;



////////////////////
// Display
////////////////////


const uint8_t RSPin = 0;
const uint8_t RWPin = 1;
const uint8_t ENPin = 4;
const uint8_t D4Pin = 5;
const uint8_t D5Pin = 6;
const uint8_t D6Pin = 7;
const uint8_t D7Pin = 8;
const uint8_t BACKLIGHT_PIN = 9;

LiquidCrystal lcd(RSPin, RWPin, ENPin, D4Pin, D5Pin, D6Pin, D7Pin);

byte UP_ARROW = 0;
byte upArrow[8] = {
  B00000,
  B00100,
  B01110,
  B11111,
  B00000,
  B00000,
  B00000,
  B00000
};

byte DOWN_ARROW = 1;
byte downArrow[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B01110,
  B00100,
  B00000
};

byte DEGREE = 2;
byte degree[8] = {
  B01100,
  B10010,
  B10010,
  B01100,
  B00000,
  B00000,
  B00000,
  B00000
};

byte FOOT = 3;
byte foot[8] = {
  B01000,
  B01000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};

byte CLICK = 4;
byte click[8] = {
  B00000,
  B00000,
  B00100,
  B01110,
  B00100,
  B00000,
  B00000,
  B00000
};

byte PITCH_ARROW = 5;
byte pitchArrow[8] = {
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100
};





///////////////////////////
// EEPROM memory addresses
///////////////////////////

// display
const int LCD_BACKLIGHT_ADDRESS = EEPROM.getAddress(sizeof(int));               // backlight setting
const int BAUD_ADDRESS = EEPROM.getAddress(sizeof(int));                        // Baud rate setting
const int SPLASH_SCREEN_ADDRESS = EEPROM.getAddress(sizeof(int));               // splash screen on/off
const int ROWS_ADDRESS = EEPROM.getAddress(sizeof(int));                        // number of rows
const int COLUMNS_ADDRESS = EEPROM.getAddress(sizeof(int));                     // number of columns

// sensors
const int VERSION_ADDRESS = EEPROM.getAddress(sizeof(int));                     // version
const int UNIT_ADDRESS = EEPROM.getAddress(sizeof(byte));                       // metric or imperial
const int PITCH_OFFSET_ADDRESS = EEPROM.getAddress(sizeof(int));                // pitch degree offset
const int ROLL_OFFSET_ADDRESS = EEPROM.getAddress(sizeof(int));                 // roll degree offset
const int MODE_ADDRESS = EEPROM.getAddress(sizeof(int));                        // which menu item is showing
const int CALIBRATE_ALTITUDE_OFFSET_ADDRESS = EEPROM.getAddress(sizeof(float)); // altitude offset
const int COMPASS_OFFSET_ADDRESS = EEPROM.getAddress(sizeof(float));            // compass offset

// inclinometer calibrate
const int X_MIN_ADDRESS = EEPROM.getAddress(sizeof(int));
const int X_MAX_ADDRESS = EEPROM.getAddress(sizeof(int));
const int Y_MIN_ADDRESS = EEPROM.getAddress(sizeof(int));
const int Y_MAX_ADDRESS = EEPROM.getAddress(sizeof(int));
const int Z_MIN_ADDRESS = EEPROM.getAddress(sizeof(int));
const int Z_MAX_ADDRESS = EEPROM.getAddress(sizeof(int));

void setup() {
  // Our current sensor actually accepts 5v now, and there's nothing currently
  // hooked up to the analog pins, but there may be in the future and most
  // sensors are 3.3v so let's just set it for now
  analogReference(EXTERNAL);

  // Set EEPROM variables if the version changes
  if (EEPROM.readInt(VERSION_ADDRESS) != VERSION) {
    memoryReset();
  }

  Wire.begin();

  setupVariables();
  setupDisplay();
  setupButton();
  setupAccelerometer();
  setupBarometer();
  setupMagnetometer();

  // Holding the switch in the down position at startup performs a factory reset
  if (digitalRead(DOWN) == LOW) {
    factoryReset();
  }
}

void loop() {

  // Were any buttons pressed?
  buttonCheck();

  // To properly record the min/max altitude we need to check it on every cycle
  updateMinMaxAltitude();

  if (mode == INCLINE) {
    loopInclinometer();
  } else if (mode == ALTITUDE) {
    loopAltimeter();
  } else if (mode == COMPASS) {
    loopCompass();
  } else if (mode == TRACK) {
    loopTrack();
  } else if (mode == MULTI) {
    loopMulti();
  } else if (mode == MINMAX) {
    loopMinMax();
  } else if (mode == CALIBRATE_ALT) {
    loopCalibrateAlt();
  } else if (mode == CALIBRATE_INC) {
    loopCalibrateInc();
  } else if (mode == TEMPERATURE) {
    loopTemperature();
  } else if (mode == BRIGHTNESS) {
    loopBrightness();
  } else if (mode == MENU) {
    loopMenu();
  }
}


////////////
// Setups //
////////////

void factoryReset() {
  memoryReset();
  setupVariables();
  setBrightness();
}

void memoryReset() {
  EEPROM.writeInt(LCD_BACKLIGHT_ADDRESS, 255);               // max brightness
  EEPROM.writeByte(UNIT_ADDRESS, 'i');                       // default to imperial
  EEPROM.writeInt(PITCH_OFFSET_ADDRESS, 0);                  // no pitch offset
  EEPROM.writeInt(ROLL_OFFSET_ADDRESS, 0);                   // no roll offset
  EEPROM.writeInt(MODE_ADDRESS, INCLINE);                    // default to inclinometer
  EEPROM.writeFloat(CALIBRATE_ALTITUDE_OFFSET_ADDRESS, 0.0); // no altitude offset
  EEPROM.writeFloat(COMPASS_OFFSET_ADDRESS, 0.0);            // no compass offset

  // Default inclinometer settings, hopefully user calibrates manually
  EEPROM.writeInt(X_MIN_ADDRESS, -272);
  EEPROM.writeInt(X_MAX_ADDRESS, 303);
  EEPROM.writeInt(Y_MIN_ADDRESS, -278);
  EEPROM.writeInt(Y_MAX_ADDRESS, 274);
  EEPROM.writeInt(Z_MIN_ADDRESS, -277);
  EEPROM.writeInt(Z_MAX_ADDRESS, 262);

  // And the current software version
  EEPROM.writeInt(VERSION_ADDRESS, VERSION);
}

// Saves the current mode to EEPROM
void saveMode() {
  EEPROM.writeInt(MODE_ADDRESS, mode);
}

// Reads variables from EEPROM and sets their RAM equivalents
void setupVariables() {
  brightness = EEPROM.readInt(LCD_BACKLIGHT_ADDRESS);
  unit = EEPROM.readByte(UNIT_ADDRESS);
  pitchOffset = EEPROM.readInt(PITCH_OFFSET_ADDRESS);
  rollOffset = EEPROM.readInt(ROLL_OFFSET_ADDRESS);
  mode = displayMenuItem = EEPROM.readInt(MODE_ADDRESS);
  calibrateAltitudeOffset = EEPROM.readFloat(CALIBRATE_ALTITUDE_OFFSET_ADDRESS);
  calibrateAltitudeDisplay = NULL;
  compassOffset = EEPROM.readFloat(COMPASS_OFFSET_ADDRESS);

  xMin = EEPROM.readInt(X_MIN_ADDRESS);
  xMax = EEPROM.readInt(X_MAX_ADDRESS);
  yMin = EEPROM.readInt(Y_MIN_ADDRESS);
  yMax = EEPROM.readInt(Y_MAX_ADDRESS);
  zMin = EEPROM.readInt(Z_MIN_ADDRESS);
  zMax = EEPROM.readInt(Z_MAX_ADDRESS);
}

void setupDisplay() {
  lcd.begin(16, 2);

  // custom characters
  lcd.createChar(UP_ARROW, upArrow);
  lcd.createChar(DOWN_ARROW, downArrow);
  lcd.createChar(DEGREE, degree);
  lcd.createChar(FOOT, foot);
  lcd.createChar(CLICK, click);
  lcd.createChar(PITCH_ARROW, pitchArrow);

  // Set up the backlight
  pinMode(BACKLIGHT_PIN, OUTPUT);
  brightness = EEPROM.readInt(LCD_BACKLIGHT_ADDRESS);
  setBrightness();

  // Splash screen
  moveToFirstLine();
  lcd.print("  Cameron Tech  ");
  moveToSecondLine();
  lcd.print("  Offroad CPU   ");
  delay(2000);
  clearScreen();
}


void setupButton() {
  pinMode(UP, INPUT);
  pinMode(PUSH, INPUT);
  pinMode(DOWN, INPUT);

  // Set pull-up resistors
  digitalWrite(UP, HIGH);
  digitalWrite(PUSH, HIGH);
  digitalWrite(DOWN, HIGH);
}


void setupAccelerometer() {
  accel.initialize();
}


void setupBarometer() {
  barometer.initialize();
  resetMinMaxAltitude();
}


void setupMagnetometer() {
  mag.initialize();
}


// Watches for button events
void buttonCheck() {
  if (digitalRead(UP) == LOW) {
    buttonState = UP;
  } else if (digitalRead(PUSH) == LOW) {
    buttonState = PUSH;
  } else if (digitalRead(DOWN) == LOW) {
    buttonState = DOWN;
  } else {
    buttonState = 0;
  }

  if (buttonState != lastState) {
    if (buttonState != 0) {
      buttonClick();
    }
    lastState = buttonState;
  }

  // debounce delay
  delay(10);
}


// Button was clicked
void buttonClick() {
  if (mode == MENU) {
    if (buttonState == DOWN) {
      displayMenuItem++;
      if (displayMenuItem == MENU_LENGTH) {   // rollover to beginning of list
        displayMenuItem = 0;
      }
    } else if (buttonState == UP) {
      displayMenuItem--;
      if (displayMenuItem < 0) {              // rollover to end of list
        displayMenuItem = MENU_LENGTH - 1;
      }
    } else if (buttonState == PUSH) {
      if (displayMenuItem == RESET) {         // Factory Reset feature
        factoryReset();
      } else{
        mode = displayMenuItem;               // select menu item
        startMode();
      }
    }
  } else {
    if (mode == CALIBRATE_ALT) {  // when in calibrate mode, up/down change values, push goes back to menu
      if (buttonState == UP) {
        incrementAltimeterCalibration();
      } else if (buttonState == DOWN) {
        decrementAltimeterCalibration();
      } else if (buttonState == PUSH) {
        saveAltitudeCalibration();
        displayMenuItem = ALTITUDE;
        mode = ALTITUDE;
        startMode();
      }
    } else if (mode == BRIGHTNESS) { // when in brightness mode, up/down changes brightness, push goes back to menu
      if (buttonState == UP) {
        increaseBrightness();
      } else if (buttonState == DOWN) {
        decreaseBrightness();
      } else {
        saveBrightness();
        returnToLastMode();
      }
    } else {                                                 // in most modes up/down goes to menu, push is optional function
      if (buttonState == UP || buttonState == DOWN) {        // Button pressed up or down to go into the menu
        lastMode = mode;
        mode = MENU;
      } else if (buttonState == PUSH) {
        if (mode == TRACK) {     // Button clicked while tracking altitude
          resetTrackingAltitude();
        } else if (mode == MINMAX) {    // Button clicked while viewing min/max
          resetMinMaxAltitude();
        } else if (mode == ALTITUDE || mode == TEMPERATURE) { // Button clicked on altimeter or thermometer
          switchUnit();
        } else if (mode == INCLINE) {   // Button clicked while viewing incline
          zeroInclinometer();
        } else if (mode == COMPASS) {
          zeroCompass();
        } else if (mode = CALIBRATE_INC) {
          saveIncCalibration();
          returnToLastMode();
        }
      }
    }
  }
}

void returnToLastMode() {
  mode = lastMode;
  displayMenuItem = lastMode;
  startMode();
}


// Called when a menu item is selected but before looping through that mode
void startMode() {
  saveMode();
  if (mode == CALIBRATE_ALT) {
    startCalibration();
  }
  if (mode == CALIBRATE_INC) {
    clearScreen();
  }
}




////////////
// Loops  //
////////////

void loopMenu() {
  if (millis() - millisCounter > 250) {
    moveToFirstLine();
    lcd.print(menuText[displayMenuItem][0]);
    lcd.write(UP_ARROW);
    moveToSecondLine();
    lcd.print(menuText[displayMenuItem][1]);
    lcd.write(DOWN_ARROW);

    resetCounter();
  }
}

void loopInclinometer() {
  if (millis() - millisCounter > 250) {

    moveToFirstLine();
    lcd.print("  Pitch   Roll ");
    lcd.write(CLICK);
    moveToSecondLine();

    int pitch, roll;
    getIncline(pitch, roll, false);
    displayIncline(roll, pitch);

    resetCounter();
  }
}


void loopAltimeter() {
  if (millis() - millisCounter > 250) {

    moveToFirstLine();
    lcd.print("    Altitude   ");
    lcd.write(CLICK);
    moveToSecondLine();
    outputAltitudeLine(false, NULL);

    resetCounter();
  }
}


void loopCompass() {
  if (millis() - millisCounter > 500) {

    float heading = getHeading();

    moveToFirstLine();
    lcd.print("     Heading   ");
    lcd.write(CLICK);
    moveToSecondLine();
    outputHeadingLine(heading);

    resetCounter();
  }

}

void loopMulti() {
  if (millis() - millisCounter > 250) {

    moveToFirstLine();

    // incline
    int x, y;
    getIncline(x, y, false);
    displayIncline(y, x);

    // Draw pitch/roll arrows
    lcd.setCursor(0,0);
    lcd.write(PITCH_ARROW);
    lcd.setCursor(14,0);
    lcd.write(127);  // left arrow
    lcd.setCursor(15,0);
    lcd.write(126);  // right arrow

    moveToSecondLine();

    // altitude
    centerText(altitudeWithUnit(getAltitude()), 9);

    lcd.setCursor(9, 1);

    // compass
    centerText(degreeToCardinal(getHeading()), 7);

    resetCounter();
  }

}

void loopTemperature() {
  if (millis() - millisCounter > 1000) {
    String output;
    moveToFirstLine();
    lcd.print("  Temperature  ");
    lcd.write(CLICK);
    moveToSecondLine();

    float temperature = getTemperature();

    if (unit == 'i') {
      output = floatToString(temperature*9/5+32, 1) + char(DEGREE) + 'F';
    } else {
      output = floatToString(temperature, 1) + char(DEGREE) + 'C';
    }
    centerText(output, 16);

    resetCounter();
  }
}

void loopTrack() {
  if (millis() - millisCounter > 500) {
    moveToFirstLine();
    lcd.print("Track Altitude ");
    lcd.write(CLICK);
    moveToSecondLine();
    outputAltitudeLine(true, NULL);
    resetCounter();
  }
}

void loopMinMax() {
  if (millis() - millisCounter > 500) {
    moveToFirstLine();
    lcd.print("   Min    Max  ");
    lcd.write(CLICK);
    moveToSecondLine();
    centerText(altitudeWithUnit(minAltitude), 8, true);
    centerText(altitudeWithUnit(maxAltitude), 8, false);

    resetCounter();
  }
}

void loopCalibrateAlt() {
  if (millis() - millisCounter > 250) {
    moveToFirstLine();
    lcd.print("  Set Altitude ");
    lcd.write(UP_ARROW);
    moveToSecondLine();

    if (calibrateAltitudeDisplay == NULL) {
      calibrateAltitudeDisplay = getAltitude();
    }
    outputAltitudeLine(false, calibrateAltitudeDisplay);
    lcd.setCursor(15,1);
    lcd.write(DOWN_ARROW);

    resetCounter();
  }
}

void loopCalibrateInc() {
  if (millis() - millisCounter > 100) {
    int x, y, z;
    accel.getAcceleration(&x, &y, &z);

    if (x < xMinCal) xMinCal = x;
    if (y < yMinCal) yMinCal = y;
    if (z < zMinCal) zMinCal = z;
    if (x > xMaxCal) xMaxCal = x;
    if (y > yMaxCal) yMaxCal = y;
    if (z > zMaxCal) zMaxCal = z;

    moveToFirstLine();
    lcd.print(" ");
    lcd.print(xMinCal);
    lcd.print(" ");
    lcd.print(yMinCal);
    lcd.print(" ");
    lcd.print(zMinCal);
    lcd.setCursor(15, 0);
    lcd.write(CLICK);

    moveToSecondLine();
    lcd.print("  ");
    lcd.print(xMaxCal);
    lcd.print("  ");
    lcd.print(yMaxCal);
    lcd.print("  ");
    lcd.print(zMaxCal);

    resetCounter();
  }
}

void loopBrightness() {
  if (millis() - millisCounter > 250) {

    moveToFirstLine();
    lcd.print("   Brightness   ");
    moveToSecondLine();

    int unsigned dots = map(brightness, MIN_BRIGHTNESS, MAX_BRIGHTNESS, 1, 16);
    for (int i=0; i<dots; i++) {
      lcd.write(BLOCK_CHAR);
    }
    for (int i=0; i<16-dots; i++) {
      lcd.write(" ");
    }
    resetCounter();
  }
}




//////////////////////////////
// Counters
//////////////////////////////

// Resets the millisecond counter
void resetCounter() {
  millisCounter = millis();
}

// A separate counter that keeps track of when we record min/max altitude
// behind the scenes
void resetMinMaxCounter() {
  minMaxAltitudeMillsCounter = millis();
}




//////////////////////////////
// Display functions
//////////////////////////////

void moveToFirstLine() {
  lcd.setCursor(0,0);
}


void moveToSecondLine() {
  lcd.setCursor(0,1);
}


void clearScreen() {
  moveToFirstLine();
  lcd.print("                ");
  moveToSecondLine();
  lcd.print("                ");
}


void increaseBrightness() {
  brightness += BRIGHTNESS_INCREMENT;
  if (brightness > MAX_BRIGHTNESS) {
    brightness = MAX_BRIGHTNESS;
  }
  setBrightness();
}


void decreaseBrightness() {
  brightness -= BRIGHTNESS_INCREMENT;
  if (brightness < MIN_BRIGHTNESS) {
    brightness = MIN_BRIGHTNESS;
  }
  setBrightness();
}


void setBrightness() {
  analogWrite(BACKLIGHT_PIN, brightness);
}


void saveBrightness() {
  EEPROM.writeInt(LCD_BACKLIGHT_ADDRESS, brightness);
}



//////////////////////////////
// Incline functions
//////////////////////////////

// Pass the shortCircuit boolean if you want to return the roll/pitch without
// offset correction (used to set offset values)
void getIncline(int &pitch, int &roll, bool shortCircuit) {
  int x, y, z;
  int xTotal = 0;
  int yTotal = 0;
  int zTotal = 0;
  int samples = 10;

  // Take 10 samples and average them to help lower noise
  for (int i=0; i<samples; i++) {
    accel.getAcceleration(&x, &y, &z);
    xTotal += x;
    yTotal += y;
    zTotal += z;
  }
  int xAvg = xTotal / samples;
  int yAvg = yTotal / samples;
  int zAvg = zTotal / samples;

  // convert to range of -90 to +90 degrees
  int xAng = map(xAvg, xMin, xMax, -90, 90);
  int yAng = map(yAvg, yMin, yMax, -90, 90);
  int zAng = map(zAvg, zMin, zMax, -90, 90);

  // convert radians to degrees
  int pitchOut = -(RAD_TO_DEG * (atan2(-yAng, -zAng) + PI));
  int rollOut = RAD_TO_DEG * (atan2(-xAng, -zAng) + PI);

  // convert left roll and forward pitch to negative degrees
  if (pitchOut < -180) {
    pitchOut = pitchOut + 360;
  }
  if (rollOut > 180) {
    rollOut = rollOut - 360;
  }

  // Sometimes we want the actual output degree measurement, not the calibrated
  // version, so pass `true` to the shortCircuit param and return them
  if (shortCircuit) {
    pitch = pitchOut;
    roll = rollOut;
    return;
  }

  // Take into account any offset
  pitchOut -= pitchOffset;
  rollOut -= rollOffset;

  // Now we need to re-add the offset in case we try to roll around 180/-180 again
  if (pitchOut < -180) {
    pitchOut += 360;
  } else if (pitchOut > 180) {
    pitchOut -= 360;
  }
  if (rollOut < -180) {
    rollOut += 360;
  } else if (rollOut > 180) {
    rollOut -= 360;
  }

  pitch = pitchOut;
  roll = rollOut;
}

// Takes the current incline degrees and sets them to 0 by recording offsets
void zeroInclinometer() {
  getIncline(pitchOffset, rollOffset, true);
  EEPROM.writeInt(PITCH_OFFSET_ADDRESS, pitchOffset);
  EEPROM.writeInt(ROLL_OFFSET_ADDRESS, rollOffset);
}


// Writes the current pitch/roll to the screen
void displayIncline(int first, int second) {
  // convert int values to strings for output
  // String firstString = String(first);// + char(DEGREE);
  // String secondString = String(second);// + char(DEGREE);

  centerText(String(first), 8, true, char(DEGREE));
  centerText(String(second), 8, false, char(DEGREE));
}


// saves min/max x/y/z to EEPROM
void saveIncCalibration() {
  EEPROM.writeInt(X_MIN_ADDRESS, xMinCal);
  EEPROM.writeInt(X_MAX_ADDRESS, xMaxCal);
  EEPROM.writeInt(Y_MIN_ADDRESS, yMinCal);
  EEPROM.writeInt(Y_MAX_ADDRESS, yMaxCal);
  EEPROM.writeInt(Z_MIN_ADDRESS, zMinCal);
  EEPROM.writeInt(Z_MAX_ADDRESS, zMaxCal);

  xMin = xMinCal;
  xMax = xMaxCal;
  yMin = yMinCal;
  yMax = yMaxCal;
  zMin = zMinCal;
  zMax = zMaxCal;
}




//////////////////////////////
// Altitude functions
//////////////////////////////

// Zeros out the tracking altitude
void resetTrackingAltitude() {
  trackingAltitudeOffset = getAltitude() + calibrateAltitudeOffset;
}

// Take the current altitude and set that as both the min and max
void resetMinMaxAltitude() {
  minAltitude = getAltitude();
  maxAltitude = minAltitude;
}


// Save min/max values every second
void updateMinMaxAltitude() {
  if (millis() - minMaxAltitudeMillsCounter > 1000) {
    float altitude = getAltitude();

    if (altitude > maxAltitude) {
      maxAltitude = altitude;
    }
    if (altitude < minAltitude) {
      minAltitude = altitude;
    }

    resetMinMaxCounter();
  }
}


// Swaps between metric/imperial measurements
void switchUnit() {
  if (unit == 'i') {
    unit = 'm';
  } else {
    unit = 'i';
  }
  EEPROM.writeByte(UNIT_ADDRESS, unit);
}


// Gets the temperature from the barometric sensor
float getTemperature() {
  barometer.setControl(BMP085_MODE_TEMPERATURE);

  long lastMicros = micros();
  while (micros() - lastMicros < barometer.getMeasureDelayMicroseconds());

  return barometer.getTemperatureC();
}


// Gets the current altitude
float getAltitude() {
  float altitude, thisAltitude;

  // Have to read temperature before getting pressure
  getTemperature();

  barometer.setControl(BMP085_MODE_PRESSURE_3);

  long lastMicros = micros();
  while (micros() - lastMicros < barometer.getMeasureDelayMicroseconds());
  float pressure = barometer.getPressure();
  thisAltitude = barometer.getAltitude(pressure);

  // Use a pseudo-complementary filter to help control the noise from the
  // altitude measurement. Use 90% of the previous measurement and 10% of the
  // current. This will delay the actual alititude reading by about 2.25
  // seconds but it will be much less noisy. With no filtering the altitude
  // jumps about +/- 3 feet with every measurement (4 times per second)
  if (lastAltitude == NULL) {
    altitude = thisAltitude;
  } else {
    altitude = 0.9 * lastAltitude + 0.1 * thisAltitude;
  }
  lastAltitude = altitude;

  return altitude;
}


// Write an entire line with the altitude centered on the line
void outputAltitudeLine(bool includeTrackingOffset, float overrideAltitude) {
  float altitude;
  moveToSecondLine();

  if (overrideAltitude == NULL) {
    altitude = getAltitude();
  } else {
    altitude = overrideAltitude;
  }

  if (includeTrackingOffset) {
    altitude -= trackingAltitudeOffset;
  }

  centerText(altitudeWithUnit(altitude), 16);
}


// Write only the altitude to the screen
String altitudeWithUnit(float altitude) {
  float calibratedAltitude = altitude + calibrateAltitudeOffset;

  if (unit == 'i') {
    return String(round(calibratedAltitude * metersToFeet)) + char(FOOT);
  } else {
    return floatToString(calibratedAltitude, 1) + 'm';
  }

}




//////////////////////////////////
// Altitude calibration functions
//////////////////////////////////

// Increments the altitude calibration offset
void incrementAltimeterCalibration() {
  if (unit == 'i') {
    calibrateAltitudeDisplay += (1 / metersToFeet);
  } else {
    calibrateAltitudeDisplay++;
  }
}


// Decrements the altitude calibration offset
void decrementAltimeterCalibration() {
  if (unit == 'i') {
    calibrateAltitudeDisplay -= (1 / metersToFeet);
  } else {
    calibrateAltitudeDisplay--;
  }
}


// Restarts the calibration process
void startCalibration() {
  calibrateAltitudeDisplay = getAltitude() + calibrateAltitudeOffset;
  calibrateAltitudeOffset = 0;
}


// Save the calibration offset
void saveAltitudeCalibration() {
  // Reset to 0 so we get a raw altitude reading from scratch
  calibrateAltitudeOffset = 0.0;
  calibrateAltitudeOffset = calibrateAltitudeDisplay - getAltitude();

  // Save to EEPROM
  EEPROM.writeFloat(CALIBRATE_ALTITUDE_OFFSET_ADDRESS, calibrateAltitudeOffset);
}



///////////////////////
// Compass functions
///////////////////////

// Returns degree heading
float getHeading() {
  int x, y, z;
  mag.getHeading(&x, &y, &z);
  float heading = atan2(y, x);
  if (heading < 0) {
    heading += 2 * M_PI;
  }
  heading = heading * 180 / M_PI;

  // remove offset from heading
  float headingWithOffset = heading - compassOffset;

  if (headingWithOffset > 359) {
    headingWithOffset -= 360;
  } else if (headingWithOffset < 0) {
    headingWithOffset += 360;
  }

  return headingWithOffset;
}

// Converts degrees to a cardinal direction (N, NE, etc)
String degreeToCardinal(float heading) {
  if (heading > 337.5 || heading <= 22.5)
    return "N";
  else if (heading > 22.5 && heading <= 67.5)
    return "NE";
  else if (heading > 67.5 && heading <= 112.5)
    return "E";
  else if (heading > 112.5 && heading <= 157.5)
    return "SE";
  else if (heading > 157.5 && heading <= 202.5)
    return "S";
  else if (heading > 202.5 && heading <= 247.5)
    return "SW";
  else if (heading > 247.5 && heading <= 292.5)
    return "W";
  else if (heading > 292.5 && heading <= 337.5)
    return "NW";
}

// Saves a compass offset for zeroing
void zeroCompass() {
  compassOffset = 0;
  compassOffset = getHeading();
  EEPROM.writeFloat(COMPASS_OFFSET_ADDRESS, compassOffset);
}

// Writes the compass to the screen
void outputHeadingLine(float heading) {
  String degree = String(int(heading)) + char(DEGREE);
  String cardinal = degreeToCardinal(heading);

  centerText(cardinal, 8);
  centerText(degree, 8);
}



//////////////////////////////
// Text centering helpers
//////////////////////////////

// Centers an integer
void centerText(int num, int width) {
  String text = String(num);
  centerString(text, width, false, NULL);
}

// Centers a float
void centerText(float num, int decimals, int width) {
  String text = floatToString(num, decimals);
  centerString(text, width, false, NULL);
}

// Centers a string
void centerText(String text, int width) {
  centerString(text, width, false, NULL);
}

// Centers a string
void centerText(String text, int width, bool prependWhiteSpace) {
  centerString(text, width, prependWhiteSpace, NULL);
}

// Centers a string with a special character on the end
void centerText(String text, int width, bool prependWhiteSpace, char specialChar) {
  centerString(text, width, prependWhiteSpace, specialChar);
}

// Converts a floating point number to a string
String floatToString(float num, int decimals) {
  int wholePart = int(num);
  int precision = pow(10, decimals);
  int fractionalPart = abs(num - wholePart) * precision;
  String text = String(wholePart);
  text += ".";
  text += String(fractionalPart);

  return text;
}

// Takes a string and outputs it centered within the given width
void centerString(String text, int width, bool prependWhiteSpace, char specialChar) {
  int textLength = text.length();
  if (specialChar != NULL) {
    textLength += 1;
  }
  int totalWhiteSpace = width - textLength;
  int left = totalWhiteSpace / 2;
  int right = left;

  // Should we put any odd whitespace at the beginning or the endminmax
  if (prependWhiteSpace) {
    left += (totalWhiteSpace % 2);
  } else {
    right += (totalWhiteSpace % 2);
  }

  for (int i=0; i<left; i++) {
    lcd.print(" ");
  }

  lcd.print(text);
  if (specialChar != NULL) {
    lcd.write(specialChar);
  }

  for (int i=0; i<right; i++) {
    lcd.print(" ");
  }
}

