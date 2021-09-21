#define DEBUG // Please comment it if you are not debugging
String githash = "2af2a9c";
String FWversion = "GM";

/*
  G-M Counter for Balloon Fik from Kiruna

ISP
---
PD0     RX
PD1     TX
RESET#  through 50M capacitor to RST#

SDcard
------
DAT3   SS   4 B4
CMD    MOSI 5 B5
DAT0   MISO 6 B6
CLK    SCK  7 B7

ANALOG
------
+      A0  PA0
-      A1  PA1
RESET  0   PB0

LED
---
LED_red  23  PC7         // LED for Dasa


                     Mighty 1284p    
                      +---\/---+
           (D 0) PB0 1|        |40 PA0 (AI 0 / D24)
           (D 1) PB1 2|        |39 PA1 (AI 1 / D25)
      INT2 (D 2) PB2 3|        |38 PA2 (AI 2 / D26)
       PWM (D 3) PB3 4|        |37 PA3 (AI 3 / D27)
    PWM/SS (D 4) PB4 5|        |36 PA4 (AI 4 / D28)
      MOSI (D 5) PB5 6|        |35 PA5 (AI 5 / D29)
  PWM/MISO (D 6) PB6 7|        |34 PA6 (AI 6 / D30)
   PWM/SCK (D 7) PB7 8|        |33 PA7 (AI 7 / D31)
                 RST 9|        |32 AREF
                VCC 10|        |31 GND
                GND 11|        |30 AVCC
              XTAL2 12|        |29 PC7 (D 23)
              XTAL1 13|        |28 PC6 (D 22)
      RX0 (D 8) PD0 14|        |27 PC5 (D 21) TDI
      TX0 (D 9) PD1 15|        |26 PC4 (D 20) TDO
RX1/INT0 (D 10) PD2 16|        |25 PC3 (D 19) TMS
TX1/INT1 (D 11) PD3 17|        |24 PC2 (D 18) TCK
     PWM (D 12) PD4 18|        |23 PC1 (D 17) SDA
     PWM (D 13) PD5 19|        |22 PC0 (D 16) SCL
     PWM (D 14) PD6 20|        |21 PD7 (D 15) PWM
                      +--------+
*/

/*
// Compiled with: Arduino 1.8.9
// MightyCore 2.0.2 https://mcudude.github.io/MightyCore/package_MCUdude_MightyCore_index.json
Fix old bug in Mighty SD library
~/.arduino15/packages/MightyCore/hardware/avr/2.0.2/libraries/SD/src/SD.cpp:
boolean SDClass::begin(uint32_t clock, uint8_t csPin) {
  if(root.isOpen()) root.close();
*/

#include <SD.h>             // Revised version from MightyCore
#include "wiring_private.h"
#include <Wire.h>           
#include "src/RTCx/RTCx.h"  // Modified version icluded
#include "Adafruit_SHT31.h"
#include <avr/wdt.h>

Adafruit_SHT31 sht31 = Adafruit_SHT31();

#define LED_red   23   // PC7
#define GM        25   // PA1
#define GPSpower  26   // PA2
#define SDpower1  1    // PB1
#define SDpower2  2    // PB2
#define SDpower3  3    // PB3
#define SS        4    // PB4
#define MOSI      5    // PB5
#define MISO      6    // PB6
#define SCK       7    // PB7
#define INT       20   // PC4

#define GPSerror 70000*16 // number of cycles for waitig for GPS in case of GPS error 
#define GPSdelay 10    // number of measurements between obtaining GPS position

uint16_t count = 0;
uint32_t serialhash = 0;
struct RTCx::tm tm;

void setup()
{
  //watchdog enable
  wdt_enable(WDTO_8S);

  Wire.setClock(100000);
  
  // Open serial communications
  Serial.begin(9600);
  Serial1.begin(9600);

  Serial.println("#Cvak...");
  
  DDRB = 0b10011110;
  PORTB = 0b00000000;  // SDcard Power OFF

  DDRA = 0b11111100;
  PORTA = 0b00000000;  // SDcard Power OFF
  DDRC = 0b11101100;
  PORTC = 0b00000000;  // SDcard Power OFF
  DDRD = 0b11111100;
  PORTD = 0b00000000;  // SDcard Power OFF

  pinMode(LED_red, OUTPUT);
  digitalWrite(LED_red, LOW);  
  
  for(int i=0; i<5; i++)  
  {
    delay(50);
    digitalWrite(LED_red, HIGH);  // Blink for anouncing start 
    delay(50);
    digitalWrite(LED_red, LOW);  
  }

  Serial.println("#Hmmm...");

  // make a string for device identification output
  String dataString = "$AIRDOS," + FWversion + "," + githash + ","; // FW version and Git hash
  
  Wire.beginTransmission(0x58);                   // request SN from EEPROM
  Wire.write((int)0x08); // MSB
  Wire.write((int)0x00); // LSB
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x58, (uint8_t)16);    
  for (int8_t reg=0; reg<16; reg++)
  { 
    uint8_t serialbyte = Wire.read(); // receive a byte
    if (serialbyte<0x10) dataString += "0";
    dataString += String(serialbyte,HEX);    
    serialhash += serialbyte;
  }

  {
    DDRB = 0b10111110;
    PORTB = 0b00001111;  // SDcard Power ON
  
    // make sure that the default chip select pin is set to output
    // see if the card is present and can be initialized:
    if (!SD.begin(SS)) 
    {
      Serial.println("#Card failed, or not present");
      // don't do anything more:
      return;
    }
  
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) 
    {
      dataFile.println(dataString);  // write to SDcard (800 ms)     
      dataFile.close();
  
      digitalWrite(LED_red, HIGH);  // Blink for Dasa
      Serial.println(dataString);  // print SN to terminal 
      digitalWrite(LED_red, LOW);          
    }  
    // if the file isn't open, pop up an error:
    else 
    {
      Serial.println("#error opening datalog.txt");
    }
    
    DDRB = 0b10011110;
    PORTB = 0b00000001;  // SDcard Power OFF          
  }    
/*
  {
    // Switch off Galileo and GLONASS; UBX-CFG-GNSS (6)+4+8*7+(2)=68 configuration bytes
    const char cmd[68]={0xB5, 0x62, 0x06, 0x3E, 0x3C, 0x00, 0x00, 0x20, 0x20, 0x07, 0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x08, 0x10, 0x00, 0x00, 0x00, 0x01, 0x01, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x03, 0x05, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x05, 0x06, 0x08, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x01, 0x53, 0x1F};
    for (int n=0;n<(68);n++) Serial1.write(cmd[n]); 
  }
*/          
  {
    // airborne <2g; UBX-CFG-NAV5 (6)+36+(2)=44 configuration bytes
    const char cmd[44]={0xB5, 0x62 ,0x06 ,0x24 ,0x24 ,0x00 ,0xFF ,0xFF ,0x07 ,0x03 ,0x00 ,0x00 ,0x00 ,0x00 ,0x10 ,0x27 , 0x00 ,0x00 ,0x05 ,0x00 ,0xFA ,0x00 ,0xFA ,0x00 ,0x64 ,0x00 ,0x5E ,0x01 ,0x00 ,0x3C ,0x00 ,0x00 , 0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x85 ,0x2A};
    for (int n=0;n<(44);n++) Serial1.write(cmd[n]); 
  }
  {
    // switch to UTC time; UBX-CFG-RATE (6)+6+(2)=14 configuration bytes
    const char cmd[14]={0xB5 ,0x62 ,0x06 ,0x08 ,0x06 ,0x00 ,0xE8 ,0x03 ,0x01 ,0x00 ,0x00 ,0x00 ,0x00 ,0x37};
    for (int n=0;n<(14);n++) Serial1.write(cmd[n]); 
  }

  // Initiates RTC
  rtc.autoprobe();
  rtc.resetClock();

  // Initiates SHT31
  sht31.begin(0x44);

  wdt_reset(); //Reset WDT
}


void loop()
{
  for(int i=0; i<(GPSdelay); i++)  // measurements between GPS aquisition
  {
    wdt_reset(); //Reset WDT
    
    int32_t GMcounts = 0;
    // dosimeter integration
    for (uint32_t i=0; i<1000000; i++)    // cca 5 s
    {
      if (digitalRead(GM) == HIGH)
      {
        GMcounts++;
        delayMicroseconds(150); 
      }
    }
    
    wdt_reset(); //Reset WDT

    // Data out
    {
      rtc.readClock(tm);
      RTCx::time_t t = RTCx::mktime(&tm);
      float temperature = sht31.readTemperature();
      float humidity = sht31.readHumidity();

      // make a string for assembling the data to log:
      String dataString = "";
  
      // make a string for assembling the data to log:
      dataString += "$GM,";
  
      dataString += String(count); 
      dataString += ",";
    
      dataString += String(t-946684800); 
      dataString += ",";
  
      dataString += String(GMcounts);
      dataString += ",";

      dataString += String(temperature); 
      dataString += ",";
      dataString += String(humidity); 
  
      count++;

    
     {
        DDRB = 0b10111110;
        PORTB = 0b00001111;  // SDcard Power ON

        // make sure that the default chip select pin is set to output
        // see if the card is present and can be initialized:
        if (!SD.begin(SS)) 
        {
          Serial.println("#Card failed, or not present");
          // don't do anything more:
          return;
        }

        // open the file. note that only one file can be open at a time,
        // so you have to close this one before opening another.
        File dataFile = SD.open("datalog.txt", FILE_WRITE);
      
        // if the file is available, write to it:
        if (dataFile) 
        {
          //digitalWrite(LED_red, HIGH);  // Blink for Dasa
          dataFile.println(dataString);  // write to SDcard (800 ms)     
          //digitalWrite(LED_red, LOW);          
          dataFile.close();
        }  
        // if the file isn't open, pop up an error:
        else 
        {
          Serial.println("#error opening datalog.txt");
        }
  
        DDRB = 0b10011110;
        PORTB = 0b00000001;  // SDcard Power OFF  
      }          

      digitalWrite(LED_red, HIGH);  // Blink for Dasa
      Serial.println(dataString);   // print to terminal (additional 700 ms in DEBUG mode)
      digitalWrite(LED_red, LOW);                
    }    
  }
  
  // GPS **********************
//  if (flux_long>TRESHOLD)
//  if (false)
  {
      // make a string for assembling the data to log:
      String dataString = "";

#define MSG_NO 12    // number of logged NMEA messages

    digitalWrite(GPSpower, HIGH); // GPS Power ON
    delay(100);
    // flush serial buffer
    while (Serial1.available()) Serial1.read();

    char incomingByte; 
    int messages = 0;
    uint32_t nomessages = 0;
    
    // make a string for assembling the NMEA to log:
    dataString = "";

    boolean flag = false;
    messages = 0;
    nomessages = 0;
    while(true)
    {
      if (Serial1.available()) 
      {
        // read the incoming byte:
        incomingByte = Serial1.read();
        nomessages = 0;
        
        if (incomingByte == '$') {flag = true; messages++;};
        if (messages > MSG_NO)
        {
          rtc.readClock(tm);
          RTCx::time_t t = RTCx::mktime(&tm);
          
          dataString += "$TIME,";
          dataString += String(t-946684800);  // RTC Time of the last GPS NMEA Message
          break;
        }
        
        // say what you got:
        if (flag && (messages<=MSG_NO)) dataString+=incomingByte;
      }
      else
      {
        nomessages++;  
        if (nomessages > GPSerror) break; // preventing of forever waiting
      }
    }
    digitalWrite(GPSpower, LOW); // GPS Power OFF

    {
        DDRB = 0b10111110;
        PORTB = 0b00001111;  // SDcard Power ON
        
        // make sure that the default chip select pin is set to output
        // see if the card is present and can be initialized:
        if (!SD.begin(SS)) 
        {
          Serial.println("#Card failed, or not present");
          // don't do anything more:
          return;
        }
        
        // open the file. note that only one file can be open at a time,
        // so you have to close this one before opening another.
        File dataFile = SD.open("datalog.txt", FILE_WRITE);
        
        // if the file is available, write to it:
        if (dataFile) 
        {
          digitalWrite(LED_red, HIGH);  // Blink for Dasa
          dataFile.println(dataString); // write to SDcard (800 ms)     
          digitalWrite(LED_red, LOW);          
          dataFile.close();
        }  
        // if the file isn't open, pop up an error:
        else 
        {
          Serial.println("#error opening datalog.txt");
        }
        
        DDRB = 0b10011110;
        PORTB = 0b00000001;  // SDcard Power OFF
    }  
#ifdef DEBUG
    Serial.println(dataString);  // print to terminal (additional 700 ms)
#endif
  }
}
