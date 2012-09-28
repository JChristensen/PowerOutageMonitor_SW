/*----------------------------------------------------------------------*
 * Arduino sketch to implement a Power Outage Logger using the          *
 * Microchip MCP79412 RTC. Logs up to 7 outages (power down/up times)   *
 * in the RTC's SRAM.                                                   *
 *                                                                      *
 * A circuit schematic and PC board for this project is available at    *
 * https://github.com/JChristensen/PowerOutageMonitor_HW                *
 *                                                                      *
 * Jack Christensen 23Aug2012                                           *
 *                                                                      *
 * The normal display is a clock showing time, date, and the number of  *
 * power outages logged in angle brackets, e.g. <4>. After a new power  *
 * outage, the alert LED is illuminated. Viewing the outage log turns   *
 * the LED off. The clock adjusts automatically for daylight saving     *
 * time.                                                                *
 *                                                                      *
 * From the clock display:                                              *
 *                                                                      *
 * (1) Press UP or DN to view the outage log. Pressing UP will show     *
 * the first (earliest) outage, pressing UP again will show the next    *
 * outage. Pressing DN will show last (most recent) outage, pressing    *
 * DN again will show the previous outage. Press SET to return to clock *
 * mode, or it will automatically return after 30 seconds.              *
 *                                                                      *
 * (2) Press SET to begin the set sequence. Press UP and DN to adjust   *
 * each parameter, hold to adjust rapidly. Press SET to advance to the  *
 * next parameter. Hold SET to cancel the set sequence.                 *
 *                                                                      *
 * (3) From clock mode, hold SET to clear the outage log.               *
 *                                                                      *
 * This work is licensed under the Creative Commons Attribution-        *
 * ShareAlike 3.0 Unported License. To view a copy of this license,     *
 * visit http://creativecommons.org/licenses/by-sa/3.0/ or send a       *
 * letter to Creative Commons, 171 Second Street, Suite 300,            *
 * San Francisco, California, 94105, USA.                               *
 *----------------------------------------------------------------------*/
 