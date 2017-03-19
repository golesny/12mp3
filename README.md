# 12mp3
Arduino MP3 player for kidz. (Inspired by Hörbert which can only play WAV and 9 albums and has no display)
No text for small kidz -> TFT to display a picture of the album.
Buttons:
- Track Fwd
- Track Bwd
- Album Fwd
- Album Bwd
Actions:
- Volume +: Album Bwd + Track Bwd + Album Fwd
- Volume -: Album Bwd + Track Bwd + Track Fwd
- Shutdown: Album Bwd + Album Fwd

## Hardware
 - Arduino Uno R3
 - Adafruit Music Maker Shield
 - Adafruit 1.44" Color TFT with Micro SD Socket
 - Pololu Mini Pushbutton Power Switch with Reverse Voltage Protection, SV
 - Adafruit PowerBoost 500 Charger - Rechargeable 5V Lipo USB Boost @ 500mA+
 - LiPo Akku 2000mAh (3.7 V, 2 mm JST)
 - some buttons
 - any Micro SD card

## Code
https://github.com/golesny/12mp3/tree/master/mp3player

## Links and References
 - https://learn.adafruit.com/adafruit-music-maker-shield-vs1053-mp3-wav-wave-ogg-vorbis-player/library-reference
 - https://learn.adafruit.com/adafruit-1-44-color-tft-with-micro-sd-socket/wiring-and-test
 - https://www.arduino.cc/en/Reference/SD
 - https://github.com/adafruit/Adafruit_VS1053_Library/blob/master/Adafruit_VS1053.h
 - https://www.hoerbert.com/
 
## Problems
 - Noise with connected speakers
 https://forums.adafruit.com/viewtopic.php?f=22&t=108913
 Solution: Replacement by Adafruit. Thanks.