Enigma+
=======

Enigma watch face variant for Pebble Time. (Colour, Digital, Dual Time).

Modified from [chadheim's](https://github.com/chadheim/pebble-watchface-slider) old black & white example.

## Display
1. 4 of 5 rows of info:
  1. Day of week (abbreviated 3 letters, uppercase).
     * This mini-bar (top-left) doubles as 'bluetooth status' indicator.
       * Coloured: connected
       * Dark: disconnected.
  2. Date (DDMM or MMDD).
  3. Coloured time bar
     * Red: AM
     * Blue: PM.
  4. Year OR alternate time (new in v2.6).
     * Dual time does not handle daylight savings time. 
2. Battery-level indicator: 3rd column.
   * Top down orange bar: charging,
   * bottom-up bar: charge remaining.

### Screenshots
![screenshot 1](https://raw.githubusercontent.com/sdneon/Enigma-plus/master/store/pebble-screenshot-1-AM.png "Watch face: AM, bluetooth connected, battery not charging")
Watch face: AM, bluetooth connected, battery not charging

![screenshot 2](https://raw.githubusercontent.com/sdneon/Enigma-plus/master/store/pebble-screenshot-2-AM,DC.png "Watch face: AM, bluetooth disconnected, battery charging")
Watch face: AM, bluetooth disconnected, battery charging

![screenshot 3](https://raw.githubusercontent.com/sdneon/Enigma-plus/master/store/pebble-screenshot-3-PM,charging.png "Watch face: PM, bluetooth connected, battery charging")
Watch face: PM, bluetooth connected, battery charging

## Dev Notes
Be warned: PebbleKit JS hates "+" (plus sign) in app names (in appinfo.json) and will crash, thereby causing app configuration page to fail!

## Credits
Thanks to:
* [chadheim](https://github.com/chadheim/pebble-watchface-slider) for his old black & white enigma Pebble watch example.
* MrForms for the [time zones list](http://www.freeformatter.com/time-zone-list-html-select.html).

## Changelog
* 2.6
  * Added choice of date format (i.e. DDMM or MMDD).
  * Added dual time: display a 2nd time instead of year in 4th row.
* 2.5
  * Added optional vibes for:
    * Bluetooth connection lost: fading vibe.
    * Hourly chirp. Default: Off, 10am to 8pm.
* v2.4
  * Weekday name (abbreviated) should now be locale specific, using strftime("%a").
* v2.3
  * Initial release.
