# Adafruit_Faux86

Adafruit_Faux86 is an Arduino library with example to run Faux86, based on excellent work at
- https://github.com/moononournation/T-Deck/blob/main/esp32-faux86/esp32-faux86.ino
- Faux86-remake: https://github.com/moononournation/Faux86-remake.git which is based on https://github.com/ArnoldUK/Faux86-remake. Since it is not indexed by arduino libraries (at the moment), you may need manually clone/download it to your library folder. Note: on Linux, you may need to make change as https://github.com/ArnoldUK/Faux86-remake/pull/5

Example can run MS-DOS 6.x + Windows 3.0 and can several DOS games such as Dangerous Dave (could hang after a while), Prince of Persia, Mario, Wolf3d etc ... and work with USB host shield (max3421e) to support USB keyboard, mouse.

## TODO

Currently only supported and tested with ESP32-S3.
- [ ] Support ESP32 and/or ESP32-S2
- [ ] Support other MCUs, maybe rp2040
- [ ] Support StdioDiskInterface via SDCard since not all MCUs have 16 MB flash to store disk images
- [ ] Improve video, x86 emulation performance since it is kind of slow (big task !!!)

## Requirements

This library depends on the following libraries:
- [Faux86-remake](https://github.com/moononournation/Faux86-remake.git) Note: on Linux, you may need to make change as https://github.com/ArnoldUK/Faux86-remake/pull/5
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library) and your specific TFT library controller e.g. Adafruit_ILI9341. Optionally if using unsupported controllers (e.g ILI9342) by Adafruit library, [Arduino_GFX](https://github.com/moononournation/Arduino_GFX.git) can also be used.
- [Adafruit TinyUSB](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) for USB keyboard support with host shield max3421e

## Running example

Out of the box disk images is provided at `disks/` folder, and can be modified by mounting with following command under Linux. For windows/mac you may need to use different tools/commands.

```
sudo mount -o loop,offset=1048576,uid=1000,gid=1000 -t vfat /path/to/hd0_12m_dos_games.img ~/mnt
```

For creating disk image from scratch, you can run [XTulator](https://github.com/mikechambers84/XTulator) and do your own installation.

### ESP32-S3

esp32-faux86.ino is the example for ESP32-S3. Since the sketch use around 1MB of flash, and MSDOS 6.x occupies 6-7MB, it is recommended to have at least 16 MB of flash using default "**16M Flash(2M APP/12.5MB FATFS)**" partition scheme. To run example:

- From IDE menu select: 
  - "USB CDC On Boot:  "Enable"
  - Partition Scheme: "16M Flash(2M APP/12.5MB FATFS)"
- uploaded one of img file in disks/ to ESP32-S3 using [arduino-esp32fs-plugin](https://github.com/lorol/arduino-esp32fs-plugin) 
  - Install arduino-esp32fs-plugin
  - Create a folder named "data" in the sketch folder
  - Copy one of the img file in disks/ to "data" folder
  - From IDE menu select: "Tools" -> "ESP32 Sketch Data Upload" then select "FatFS" then click "OK"
- Edit the sketch to select correct disk image filename

    ```C
      // harddisk drive image: can be found in disks/ folder
      //vmConfig.diskDriveC = hostInterface.openFile("/ffat/hd0_12m_win30.img");
      vmConfig.diskDriveC = hostInterface.openFile("/ffat/hd0_12m_games.img");
    ```
- Compile and upload sketch to ESP32-S3