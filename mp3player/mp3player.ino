#include <EEPROM.h>
#include <Arduino.h>  // for type definitions
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <avr/pgmspace.h>

// uncomment to turn off DEBUG
#define DEBUG 1
// while coding you can force reindexing the SD card
#define FORCE_REINDEX false

#define MAX_ALBUMS 50 // will consume 2 bytes * MAX_ALBUMS of dynamic memory (Idea, if we need more mem: save offsets in a second fixed length file _OFFSETS on SD card)

// struct to persist the config in the EEPROM
struct state_t
{
    int track;
    int album;
    int maxAlbum;
    int idxLen;
    int albumOffsets[MAX_ALBUMS];
} state;

// strings
const char ERR_NO_VS1053[] PROGMEM          = " No VS1053";
const char ERR_BMP_NOT_RECOGNIZED[] PROGMEM = " BMP format not recognized.";
const char ERR_FILE_NOT_FOUND[] PROGMEM     = " File not found:";
const char ERR_NO_SD_CARD[] PROGMEM         = " SD not found!";
const char ERR_DREQ_PIN[] PROGMEM           = " DREQ not an interrupt!";
const char ERR_NO_FILES_FOUND[] PROGMEM     = " No files found ";
const char ERR_SEEK_FAILED[] PROGMEM        = " seek in file failed to pos ";
const char ERR_INDEX_FAILED[] PROGMEM       = " failed to create the index";

const char MSG_SETUP[] PROGMEM              = " setup";
const char MSG_SKIP_INDEX_CREATE[] PROGMEM  = " skipping index creation. /_IDX already exists.";
const char MSG_LOADING[] PROGMEM            = " Loading:";
const char MSG_DEBUG[] PROGMEM              = "DEBUG:";
const char MSG_FATAL[] PROGMEM              = "FATAL:";
const char MSG_FORCE_REINDEX[] PROGMEM      = " Force reindexing ";

const char MSG_LOOP_START[] PROGMEM         = "======";
const char MSG_PATH[] PROGMEM               = " path: ";
const char MSG_CLOSING[] PROGMEM            = " closing ";
const char MSG_IGNORING[] PROGMEM           = " Ignoring ";
const char ERR_OPEN_FILE[] PROGMEM          = " Could not open file ";
const char MSG_START_PLAY[] PROGMEM         = " Start playing ";
const char MSG_STOP_PLAY[] PROGMEM          = " Stop playing ";
const char MSG_BUTTON[] PROGMEM             = " Button: ";
const char MSG_NEW_INDEX[] PROGMEM          = " Creating new index ";
const char MSG_CLOSING_INDEX[] PROGMEM      = " Closing index ";
const char MSG_OPEN_FILE[] PROGMEM          = " :::: Open file :::: ";
const char MSG_CLOSE_FILE[] PROGMEM         = " Closing file ";
const char MSG_POS[] PROGMEM                = " Pos ";
const char MSG_AVAIL[] PROGMEM              = " Available ";
const char MSG_TRACK[] PROGMEM              = " Track ";
const char MSG_ALBUM[] PROGMEM              = " Album ";
const char MSG_OFFSET[] PROGMEM             = " Offset ";
const char MSG_MAXALBUM[] PROGMEM           = " Max Album ";
const char MSG_READ_TRACK[] PROGMEM         = " Reading Track ";
const char MSG_VOLUME[] PROGMEM             = " volume ";
const char MSG_SAVE_STATE[] PROGMEM         = " save state";
const char MSG_SHUTDOWN[] PROGMEM           = " shutdown";
const char MSG_ACTION[] PROGMEM             = " action ";

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
#define GPIO_ALBUM_FWD  1
#define GPIO_TRACK_FWD  2
#define GPIO_TRACK_BWD  3
#define GPIO_ALBUM_BWD  4
#define GPIO_SHUTDOWN   5
#define DIG_IO_POWER_ON 5


// constants for actions
#define ACTION_TRACK_FWD   0b00000001
#define ACTION_TRACK_BWD   0b00000010
#define ACTION_ALBUM_FWD   0b00000100
#define ACTION_ALBUM_BWD   0b00001000
#define ACTION_VOLUME_UP   0b00001110
#define ACTION_VOLUME_DOWN 0b00001011
#define ACTION_SHUTDOWN    0b00001100

#define IDX_LINE_LENGTH 24
#define IDX_LINE_LENGTH_W_LF 26
#define VOLUME_MAX 25
#define VOLUME_INIT 50
#define VOLUME_MIN 60

int currentAlbum;
int currentTrack;
bool forceCoverPaint = true; // on start-up cover must be painted
int volume = VOLUME_INIT;

char trackpath[IDX_LINE_LENGTH_W_LF];
char albumpath[IDX_LINE_LENGTH_W_LF];
File idxFile;

// Option 1 (recommended): must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS,  TFT_DC, TFT_RST);

// create mp3 maker shield object
Adafruit_VS1053_FilePlayer musicPlayer = Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

void setup() {
  // we have to set these pins first that the power switch is turned off (by setting ON to HIGH)
  pinMode(DIG_IO_POWER_ON, OUTPUT);
  digitalWrite(DIG_IO_POWER_ON, HIGH);

  #if defined(DEBUG)
  Serial.begin(9600);
  delay(100);
  #endif
  mp3player_dbg(__LINE__, MSG_SETUP);
  
  if (!SD.begin(CARDCS)) {
    mp3player_fatal(__LINE__, ERR_NO_SD_CARD);
  }
 
  tft.initR(INITR_144GREENTAB);   // initialize a ST7735S chip, black tab
  // initialise the music player
  if (! musicPlayer.begin()) { // initialise the music player
     mp3player_fatal(__LINE__, ERR_NO_VS1053);
  }  

  //musicPlayer.sineTest(0x44, 250);    // Make a tone to indicate VS1053 is working

  if (EEPROM_readAnything(0, state) > 0) {
    currentTrack = state.track;
    currentAlbum = state.album;    
  } else {
    currentTrack = 0;
    currentAlbum = 0;
  }
  updateIndex();
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(volume,volume);
  
  // This option uses a pin interrupt. No timers required! But DREQ
  // must be on an interrupt pin. For Uno/Duemilanove/Diecimilla
  // that's Digital #2 or #3
  // See http://arduino.cc/en/Reference/attachInterrupt for other pins
  // *** This method is preferred
  if (! musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT)) {
    mp3player_fatal(__LINE__, ERR_DREQ_PIN);
  }
  // setup pins
  musicPlayer.GPIO_pinMode(GPIO_TRACK_FWD, INPUT);
  musicPlayer.GPIO_pinMode(GPIO_TRACK_BWD, INPUT);
  musicPlayer.GPIO_pinMode(GPIO_ALBUM_FWD, INPUT);
  musicPlayer.GPIO_pinMode(GPIO_ALBUM_BWD, INPUT);
  musicPlayer.GPIO_pinMode(GPIO_SHUTDOWN, OUTPUT);
  musicPlayer.GPIO_digitalWrite(GPIO_SHUTDOWN, LOW);
  // open Index File
  idxFile = getIndexFile(FILE_READ);
  mp3player_dbgi(__LINE__, MSG_AVAIL, idxFile.available());
}

/**
 * Main prog loop
 */
void loop() {
  mp3player_dbg(__LINE__, MSG_LOOP_START);

  // stop
  if (musicPlayer.playingMusic) {
    mp3player_dbg(__LINE__, MSG_STOP_PLAY, musicPlayer.currentTrack.name());
    musicPlayer.stopPlaying();
  }
  // play
  tftPrintTrackPositionUnmarked(currentTrack, true);
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

void saveState() {
  mp3player_dbg(__LINE__, MSG_SAVE_STATE);
  state.album = currentAlbum;
  state.track = currentTrack;
  EEPROM_writeAnything(0, state);
}

void shutdownNow() {
  mp3player_dbg(__LINE__, MSG_SHUTDOWN);
  digitalWrite(DIG_IO_POWER_ON, LOW);
  musicPlayer.GPIO_digitalWrite(GPIO_SHUTDOWN, HIGH);
}

byte getUserAction(int* btCount) {
  byte userAction = 0;
  if (musicPlayer.GPIO_digitalRead(GPIO_ALBUM_BWD) == HIGH) {
      userAction |= ACTION_ALBUM_BWD; // write the bit
      mp3player_dbg(__LINE__, MSG_BUTTON, "|<");
      *btCount += 1;
    }
    if (musicPlayer.GPIO_digitalRead(GPIO_TRACK_BWD) == HIGH) {
      userAction |= ACTION_TRACK_BWD; // write the bit
      mp3player_dbg(__LINE__, MSG_BUTTON, "<");
      *btCount += 1;
    } 
    if (musicPlayer.GPIO_digitalRead(GPIO_TRACK_FWD) == HIGH) {
      userAction |= ACTION_TRACK_FWD; // write the bit
      mp3player_dbg(__LINE__, MSG_BUTTON, ">");
      *btCount += 1;
    }    
    if (musicPlayer.GPIO_digitalRead(GPIO_ALBUM_FWD) == HIGH) {
      userAction |= ACTION_ALBUM_FWD; // write the bit
      mp3player_dbg(__LINE__, MSG_BUTTON, ">|");
      *btCount += 1;
    }
    if (userAction != 0) {
      mp3player_dbgi(__LINE__, MSG_ACTION, userAction);
      mp3player_dbgi(__LINE__, MSG_BUTTON, *btCount);
    }
    return userAction;
}

void waitForButtonOrTrackEnd() {
  byte savedUserAction = 0;
  int btCount = 0;
  bool exitLoop = false;
  unsigned long lastMultiButtonTS = 0;
  while ( ! exitLoop ) {
    btCount = 0;
    byte userAction = getUserAction(&btCount);
    // check that button has been released
    if (btCount == 0) {
      if (savedUserAction != 0) {
        // buttons released --> leave loop only if user action shall be executed
        handleUserAction(savedUserAction);
        exitLoop = true;
      } else {
        delay(100);
      }
    } else if (btCount == 1) {
      if ((lastMultiButtonTS + 1000) < millis()) { // after a special command, we ignore one button commands 1 sec
        mp3player_dbgi(__LINE__, MSG_BUTTON, btCount);
        // single button action (save until button is released)
        savedUserAction = userAction;
        delay(200);
      }
    } else {
      // multi button action
      if (userAction == ACTION_SHUTDOWN) {
        saveState();
        shutdownNow();
      } else if (userAction == ACTION_VOLUME_UP || userAction == ACTION_VOLUME_DOWN) {
        handleUserAction(userAction);
        // don't leave the loop
        savedUserAction = 0;
        lastMultiButtonTS = millis();
        delay(350);
      }
    } // end special actions

    // if track has ended playing
    if (!musicPlayer.playingMusic) {
      // play the next track
      userAction = ACTION_TRACK_FWD;
      // if the album is at the end of an album we shutdown the player
      // next startup the player will start at the next album (that why we handleUserAction first)
      if (hasLastTrackReached(currentAlbum, currentTrack)) {        
        handleUserAction(userAction);
        saveState();
        shutdownNow();
      } else {
        handleUserAction(userAction);
        exitLoop = true;
      }
    }
  }
  mp3player_dbg(__LINE__, MSG_BUTTON, "loop end");
}

int getOffset(int albumNo, int lineNo) {
  return state.albumOffsets[albumNo] + IDX_LINE_LENGTH_W_LF * lineNo;
}

boolean hasLastTrackReached(int albumNo, int trackNo) {
  int offsetOfNextAlbum;
  if (albumNo == state.maxAlbum) {
    offsetOfNextAlbum = idxFile.size();
  } else {
    offsetOfNextAlbum = getOffset(albumNo + 1, 0);
  }
  int offset = getOffset(albumNo, trackNo + 1) + IDX_LINE_LENGTH_W_LF; 
  mp3player_dbgi(__LINE__, MSG_OFFSET, offset);
  mp3player_dbgi(__LINE__, MSG_OFFSET, offsetOfNextAlbum);
  if (offset >= offsetOfNextAlbum) {
    return true;
  }
  return false;
}

void handleUserAction(byte action) {
  // handle user action
  if (action > 0) {
    if (action == ACTION_TRACK_FWD) {
      if (hasLastTrackReached(currentAlbum, currentTrack)) {
        action = ACTION_ALBUM_FWD;
      } else {
        tftPrintTrackPositionUnmarked(currentTrack, false);
        currentTrack++;        
      }
    }
    if (action == ACTION_TRACK_BWD) {
      tftPrintTrackPositionUnmarked(currentTrack, false);
      currentTrack--;
      if (currentTrack < 0) {
        action = ACTION_ALBUM_BWD;
      } else {
        tftPrintTrackPositionUnmarked(currentTrack, false);
      }
    }
    if (action == ACTION_ALBUM_FWD) {
      currentTrack = 0;
      currentAlbum++;
      forceCoverPaint = true;
      if (currentAlbum > state.maxAlbum) {
        currentAlbum = 0;
      }
    }
    if (action == ACTION_ALBUM_BWD) {
      currentTrack = 0;
      currentAlbum--;
      forceCoverPaint = true;
      // overflow detection
      if (currentAlbum < 0) {
        currentAlbum = state.maxAlbum;
      }
    }
    // volume actions
    if (action == ACTION_VOLUME_UP) {
      volume -= 5; // decrease means louder
      if (volume < VOLUME_MAX) {
        volume = VOLUME_MAX;
      }
      mp3player_dbgi(__LINE__, MSG_VOLUME, volume);
      musicPlayer.setVolume(volume,volume);
    }
    else if (action == ACTION_VOLUME_DOWN) {
      volume += 5; // increase means more quiet
      if (volume > VOLUME_MIN) {
        volume = VOLUME_MIN;        
      }
      mp3player_dbgi(__LINE__, MSG_VOLUME, volume);
      musicPlayer.setVolume(volume,volume);
    }
    // debug and save state only if non-volume action
    else if (! (action & ACTION_ALBUM_BWD && action & ACTION_TRACK_BWD) ) {
      mp3player_dbgi(__LINE__, MSG_ALBUM, currentAlbum);
      mp3player_dbgi(__LINE__, MSG_TRACK, currentTrack);
      saveState();
    }
  }
}

File getIndexFile(uint8_t mode) {
  return SD.open("/_IDX", mode);
}

// read path from index
void getIndexEntry(int albumNo, int trackNo, char* returnVal) {
  char s[IDX_LINE_LENGTH];  
  int pos = getOffset(albumNo, trackNo);
  idxFile.seek(pos);  
  if (idxFile.position() == -1) {
    mp3player_fatal(__LINE__, ERR_SEEK_FAILED, pos);
  } else {
    mp3player_dbgi(__LINE__, MSG_POS, idxFile.position());
  }  
  idxFile.read(&s[0], IDX_LINE_LENGTH - 1);
  s[IDX_LINE_LENGTH - 1] = 0;
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  trim(s);
  mp3player_dbg(__LINE__, MSG_PATH, &s[0]);
  strcpy(returnVal, &s[0]);  
}

const char* getCurrentTrackpath() {  
  bool last = false;
  // first entry is the album path  
  getIndexEntry(currentAlbum, 0, &trackpath[0]);
  bmpDrawCover(trackpath);  
  // read track path
  getIndexEntry(currentAlbum, currentTrack + 1, &trackpath[0]);
  mp3player_dbgi(__LINE__, MSG_READ_TRACK, currentTrack);  
  return &trackpath[0];  
}

void updateIndex() {
  if (FORCE_REINDEX) {
    mp3player_dbg(__LINE__, MSG_FORCE_REINDEX);
    SD.remove("/_IDX");
  }
  // check if file must be recreated
  if ( FORCE_REINDEX != true && SD.exists("/_IDX") && state.idxLen > 0) {
    File idxFile = getIndexFile(FILE_READ);
    int len = idxFile.available();
    idxFile.close();
    if (len == state.idxLen) {
      // ok, saved len is equals (we assume that the file is ok)
      mp3player_dbg(__LINE__, MSG_SKIP_INDEX_CREATE);      
      return;
    }
  }
  // reset state
  state.idxLen = 0;
  saveState();
  // start generation
  int len = 0;
  int offset = 0;
  int albumNo = -1;  
  // create a new index
  mp3player_dbg(__LINE__, MSG_NEW_INDEX);
  File rootDir = SD.open("/");  
  File fIdx = getIndexFile(FILE_WRITE | O_TRUNC);
  while (true) {
    File albumDir = rootDir.openNextFile();    
    if (!albumDir) {
      // last file reached
      mp3player_dbg(__LINE__, MSG_CLOSING_INDEX, albumDir.name());
      break;
    }
    mp3player_dbg(__LINE__, MSG_OPEN_FILE, albumDir.name());
    if (albumDir.isDirectory() && strcmp(albumDir.name(), "SYSTEM~1") != 0) {
      // in rootDir each dir is an album
      boolean dirEntryWritten = false;
      while (true) {
        File track = albumDir.openNextFile();
        if (!track) {
          // last file reached
          break;
        }
        mp3player_dbg(__LINE__, MSG_TRACK, track.name());
        if (IsValidFileExtension(track.name())) {
          if (!dirEntryWritten) {
            // a directory must have at least 1 song to be indexed
            state.albumOffsets[++albumNo] = offset;
            mp3player_dbgi(__LINE__, MSG_ALBUM, albumNo);
            mp3player_dbgi(__LINE__, MSG_OFFSET, offset);
            // print album path
            fIdx.print('/');
            fIdx.print(albumDir.name());
            len = 1 + strlen(albumDir.name());
            while (len < IDX_LINE_LENGTH) {
              fIdx.print(' ');
              len++;
            }
            fIdx.println("");
            offset += IDX_LINE_LENGTH_W_LF;
            dirEntryWritten = true;
          }
          // all MP3s in album
          fIdx.print('/');
          fIdx.print(albumDir.name());
          fIdx.print('/');
          fIdx.print(track.name());
          len = 2 + strlen(albumDir.name()) + strlen(track.name());
          while (len < IDX_LINE_LENGTH) {
            fIdx.print(' ');
            len++;
          }
          fIdx.println("");
          offset += IDX_LINE_LENGTH_W_LF;
        } else {
          mp3player_dbg(__LINE__, MSG_IGNORING, track.name());
        }
        track.close();
      }
      mp3player_dbg(__LINE__, MSG_CLOSING, albumDir.name());
      albumDir.close();
    } else {
      mp3player_dbg(__LINE__, MSG_IGNORING, albumDir.name());
    }
  }
  // to check if update index process has been aborted we save the length of the file
  fIdx.seek(0);
  state.idxLen = fIdx.available();
  fIdx.close();
  rootDir.close();
  state.maxAlbum = albumNo;
  mp3player_dbgi(__LINE__, MSG_MAXALBUM, state.maxAlbum);
  if (state.idxLen == 0 || state.maxAlbum == -1) {
    mp3player_fatal(__LINE__, ERR_INDEX_FAILED);
  }
  saveState();
}

void bmpDrawCover(const char* dir) {
  if (forceCoverPaint) {
    char coverpath[20];
    strcpy(&coverpath[0], dir);
    size_t len = strlen(dir);
    coverpath[len] = '/';
    strcpy(&coverpath[len+1], "cover.bmp");
    bmpDraw(coverpath, 0, 0);
    tftPrintTrackPositionMarker();
    forceCoverPaint = false;
  }  
}

/*
   +-- 14 +-.-+ 20 --.-- 20 --.-- .... --.-- 14 --+
          |   |
          |   |
          +---+
*/
void tftPrintTrackPositionMarker() {
  for (int i=0; i<20; i++) {    
    if (!hasLastTrackReached(currentAlbum, i-1)) {
      tftPrintTrackPositionUnmarked(i, false);
    }    
  }
  // draw the current track marker
  tftPrintTrackPositionUnmarked(currentTrack, true);
}

void tftPrintTrackPositionUnmarked(int no, bool active) {  
  uint16_t colorFill;
  int pos = no % 5;
  if (no >= 10) {
    // bottom and left we have to start at left/bottom
    pos = 4 - pos;
  }
  pos = 14+8+pos*20;
  int x;
  int y;
  if (no < 5) {
    x = pos;
    y = 0;
  } else if (no < 10) {
    x = 124;
    y = pos;
  } else if (no < 15) {
    x = pos;
    y = 124;
  } else if (no < 20) {
    x = 0;
    y = pos;
  }
  if (active) {
    // http://www.barth-dev.de/online/rgb565-color-picker/
    colorFill = 0x0600;
  } else {
    colorFill = ST7735_WHITE;
  }
  tft.fillRect(x, y, 5, 5, colorFill);
  tft.drawRect(x, y, 5, 5, ST7735_BLACK);
}

/* 
 * text*  
 * color examples: ST7735_WHITE, ST7735_BLACK
 */
void tft_text(const char msg[], uint16_t color) {
  tft.fillScreen(ST7735_RED);
  tft.setCursor(0, 0);
  tft.setTextColor(color);
  tft.setTextWrap(true);
  char c;
  while((c = pgm_read_byte(msg++))) { // read all chars
    tft.print(c);   // and print
  }  
}

void mp3player_fatal(const int numb, const char msg[]) {
  tft_text(msg, ST7735_WHITE);
  mp3player_dbg(numb, msg);
  while(1);
}

void mp3player_fatal(const int numb, const char msg[], const char *param) {
  tft_text(msg, ST7735_WHITE);
  mp3player_dbg(numb, msg, param);
  while(1);
}

void mp3player_fatal(const int numb, const char msg[], long param) {
  tft_text(msg, ST7735_WHITE);
  mp3player_dbgi(numb, msg, param);
  while(1);
}

void mp3player_dbg(const int lineno, const char msg[]) {
  #if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // alle chars lesen
    Serial.write(c);
  }
  Serial.write('\n');
  #endif
}

void mp3player_dbg(const int lineno, const char msg[], const char *param) {
  #if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // read all chars
    Serial.write(c);
  }
  Serial.println(param);
  #endif
}

void mp3player_dbgi(const int lineno, const char msg[], long param) {
  #if defined(DEBUG)
  Serial.print(lineno);
  Serial.print(":");
  char c;
  while((c = pgm_read_byte(msg++))) { // read all chars
    Serial.write(c);
  }
  Serial.println(param);
  #endif
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

int IsValidFileExtension(const char *str) {
  return EndsWith(str, ".MP3") || EndsWith(str, ".M4A");
}

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

size_t trim(char* s) {
    char* p = s;
    size_t len = strlen(p);    
    while(isspace(p[len - 1])) p[--len] = 0;
    //while(* p && isspace(* p)) ++p, --len;

    //memmove(s, p, len + 1);
    return len;
}

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = EEPROM.read(ee++);
    return i;
}


