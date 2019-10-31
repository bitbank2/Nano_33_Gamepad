Nano 33 Gamepad<br>
Project started 10/26/2019<br>
Copyright (c) 2019 BitBank Software, Inc.<br>
Written by Larry Bank<br>
bitbank@pobox.com<br>
<br>
![Nano 33 Gamepad](/demo.jpg?raw=true "Nano 33 Gamepad")
<br>
The purpose of this code is to use BLE HID gamepads with the Arduino 33 BLE
The Nano 33 BLE libraries don't include any HID support, so this code is
very rudamentary and instead of properly parsing the HID reports, it makes
assumptions to work with 2 popular devices (ACGAM R1 and MINI PLUS).
There is a #define switch to send the output to either the serial monitor
or a 128x64 SSD1306 OLED display. The code was written to learn about HID
over GATT and how difficult it would be to implement a minimal HID host
using the Nano 33 BLE board.
<br>

