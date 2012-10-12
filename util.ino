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
void lcdDateTime(void)
{
    lcd.setCursor(0, 0);
    printTime(lcd, local);
    lcd << tcr -> abbrev;
    lcd.setCursor(0, 1);
    printDate(lcd, local);

    //display the number of outages logged
    lcd.setCursor(13, 0);
    if (nOutage > 0)
        lcd << '<' << _DEC(nOutage) << '>';
    else
        lcd << F("   ");
}

//adjust lcd brightness
void brAdjust(void)
{
    int br;
    int pc = photoCell.reading(analogRead(PHOTOCELL_PIN));
    
    br = map(constrain(pc, 50, 550), 50, 550, 10, 1);
    analogWrite(BACKLIGHT_PIN, br * 255 / 10);
}

//Leap years are those divisible by 4, but not those divisible by 100,
//except that those divisble by 400 *are* leap years.
//See Kernighan & Ritchie, 2nd edition, section 2.5.
boolean isLeap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

