//MCP79412 RTC Power Outage Logger with LCD display.
//Developed with Arduino v1.0.1.

#include <Button.h>               //http://github.com/JChristensen/Button
#include <LiquidCrystal.h>        //http://arduino.cc/en/Reference/LiquidCrystal (included with Arduino IDE)
#include <MCP79412RTC.h>          //https://github.com/JChristensen/Timezone
#include <movingAvg.h>            //https://github.com/JChristensen/movingAvg
#include <Streaming.h>            //http://arduiniana.org/libraries/streaming/
#include <Time.h>                 //http://www.arduino.cc/playground/Code/Time
#include <Timezone.h>             //https://github.com/JChristensen/Timezone
#include <Wire.h>                 //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)

//pin definitions and other constants
#define LCD_RS 2                  //16x2 LCD display
#define LCD_EN A3
#define LCD_D4 A1
#define LCD_D5 A0
#define LCD_D6 13
#define LCD_D7 12
#define RTC_POWER 3
#define DS18B20 6                 //and/or external sensor
#define DN_BUTTON 7
#define ALERT_LED 8
#define BACKLIGHT_PIN 9
#define UP_BUTTON 10
#define SET_BUTTON 11
#define PHOTOCELL_PIN A2
#define DEBOUNCE_TIME 25
#define REPEAT_FIRST 600          //ms required before repeating on long press
#define REPEAT_INCR 200           //repeat interval for long press
#define LONG_PRESS 1000           //long button press, ms
#define DOUBLE_PRESS 500          //ms to detect two buttons pressed simultaneously
#define DISP_TIMEOUT 30000        //idle timeout to return to clock display from outage display
#define MSG_DELAY 2000            //ms to delay when displaying feedback messages

//logger definitions
#define FIRST_OUTAGE_ADDR 0x08    //address of first outage timestamps in RTC SRAM
#define OUTAGE_LENGTH 8           //8 data bytes for each outage (start and end timestamps, both are time_t values)
#define MAX_OUTAGES 7             //maximum number of outage timestamp pairs that can be stored in SRAM
#define MAX_OUTAGE_ADDR FIRST_OUTAGE_ADDR + OUTAGE_LENGTH * (MAX_OUTAGES - 1)    //last outage address
#define APP_ID 1                  //APP_ID and 4 bytes of the RTC ID are stored in sram to provide
                                  //a way to recognize that the logging data structure has been initialized
#define RTC_ID_LO 0x00            //lower 4 bytes of RTC unique ID are stored at sram addr 0x00
#define APP_ID_ADDR 0x04          //address of appID (1)
#define NBR_OUTAGES_ADDR 0x05     //address containing number of outages currently stored in SRAM
#define NEXT_OUTAGE_ADDR 0x06     //address containing pointer to next outage
#define TZ_INDEX_ADDR 0x07        //address containing timezone index

//8-byte RTC "unique ID" with access to upper and lower halves
union {
    uint8_t b[8];
    struct {
        uint32_t hi;
        uint32_t lo;
    };
} rtcID;

//Continental US Time Zones
TimeChangeRule EDT = {"EDT", Second, Sun, Mar, 2, -240};    //Daylight time = UTC - 4 hours
TimeChangeRule EST = {"EST", First, Sun, Nov, 2, -300};     //Standard time = UTC - 5 hours
Timezone Eastern(EDT, EST);
TimeChangeRule CDT = {"CDT", Second, Sun, Mar, 2, -300};    //Daylight time = UTC - 5 hours
TimeChangeRule CST = {"CST", First, Sun, Nov, 2, -360};     //Standard time = UTC - 6 hours
Timezone Central(CDT, CST);
TimeChangeRule MDT = {"MDT", Second, Sun, Mar, 2, -360};    //Daylight time = UTC - 6 hours
TimeChangeRule MST = {"MST", First, Sun, Nov, 2, -420};     //Standard time = UTC - 7 hours
Timezone Mountain(MDT, MST);
TimeChangeRule PDT = {"PDT", Second, Sun, Mar, 2, -420};    //Daylight time = UTC - 7 hours
TimeChangeRule PST = {"PST", First, Sun, Nov, 2, -480};     //Standard time = UTC - 8 hours
Timezone Pacific(PDT, PST);
TimeChangeRule utcRule = {"UTC", First, Sun, Nov, 2, 0};    //No change for UTC
Timezone UTC(utcRule, utcRule);
Timezone *timezones[] = { &Eastern, &Central, &Mountain, &Pacific, &UTC };
Timezone *tz;               //pointer to the time zone
uint8_t tzIndex;            //index to the timezones[] array (persisted in RTC SRAM)
char *tzNames[] = { "Eastern", "Central", "Mountain", "Pacific", "UTC  " };
TimeChangeRule *tcr;        //pointer to the time change rule, use to get TZ abbrev
time_t utc, local, lastUTC, tSet;
const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
tmElements_t tmSet;

LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
movingAvg photoCell;

Button btnSet = Button(SET_BUTTON, true, true, DEBOUNCE_TIME);
Button btnUp = Button(UP_BUTTON, true, true, DEBOUNCE_TIME);
Button btnDn = Button(DN_BUTTON, true, true, DEBOUNCE_TIME);

uint8_t nOutage;                 //number of outages stored in sram
int8_t outageNbr;                //number of the displayed outage
unsigned long ms, msLastPress;

void setup(void)
{
    //Serial.begin(115200);
    digitalWrite(PHOTOCELL_PIN, HIGH);      //turn on pullup resistor
    pinMode(ALERT_LED, OUTPUT);
    pinMode(RTC_POWER, OUTPUT);
    delay(100);
    digitalWrite(RTC_POWER, HIGH);
    delay(100);
    
    //splash screen
    lcd.begin(16, 2);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd << F("  Power Outage");
    lcd.setCursor(0,1);
    lcd << F("  Logger  v1.0");
    analogWrite(BACKLIGHT_PIN, 255);        //backlight full on
    digitalWrite(ALERT_LED, HIGH);          //lamp test
    delay(MSG_DELAY);
    digitalWrite(ALERT_LED, LOW);
    
    //get tz index from RTC SRAM
    tzIndex = RTC.sramRead(TZ_INDEX_ADDR);
    if ( tzIndex >= sizeof(tzNames)/sizeof(tzNames[0]) ) {    //valid value?
        tzIndex = 0;                                          //no, use first TZ in the list
        RTC.sramWrite(TZ_INDEX_ADDR, tzIndex);
    }
    tz = timezones[tzIndex];                                  //set the tz
    
    //set up RTC synchronization
    setSyncProvider(RTC.get);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd << F("RTC SYNC");
    if (timeStatus() != timeSet) {
        lcd << F(" FAIL");
        digitalWrite(ALERT_LED, HIGH);
        while (1) {
            digitalWrite( ALERT_LED, !digitalRead(ALERT_LED));
            delay(1000);
        }
    }

    //RTC unique ID
    RTC.idRead(rtcID.b);
    lcd.setCursor(0, 1);
    for (uint8_t i=0; i<8; i++) lcd << (rtcID.b[i] < 16 ? "0" : "" ) << _HEX(rtcID.b[i]);
    delay(MSG_DELAY);
    while (btnSet.read());    //user can hold the message by holding the set button
    lcd.clear();

    nOutage = logOutage();    //log an outage if one occurred
}

uint8_t STATE;                //current state machine state
enum {RUN, DISP_OUTAGE, SET_START, SET_TZ, SET_CALIB, SET_YR, SET_MON, SET_DAY, SET_HR, SET_MIN, SET_SEC, SET_END};    //state machine states

void loop(void)
{
    int yr, days;
    int calib, newCalib;

    ms = millis();
    btnSet.read();
    btnUp.read();
    btnDn.read();
    
    switch (STATE) {
        
        case RUN:
            if (btnSet.wasReleased()) {
                STATE = SET_START;
                break;
            }
            else if (btnSet.pressedFor(LONG_PRESS)) {
                logInit();
                while (btnSet.read());                //wait for button to be released
                break;
            }
            else if (btnDn.wasReleased()) {
                msLastPress = btnDn.lastChange();
                outageNbr = nOutage;
                STATE = displayOutage(outageNbr);
                break;
            }
            else if (btnUp.wasReleased()) {
                msLastPress = btnUp.lastChange();
                outageNbr = 1;
                STATE = displayOutage(outageNbr);
                break;
            }

            utc = now();
            if (utc != lastUTC) {
                lastUTC = utc;
                local = (*tz).toLocal(utc, &tcr);
                lcdDateTime();
                brAdjust();
            }
            break;
            
        case DISP_OUTAGE:
            if (btnSet.wasReleased() || ms - msLastPress >= DISP_TIMEOUT) {
                lcd.clear();
                STATE = RUN;
            }            
            else if (btnDn.wasReleased()) {
                msLastPress = btnDn.lastChange();
                if ( --outageNbr < 1 ) outageNbr = nOutage;
                STATE = displayOutage(outageNbr);
            }
            else if (btnUp.wasReleased()) {
                msLastPress = btnUp.lastChange();
                if ( ++outageNbr > nOutage ) outageNbr = 1;
                STATE = displayOutage(outageNbr);
            }
            else if (btnSet.pressedFor(LONG_PRESS)) {
                logInit();
                while (btnSet.read());                //wait for button to be released
                STATE = RUN;
                break;
            }
            break;
            
        case SET_START:
            lcd.clear();
            STATE = SET_TZ;
            break;
            
        case SET_TZ:
            STATE = SET_CALIB;
            tzIndex = setVal("Timezone: ", tzIndex, 0, sizeof(tzNames)/sizeof(tzNames[0]) - 1, 0);
            if (STATE == RUN) break;
            tz = timezones[tzIndex];
            RTC.sramWrite(TZ_INDEX_ADDR, tzIndex);    //save it
            break;
            
        case SET_CALIB:
            STATE = SET_YR;
            lcd.clear();
            calib = RTC.calibRead();
            newCalib = setVal("Calibration:", calib, -127, 127, 0);
            if (STATE == RUN) break;
            if (newCalib != calib) RTC.calibWrite(newCalib);
            break;

        case SET_YR:
            STATE = SET_MON;
            lcd.clear();
            yr = setVal("Year: ", year(local), 2000, 2100, 0);
            if (STATE == RUN) break;
            tmSet.Year = CalendarYrToTm(yr);
            break;
        
        case SET_MON:
            STATE = SET_DAY;
            tmSet.Month = setVal("Month:", month(local), 1, 12, 5);
            if (STATE == RUN) break;
            break;
        
        case SET_DAY:
            STATE = SET_HR;
            days = monthDays[tmSet.Month - 1];
            if (tmSet.Month == 2 && isLeap(yr)) days++;
            tmSet.Day = setVal("Day:  ", day(local), 1, days, 9);
            if (STATE == RUN) break;
            break;
        
        case SET_HR:
            STATE = SET_MIN;
            lcd.clear();
            tmSet.Hour = setVal("Hour:  ", hour((*tz).toLocal(now())), 0, 23, 0);
            if (STATE == RUN) break;
            lcd.setCursor(2, 1);
            lcd << ':';
            break;
        
        case SET_MIN:
            STATE = SET_SEC;
            tmSet.Minute = setVal("Minute: ", minute((*tz).toLocal(now())), 0, 59, 3);
            if (STATE == RUN) break;
            lcd.setCursor(5, 1);
            lcd << ':';
            break;
        
        case SET_SEC:
            STATE = SET_END;
            tmSet.Second = setVal("Second: ", second((*tz).toLocal(now())), 0, 59, 6);
            if (STATE == RUN) break;
            break;
        
        case SET_END:            
            tSet = makeTime(tmSet);
            setTime((*tz).toUTC(tSet));
            RTC.set(now());
            STATE = RUN;
            lcd.clear();
            break;
    }        
}

enum {VAL_MONTH, VAL_DAY, VAL_CALIB, VAL_TZ, VAL_SEC, VAL_OTHER};
uint8_t setType;
enum {WAIT, INCR, DECR, ZERO};

//prompt the user to set a value
int setVal(char *tag, int val, int minVal, int maxVal, uint8_t pos)
{
    uint8_t VAL_STATE;
    unsigned long rpt = REPEAT_FIRST;
    
    if (strncmp(tag, "Mo", 2) == 0) setType = VAL_MONTH;
    else if (strncmp(tag, "Da", 2) == 0) setType = VAL_DAY;
    else if (strncmp(tag, "Ca", 2) == 0) setType = VAL_CALIB;
    else if (strncmp(tag, "Ti", 2) == 0) setType = VAL_TZ;
    else if (strncmp(tag, "Se", 2) == 0) setType = VAL_SEC;
    else setType = VAL_OTHER;

    lcd.setCursor(0, 0);
    lcd << F("Set ") << tag;
    lcd.setCursor(pos, 1);
    dispVal(val);

    VAL_STATE = WAIT;

    while ( !btnSet.wasReleased() ) {
        btnSet.read();
        btnUp.read();
        btnDn.read();
        
        if ( btnSet.pressedFor(LONG_PRESS) ) {
            lcd.clear();
            lcd << F("Set Canceled");
            while (btnSet.read());    //wait for button to be released
            delay(MSG_DELAY);
            lcd.clear();
            STATE = RUN;
            return 0;
        }

        switch (VAL_STATE) {
            
            case WAIT:                                //wait for a button event
                if (btnUp.wasPressed())
                    VAL_STATE = INCR;
                else if (btnDn.wasPressed())
                    VAL_STATE = DECR;
                else if (btnUp.wasReleased())         //reset the long press interval
                    rpt = REPEAT_FIRST;
                else if (btnDn.wasReleased())
                    rpt = REPEAT_FIRST;
                else if ((setType == VAL_CALIB || setType == VAL_SEC) && btnUp.pressedFor(DOUBLE_PRESS) && btnDn.pressedFor(DOUBLE_PRESS))
                    VAL_STATE = ZERO;
                else if (btnUp.pressedFor(rpt)) {     //check for long press
                    rpt += REPEAT_INCR;               //increment the long press interval
                    VAL_STATE = INCR;
                }
                else if (btnDn.pressedFor(rpt)) {
                    rpt += REPEAT_INCR;
                    VAL_STATE = DECR;
                }
                break;
    
            case INCR:                                //increment the counter
                if (++val > maxVal) val = minVal;     //wrap if max exceeded
                VAL_STATE = WAIT;
                lcd.setCursor(pos, 1);
                dispVal(val);
                break;
    
            case DECR:                                //decrement the counter
                if (--val < minVal) val = maxVal;     //wrap if min exceeded
                VAL_STATE = WAIT;
                lcd.setCursor(pos, 1);
                dispVal(val);
                break;
    
            case ZERO:                                //zero the value
                val = 0;
                VAL_STATE = WAIT;
                lcd.setCursor(pos, 1);
                dispVal(val);
                while (btnUp.read() || btnDn.read()); //wait for both buttons to be released
                break;
        }
    }
    return val;
}

//display values in different formats for setting various fields
void dispVal(int val)
{
    switch (setType) {
        
        case VAL_MONTH:
            lcd << monthShortStr(val);
            break;
        
        case VAL_DAY:
            tmSet.Day = val;
            tSet = makeTime(tmSet);        
            printI00(lcd, val, ' ');
            lcd << dayShortStr(weekday(tSet));
            break;
        
        case VAL_CALIB:
            lcd << _DEC(val) << F("   ");
            break;
        
        case VAL_TZ:
            lcd << tzNames[val] << F("    ");
            break;

        case VAL_SEC:
        case VAL_OTHER:
            printI00(lcd, val, ' ');
            break;
    
    }
}

//display the given outage number, where 1 is the earliest outage.
//returns the next state machine state (RUN if there are no outages to display).
uint8_t displayOutage(int8_t outageNbr)
{
    uint8_t addr;                 //outage address in sram
    time_t powerDown, powerUp;    //power outage timestamps
    
    if (nOutage == 0) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd << F("No outages!");
        delay(MSG_DELAY);
        return RUN;
    }
    else if (outageNbr <= nOutage) {
        //find the address of the earliest outage in the log
        addr = nOutage < MAX_OUTAGES ? FIRST_OUTAGE_ADDR : RTC.sramRead(NEXT_OUTAGE_ADDR);

        //calculate the address of outage "n"
        addr = ((addr - FIRST_OUTAGE_ADDR + (outageNbr - 1) * OUTAGE_LENGTH) % (MAX_OUTAGES * OUTAGE_LENGTH)) + FIRST_OUTAGE_ADDR;
    
        lcd.clear();
        powerDown = (*tz).toLocal(read32(addr));
        powerUp = (*tz).toLocal(read32(addr + 4));
    
        lcd.setCursor(0, 0);
        lcd << _DEC(outageNbr) << F(" DN ");
        printI00(lcd, hour(powerDown), ':');
        printI00(lcd, minute(powerDown), ' ');
        printI00(lcd, day(powerDown), ' ');
        lcd.setCursor(13, 0);
        lcd << monthShortStr(month(powerDown));
    
        lcd.setCursor(0, 1);
        lcd << F("  UP ");
        printI00(lcd, hour(powerUp), ':');
        printI00(lcd, minute(powerUp), ' ');
        printI00(lcd, day(powerUp), ' ');
        lcd.setCursor(13, 1);
        lcd << monthShortStr(month(powerUp));

        digitalWrite(ALERT_LED, LOW);        //turn the alert LED off
        return DISP_OUTAGE;
    }
}
