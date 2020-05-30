#define VER_NUM   "v0.1.1"
#define VER_DATE  "2020-05-30"
#define VERSION   "TT Indoor Mk2 " VER_NUM " " VER_DATE


// Un-comment for detailed debug or for trace to give you an overview of what's happening
// However the DS18B20 & DHT libraries with these turned on will push the RAM requirements
// So for final deployment, please please comment both of these out

//#define DEBUG
//#define TRACE

#define SERIAL_BAUD  115200 

/* Change log

  v0.0.1  2019-07-26    First hacked together release based on MiniThing
  v0.1.0  2019-11-10    First version
  v0.1.1  2020-05-30    Tidy for GitHub
	
*/


// -------------------------------------------------------------------------------------------------
// Includes

#include <avr/sleep.h>
#include <avr/wdt.h>

#include <EEPROM.h>

#include <SPI.h>

#include <lmic.h>
#include <hal/hal.h>


// DHT11 - basic temp & humidity sensor
#include "DHT.h"

// DS18B20 - temp 
#include <OneWire.h>
#include <DallasTemperature.h>


// -------------------------------------------------------------------------------------------------
// LoRaWAN settings and so forth

// Schedule data transmission (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 60 * 15;  // Send every x seconds
const unsigned DATA_CHECK_INTERVAL = 150;    // Check data every x seconds


// Data structure and job list
uint8_t mydata[] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };

// Thing addresses
// descartes-tt-indoor-002 on descartes-room-monitor
static const u4_t DEVADDR = 0x12345678; // <-- Change this address for every node!  LoRaWAN end-device address (DevAddr)
static const PROGMEM u1_t NWKSKEY[16] =  { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 }; // LoRaWAN NwkSKey, network session key
static const PROGMEM u1_t  APPSKEY[16] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00 }; // LoRaWAN AppSKey, application session key


// RFM95 Pin mapping
const lmic_pinmap lmic_pins = {
  .nss = 10,
  .rxtx = LMIC_UNUSED_PIN,
  .rst = LMIC_UNUSED_PIN,
  .dio = {3, 2, LMIC_UNUSED_PIN}, // DIO0, DIO1 - these are for the TinyThing PCB
};


// These callbacks are only used in over-the-air activation, so they are left empty here (we cannot leave them out completely unless DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }


// -------------------------------------------------------------------------------------------------
// Infrastructure

unsigned long loops = 0L;       // Loop count - unsigned int expires after 6 days, this expires after 1089 years ...
unsigned long lastSentLoop = 0L;  // The last value of loop that we sent
unsigned long lastDataCheckLoop = 0L;   // The last value of loop that we did something with the LED
bool justBooted = true;         // Have we just booted?
bool sleepOK = false;           // Is it OK to go to sleep

unsigned int txInterval = TX_INTERVAL;
unsigned int dataCheckInterval = DATA_CHECK_INTERVAL;


// -------------------------------------------------------------------------------------------------
// Application / hardware pin mapping

#define LDR_Pwr  A0
#define LDR_Input  A1

#define BattV_Pwr A2
#define BattV_Input A3

// DHT11
#define DHT_Pwr 6
#define DHT_Input 7
#define DHT_Type DHT11
DHT dht(DHT_Input, DHT_Type);

// Dallas DS18B20
#define oneWire_Bus  8
#define oneWire_Pwr  9
DeviceAddress theDS18B20;
OneWire oneWire(oneWire_Bus);  // Setup a oneWire instance
DallasTemperature DS18B20(&oneWire);  // Setup a DS18B20 instance


// Alarms
struct eeDataStructure {
  byte flags;   //  0 = not initalised
  int Batt_Low;
  int TMP_Low;
  int TMP_High;
  byte TMP_Mode; // 0 = outside range, 1 = inside range
  byte TMP_Hys; // subtract/add to reset from alarm - value calculated server side
};

eeDataStructure eeData;
bool Alarmed = false;


/// #################################################################################################
// v1.3 descartes standard trace & debug definitions - turn on & off at the top of this file

// These items are always on so we get a message on startup to identify code base details

#define SERIAL_BEGIN(baudRate)  Serial.begin(baudRate);

#define SERIAL_BANNER(before, after)  \
    Serial.print(F(before));      \
    Serial.print(F("########################################"));  \
    Serial.print(F(after));

#define SERIAL_PRINT(x)     Serial.print(x);
#define SERIAL_PRINTLN(x)   Serial.println(x);
#define SERIAL_PRINTF(x)     Serial.print(F(x));
#define SERIAL_PRINTLNF(x)   Serial.println(F(x));

int freeRam(){
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
  
#define SERIAL_SHOWRAM()    \
    Serial.print(F("Free RAM = "));   \
    Serial.println(freeRam());


// This is where we turn things on and off depending on the flags set
    
#if defined(TRACE) || defined(DEBUG)
  
  #define SERIAL_TIME(type)   \
    Serial.print(type);     \
    Serial.print(F(" "));   \
    Serial.print(F("millis: "));\
    Serial.println(millis());
  
  #define SERIAL_THETIME()    \
    Serial.print(F("theTime: ")); \
    Serial.println(millis());
  
  #define SERIAL_PRINTSTART(type, x)  \
    Serial.print(type);       \
    Serial.print(F(" "));     \
    Serial.print(x);
  #define SERIAL_PRINTEND(x)    Serial.println(x);
    
  #define SERIAL_PRINTLINE(type, x) \
    Serial.print(type);       \
    Serial.print(F(" "));     \
    Serial.println(x);
    
  #define SERIAL_S(type, str)   \
    Serial.print(type);     \
    Serial.print(F(" "));   \
    Serial.print(__FUNCTION__); \
    Serial.print(F(":"));   \
    Serial.print(__LINE__);     \
    Serial.print(F(" "));   \
    Serial.println(F(str));
  
  #define SERIAL_SX(type, str, val)    \
    Serial.print(type);     \
    Serial.print(F(" "));   \
    Serial.print(__FUNCTION__); \
    Serial.print((":"));    \
    Serial.print(__LINE__);     \
    Serial.print(F(" "));   \
    Serial.print(F(str));   \
    Serial.print(F(" = "));   \
    Serial.println(val);
    
  #define PRINTBIN(Num) for (uint32_t t = (1UL<< (sizeof(Num)*8)-1); t; t >>= 1) Serial.write(Num & t ? '1' : '0'); // Prints a binary number with leading zeros (Automatic Handling) 
    
#else

  #define SERIAL_TIME()
  #define SERIAL_PRINTSTART(type, x)
  #define SERIAL_PRINTEND(x)
  #define SERIAL_PRINTLINE(type, x)
  #define SERIAL_S(str)
  #define SERIAL_SX(str, val)

  #define PRINTBIN(Num)

#endif

#ifdef TRACE
  #define TRACE_TIME()    SERIAL_TIME(F("T"))
  #define TRACE_PRINTSTART(x) SERIAL_PRINTSTART(F("T"), x)
  #define TRACE_PRINT(x)    SERIAL_PRINT(x)
  #define TRACE_PRINTEND(x) SERIAL_PRINTEND(x)
  #define TRACE_PRINTLN(x)    SERIAL_PRINTLN(x)
  #define TRACE_PRINTLINE(x)  SERIAL_PRINTLINE(F("T"), x)
  #define TRACE_S(str)    SERIAL_S(F("T"), str) 
  #define TRACE_SX(str, val)  SERIAL_SX(F("T"), str, val)
#else
  #define TRACE_TIME() 
  #define TRACE_PRINTSTART(x)
  #define TRACE_PRINT(x)
  #define TRACE_PRINTEND(x)
  #define TRACE_PRINTLN(x)
  #define TRACE_PRINTLINE(x)
  #define TRACE_S(str)
  #define TRACE_SX(str, val)
#endif

#ifdef DEBUG
  #define DEBUG_TIME()    SERIAL_TIME(F("D"))
  #define DEBUG_PRINTSTART(x) SERIAL_PRINTSTART(F("T"), x)
  #define DEBUG_PRINT(x)    SERIAL_PRINT(x)
  #define DEBUG_PRINTEND(x) SERIAL_PRINTEND(x)
  #define DEBUG_PRINTLN(x)    SERIAL_PRINTLN(x)
  #define DEBUG_PRINTLINE(x)  SERIAL_PRINTLINE(F("T"), x)
  #define DEBUG_S(str)    SERIAL_S(F("D"), str) 
  #define DEBUG_SX(str, val)  SERIAL_SX(F("D"), str, val)
#else
  #define DEBUG_TIME() 
  #define DEBUG_PRINTSTART(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTEND(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTLINE(x)
  #define DEBUG_S(str)
  #define DEBUG_SX(str, val)
#endif
