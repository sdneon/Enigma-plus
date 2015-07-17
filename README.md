Enigma+
=======

Enigma watch face variant for Pebble Time. (Colour, Digital).

Modified from [chadheim's](https://github.com/chadheim/pebble-watchface-slider) old black & white example.

## Display
1. 4 of 5 rows of info:
  1. Day of week (abbreviated 3 letters, uppercase).
    * This mini-bar (top-left) doubles as 'bluetooth status' indicator.
        * Coloured: connected
        * Dark: disconnected.
  2. Date (DDMM).
  3. Coloured time bar
    * Red: AM
    * Blue: PM.
  4. Year.
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

## Changelog
* 2.5
  * Added optional vibes for:
    * Bluetooth connection lost: fading vibe.
    * Hourly chirp. Default: Off, 10am to 8pm.
* v2.4
  * Weekday name (abbreviated) should now be locale specific, using strftime("%a").
* v2.3
  * Initial release.
