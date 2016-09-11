#include <SPI.h>
#include <Adafruit_WINC1500.h>

// Below is for second UART
#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function

// Setup wifi
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2

Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);

char ssid[] = "Handle-Talkie";      //  created AP name

int status = WL_IDLE_STATUS;
Adafruit_WINC1500Server server(80);

typedef enum{
    SO_50 = 0,
    AO_85,
    LILAC
} FmBird;

typedef enum{
    PTT_VHF = 5,
    PTT_UHF = 6,
    PTT_INPUT = 12
} PttPins;

typedef enum{
    VHF_UPLINK = 1,
    UHF_UPLINK = 2
} UplinkType;

FmBird currentBird;

UplinkType uType = VHF_UPLINK;

int currentChannel = 0;
int lastChannel = 0;
int pttState = 0;

float vhfFreq, uhfFreq;

String birdName = "";
String currentPL = "0000";
String channelTag = "";

// Required for second UART
Uart Serial2 (&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2);

void SERCOM1_Handler()
{
    Serial2.IrqHandler();
}

void setup(){
    // Enable wifi module
    pinMode(WINC_EN, OUTPUT);
    digitalWrite(WINC_EN, HIGH);
    
    Serial1.begin(9600);
    Serial2.begin(9600);
    
    // Assign pins 10 & 11 SERCOM functionality
    pinPeripheral(10, PIO_SERCOM);
    pinPeripheral(11, PIO_SERCOM);
    
    // Setup PTT control pins
    pinMode(PTT_VHF, OUTPUT);
    pinMode(PTT_UHF, OUTPUT);
    pinMode(PTT_INPUT, INPUT);
    // PTT inputs are inverted logic, put all in RX
    digitalWrite(PTT_VHF, HIGH);
    digitalWrite(PTT_UHF, HIGH);
    // Set some default
    switchToBird(SO_50);
    
    // Create open network. Change this line if you want to create an WEP network:
    if (WiFi.beginAP(ssid) != WL_CONNECTED) {
        // don't continue
        while (true);
    }
    
    // wait 10 seconds for connection:
    delay(10000);
    // start the web server on port 80
    server.begin();
}

void loop(){
    serverLoop();
    
    pttState = digitalRead(PTT_INPUT);
    
    if(pttState)
    {
        switch (uType) {
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
        digitalWrite(PTT_VHF, HIGH);
        digitalWrite(PTT_UHF, HIGH);
    }
}

void switchToBird(FmBird bird){
    currentBird = bird;
    currentChannel = 0;
    
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
            birdName = "Lilac Sat";
            uType = VHF_UPLINK;
            lastChannel = 4;
        }
            break;
    }
    
    switchToChannel(currentChannel);
}

void channelUp(){
    if(currentChannel >= lastChannel)
        currentChannel = 0;
    else
        currentChannel++;

    switchToChannel(currentChannel);
}

void channelDown(){
    if(currentChannel <= 0)
        currentChannel = lastChannel;
    else
        currentChannel--;

    switchToChannel(currentChannel);
}

void upSat(){
    if (currentBird == SO_50) {
        currentBird = AO_85;
    }
    else if(currentBird == AO_85){
        currentBird = LILAC;
    }
    else if( currentBird == LILAC){
        currentBird = SO_50;
    }
    switchToBird(currentBird);
}

void downSat(){
    if (currentBird == SO_50) {
        currentBird = LILAC;
    }
    else if(currentBird == AO_85){
        currentBird = SO_50;
    }
    else if( currentBird == LILAC){
        currentBird = AO_85;
    }
    switchToBird(currentBird);
}

void switchToChannel(int channel){

    switch(currentBird)
    {
        case SO_50:
        {
            switch (channel) {
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
                    channelTag = "AOS";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.81000;
                }
                    break;

                case 2:
                {
                    channelTag = "AOS + 1";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.80500;
                }
                    break;

                case 3:
                {
                    channelTag = "AOS + 2";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.80000;
                }
                    break;

                case 4:
                {
                    channelTag = "TCA";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.79500;
                }
                    break;

                case 5:
                {
                    channelTag = "LOS - 2";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.79000;
                }
                    break;

                case 6:
                {
                    channelTag = "LOS - 1";
                    currentPL = "0001";
                    vhfFreq = 145.85000;
                    uhfFreq = 436.78500;
                }
                    break;

                case 7:
                {
                    channelTag = "LOS";
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
            switch (channel) {
                case 0:
                {
                    channelTag = "AOS";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.16000;
                }
                    break;

                case 1:
                {
                    channelTag = "AOS + 1";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.16500;
                }
                    break;

                case 2:
                {
                    channelTag = "TCA";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.17000;
                }
                    break;

                case 3:
                {
                    channelTag = "LOS - 1";
                    currentPL = "0001";
                    vhfFreq = 145.98000;
                    uhfFreq = 435.17500;
                }
                    break;

                case 4:
                {
                    channelTag = "LOS";
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
            switch (channel) {
                case 0:
                {
                    channelTag = "AOS";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.21000;
                }
                    break;
                    
                case 1:
                {
                    channelTag = "AOS + 1";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.20500;
                }
                    break;
                    
                case 2:
                {
                    channelTag = "TCA";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.20000;
                }
                    break;
                    
                case 3:
                {
                    channelTag = "LOS - 1";
                    currentPL = "0000";
                    vhfFreq = 144.35000;
                    uhfFreq = 437.19500;
                }
                    break;
                    
                case 4:
                {
                    channelTag = "LOS";
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
    
    
    // Set VHF
    Serial1.print("AT+DMOSETGROUP=1,");         // begin message
    Serial1.print(vhfFreq,4);
    Serial1.print(",");
    Serial1.print(vhfFreq,4);
    Serial1.print(",");
    Serial1.print(currentPL);
    Serial1.print(",");
    
    if(uType == VHF_UPLINK)
        Serial1.print("8");
    else
        Serial1.print("0");

    Serial1.println(",0000");

    // Set UHF change to serial 2
    Serial2.print("AT+DMOSETGROUP=1,");         // begin message
    Serial2.print(uhfFreq,4);
    Serial2.print(",");
    Serial2.print(uhfFreq,4);
    Serial2.print(",");
    Serial2.print(currentPL);
    Serial2.print(",");
    
    if(uType == UHF_UPLINK)
        Serial2.print("8");
    else
        Serial2.print("0");
    
    Serial2.println(",0000");
}

void serverLoop(){
    Adafruit_WINC1500Client client = server.available();   // listen for incoming clients
    
    if (client) {                             // if you get a client,
        String currentLine = "";                // make a String to hold incoming data from the client
        while (client.connected()) {            // loop while the client's connected
            if (client.available()) {             // if there's bytes to read from the client,
                char c = client.read();             // read a byte, then
                if (c == '\n') {                    // if the byte is a newline character
                    
                    // if the current line is blank, you got two newline characters in a row.
                    // that's the end of the client HTTP request, so send a response:
                    if (currentLine.length() == 0) {
                        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                        // and a content-type so the client knows what's coming, then a blank line:
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println();

                        client.print("<style>html, body{top:0px; padding-top:0; margin:auto; position:relative; width:950px; height:100%; }</style>");

                        //"<style type=\"text/css\">#maincontainer{top:0px; padding-top:0; margin:auto; position:relative; width:950px; height:100%; }</style>"
                        
                        client.print("<h1><center>Satellite: ");
                        client.print(birdName);
                        client.print("</center>");

                        client.print("<center><form action=\"/BD\"><input type=\"submit\" value=\"< Satellite\" /></form> <form action=\"/BU\"><input type=\"submit\" value=\"Satellite >\" /></form></center>");

                        
                        client.print("<center>Channel: ");
                        client.print(channelTag);
                        client.print("</center></h1>");
                        
                        client.print("<center><form action=\"/CD\"><input type=\"submit\" value=\"< Channel\" /></form> <form action=\"/CU\"><input type=\"submit\" value=\"Channel >\" /></form></center>");

                        // the content of the HTTP response follows the header:
                        //client.print("<h1><center>Downlink: 436.795 MHz ");
                        //client.print("Uplink: 145.850 MHz</center></h1><br>");

                        // The HTTP response ends with another blank line:
                        client.println();
                        // break out of the while loop:
                        break;
                    }
                    else {      // if you got a newline, then clear currentLine:
                        currentLine = "";
                    }
                }
                else if (c != '\r') {    // if you got anything else but a carriage return character,
                    currentLine += c;      // add it to the end of the currentLine
                }
                
                // Check to see if the client request was "GET /H" or "GET /L":
                if (currentLine.endsWith("GET /BD")) {
                    downSat();
                }
                if (currentLine.endsWith("GET /BU")) {
                    upSat();
                }
                if (currentLine.endsWith("GET /CD")) {
                    channelDown();
                }
                if (currentLine.endsWith("GET /CU")) {
                    channelUp();
                }
            }
        }
        // close the connection:
        client.stop();
    }
}

