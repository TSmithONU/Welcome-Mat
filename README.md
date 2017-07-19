# Welcome-Mat
Board files and software for WOSU Welcome Mat

The WOSU Welcome Mat consists of 7 lamps with WOSU logos. The goal of this project was to rebuild and update the previous hardware. To do this, used a Teensy 3.2, VS1053 breakout from Adafruit, and a LiDar sensor.
The LiDar sensor is calibrated to set threshold. When that threshold is crossed, it triggers playback of an audio file. The program contains a command set for use on the serial port. This allows the user to run diagnostic commands.
These include recalibration, scanning the SD card, taking a LiDar reading, and more. This repository contains the custom PCB files, as well as the teensy software.
