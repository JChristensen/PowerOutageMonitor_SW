//log a new outage if one occurred.
//initializes the log data structure in the RTC SRAM if needed.
uint8_t logOutage(void)
{
uint8_t nOutage;              //number of outages stored in sram
uint8_t nextOutage;           //address of next outage timestamps in sram
uint8_t outageAddr;           //outage address in sram
time_t powerDown, powerUp;    //power outage timestamps

    if (!logExists()) logInit();
    
    //if an outage has occurred, record it
    nOutage = RTC.sramRead(NBR_OUTAGES_ADDR);
    if ( RTC.powerFail(&powerDown, &powerUp) ) {
        nextOutage = RTC.sramRead(NEXT_OUTAGE_ADDR);
        write32(nextOutage, powerDown);
        write32(nextOutage + 4, powerUp);
        nextOutage += OUTAGE_LENGTH;
        if (nextOutage > MAX_OUTAGE_ADDR) nextOutage = FIRST_OUTAGE_ADDR;
        RTC.sramWrite(NEXT_OUTAGE_ADDR, nextOutage);
        if (nOutage < MAX_OUTAGES) RTC.sramWrite(NBR_OUTAGES_ADDR, ++nOutage);
        digitalWrite(ALERT_LED, HIGH);        //turn on the LED to alert the user
    }
    return nOutage;    //return number of outages logged
}

//test whether the log structure is already set up
boolean logExists(void)
{
    uint32_t loID;                //lower half of the unique ID read from sram
    uint8_t appID;                //app ID read from sram

    RTC.idRead(rtcID.b);          //get the RTC's ID
    loID = read32(RTC_ID_LO);     //if already initialized, the lower half of the ID is stored at SRAM addr 0x00,
    appID = RTC.sramRead(APP_ID_ADDR);     //and the app ID (1) is at addr 0x04.
    return (loID == rtcID.lo) && (appID == 1);
}

//initialize the log structure
void logInit(void)
{
    RTC.idRead(rtcID.b);                                //get the RTC's ID
    write32(RTC_ID_LO, rtcID.lo);                       //least significant half of the RTC unique ID
    RTC.sramWrite(APP_ID_ADDR, APP_ID);                 //app ID
    RTC.sramWrite(NBR_OUTAGES_ADDR, 0);                 //number of outages
    RTC.sramWrite(NEXT_OUTAGE_ADDR, FIRST_OUTAGE_ADDR); //next location for outage times
    tzIndex = RTC.sramRead(TZ_INDEX_ADDR);              //tz index, init to first in list if not valid value
    if ( tzIndex >= sizeof(tzNames)/sizeof(tzNames[0]) ) {    //valid value?
        tzIndex = 0;                                          //no, use first TZ in the list
        RTC.sramWrite(TZ_INDEX_ADDR, tzIndex);
    }
    tz = timezones[tzIndex];                            //set the tz

    nOutage = 0;
    digitalWrite(ALERT_LED, LOW);                       //ensure the LED is off
    lcd.clear();
    lcd << F("Log initialized");
    delay(MSG_DELAY);
    lcd.clear();
}

//write a time_t or other uint32_t value to sram starting at addr
void write32(uint8_t addr, uint32_t t)
{
    union {
        uint8_t b[4];
        uint32_t t;
    } i;

    i.t = t;
    RTC.sramWrite(addr, i.b, 4);
}

//read a time_t or other uint32_t value from sram starting at addr
time_t read32(uint8_t addr)
{
    union {
        uint8_t b[4];
        time_t t;
    } i;

    RTC.sramRead(addr, i.b, 4);
    return i.t;
}

//destroy the logging data structure and log data
void logClear(void)
{
    for (uint8_t i=0; i<MAX_OUTAGE_ADDR + OUTAGE_LENGTH; i++) {
        RTC.sramWrite(i, 0);
    }
}
