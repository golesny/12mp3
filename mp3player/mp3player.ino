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
const char ERR_NO_FILES_FOUND[] PROGMEM     = " No files found ";
const char ERR_SEEK_FAILED[] PROGMEM        = " seek in file failed to pos ";

const char MSG_SETUP[] PROGMEM              = " setup";
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
const char MSG_NEW_INDEX[] PROGMEM          = " Creating new index in ";
const char MSG_WRITE_INDEX[] PROGMEM        = " Writing index ";
const char MSG_CLOSING_INDEX[] PROGMEM      = " Closing index ";
const char MSG_COULD_NOT_SEEK_ZERO[] PROGMEM= " Could not seek(0) ";
const char MSG_OPEN_FILE[] PROGMEM          = " Open file ";
const char MSG_CLOSE_FILE[] PROGMEM         = " Closing file ";
const char MSG_POS[] PROGMEM                = " Pos ";
const char MSG_AVAIL[] PROGMEM              = " Available ";
const char MSG_TRACK[] PROGMEM              = " Track ";
const char MSG_ALBUM[] PROGMEM              = " Album ";
const char MSG_MAXTRACK[] PROGMEM           = " Max Track ";
const char MSG_MAXALBUM[] PROGMEM           = " Max Album ";
const char MSG_READ_TRACK[] PROGMEM         = " Reading Track ";

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

#define IDX_LINE_LENGTH 24
#define MAX_ALBUMS 50

int currentAlbum;
int currentTrack;
int currentMaxTrack;
int maxAlbum;
int albumOffsets[MAX_ALBUMS];

byte buttonPressed;
char trackpath[IDX_LINE_LENGTH + 1];
char albumpath[IDX_LINE_LENGTH + 1];

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// create mp3 maker shield object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

void setup() {
  Serial.begin(9600);
  delay(200);
  mp3player_dbg(__LINE__, MSG_SETUP);

  tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab
  delay(500);
  tftInitScreen();
  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     mp3player_fatal(__LINE__, ERR_NO_VS1053);
  }  

  musicPlayer.sineTest(0x44, 250);    // Make a tone to indicate VS1053 is working
 
  if (!SD.begin(CARDCS)) {
    mp3player_fatal(__LINE__, ERR_NO_SD_CARD);
  }
  currentTrack = 0;
  currentAlbum = 0;  
  maxAlbum = updateIndex("/");
  mp3player_dbgi(__LINE__, MSG_MAXALBUM, maxAlbum);
  getIndexEntry("/", currentAlbum, &albumpath[0]);
  currentMaxTrack = updateIndex(albumpath);  
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
  
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
  musicPlayer.GPIO_pinMode(BUTTON_GPIO_TRACK_BWD, INPUT);
  musicPlayer.GPIO_pinMode(BUTTON_GPIO_ALBUM_FWD, INPUT);
  musicPlayer.GPIO_pinMode(BUTTON_GPIO_ALBUM_BWD, INPUT);
}

void loop() {
  mp3player_dbg(__LINE__, MSG_LOOP_START);
  
  if (musicPlayer.playingMusic) {
    mp3player_dbg(__LINE__, MSG_STOP_PLAY, musicPlayer.currentTrack.name());
    musicPlayer.stopPlaying();
  }
  const char* trackpath = getCurrentTrackpath();
  mp3player_dbg(__LINE__, MSG_START_PLAY, trackpath);
  if (! musicPlayer.startPlayingFile(trackpath)) {
    mp3player_fatal(__LINE__, ERR_OPEN_FILE, trackpath);
  }  
  // file is now playing in the 'background' so now's a good time
  // to do something else like handling LEDs or buttons :)
  waitForButtonOrTrackEnd();
  delay(250); 
}

void waitForButtonOrTrackEnd() {  
  resetButtons();
  while (buttonPressed == 0 && musicPlayer.playingMusic) {
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_TRACK_FWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_TRACK_FWD;
      mp3player_dbg(__LINE__, MSG_BUTTON, ">");      
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_TRACK_BWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_TRACK_BWD;
      mp3player_dbg(__LINE__, MSG_BUTTON, "<");
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_ALBUM_FWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_ALBUM_FWD;
      mp3player_dbg(__LINE__, MSG_BUTTON, ">|");
    }
    if (musicPlayer.GPIO_digitalRead(BUTTON_GPIO_ALBUM_BWD) == HIGH) {
      buttonPressed = BUTTON_GPIO_ALBUM_BWD;
      mp3player_dbg(__LINE__, MSG_BUTTON, "|<");
    }
  }
  handleUserAction(buttonPressed);
}

void handleUserAction(byte action) {
  // handle user action
  if (action > 0) {    
    if (action == BUTTON_GPIO_TRACK_FWD) {
      currentTrack++;
      if (currentTrack > currentMaxTrack) {
        action = BUTTON_GPIO_ALBUM_FWD;
      }
    }
    if (action == BUTTON_GPIO_TRACK_BWD) {
      currentTrack--;
      if (currentTrack < 0) {
        action = BUTTON_GPIO_ALBUM_BWD;
      }
    }
    if (action == BUTTON_GPIO_ALBUM_FWD) {
      currentTrack = 0;
      currentMaxTrack = -1;
      int oldCurrentAlbum = currentAlbum;
      while (currentMaxTrack == -1) {
        currentAlbum++;
        // loop detection
        if (oldCurrentAlbum == currentAlbum) {
          mp3player_fatal(__LINE__, ERR_NO_FILES_FOUND); 
        }
        // overflow detection
        if (currentAlbum > maxAlbum) {
          currentAlbum = 0;        
        }        
        getIndexEntry("/", currentAlbum, &albumpath[0]);
        currentMaxTrack = updateIndex(albumpath);
      }
    }
    if (action == BUTTON_GPIO_ALBUM_BWD) {
      currentTrack = 0;
      currentMaxTrack = -1;
      int oldCurrentAlbum = currentAlbum;
      while (currentMaxTrack == -1) {
        currentAlbum--;
        // loop detection
        if (oldCurrentAlbum == currentAlbum) {
          mp3player_fatal(__LINE__, ERR_NO_FILES_FOUND); 
        }
        // overflow detection
        if (currentAlbum < 0) {
          currentAlbum = maxAlbum;
        }        
        getIndexEntry("/", currentAlbum, &albumpath[0]);
        currentMaxTrack = updateIndex(albumpath);
      }
    }
  }
  mp3player_dbgi(__LINE__, MSG_ALBUM, currentAlbum);
  mp3player_dbgi(__LINE__, MSG_TRACK, currentTrack);
  mp3player_dbgi(__LINE__, MSG_MAXTRACK, currentMaxTrack);
}

void resetButtons() {
  buttonPressed = 0;
}

File getIndexFile(const char* dir, uint8_t mode = FILE_READ) {
  char idxpath[IDX_LINE_LENGTH];
  idxpath[0] = '/';
  size_t len = 1;
  if (strcmp(dir, "/") != 0) {
    // not root dir  
    strcpy(&idxpath[0], dir);    
    len = strlen(dir);
    idxpath[len++] = '/';
  }  
  strcpy(&idxpath[len], "_IDX/");
  SD.mkdir(&idxpath[0]);
  strcpy(&idxpath[len+5], "_FILES");
  mp3player_dbg(__LINE__, MSG_PATH, &idxpath[0]);
  return SD.open(idxpath, mode);
}

void getIndexEntry(const char* path, int number, char* returnVal) {
  // read path from index
  char s[IDX_LINE_LENGTH];
  File fIdx = getIndexFile(path);  
  fIdx.seek((IDX_LINE_LENGTH + 3) * number);  
  if (fIdx.position() == -1) {
    mp3player_fatal(__LINE__, ERR_SEEK_FAILED, number);
  } else {
    mp3player_dbgi(__LINE__, MSG_POS, fIdx.position());
  }
  int available = fIdx.available();
  mp3player_dbgi(__LINE__, MSG_AVAIL, available);
  fIdx.read(&s[0], IDX_LINE_LENGTH - 1);
  s[IDX_LINE_LENGTH - 1] = 0;
  fIdx.close();  
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  trim(s);
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  strcpy(returnVal, &s[0]);  
}

const char* getCurrentTrackpath() {  
  bool last = false;
  getIndexEntry("/", currentAlbum, &albumpath[0]);  
  if (currentTrack == 0){
    bmpDrawCover(albumpath);
  }
  mp3player_dbgi(__LINE__, MSG_READ_TRACK, currentTrack);
  getIndexEntry(&albumpath[0], currentTrack, &trackpath[0]);  
  return &trackpath[0];  
}

int updateIndex(const char* dir) {
  int count = -1;
  int len = 0;
  // create a new index
  mp3player_dbg(__LINE__, MSG_NEW_INDEX, dir);
  File currentDir = SD.open(dir);  
  File fIdx = getIndexFile(dir, FILE_WRITE | O_TRUNC);
  while (true) {
    File t = currentDir.openNextFile();    
    if (!t) {
      // last file reached
      mp3player_dbg(__LINE__, MSG_CLOSING_INDEX, dir);
      break;
    }
    mp3player_dbg(__LINE__, MSG_OPEN_FILE, t.name());
    if (EndsWithMp3(t.name())) {
      count++;
      // in subdirs all MP3s
      mp3player_dbgi(__LINE__, MSG_WRITE_INDEX, count);      
      fIdx.print('/');
      fIdx.print(currentDir.name());
      fIdx.print('/');
      fIdx.print(t.name());
      len = 2 + strlen(currentDir.name()) + strlen(t.name());
    } else if (t.isDirectory() && strcmp(t.name(), "SYSTEM~1") != 0 && strcmp(t.name(), "_IDX") != 0) {
      count++;
      // in rootDir all directories
      mp3player_dbgi(__LINE__, MSG_WRITE_INDEX, count);
      fIdx.print('/');
      fIdx.print(t.name());
      len = 1 + strlen(t.name());      
    } else {
      mp3player_dbg(__LINE__, MSG_IGNORING, t.name());
    }
    // finish the line with fixed length (for easier read in)
    if (len > 0) {
      while (len < IDX_LINE_LENGTH) {
        fIdx.print(' ');
        len++;
      }
      fIdx.println(' ');
    }
    // close file
    t.close();
  }
  fIdx.flush();
  fIdx.close();
  currentDir.close();
  return count;
}

void bmpDrawCover(const char* dir) {
  char coverpath[20];  
  strcpy(&coverpath[0], dir);
  size_t len = strlen(dir);
  coverpath[len] = '/';
  strcpy(&coverpath[len+1], "cover.bmp");
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

void mp3player_fatal(const int numb, const char msg[], long param) {
  mp3player_dbgi(numb, msg, param);
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

void mp3player_dbgi(const int lineno, const char msg[], long param) {
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // alle chars lesen
    Serial.write(c);   // und ausgeben
  }
  Serial.println(param);  // neue Zeile
}

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

void trim(char* s) {
    char* p = s;
    int len = strlen(p);    
    while(isspace(p[len - 1])) p[--len] = 0;
    //while(* p && isspace(* p)) ++p, --len;

    //memmove(s, p, len + 1);
}

