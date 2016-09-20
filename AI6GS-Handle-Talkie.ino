// Adafruit part 1947, 2.8" TFT with cap touch
#include <Adafruit_GFX.h>    // Core graphics library
#include "Adafruit_ILI9341.h"
#include <SPI.h>
#include <SD.h>
#include <Wire.h>      // this is needed for FT6206
#include <Adafruit_FT6206.h>

// Fonts
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMono12pt7b.h>

// For second UART
#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function

// Bank switching
typedef enum{
    SO_50 = 0,
    AO_85,
    LILAC
} FmBird;

// Configure I/O, could us some #defines
typedef enum{
    PTT_VHF = 5,
    PTT_UHF = 6,
    PTT_INPUT = 13
} PttPins;

// Used to determine which transceiver to key when PTT is pressed
// Also sets the squelch level on the RX side from open to full
typedef enum{
    VHF_UPLINK = 1,
    UHF_UPLINK = 2,
    SIMPLEX = 3
} UplinkType;

// Current state
FmBird currentBird;
UplinkType uType = VHF_UPLINK;
int currentChannel = 0;
int currentVolume = 4;
int lastChannel = 0;
float vhfFreq, uhfFreq;
// Strings for Serial comms to modules
// Others are for TFT display
String birdName = "";
String currentPL = "0000";
String channelTag = "";
// Your call, may need to center down below
String callSign = "AI6GS";

// Where we were the last time the display is refreshed
// We get less flickering this way
// Optimize channel switch speed
float lastVHF, lastUHF;
// Optimize for redraw loop
float lastUp, lastDown;
// Optimize for redraw loop.
String lastSat, lastTag;

// Repeat touch avoidance
boolean wasTouched = true;
unsigned long previousMillis = 0;
const long interval = 500;

// Reconfigure the SERCOM for a second serial port for the UHF module
// Required for second UART
Uart Serial2 (&sercom1, 12, 11, SERCOM_RX_PAD_0, UART_TX_PAD_2);

//  The display configuration
#define TFT_DC 9
#define TFT_CS 10
#define SD_CS A5

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_FT6206 ctp = Adafruit_FT6206();

// Setup interrupts
void SERCOM1_Handler()
{
    Serial2.IrqHandler();
}

void setup()
{
    // Startup the hardware interfaces
    tft.begin();
    ctp.begin(40);
    
    Serial1.begin(9600);
    Serial2.begin(9600);
    
    // Assign pins 10 & 11 SERCOM functionality
    pinPeripheral(11, PIO_SERCOM);
    pinPeripheral(12, PIO_SERCOM);
    
    // Setup PTT control pins
    pinMode(PTT_VHF, OUTPUT);
    pinMode(PTT_UHF, OUTPUT);
    pinMode(PTT_INPUT, INPUT);
    
    // PTT inputs are inverted logic, put all in RX
    digitalWrite(PTT_VHF, HIGH);
    digitalWrite(PTT_UHF, HIGH);
    
    // Load the background and setup the display
    SD.begin(SD_CS);
    bmpDraw("htBG.bmp", 0, 0);
    
    drawStatics();
    tftDrawVolume();
    switchToBird(SO_50);
}

void scanForTouches()
{
    if (ctp.touched() && !wasTouched)
    {
        wasTouched = true;
        // Retrieve a point
        TS_Point p = ctp.getPoint();
        
        // flip it around to match the screen.
        p.x = map(p.x, 0, 240, 240, 0);
        p.y = map(p.y, 0, 320, 320, 0);
        
        // Find the corners
        if(p.x < 80)
        {
            // On the left
            if(p.y < 70)
            {
                // On the top
                channelDown();
            }
            if(p.y > 255)
            {
                // On the bottom
                volumeDown();
            }
        }
        
        if(p.x > 180)
        {
            // On the right
            if(p.y < 70)
            {
                // On the top
                channelUp();
            }
            if(p.y > 255)
            {
                // On the bottom
                volumeUp();
            }
        }
        
    }
}

void loop()
{
    // Make changes if we need to
    scanForTouches();

    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        wasTouched = false;
    }
    
    // Are whe transmitting?
    if(digitalRead(PTT_INPUT))
    {
        // Which module do we key?
        switch (uType)
        {
            case VHF_UPLINK:
            {
                digitalWrite(PTT_VHF, LOW);
            }
                break;
                
            case UHF_UPLINK:
            {
                digitalWrite(PTT_UHF, LOW);
            }
                break;
                
            default:
                break;
        }
    }
    else
    {
        // Ensure we're on RX for all other cases
        digitalWrite(PTT_VHF, HIGH);
        digitalWrite(PTT_UHF, HIGH);
    }
}

// Switch banks
void switchToBird(FmBird bird)
{
    currentBird = bird;
    //currentChannel = 0;
    
    switch(currentBird)
    {
        case SO_50:
        {
            birdName = "SO-50";
            uType = VHF_UPLINK;
            lastChannel = 7;
        }
            break;
            
        case AO_85:
        {
            birdName = "AO-85";
            uType = UHF_UPLINK;
            lastChannel = 4;
        }
            break;
            
        case LILAC:
        {
            birdName = "Lilac";
            uType = VHF_UPLINK;
            lastChannel = 4;
        }
            break;
    }
    switchToChannel(currentChannel);
}

// Volume and channel controls
void volumeUp()
{
    if(currentVolume < 8)
        currentVolume++;
    else
        return;
    tftDrawVolume();
    updateVolume();
}

void volumeDown()
{
    if(currentVolume > 1)
        currentVolume--;
    else
        return;
    tftDrawVolume();
    updateVolume();
}

void channelUp()
{
    if(currentChannel >= lastChannel)
    {
        currentChannel = 0;
        upSat();
        return;
    }
    else
        currentChannel++;
    
    switchToChannel(currentChannel);
}

void channelDown()
{
    if(currentChannel <= 0)
    {
        downSat();
        currentChannel = lastChannel;
        return;
    }
    else
        currentChannel--;
    
    switchToChannel(currentChannel);
}

void upSat()
{
    if (currentBird == SO_50)
    {
        currentBird = AO_85;
    }
    else if(currentBird == AO_85)
    {
        currentBird = LILAC;
    }
    else if( currentBird == LILAC)
    {
        currentBird = SO_50;
    }
    switchToBird(currentBird);
}

void downSat()
{
    if (currentBird == SO_50)
    {
        currentBird = LILAC;
    }
    else if(currentBird == AO_85)
    {
        currentBird = SO_50;
    }
    else if( currentBird == LILAC)
    {
        currentBird = AO_85;
    }
    switchToBird(currentBird);
}

void updateVolume()
{
    // VHF
    Serial1.print("AT+DMOSETVOLUME=");
    Serial1.println(currentVolume);
    // UHF
    Serial2.print("AT+DMOSETVOLUME=");
    Serial2.println(currentVolume);
}

// FIll vars for channel
void switchToChannel(int channel){
    
    switch(currentBird)
    {
        case SO_50:
        {
            switch (channel)
            {
                case 0: // Enable
                {
                    channelTag = "Enable";
                    currentPL = "0003";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.81000;
                }
                    break;
                    
                case 1:
                {
                    channelTag = "AOS   ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.81000;
                }
                    break;
                    
                case 2:
                {
                    channelTag = "AOS+1 ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.80500;
                }
                    break;
                    
                case 3:
                {
                    channelTag = "AOS+2 ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.80000;
                }
                    break;
                    
                case 4:
                {
                    channelTag = "TCA   ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.79500;
                }
                    break;
                    
                case 5:
                {
                    channelTag = "LOS-2 ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.79000;
                }
                    break;
                    
                case 6:
                {
                    channelTag = "LOS-1 ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.78500;
                }
                    break;
                    
                case 7:
                {
                    channelTag = "LOS   ";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.78000;
                }
                    break;
                    
                default:
                    break;
            }
        }
            break;
            
        case AO_85:
        {
            switch (channel)
            {
                case 0:
                {
                    channelTag = "AOS   ";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.16000;
                }
                    break;
                    
                case 1:
                {
                    channelTag = "AOS+1 ";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.16500;
                }
                    break;
                    
                case 2:
                {
                    channelTag = "TCA   ";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.17000;
                }
                    break;
                    
                case 3:
                {
                    channelTag = "LOS-1 ";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.17500;
                }
                    break;
                    
                case 4:
                {
                    channelTag = "LOS   ";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.18000;
                }
                    break;
                    
                default:
                    break;
            }
        }
            break;
            
        case LILAC:
        {
            switch (channel)
            {
                case 0:
                {
                    channelTag = "AOS   ";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.21000;
                }
                    break;
                    
                case 1:
                {
                    channelTag = "AOS+1  ";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.20500;
                }
                    break;
                    
                case 2:
                {
                    channelTag = "TCA   ";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.20000;
                }
                    break;
                    
                case 3:
                {
                    channelTag = "LOS-1 ";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.19500;
                }
                    break;
                    
                case 4:
                {
                    channelTag = "LOS   ";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.19000;
                }
                    break;
                    
                default:
                    break;
            }
        }
            break;
    }
    
    // Setup the modules, make changes only if we need to
    // Set VHF
    if( lastVHF != vhfFreq)
    {
        Serial1.print("AT+DMOSETGROUP=1,");
        Serial1.print(vhfFreq,4);
        Serial1.print(",");
        Serial1.print(vhfFreq,4);
        Serial1.print(",");
        Serial1.print(currentPL);
        Serial1.print(",");
        
        // If we are the uplink then squelch the receiver
        if(uType == VHF_UPLINK)
            Serial1.print("8");
        else
            Serial1.print("0");
        
        Serial1.println(",0000");
    }
    
    // Set UHF
    if(lastUHF != uhfFreq)
    {
        Serial2.print("AT+DMOSETGROUP=1,");
        Serial2.print(uhfFreq,4);
        Serial2.print(",");
        Serial2.print(uhfFreq,4);
        Serial2.print(",");
        Serial2.print(currentPL);
        Serial2.print(",");
        
        // If we are the uplink then squelch the receiver
        if(uType == UHF_UPLINK)
            Serial2.print("8");
        else
            Serial2.print("0");
        
        Serial2.println(",0000");
    }
    // Update the comparison data
    lastVHF = vhfFreq;
    lastUHF = uhfFreq;
    
    tftUpdate();
}

// Static font based drawing, called in setup
void drawStatics()
{
    tft.setFont(&FreeMono12pt7b);
    tft.setTextColor(ILI9341_RED);
    tft.setTextSize(1);
    
    tft.setCursor(3, 84);
    tft.println("Uplink");
    
    tft.setCursor(156, 116);
    tft.println("MHz");
    
    tft.setTextColor(ILI9341_GREEN);
    
    tft.setCursor(3, 144);
    tft.println("Downlink");
    
    tft.setCursor(156, 176);
    tft.println("MHz");
    
    tft.setTextColor(ILI9341_RED);
    tft.setCursor(75, 250);
    tft.println("Volume");
    
    tft.setFont(&FreeMonoBold18pt7b);
    tft.setCursor(64, 220);
    tft.setTextColor(ILI9341_CYAN);
    tft.print(callSign);
}

// Draw the volume control, could use some work
void tftDrawVolume()
{
    int width = 174;
    int bottomY = tft.height() - 3;
    int triangleHeight = 56;
    float barWidth = (float)width;
    int topCornerY = bottomY - triangleHeight;
    float offset = (float)width * ((float)currentVolume / 9.0);
    tft.fillTriangle(65, bottomY, width, (bottomY - triangleHeight), width, bottomY, ILI9341_RED);
    tft.fillRect(65 + (int)offset, topCornerY, (barWidth - (int)offset) - 64, 58, ILI9341_BLACK);
    delay(100); //Remove multifire delay
}

// Update the display
void tftUpdate()
{
    // Decide the links
    float upF, downF;
    
    if (uType == VHF_UPLINK)
    {
        upF = vhfFreq;
        downF = uhfFreq;
    }
    else
    {
        upF = uhfFreq;
        downF = vhfFreq;
    }
    
    // Update on channel  display where it is needed
    if(lastSat != birdName)
    {
        tft.fillRect(62, 0, 112, 30, ILI9341_BLACK);
        tft.setFont(&FreeMonoBold18pt7b);
        tft.setCursor(64,24);
        tft.setTextColor(ILI9341_BLUE);
        tft.setTextSize(1);
        tft.println(birdName);
        tft.setFont();
    }
    
    if(lastTag != channelTag)
    {
        tft.fillRect(62, 39, 112, 24, ILI9341_BLACK);
        tft.setFont(&FreeMonoBold12pt7b);
        tft.setTextColor(ILI9341_BLUE);
        tft.setCursor(64, 56);
        tft.setTextSize(1);
        tft.println(channelTag);
        tft.setFont();
    }
    
    if(lastUp != upF)
    {
        tft.fillRect(0, 92, 152, 28, ILI9341_BLACK);
        tft.setFont(&FreeMonoBold18pt7b);
        tft.setCursor(3, 116);
        tft.setTextColor(ILI9341_RED);
        tft.print(upF, 3);
        tft.setFont();
    }
    
    if(lastDown != downF)
    {
        tft.fillRect(0, 150, 152, 28, ILI9341_BLACK);
        tft.setFont(&FreeMonoBold18pt7b);
        tft.setTextColor(ILI9341_GREEN);
        tft.setCursor(3, 174);
        tft.print(downF, 3);
        tft.setFont();
    }
    
    lastUp = upF;
    lastDown = downF;
    lastSat = birdName;
    lastTag = channelTag;
}

// Bitmap handling
// Shamelessly borrowed from Adafruit tutorial
#define BUFFPIXEL 20

void bmpDraw(char *filename, uint8_t x, uint16_t y)
{
    
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
    uint32_t pos = 0, startTime = millis();
    
    if((x >= tft.width()) || (y >= tft.height())) return;
    
    // Open requested file on SD card
    if ((bmpFile = SD.open(filename)) == NULL)
    {
        return;
    }
    
    // Parse BMP header
    if(read16(bmpFile) == 0x4D42)
    { // BMP signature
        (void)read32(bmpFile);
        (void)read32(bmpFile); // Read & ignore creator bytes
        bmpImageoffset = read32(bmpFile); // Start of image data
        // Read DIB header
        (void)read32(bmpFile);;
        bmpWidth  = read32(bmpFile);
        bmpHeight = read32(bmpFile);
        if(read16(bmpFile) == 1)
        { // # planes -- must be '1'
            bmpDepth = read16(bmpFile); // bits per pixel
            if((bmpDepth == 24) && (read32(bmpFile) == 0))
            { // 0 = uncompressed
                
                goodBmp = true; // Supported BMP format -- proceed!
                
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
                    if(bmpFile.position() != pos)
                    { // Need seek?
                        bmpFile.seek(pos);
                        buffidx = sizeof(sdbuffer); // Force buffer reload
                    }
                    
                    for (col=0; col<w; col++) { // For each pixel...
                        // Time to read more pixel data?
                        if (buffidx >= sizeof(sdbuffer))
                        { // Indeed
                            bmpFile.read(sdbuffer, sizeof(sdbuffer));
                            buffidx = 0; // Set index to beginning
                        }
                        
                        // Convert pixel from BMP to TFT format, push to display
                        b = sdbuffer[buffidx++];
                        g = sdbuffer[buffidx++];
                        r = sdbuffer[buffidx++];
                        tft.pushColor(tft.color565(r,g,b));
                    } // end pixel
                } // end scanline
            } // end goodBmp
        }
    }
    
    bmpFile.close();
}

// These read 16- and 32-bit types from the SD card file.
// BMP data is stored little-endian, Arduino is little-endian too.
// May need to reverse subscript order if porting elsewhere.

uint16_t read16(File &f)
{
    uint16_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read(); // MSB
    return result;
}

uint32_t read32(File &f)
{
    uint32_t result;
    ((uint8_t *)&result)[0] = f.read(); // LSB
    ((uint8_t *)&result)[1] = f.read();
    ((uint8_t *)&result)[2] = f.read();
    ((uint8_t *)&result)[3] = f.read(); // MSB
    return result;
}

