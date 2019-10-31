//
// Sample sketch to connect a BLE HID Gamepad to the Arduino Nano 33 BLE
// This code doesn't implement a full HID report parser and instead
// supports only the 2 most popular devices I could find (ACGAM R1 and MINI PLUS)
//
// Copyright (c) 2019 BitBank Software, Inc.
// written by Larry Bank (bitbank@pobox.com)
// project started 10/26/2019
//

#include <ArduinoBLE.h>

void ProcessHIDReport(uint8_t *ucData, int iLen);
void ShowMsg(char *msg, int iLine);

//
// Define this to display results on a 128x64 OLED
// otherwise output will be sent to the serial monitor
#define USE_OLED

#ifdef USE_OLED
#include <ss_oled.h>
#endif

enum {
  CONTROLLER_ACGAM_R1 = 0,
  CONTROLLER_MINIPLUS,
  CONTROLLER_UNKNOWN
};

char *szACGAM = (char *)"ACGAM R1          ";
char *szMINIPLUS = (char *)"MINI PLUS";
static int iControllerType;
volatile static int bConnected, bChanged;

// Current button state
// The first 8 bits are for the (up to 8) push buttons
static uint16_t u16Buttons;
// bits to define the direction pad since it's not analog
#define BUTTON_LEFT 0x100
#define BUTTON_RIGHT 0x200
#define BUTTON_UP 0x400
#define BUTTON_DOWN 0x800

// Graphics for the controller real time display
const uint8_t ucArrow[] PROGMEM = {
 0x01,0x80,0x02,0x40,0x04,0x20,0x08,0x10,0x10,0x08,0x20,0x04,0x40,0x02,0x80,0x01,
 0x80,0x01,0xfc,0x3f,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x07,0xe0
};

const uint8_t ucFilledArrow[] PROGMEM = {
 0x01,0x80,0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x7f,0xfe,0xff,0xff,
 0xff,0xff,0xff,0xff,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0,0x07,0xe0
};

const uint8_t ucSquare[] PROGMEM = {
 0x00,0x00,0x00,0x00,0x3f,0xfc,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x04,
 0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x20,0x04,0x3f,0xfc,0x00,0x00,0x00,0x00
};

const uint8_t ucFilledSquare[] PROGMEM = {
 0x00,0x00,0x00,0x00,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,
 0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x3f,0xfc,0x00,0x00,0x00,0x00
};

void setup()
{
#ifdef USE_OLED
  oledInit(OLED_128x64, 0,0, -1, -1, 400000L);
  oledFill(0, 1);
#else
  Serial.begin(115200);
  while (!Serial);
#endif

  ShowMsg("Scanning for BLE ctrl", 0);

  // begin initialization
  if (!BLE.begin())
  {
    ShowMsg("starting BLE failed!", 0);
    while (1);
  }
  // start scanning for peripheral
  BLE.scan();
  
  u16Buttons = 0;
} /* setup() */

void loop()
{
char szTemp[64];

  bConnected = 0;
  // check if a peripheral has been discovered
  BLEDevice peripheral = BLE.available();

  if (peripheral) {
    // discovered a peripheral, print out address, local name, and advertised service
    sprintf(szTemp, "Found %s", peripheral.address().c_str());
    ShowMsg(szTemp,1);
    sprintf(szTemp, "%s - %s", peripheral.localName().c_str(), peripheral.advertisedServiceUuid().c_str());
    ShowMsg(szTemp, 2);


  // Compare it to known controllers
    iControllerType = CONTROLLER_UNKNOWN;
    if (strcmp(peripheral.localName().c_str(), szACGAM) == 0)
       iControllerType = CONTROLLER_ACGAM_R1;
    else if (strcmp(peripheral.localName().c_str(), szMINIPLUS) == 0)
       iControllerType = CONTROLLER_MINIPLUS;
       
    // Check if the peripheral is a HID device
    if (peripheral.advertisedServiceUuid() == "1812" && iControllerType != CONTROLLER_UNKNOWN) {
      // stop scanning
      BLE.stopScan();

      monitorActions(peripheral);

      // peripheral disconnected, start scanning again
      BLE.scan();
      }
    }
} /* loop() */

//
// Callback which handles HID report updates
//
void HIDReportWritten(BLEDevice central, BLECharacteristic characteristic)
{
int iLen, i;
uint8_t ucTemp[128];

  // central wrote new HID report info
  iLen = characteristic.readValue(ucTemp, sizeof(ucTemp));
  ProcessHIDReport(ucTemp, iLen);
} /* HIDReportWritten() */
void monitorActions(BLEDevice peripheral) {

char ucTemp[128];
int i, iLen, iCount;
//BLECharacteristic hidRep[16];
BLEService hidService;
//int ihidCount = 0;

  // connect to the peripheral
  ShowMsg("Connecting...", 4);
  if (peripheral.connect()) {
    ShowMsg("Connected", 5);
  } else {
    ShowMsg("Failed to connect!", 5);
    return;
  }

  // discover peripheral attributes
//  Serial.println("Discovering service 0x1812 ...");
  if (peripheral.discoverService("1812")) {
    ShowMsg("0x1812 discovered", 6);
  } else {
    ShowMsg("0x1812 disc failed", 6);
    peripheral.disconnect();

    while (1);
    return;
  }

  hidService = peripheral.service("1812"); // get the HID service

//  Serial.print("characteristic count = "); Serial.println(hidService.characteristicCount(), DEC);

  iCount = hidService.characteristicCount();
  for (i=0; i<iCount; i++)
  {
    BLECharacteristic bc = hidService.characteristic(i);
//    Serial.print("characteristic "); Serial.print(i, DEC);
//    Serial.print(" = "); Serial.println(bc.uuid());
//    Serial.print("Descriptor count = "); Serial.println(bc.descriptorCount(), DEC);
    if (strcasecmp(bc.uuid(),"2a4D") == 0) // enable notify
    {
//      hidRep[ihidCount++] = bc;
      bc.subscribe();
      bc.setEventHandler(BLEWritten, HIDReportWritten);
    }
  }
    
  BLECharacteristic protmodChar = hidService.characteristic("2A4E"); // get protocol mode characteristic
  if (protmodChar != NULL)
  {
//    Serial.println("Setting Protocol mode to REPORT_MODE");
    protmodChar.writeValue((uint8_t)0x01); // set protocol report mode (we want reports) 
  }

// DEBUG - for now, we're not parsing the report map
//  BLECharacteristic reportmapChar = hidService.characteristic("2A4B");
//  if (reportmapChar != NULL)
//  {
//    iLen = reportmapChar.readValue(ucTemp, 128);
//    Serial.print("Read report map: ");
//    for (i=0; i<iLen; i++)
//    {
//      Serial.print("0x"); Serial.print(ucTemp[i], HEX); Serial.print(", ");
//    }
//    Serial.println(" ");
//  }

// subscribe to the HID report characteristic
//  Serial.println("Subscribing to HID report characteristic(s) ...");
//  if (!ihidCount) {
//    ShowMsg("no HID report characteristics found!", 0);
//    peripheral.disconnect();
//    return;
//  }
//  else
//  {
//    for (i=0; i<ihidCount; i++) // The reports are split among multiple characteristics (different buttons/controls)
//    {
//      hidRep[i].subscribe(); // subscribe to all of them
//      hidRep[i].setEventHandler(BLEWritten, HIDReportWritten);
//    }
//  }
  ShowMsg("Ready to use!", 7);
  delay(2000);
#ifdef USE_OLED
  oledFill(0,1);
#endif
  bChanged = 1; // force a repaint before the controls are touched, otherwise it will look like it's hung
  
  while (peripheral.connected()) {
    // while the peripheral is connected
    if (bChanged)
    {
      bChanged = 0;
#ifdef USE_OLED
      ShowControllerState(u16Buttons); // update OLED graphic display
#else
      sprintf(ucTemp, "Buttons changed: 0x%04x", u16Buttons);
      ShowMsg(ucTemp, 0);
#endif
    }
  } // while connected

//  Serial.println("HID device disconnected!");
} /* monitorActions() */

//
// Convert HID report bytes into
// my button and joystick variables
//
void ProcessHIDReport(uint8_t *ucData, int iLen)
{
int i;

  bChanged = 1;
  switch (iControllerType)
  {
    case CONTROLLER_ACGAM_R1:
      if (iLen == 2) // it always writes 2 byte reports
      {
        // assumes it's in "Game Mode"
        u16Buttons = ucData[0]; // 6 buttons
        i = ucData[1] & 0xc0; // Y axis (0x40 == centered)
        if (i == 0) // up
           u16Buttons |= BUTTON_UP;
        else if (i == 0x80) // down
           u16Buttons |= BUTTON_DOWN;
        i = ucData[1] & 0x30; // X axis (0x10 == centered)
        if (i == 0) // left
           u16Buttons |= BUTTON_LEFT;
        else if (i == 0x20)
           u16Buttons |= BUTTON_RIGHT;
      }
      break;
    case CONTROLLER_MINIPLUS:
      if (iLen == 9) // it always writes 9 byte reports
      {
        // directions
        u16Buttons &= ~(BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT | BUTTON_RIGHT);
        if (ucData[2] == 0x81) // left
           u16Buttons |= BUTTON_LEFT;
        else if (ucData[2] == 0x7f) // right
           u16Buttons |= BUTTON_RIGHT;
        if (ucData[3] == 0x81) // up
           u16Buttons |= BUTTON_UP;
        else if (ucData[3] == 0x7f) // down
           u16Buttons |= BUTTON_DOWN;
        u16Buttons &= 0xff00; // gather other buttons
        u16Buttons |= ucData[0]; // A/B/C/D buttons
        u16Buttons |= (ucData[1] << 3); // start + select
      }
      break;
  } // switch on controller type
} /* ProcessHIDReport() */

#ifdef USE_OLED
//
// Display the current state of the controller
// with graphics on the OLED
//
void ShowControllerState(uint16_t butts)
{
int i;
uint8_t ucDirX[] = {0,32,16,16,16,48,64};
uint8_t ucDirY[] = {4,4,2,6,4,4,4};
uint8_t ucButtX[] = {64, 80, 96, 112, 64, 80, 96, 112};
uint8_t ucButtY[] = {2,2,2,2, 6,6,6,6};
const uint8_t *pEmpty[] = {ucArrow, ucArrow,ucArrow,ucArrow};
const uint8_t *pFilled[] = {ucFilledArrow, ucFilledArrow, ucFilledArrow, ucFilledArrow};
uint8_t ucAngle[] = {ANGLE_270, ANGLE_90, ANGLE_0, ANGLE_FLIPY, ANGLE_0,ANGLE_0,ANGLE_0,ANGLE_0};
uint8_t *pImage;

  // Draw the directional arrows
  for (i=0; i<4; i++)
  {
    if (butts & (0x100 << i))
      pImage = (uint8_t *)pFilled[i];
    else
      pImage = (uint8_t *)pEmpty[i];     
    oledDrawTile((const uint8_t *)pImage, ucDirX[i], ucDirY[i], ucAngle[i], 0, 1);  
  }
  // Print the button bits
  oledWriteString(4, 64,1," 0 1 2 3", FONT_NORMAL, 0, 1); // use the scroll offset to center the numbers
  oledWriteString(4, 64,5," 4 5 6 7", FONT_NORMAL, 0, 1);
  // Draw the buttons
  for (i=0; i<8; i++)
  {
    if (butts & (0x1 << i))
      pImage = (uint8_t *)ucFilledSquare;
    else
      pImage = (uint8_t *)ucSquare;     
    oledDrawTile((const uint8_t *)pImage, ucButtX[i], ucButtY[i], 0, 0, 1);  
  }
} /* ShowControllerstate() */
#endif // USE_OLED
//
// Display a string message
// On the OLED, you pass the line number
// On the serial monitor, it just gets written continuously
//
void ShowMsg(char *msg, int iLine)
{
#ifdef USE_OLED
  int iFont = (strlen(msg) >= 16) ? FONT_SMALL : FONT_NORMAL;
  oledWriteString(0,0,iLine,msg, iFont, 0, 1);
#else
  Serial.println(msg);
#endif  
} /* ShowMsg() */
