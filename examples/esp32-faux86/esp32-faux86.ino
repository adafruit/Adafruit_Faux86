/*******************************************************************************
 * Arduino ESP32 Faux86
 *
 * Original example can be found here:
 * https://github.com/moononournation/T-Deck/blob/main/esp32-faux86/esp32-faux86.ino
 *
 * Required libraries:
 * - Arduino_GFX: https://github.com/moononournation/Arduino_GFX.git
 * - Faux86-remake: https://github.com/moononournation/Faux86-remake.git
 *   Note: on Linux, you may need to make change as https://github.com/ArnoldUK/Faux86-remake/pull/5
 * - For uploading files to FFat: https://github.com/lorol/arduino-esp32fs-plugin
 *
 * Arduino IDE Settings for T-Deck
 * Board:            "ESP32S3 Dev Module"
 * USB CDC On Boot:  "Enable"
 * Flash Mode:       "QIO 120MHz"
 * Flash Size:       "16MB(128Mb)"
 * Partition Scheme: "16M Flash(2M APP/12.5MB FATFS)"
 * PSRAM:            "OPI PSRAM"
 ******************************************************************************/

#include <Arduino_GFX_Library.h>

// mimic metro s3: TODO remove later
#ifndef ARDUINO_METRO_ESP32S3
  #define ARDUINO_METRO_ESP32S3
#endif

#if defined(ARDUINO_ESP32_S3_BOX)

#define TFT_DC 4
#define TFT_CS 5
#define TFT_SCK 7
#define TFT_MOSI 6
#define TFT_RST 48
#define TFT_BL 45

#define TFT_Controller Arduino_ILI9342
#define TFT_SPEED_HZ (40*1000*1000ul)
#define TFT_ROTATION 4

#elif defined(ARDUINO_METRO_ESP32S3)

#define TFT_DC 9
#define TFT_CS 10
// Feather
//#define TFT_DC 10
//#define TFT_CS 9
#define TFT_SCK 39
#define TFT_MOSI 42
#define TFT_RST GFX_NOT_DEFINED
//#define TFT_BL 45

#define TFT_Controller Arduino_ILI9341
#define TFT_SPEED_HZ (60*1000*1000ul)
#define TFT_ROTATION 1

#else

// #include "TDECK_PINS.h"

#define TFT_DC 11
#define TFT_CS 12
#define TFT_SCK 40
#define TFT_MOSI 41
#define TFT_RST GFX_NOT_DEFINED
#define TFT_BL 42

#define TFT_Controller Arduino_ST7789
#define TFT_SPEED_HZ (80*1000*1000ul)
#define TFT_ROTATION 1

#endif

Arduino_DataBus* bus = new Arduino_ESP32SPIDMA(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI);
Arduino_TFT* gfx = new TFT_Controller(bus, TFT_RST, TFT_ROTATION, false /* IPS */);

#define TRACK_SPEED 2
#define KEY_SCAN_MS_INTERVAL 200

/*******************************************************************************
 * Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

#include "keymap.h"

#include <WiFi.h>
#include <FFat.h>
#include <VM.h>

#include "asciivga_dat.h"
#include "pcxtbios_bin.h"
#include "rombasic_bin.h"
#include "videorom_bin.h"

bool keyboard_interrupted = false;

void IRAM_ATTR ISR_key() {
  keyboard_interrupted = true;
}

unsigned long next_key_scan_ms = 0;
bool trackball_interrupted = false;
int16_t trackball_up_count = 1;
int16_t trackball_down_count = 1;
int16_t trackball_left_count = 1;
int16_t trackball_right_count = 1;
int16_t trackball_click_count = 0;
bool mouse_downed = false;

void IRAM_ATTR ISR_up() {
  trackball_interrupted = true;
  trackball_up_count <<= TRACK_SPEED;
}

void IRAM_ATTR ISR_down() {
  trackball_interrupted = true;
  trackball_down_count <<= TRACK_SPEED;
}

void IRAM_ATTR ISR_left() {
  trackball_interrupted = true;
  trackball_left_count <<= TRACK_SPEED;
}

void IRAM_ATTR ISR_right() {
  trackball_interrupted = true;
  trackball_right_count <<= TRACK_SPEED;
}

void IRAM_ATTR ISR_click() {
  trackball_interrupted = true;
  ++trackball_click_count;
}

#include "ArduinoInterface.h"

Faux86::VM* vm86;
Faux86::ArduinoHostSystemInterface hostInterface(gfx);

uint16_t* vga_framebuffer;

void setup() {
  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("esp32-faux86");

  Serial.println("Init display");
  if (!gfx->begin(TFT_SPEED_HZ)) {
    Serial.println("Init display failed!");
  }
  gfx->fillScreen(BLACK);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

#if 0
  Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL, TDECK_I2C_FREQ);

  Serial.println("Init touchscreen");
  touch_init(gfx->width(), gfx->height(), gfx->getRotation());

  Serial.println("Init LILYGO Keyboard");
  pinMode(TDECK_KEYBOARD_INT, INPUT_PULLUP);
  attachInterrupt(TDECK_KEYBOARD_INT, ISR_key, FALLING);
  Wire.requestFrom(TDECK_KEYBOARD_ADDR, 1);
  if (Wire.read() == -1) {
    Serial.println("LILYGO Keyboad not online!");
  }

  // Init trackball
  pinMode(TDECK_TRACKBALL_UP, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_UP, ISR_up, FALLING);
  pinMode(TDECK_TRACKBALL_DOWN, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_DOWN, ISR_down, FALLING);
  pinMode(TDECK_TRACKBALL_LEFT, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_LEFT, ISR_left, FALLING);
  pinMode(TDECK_TRACKBALL_RIGHT, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_RIGHT, ISR_right, FALLING);
  pinMode(TDECK_TRACKBALL_CLICK, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_CLICK, ISR_click, FALLING);
#endif

  if (!FFat.begin(false)) {
    Serial.println("ERROR: File system mount failed!");
  }

  Faux86::Config vmConfig(&hostInterface);

  /* CPU settings */
  vmConfig.singleThreaded = true; // only WIN32 support multithreading
  vmConfig.slowSystem = true;      // slow system will reserve more time for audio, if enabled
  vmConfig.cpuSpeed = 0;          // no limit

  /* Video settings */
  vmConfig.frameDelay = 200; // 200ms 5fps
  vmConfig.framebuffer.width = 720;
  vmConfig.framebuffer.height = 480;

  /* Audio settings */
  vmConfig.enableAudio = false;
  vmConfig.useDisneySoundSource = false;
  vmConfig.useSoundBlaster = false;
  vmConfig.useAdlib = false;
  vmConfig.usePCSpeaker = true;
  vmConfig.audio.sampleRate = 22050; // 32000 //44100 //48000;
  vmConfig.audio.latency = 200;

  /* set BIOS ROM */
  // vmConfig.biosFile = hostInterface.openFile("/ffat/pcxtbios.bin");
  vmConfig.biosFile = new Faux86::EmbeddedDisk(pcxtbios_bin, pcxtbios_bin_len);

  /* set Basic ROM */
  // vmConfig.romBasicFile = hostInterface.openFile("/ffat/rombasic.bin");
  vmConfig.romBasicFile = new Faux86::EmbeddedDisk(rombasic_bin, rombasic_bin_len);

  /* set Video ROM */
  // vmConfig.videoRomFile = hostInterface.openFile("/ffat/videorom.bin");
  vmConfig.videoRomFile = new Faux86::EmbeddedDisk(videorom_bin, videorom_bin_len);

  /* set ASCII FONT Data */
  // vmConfig.asciiFile = hostInterface.openFile("/ffat/asciivga.dat");
  vmConfig.asciiFile = new Faux86::EmbeddedDisk(asciivga_dat, asciivga_dat_len);

  /* floppy drive image */
  //vmConfig.diskDriveA = hostInterface.openFile("/ffat/fd0.img");

  /* harddisk drive image */
  vmConfig.diskDriveC = hostInterface.openFile("/ffat/hd0_12m_win30.img");

  /* set boot drive */
  vmConfig.setBootDrive("hd0");

  vga_framebuffer = (uint16_t*) calloc(VGA_FRAMEBUFFER_WIDTH * VGA_FRAMEBUFFER_HEIGHT, sizeof(uint16_t));
  if (!vga_framebuffer) {
    Serial.println("Failed to allocate vga_framebuffer");
  }

  vm86 = new Faux86::VM(vmConfig);
  if (vm86->init()) {
    hostInterface.init(vm86);
  }
}

void loop() {
  vm86->simulate();
  // hostInterface.tick();

  /* handle keyboard input */
  if (keyboard_interrupted || (millis() > next_key_scan_ms)) {
#if 0
    Wire.requestFrom(TDECK_KEYBOARD_ADDR, 1);
    while (Wire.available() > 0) {
      char key = Wire.read();
      if (key != 0) {
        uint16_t keyxt = ascii2xtMapping[key];
        // Serial.printf("key: %c, keyxt: %0x\n", key, keyxt);
        vm86->input.handleKeyDown(keyxt);
        vm86->simulate();
        vm86->input.handleKeyUp(keyxt);
        next_key_scan_ms = millis() + KEY_SCAN_MS_INTERVAL;
      }
    }
#endif
    keyboard_interrupted = false;
  }

#if 0
  /* handle trackball input */
  if (trackball_interrupted) {
    if (trackball_click_count > 0) {
      Serial.println("vm86->mouse.handleButtonDown(Faux86::SerialMouse::ButtonType::Left);");
      vm86->mouse.handleButtonDown(Faux86::SerialMouse::ButtonType::Left);
      mouse_downed = true;
    }
    int16_t x_delta = trackball_right_count - trackball_left_count;
    int16_t y_delta = trackball_down_count - trackball_up_count;
    if ((x_delta != 0) || (y_delta != 0)) {
      Serial.printf("x_delta: %d, y_delta: %d\n", x_delta, y_delta);
      vm86->mouse.handleMove(x_delta, y_delta);
    }
    trackball_interrupted = false;
    trackball_up_count = 1;
    trackball_down_count = 1;
    trackball_left_count = 1;
    trackball_right_count = 1;
    trackball_click_count = 0;
  } else if (mouse_downed) {
    if (digitalRead(TDECK_TRACKBALL_CLICK) == HIGH) // released
    {
      Serial.println("vm86->mouse.handleButtonUp(Faux86::SerialMouse::ButtonType::Left);");
      vm86->mouse.handleButtonUp(Faux86::SerialMouse::ButtonType::Left);
      mouse_downed = false;
    }
  }
#endif
}
