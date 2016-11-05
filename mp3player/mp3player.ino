// #include <EEPROM.h>
#include <Arduino.h>  // for type definitions
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <avr/pgmspace.h>

// strings
const char ERR_NO_VS1053[] PROGMEM          = " No VS1053";
const char ERR_BMP_NOT_RECOGNIZED[] PROGMEM = " BMP format not recognized.";
const char ERR_FILE_NOT_FOUND[] PROGMEM     = " File not found:";
const char ERR_NO_SD_CARD[] PROGMEM         = " SD not found!";
const char ERR_DREQ_PIN[] PROGMEM           = " DREQ not an interrupt!";

const char ERR_NO_ROOT_DIR[] PROGMEM        = " rootDir not initialized";
const char MSG_NO_CURR_DIR[] PROGMEM        = " no currentDir";
const char MSG_LOADING[] PROGMEM            = " Loading:";
const char MSG_DEBUG[] PROGMEM              = "DEBUG:";
const char MSG_FATAL[] PROGMEM              = "FATAL:";

const char MSG_LOOP_START[] PROGMEM         = "======";
const char MSG_PATH[] PROGMEM               = " path: ";
const char MSG_LAST_TRACK[] PROGMEM         = " Last track";
const char MSG_IGNORE[] PROGMEM             = " Ignoring: ";
const char MSG_NO_DIRS[] PROGMEM            = " No dirs";
const char MSG_REWINDING[] PROGMEM          = " rew dir ";
const char MSG_VALID_ALBUM[] PROGMEM        = " val album ";
const char MSG_CLOSING[] PROGMEM            = " closing ";
const char MSG_IGNORING[] PROGMEM           = " Ignoring ";
const char MSG_NO_CURRENT_DIR[] PROGMEM     = " no current dir ";
const char ERR_OPEN_FILE[] PROGMEM          = " Could not open file ";
const char MSG_START_PLAY[] PROGMEM         = " Start playing ";
const char MSG_STOP_PLAY[] PROGMEM          = " Stop playing ";
const char MSG_BUTTON[] PROGMEM             = " Button: ";

// For the breakout, you can use any 2 or 3 pins
// These pins will also work for the 1.8" TFT shield
#define TFT_CS     10
#define TFT_RST    0  // you can also connect this to the Arduino reset
                      // in which case, set this #define pin to 0!
#define TFT_DC     8

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7       // VS1053 chip select pin (output)
#define SHIELD_DCS    6       // VS1053 Data/command select pin (output)

// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin

// my own pins
#define BUTTON_GPIO_TRACK_FWD  1
#define BUTTON_GPIO_TRACK_BWD  2
#define BUTTON_GPIO_ALBUM_FWD  3
#define BUTTON_GPIO_ALBUM_BWD  4

File rootDir;
File currentDir;
byte buttonPressed;

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// create mp3 maker shield object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

void setup() {  
  Serial.begin(9600);
  delay(100);  

  tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab
  delay(500);
  tftInitScreen();
  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     mp3player_fatal(__LINE__, ERR_NO_VS1053);
  }  

  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    mp3player_fatal(__LINE__, ERR_NO_SD_CARD);
  }
  rootDir = SD.open("/");
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);
  
  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT)) {
    mp3player_fatal(__LINE__, ERR_DREQ_PIN);
  }
  resetButtons();
  // setup pins
  musicPlayer.GPIO_pinMode(BUTTON_GPIO_TRACK_FWD, INPUT);
}

void loop() {
  mp3player_dbg(__LINE__, MSG_LOOP_START);
  //int bt = getPressedButton();
  //char* trackname = updateCurrentAlbumTrack(BT_NONE);
  
  if (musicPlayer.playingMusic) {
    mp3player_dbg(__LINE__, MSG_STOP_PLAY, musicPlayer.currentTrack.name());
    musicPlayer.stopPlaying();
  }
  const char* trackpath = getNextTrackpath();
  mp3player_dbg(__LINE__, MSG_START_PLAY, trackpath);
  if (! musicPlayer.startPlayingFile(trackpath)) {
    mp3player_fatal(__LINE__, ERR_OPEN_FILE, trackpath);
  }  
  
  resetButtons();
  
  // file is now playing in the 'background' so now's a good time
  // to do something else like handling LEDs or buttons :)
  // wait for a button
  while (buttonPressed == 0 && musicPlayer.playingMusic) {
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_TRACK_FWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_TRACK_FWD;
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_TRACK_BWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_TRACK_BWD;
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_ALBUM_FWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_ALBUM_FWD;
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_ALBUM_BWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_ALBUM_BWD;
    }
  }    
  if (buttonPressed > 0) {
    char c[] = {'-','\0'};
    c[0] = buttonPressed + 48;
    mp3player_dbg(__LINE__, MSG_BUTTON, &c[0]);
    // do Action
    resetButtons();
  }
  //updateCurrentAlbumTrack(bt);    
  delay(250); 
}

void resetButtons() {
  buttonPressed = 0;
}

const char* getNextTrackpath() {
  static char trackpath[22];
  trackpath[0] = '/';

  if (!rootDir) {
    mp3player_fatal(__LINE__, ERR_NO_ROOT_DIR);
  }
  
  if (!currentDir) {
    mp3player_dbg(__LINE__, MSG_NO_CURRENT_DIR);
  } else {
    // search next track in current dir
    while (true) {
      File t = currentDir.openNextFile();
      if (!t) {
        // last file reached
        mp3player_dbg(__LINE__, MSG_LAST_TRACK); //  Simulate next album button press
        break;
      } else if (EndsWithMp3(t.name())) {       
        strcpy(&trackpath[1], currentDir.name());
        size_t len = strlen(trackpath);
        trackpath[len] = '/';
        strcpy(&trackpath[len+1], t.name());
        mp3player_dbg(__LINE__, MSG_PATH, trackpath);
        t.close();
        return trackpath;
      } else {
        mp3player_dbg(__LINE__, MSG_IGNORE, t.name());
        t.close();
      }
    }
  }

  bool last = false;
  while (true) {      
    File d = rootDir.openNextFile();
    if (!d) {
      // none found or last
      if (last == true) {
        mp3player_fatal(__LINE__, MSG_NO_DIRS);
      }
      mp3player_dbg(__LINE__, MSG_REWINDING, rootDir.name());
      rootDir.rewindDirectory();
      last = true;        
    } else if (d.isDirectory() && strcmp(d.name(), "SYSTEM~1") != 0) {
       // found a valid directory
       strcpy(&trackpath[1], d.name());
       mp3player_dbg(__LINE__, MSG_VALID_ALBUM, d.name());
       if (!(!currentDir)) {
         mp3player_dbg(__LINE__, MSG_CLOSING, currentDir.name());
         currentDir.close();
       }       
       currentDir = d;
       bmpDrawCover(currentDir);
       return getNextTrackpath();
       break;
    } else {
      mp3player_dbg(__LINE__, MSG_IGNORING, d.name());
      d.close();      
    }
  }
}

void bmpDrawCover(File dir) {
  char coverpath[20];
  coverpath[0] = '/';
  strcpy(&coverpath[1], dir.name());
  size_t len = strlen(dir.name());
  coverpath[len+1] = '/';
  strcpy(&coverpath[len+2], "cover.bmp");
  bmpDraw(coverpath, 0, 0);
}

void tftInitScreen() {  
  tft.fillScreen(ST7735_BLACK);
  tft.fillRoundRect(25, 10, 78, 60, 8, ST7735_WHITE);
  tft.fillTriangle(42, 20, 42, 60, 90, 40, ST7735_RED);
}

void testdrawtext(const char *text, uint16_t color) {
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(text);
}

void mp3player_fatal(const int numb, const char msg[]) {
  mp3player_dbg(numb, msg);
  while(1);
}

void mp3player_fatal(const int numb, const char msg[], const char *param) {
  mp3player_dbg(numb, msg, param);
  while(1);
}

void mp3player_dbg(const int lineno, const char msg[]) {
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // alle chars lesen
    Serial.write(c);   // und ausgeben
  }
  Serial.write('\n');  // neue Zeile
}

void mp3player_dbg(const int lineno, const char msg[], const char *param) {
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // alle chars lesen
    Serial.write(c);   // und ausgeben
  }
  Serial.println(param);  // neue Zeile
}

/*int getPressedButton() {
  float btTrack = analogRead(BUTTON_TRACK_PIN);
  float btAlbum = analogRead(BUTTON_ALBUM_PIN);
  if (btTrack < 1020) {
    Serial.print("[DEBUG] Track V:");
    Serial.println(btTrack);
    //Serial.print("[DEBUG] Album V:");
    //Serial.println(btAlbum);
  }
  if (btTrack > 1000) {
    return BT_NONE;
  } else if (btTrack > 325) {
    return BT_TRACK_FWD;
  } else if (btTrack > 90) {
    return BT_TRACK_BWD;
  }

    if (btAlbum > 1000) {
    return BT_NONE;
  } else if (btAlbum > 325) {
    return BT_ALBUM_FWD;
  } else if (btAlbum > 90) {
    return BT_ALBUM_BWD;
  }
}*/

/*char* updateCurrentAlbumTrack(int bt) {
  char* trackname = "test";
if (bt != BT_NONE) {
    if (lastButton != bt) {
    lastButton = bt;
  
    switch (bt) {
      case BT_TRACK_FWD:
      testdrawtext(">", ST7735_WHITE);
      Serial.println(">");
      configuration.track++;
      if (configuration.track >= maxTrack[configuration.album]) {
        configuration.track = 0;
      }
      break;
      case BT_TRACK_BWD:
      testdrawtext("<", ST7735_WHITE);
      Serial.println("<");
      configuration.track--;
      if (configuration.track < 0) {
        configuration.track = maxTrack[configuration.album] - 1;
      }
      break;
      case BT_ALBUM_FWD:
      testdrawtext(">|", ST7735_WHITE);
      Serial.println(">|");
      configuration.album++;
      if (configuration.album >= maxAlbum) {
        configuration.album = 0;
      }
      configuration.track = 0;
      break;
      case BT_ALBUM_BWD:
      testdrawtext("|<", ST7735_WHITE);
      Serial.println("|<");
      configuration.album--;
      if (configuration.album < 0) {
        configuration.album = maxAlbum - 1;
      }
      configuration.track = 0;
      break;    
    }    
    Serial.print(configuration.album);
    Serial.print("/");
    Serial.println(configuration.track);

    EEPROM_writeAnything(0, configuration);
    }
  } else {
    lastButton = BT_NONE;
  }
  return trackname;
}*/

int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr) {
        return 0;
    }
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int EndsWithMp3(const char *str) { return EndsWith(str, ".MP3"); }

/* copied from Example */
// This function opens a Windows Bitmap (BMP) file and
// displays it at the given coordinates.  It's sped up
// by reading many pixels worth of data at a time
// (rather than pixel by pixel).  Increasing the buffer
// size takes more of the Arduino's precious RAM but
// makes loading a little faster.  20 pixels seems a
// good balance.

#define BUFFPIXEL 20

void bmpDraw( char *filename, uint8_t x, uint8_t y) {

  File     bmpFile;
  int      bmpWidth, bmpHeight;   // W+H in pixels
  uint8_t  bmpDepth;              // Bit depth (currently must be 24)
  uint32_t bmpImageoffset;        // Start of image data in file
  uint32_t rowSize;               // Not always = bmpWidth; may have padding
  uint8_t  sdbuffer[3*BUFFPIXEL]; // pixel buffer (R+G+B per pixel)
  uint8_t  buffidx = sizeof(sdbuffer); // Current position in sdbuffer
  boolean  goodBmp = false;       // Set to true on valid header parse
  boolean  flip    = true;        // BMP is stored bottom-to-top
  int      w, h, row, col;
  uint8_t  r, g, b;
  uint32_t pos = 0;

  if((x >= tft.width()) || (y >= tft.height())) return;

  mp3player_dbg(__LINE__, MSG_LOADING, filename);

  // Open requested file on SD card
  bmpFile = SD.open(filename);
  if (!bmpFile) {
    mp3player_dbg(__LINE__, ERR_FILE_NOT_FOUND, filename);
    return;
  }

  // Parse BMP header
  if(read16(bmpFile) == 0x4D42) { // BMP signature
    // Read & ignore file size
    (void)read32(bmpFile);
    (void)read32(bmpFile); // Read & ignore creator bytes
    bmpImageoffset = read32(bmpFile); // Start of image data
    //Serial.print("Image Offset: "); Serial.println(bmpImageoffset, DEC);
    // Read & ignore DIB header
    (void)read32(bmpFile);
    bmpWidth  = read32(bmpFile);
    bmpHeight = read32(bmpFile);
    if(read16(bmpFile) == 1) { // # planes -- must be '1'
      bmpDepth = read16(bmpFile); // bits per pixel
      //Serial.print("Bit Depth: "); Serial.println(bmpDepth);
      if((bmpDepth == 24) && (read32(bmpFile) == 0)) { // 0 = uncompressed

        goodBmp = true; // Supported BMP format -- proceed!
        /*Serial.print("Image size: ");
        Serial.print(bmpWidth);
        Serial.print('x');*/

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      = false;
        }

        // Crop area to be loaded
        w = bmpWidth;
        h = bmpHeight;
        if((x+w-1) >= tft.width())  w = tft.width()  - x;
        if((y+h-1) >= tft.height()) h = tft.height() - y;

        // Set TFT address window to clipped image bounds
        tft.setAddrWindow(x, y, x+w-1, y+h-1);

        for (row=0; row<h; row++) { // For each scanline...

          // Seek to start of scan line.  It might seem labor-
          // intensive to be doing this on every line, but this
          // method covers a lot of gritty details like cropping
          // and scanline padding.  Also, the seek only takes
          // place if the file position actually needs to change
          // (avoids a lot of cluster math in SD library).
          if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
            pos = bmpImageoffset + (bmpHeight - 1 - row) * rowSize;
          else     // Bitmap is stored top-to-bottom
            pos = bmpImageoffset + row * rowSize;
          if(bmpFile.position() != pos) { // Need seek?
            bmpFile.seek(pos);
            buffidx = sizeof(sdbuffer); // Force buffer reload
          }

          for (col=0; col<w; col++) { // For each pixel...
            // Time to read more pixel data?
            if (buffidx >= sizeof(sdbuffer)) { // Indeed
              bmpFile.read(sdbuffer, sizeof(sdbuffer));
              buffidx = 0; // Set index to beginning
            }

            // Convert pixel from BMP to TFT format, push to display
            b = sdbuffer[buffidx++];
            g = sdbuffer[buffidx++];
            r = sdbuffer[buffidx++];
            tft.pushColor(tft.Color565(r,g,b));
          } // end pixel
        } // end scanline
      } // end goodBmp
    }
  }

  bmpFile.close();
  if(!goodBmp) mp3player_dbg(__LINE__, ERR_BMP_NOT_RECOGNIZED);
}


uint16_t read16(File f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(File f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

