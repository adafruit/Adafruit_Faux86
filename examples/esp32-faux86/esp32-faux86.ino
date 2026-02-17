/*******************************************************************************
 * Arduino ESP32 Faux86
 *
 * Original example can be found here:
 * https://github.com/moononournation/T-Deck/blob/main/esp32-faux86/esp32-faux86.ino
 *
 * Required libraries:
 * - Faux86-remake: https://github.com/moononournation/Faux86-remake.git
 *   Note: on Linux, you may need to make change as
 *https://github.com/ArnoldUK/Faux86-remake/pull/5
 * - Adafruit GFX: https://github.com/adafruit/Adafruit-GFX-Library and your
 *specific TFT library controller e.g. Adafruit_ILI9341
 * - Arduino_GFX: https://github.com/moononournation/Arduino_GFX.git
 * - For uploading files to FFat:
 *https://github.com/lorol/arduino-esp32fs-plugin
 *
 * Arduino IDE Settings
 * Board:            "ESP32S3 Dev Module"
 * USB CDC On Boot:  "Enable"
 * Partition Scheme: "16M Flash(2M APP/12.5MB FATFS)"
 *
 * uploaded one of img file in disks/ to ESP32-S3 using
 *[arduino-esp32fs-plugin](https://github.com/lorol/arduino-esp32fs-plugin)
 * - Install arduino-esp32fs-plugin
 * - Create a folder named "data" in the sketch folder
 * - Copy one of the img file in disks/ to "data" folder
 * - From IDE menu select: "Tools" -> "ESP32 Sketch Data Upload" then select
 *"FatFS" then click "OK"
 ******************************************************************************/

#include "Adafruit_GFX.h"
#include "Adafruit_TinyUSB.h"
#include "SPI.h"

#include "Adafruit_Faux86.h"

#define TFT_DC 11
#define TFT_CS 12
#define TFT_SCK SCK
#define TFT_MOSI MOSI
#define TFT_RST -1
#define TFT_BL -1

#define TFT_SPEED_HZ (60 * 1000 * 1000ul)
#define TFT_ROTATION 1

#include "Adafruit_ILI9341.h"
Adafruit_SPITFT *gfx = new Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);

#define MAX3421_SCK SCK
#define MAX3421_MOSI MOSI
#define MAX3421_MISO MISO
#define MAX3421_CS 10
#define MAX3421_INT 9
Adafruit_USBH_Host USBHost(&SPI, MAX3421_SCK, MAX3421_MOSI, MAX3421_MISO,
                           MAX3421_CS, MAX3421_INT);

#include <FFat.h>
#include <WiFi.h>

Faux86::VM *vm86;
Faux86::ArduinoHostSystemInterface hostInterface(gfx);
Faux86::Config vmConfig(&hostInterface);

uint16_t *vga_framebuffer;

void vm86_task(void *param) {
  (void)param;

  if (!FFat.begin(false)) {
    Serial.println("ERROR: File system mount failed!");
  }

  /* CPU settings */
  vmConfig.singleThreaded = true; // only WIN32 support multithreading
  vmConfig.slowSystem =
      true; // slow system will reserve more time for audio, if enabled
  vmConfig.cpuSpeed = 0; // no limit

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
  vmConfig.romBasicFile =
      new Faux86::EmbeddedDisk(rombasic_bin, rombasic_bin_len);

  /* set Video ROM */
  // vmConfig.videoRomFile = hostInterface.openFile("/ffat/videorom.bin");
  vmConfig.videoRomFile =
      new Faux86::EmbeddedDisk(videorom_bin, videorom_bin_len);

  /* set ASCII FONT Data */
  // vmConfig.asciiFile = hostInterface.openFile("/ffat/asciivga.dat");
  vmConfig.asciiFile = new Faux86::EmbeddedDisk(asciivga_dat, asciivga_dat_len);

  /* floppy drive image */
  // vmConfig.diskDriveA = hostInterface.openFile("/ffat/fd0.img");

  // harddisk drive image: can be found in disks/ folder
  // vmConfig.diskDriveC = hostInterface.openFile("/ffat/hd0_12m_win30.img");
  vmConfig.diskDriveC = hostInterface.openFile("/ffat/hd0_12m_games.img");

  /* set boot drive */
  vmConfig.setBootDrive("hd0");

  vga_framebuffer = (uint16_t *)calloc(
      VGA_FRAMEBUFFER_WIDTH * VGA_FRAMEBUFFER_HEIGHT, sizeof(uint16_t));
  if (!vga_framebuffer) {
    Serial.println("Failed to allocate vga_framebuffer");
  }

  vm86 = new Faux86::VM(vmConfig);
  if (vm86->init()) {
    hostInterface.init(vm86);
  }

  while (1) {
    vm86->simulate();
    // hostInterface.tick();

    // simulated() call yield() inside but since this is highest priority task
    // we should call vTaskDelay(1) to allow other task to run
    vTaskDelay(1);
  }
}

/*******************************************************************************
 * Please config the touch panel in touch.h
 ******************************************************************************/
#include "touch.h"

#define TRACK_SPEED 2
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

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+
void setup() {
  WiFi.mode(WIFI_OFF);

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial) delay(10);
  Serial.println("esp32-faux86");

  Serial.println("Init display");
  gfx->begin(TFT_SPEED_HZ);
  gfx->setRotation(TFT_ROTATION);
  gfx->fillScreen(0x000000); // black

#if defined(TFT_BL) && (TFT_BL != -1)
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

#if 0
  Wire.begin(TDECK_I2C_SDA, TDECK_I2C_SCL, TDECK_I2C_FREQ);

  Serial.println("Init touchscreen");
  touch_init(gfx->width(), gfx->height(), gfx->getRotation());

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

  // Create a thread with high priority to run VM 86
  // For S3" since we don't use wifi in this example. We can it on core0
  xTaskCreateUniversal(vm86_task, "vm86", 8192, NULL, 10, NULL, 0);

  Serial.println("Init USBHost with MAX3421");
  if (!USBHost.begin(1)) {
    Serial.println("Failed to init USBHost");
  }
}

void loop() {
  USBHost.task();

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

// look up new key in previous keys
static bool find_key_in_report(hid_keyboard_report_t const *report,
                               uint8_t keycode) {
  for (uint8_t i = 0; i < 6; i++) {
    if (report->keycode[i] == keycode)
      return true;
  }
  return false;
}

static void process_kbd_report(hid_keyboard_report_t const *report) {
  // previous report to check key released
  static hid_keyboard_report_t prev_report = {0, 0, {0}};

  // modifier
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t const mask = 1 << i;
    uint8_t const new_modifier = report->modifier & mask;
    uint8_t const old_modifier = prev_report.modifier & mask;
    if (new_modifier != old_modifier) {
      // modifier change
      if (new_modifier) {
        vm86->input.handleKeyDown(modifier2xtMapping[i]);
      } else {
        vm86->input.handleKeyUp(modifier2xtMapping[i]);
      }

      vm86->input.tick();
    }
  }

  // keycode
  for (uint8_t i = 0; i < 6; i++) {
    // new key pressed
    uint8_t const new_key = report->keycode[i];
    if (new_key && !find_key_in_report(&prev_report, new_key)) {
      vm86->input.handleKeyDown(usb2xtMapping[new_key]);
      vm86->input.tick();
    }

    // old key released
    uint8_t const old_key = prev_report.keycode[i];
    if (old_key && !find_key_in_report(report, old_key)) {
      vm86->input.handleKeyUp(usb2xtMapping[old_key]);
      vm86->input.tick();
    }
  }

  prev_report = *report;
}

//--------------------------------------------------------------------+
// TinyUSB Host callbacks
//--------------------------------------------------------------------+

extern "C" {
// Invoked when device with hid interface is mounted
// Report descriptor is also available for use.
// tuh_hid_parse_report_descriptor() can be used to parse common/simple enough
// descriptor. Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE,
// it will be skipped therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                      uint8_t const *desc_report, uint16_t desc_len) {
  (void)desc_report;
  (void)desc_len;

  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
    Serial.printf("HID Keyboard mounted\r\n");
    if (!tuh_hid_receive_report(dev_addr, instance)) {
      Serial.printf("Error: cannot request to receive report\r\n");
    }
  }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
  Serial.printf("HID device address = %d, instance = %d is unmounted\r\n",
                dev_addr, instance);
}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                uint8_t const *report, uint16_t len) {
  (void)len;
  uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
  // Serial.printf("HID report len = %u\r\n", len);

  switch (itf_protocol) {
  case HID_ITF_PROTOCOL_KEYBOARD:
    process_kbd_report((hid_keyboard_report_t const *)report);
    break;

  case HID_ITF_PROTOCOL_MOUSE:
    // process_mouse_report((hid_mouse_report_t const *) report);
    break;

  default:
    break;
  }

  // continue to request to receive report
  if (!tuh_hid_receive_report(dev_addr, instance)) {
    Serial.printf("Error: cannot request to receive report\r\n");
  }
}

} // extern "C"
