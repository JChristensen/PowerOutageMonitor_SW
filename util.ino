int pc;    //photocell reading (moving avg)

//print time to Serial or lcd
void printTime(Print &p, time_t t)
{
    printI00(p, hour(t), ':');
    printI00(p, minute(t), ':');
    printI00(p, second(t), ' ');
}

//print date to Serial or lcd
void printDate(Print &p, time_t t)
{
    p << dayShortStr(weekday(t)) << ' ';
    printI00(p, day(t), ' ');
    p << monthShortStr(month(t)) << ' ' << _DEC(year(t));
}

//Print an integer in "00" format (with leading zero),
//followed by a delimiter character to Serial or lcd.
//Input value assumed to be between 0 and 99.
void printI00(Print &p, int val, char delim)
{
    if (val < 10) p << '0';
    p << _DEC(val) << delim;
    return;
}

//print date, time and number of outages logged to the lcd
void lcdDateTime(uint8_t type)
{
    lcd.setCursor(0, 0);        //time on first row
    printTime(lcd, local);
    if (pcTest)
        lcd << _DEC(pc);
    else
        lcd << tcr -> abbrev;
    
    if (nOutage > 0)            //followed by number of outages
        lcd << " <" << _DEC(nOutage) << '>';
    else
        lcd << F("   ");
    
    lcd.setCursor(0, 1);        //date on second row
    switch (type / 2) {

        default:            //date
            printDate(lcd, local);
            break;

        case 1:            //sunrise
            lcd << F("Sunrise ");
            printI00(lcd, sunriseH, ':');
            printI00(lcd, sunriseM, ' ');
            lcd << F("  ");
            break;
            
        case 2:            //sunset
            lcd << F("Sunset ");
            printI00(lcd, sunsetH, ':');
            printI00(lcd, sunsetM, ' ');
            lcd << F("   ");
            break;
    }
}

//adjust lcd brightness
void brAdjust(void)
{
    int br;

    pc = photoCell.reading(analogRead(PHOTOCELL_PIN));
    br = map(constrain(pc, 50, 550), 50, 550, 10, 1);
    analogWrite(BACKLIGHT_PIN, br * 255 / 10);
}

