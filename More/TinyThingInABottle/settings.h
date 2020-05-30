#define VER_NUM   "v0.1.4"
#define VER_DATE  "2019-10-23"
#define VERSION   "TinyThingInABottle " VER_NUM " " VER_DATE
#define DISP_VER  VER_NUM "\n" VER_DATE

//#define DEBUG
#define TRACE
#define SERIAL_BAUD  115200 

/* Change log

	v0.1.1	2019-07-26    First hacked together release based on MiniThing
	v0.1.2	2019-08-07    Enhanced the LED job for the LP test
  v0.1.3  2019-10-13    Clean up for Things Conference UK
  v0.1.4  2019-10-23    Outside battery powered Thing
	
*/

// -------------------------------------------------------------------------------------------------
// Includes

#include <avr/sleep.h>
#include <avr/wdt.h>

#include <EEPROM.h>

#include <SPI.h>

#include <lmic.h>
#include <hal/hal.h>


// -------------------------------------------------------------------------------------------------
// LoRaWAN settings and so forth

// Schedule data transmission (might become longer due to duty cycle limitations).
const unsigned TX_INTERVAL = 60 * 5;  // Send every x seconds
const unsigned DATA_CHECK_INTERVAL = 60;    // Check data every x seconds


// Data structure and job list
uint8_t mydata[] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
//static osjob_t sendjob;    // Radio transmission
//static osjob_t datajob;   // Data collection, threshold checking
//static osjob_t ledjob;    // Used to flash an LED so we can tell it's alive


// Thing addresses
// AA Power Testing descartes-tinything-v0_1a-sn123456 on descartes-office-test01
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

#define LED_PIN  9
bool LED = true;    // Should we be flashing the LED?
bool LEDon = true;  // Is the LED on or off
#define BLINK101    3 // How often in seconds
#define BLINK102    2  // How long for in MILLI-seconds
#define BLINK103    2 // How often when alarmed in seconds

#define BattV_Input A5

#define Switch_Input 6
bool SwitchPrev = false;  // Used to debounce the switch

#define TMP_Pwr  7
#define TMP_Input  A6

#define LDR_Pwr  8
#define LDR_Input  A7


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


// #################################################################################################
// v1.1 descartes standard trace & debug definitions - turn on & off at the top of this file

#if defined(TRACE) || defined(DEBUG)

  #define SERIAL_BEGIN(baudRate)  Serial.begin(baudRate);
  
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
  #define SERIAL_PRINT(x)     Serial.print(x);
  #define SERIAL_PRINTEND(x)    Serial.println(x);
    
  #define SERIAL_PRINTLN(x)     Serial.println(x);
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
    
  #define SERIAL_BANNER(before, after)  \
    Serial.print(F(before));      \
    Serial.print(F("########################################"));  \
    Serial.print(F(after));

  int freeRam(){
    extern int __heap_start, *__brkval; 
    int v; 
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
  }
  
  #define SERIAL_SHOWRAM()    \
    Serial.print(F("Free RAM = "));   \
    Serial.println(freeRam());
    
#else

  #define SERIAL_BEGIN(baudRate)
  #define SERIAL_TIME()
  #define SERIAL_PRINTSTART(type, x)
  #define SERIAL_PRINT(x)
  #define SERIAL_PRINTEND(x)
  #define SERIAL_PRINTLN(x)
  #define SERIAL_PRINTLINE(type, x)
  #define SERIAL_S(str)
  #define SERIAL_SX(str, val)
  #define SERIAL_BANNER(before, after)
  #define SERIAL_SHOWRAM()
  
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
