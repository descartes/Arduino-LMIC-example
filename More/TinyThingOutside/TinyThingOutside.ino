#include "settings.h"


// -------------------------------------------------------------------------------------------------
// Application / Tasks

// Send task
void doSend() {
  TRACE_PRINTLN("")
  TRACE_S("doSend")
  
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    TRACE_S("OP_TXRXPEND, not sending")

  } else {
    
    buildPayload();

    //TRACE_SX("SizeOf",sizeof(mydata))
    
    // Prepare upstream data transmission at the next possible time.
    #ifdef DOSEND
    LMIC_setTxData2(2, mydata, sizeof(mydata), 0);
    #endif
    
    DEBUG_S("Packet queued")
    
  }
  
  // Next TX is scheduled after TX_COMPLETE event.
}


// Respond to incoming data
void processIncoming() {
//
//  if ((LMIC.dataLen == 9) && LMIC.frame[LMIC.dataBeg]) { // Nine bytes, first one not zero, must be an update
//    TRACE_S("eeData update")
//
//    eeData.flags    = LMIC.frame[LMIC.dataBeg];
//    eeData.Batt_Low = LMIC.frame[LMIC.dataBeg + 1] * 256 + LMIC.frame[LMIC.dataBeg + 2];
//    eeData.TMP_Low = LMIC.frame[LMIC.dataBeg + 3] * 256 + LMIC.frame[LMIC.dataBeg + 4];
//    eeData.TMP_High  = LMIC.frame[LMIC.dataBeg + 5] * 256 + LMIC.frame[LMIC.dataBeg + 6];
//    eeData.TMP_Mode = LMIC.frame[LMIC.dataBeg + 7];
//    eeData.TMP_Hys  = LMIC.frame[LMIC.dataBeg + 8];
//
//    EEPROM.put(0, eeData);
//
//    TRACE_SX("flags", eeData.flags)
//    TRACE_SX("Batt_Low", eeData.Batt_Low)
//    TRACE_SX("TMP_Low", eeData.TMP_Low)
//    TRACE_SX("TMP_High", eeData.TMP_High)
//    TRACE_SX("TMP_Mode", eeData.TMP_Mode)
//    TRACE_SX("TMP_Hys", eeData.TMP_Hys)
//
//  } else if (LMIC.frame[LMIC.dataBeg]) {
//    TRACE_SX("Unknown", LMIC.frame[LMIC.dataBeg])
//
//  } else {
//    TRACE_S("0x00 rec'd")
//
//  }

}


// -------------------------------------------------------------------------------------------------
// Read sensors & build payload

int Batt_Read() {
  digitalWrite(BattV_Pwr, HIGH);    // Turn on power to sensor
  analogReference(INTERNAL);
  delay(10);
  int v = analogRead(BattV_Input);  // Read sensor
  for (byte c = 0; c < 3; c++) {
    delay(10);
    v = analogRead(BattV_Input);  // Read sensor
  }
  digitalWrite(BattV_Pwr, LOW);     // Turn it off
  analogReference(DEFAULT);
  return v;
}

int LDR_Read() {
  digitalWrite(LDR_Pwr, HIGH);    // Turn on power to sensor
  delay(10);
  int v = analogRead(LDR_Input);  // Read sensor
  digitalWrite(LDR_Pwr, LOW);     // Turn it off
  return v;
}

int Moist_Read() {
  digitalWrite(Moist_Pwr, HIGH);    // Turn on power to sensor
  delay(50);
  int v = analogRead(Moist_Input);  // Read sensor
  digitalWrite(Moist_Pwr, LOW);     // Turn it off
  return v;
}

int DS18_Read(DeviceAddress deviceAddress) {
  int v = (int) (DS18B20.getTempC(deviceAddress) * 100.0);
  return v;
}


byte buildPayload() {
  DEBUG_S("buildPayload")
  
  int v = 0;
  
  byte flags = 0;
  if (justBooted) {
    flags += 2;
    justBooted = 0;
  }

  mydata[0] = flags;
  
  v = Batt_Read();
  TRACE_SX("BattV_Value", v)
  mydata[1] = highByte(v);
  mydata[2] = lowByte(v);
  
  v = LDR_Read();
  TRACE_SX("LDR_Value", v)
  mydata[3] = highByte(v);
  mydata[4] = lowByte(v);

  v = Moist_Read();
  TRACE_SX("Moist_Value", v)
  mydata[5] = highByte(v);
  mydata[6] = lowByte(v);

  delay(750);
  
  v = DS18_Read(DS18_Box);
  TRACE_SX("Box", v)
  mydata[7] = highByte(v);
  mydata[8] = lowByte(v);

  v = DS18_Read(DS18_75);
  TRACE_SX("75", v)
  mydata[9] = highByte(v);
  mydata[10] = lowByte(v);

  v = DS18_Read(DS18_Gnd);
  TRACE_SX("Gnd", v)
  mydata[11] = highByte(v);
  mydata[12] = lowByte(v);

  v = DS18_Read(DS18_Soil);
  TRACE_SX("Soil", v)
  mydata[13] = highByte(v);
  mydata[14] = lowByte(v);

  digitalWrite(oneWire_Pwr, LOW);
  
}


void setupIO() {
  DEBUG_S("setupIO")

  ADCSRA = 134; // Enable ADC
  analogReference(DEFAULT);

  pinMode(LDR_Pwr, OUTPUT);
  pinMode(LDR_Input, INPUT);

  pinMode(BattV_Pwr, OUTPUT);
  pinMode(BattV_Input, INPUT);

  pinMode(Moist_Pwr, OUTPUT);
  pinMode(Moist_Input, INPUT);
  //digitalWrite(Moist_Pwr, HIGH);

  pinMode(oneWire_Pwr, OUTPUT);
  digitalWrite(oneWire_Pwr, HIGH);
  DS18B20.begin();
  
//  Serial.print("Found ");
//  Serial.print(DS18B20.getDeviceCount(), DEC);
//  Serial.println(" devices.");

  //Serial.print("Device Resolution: ");
  //Serial.println(DS18B20.getResolution(DS18_Soil), DEC); 

//  DS18B20.setResolution(DS18_Box, 12);
//  DS18B20.setResolution(DS18_75, 12);
//  DS18B20.setResolution(DS18_Gnd, 12);
//  DS18B20.setResolution(DS18_Soil, 12);
  
  DS18B20.requestTemperatures();
}

// -------------------------------------------------------------------------------------------------
// Standard lmic event handler

void onEvent (ev_t ev) {
  DEBUG_S("onEvent")

  switch (ev) {
    case EV_TXCOMPLETE:
      TRACE_S("EV_TXCOMPLETE")

      if (LMIC.txrxFlags & TXRX_ACK) {
        TRACE_S("Received ack")
      }

      DEBUG_SX("Bytes received", LMIC.dataLen)

      if (LMIC.dataLen) {
        
#ifdef TRACE
        for (byte c = 0; c < LMIC.dataLen; c++) {
          Serial.print(F("0x"));
          Serial.print(LMIC.frame[LMIC.dataBeg + c], HEX);
          Serial.print(F(" "));
        }
        Serial.println();

        Serial.print(F("RSSI="));
        Serial.print(LMIC.rssi);
        Serial.print(F(", SNR="));
        Serial.println(LMIC.snr);
#endif

        processIncoming();
      }

      DEBUG_PRINTLN("")  // Bit of white space between tx/rx entries

      sleepOK = true;

      // Call to sendJob is now done in the loop as a function of the number of sleeps
      break;

    case EV_TXSTART:
      TRACE_S("EV_TXSTART")
      break;

    default:
      TRACE_SX("Unknown event", ev)
      break;
  }
  DEBUG_S("Event end")
}


// -------------------------------------------------------------------------------------------------
// Arduino setup

void setup() {
  SERIAL_BEGIN(SERIAL_BAUD)
  SERIAL_BANNER("\n\n", "\n\n")
  SERIAL_PRINTLN(VERSION)
  SERIAL_PRINTLN(__FILE__)
  SERIAL_BANNER("\n", "\n\n")
  SERIAL_SHOWRAM()
  SERIAL_PRINTLN("Setup started")

  // Application setup .....
  //mydata[16] = 0x0;
  // Setup the I/O
  setupIO();
  TRACE_SX("DS18 found",DS18B20.getDeviceCount())

  // LoRa setup .....
  os_init();      // LMIC init
  LMIC_reset();   // Reset the MAC state. Session and pending data transfers will be discarded.

  // Set static session parameters for ABO
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x01, DEVADDR, nwkskey, appskey);

  //LMIC_selectSubBand(3);    // Select frequencies range
  LMIC_setLinkCheckMode(0);   // Disable link check validation
  LMIC_setAdrMode(false);     // Disable ADR
  LMIC.dn2Dr = DR_SF9;        // TTN uses SF9 for its RX2 window.
  LMIC_setDrTxpow(DR_SF7,14); // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)

  TRACE_S("Setup complete")

  doSend(); // Start LoRaWAN job

}

// -------------------------------------------------------------------------------------------------
// Arduino loop

void loop() {
  
  os_runloop_once();

  // if we've moved forward enough loops of 8 seconds to exceed the txInterval
  if (((loops - lastSentLoop) * 8) > txInterval) {    // 8 being the number of seconds asleep
    TRACE_S("Time to send")
    #ifdef DEBUG
    SERIAL_SHOWRAM()
    #endif
    
    sleepOK = false;      // Do not sleep whilst we are queuing a send and waiting for the receive window - it is reset when complete event is run
    lastSentLoop = loops;
    
    setupIO();
    doSend();
  }

  if (sleepOK) {
    loops++;
    DEBUG_SX("Loops", loops)
    gotoSleep();
  }

}


// -------------------------------------------------------------------------------------------------
// Low Power - as seen at http://www.gammon.com.au/power

// watchdog interrupt
ISR (WDT_vect) {
   wdt_disable();  // disable watchdog
}  // end of WDT_vect


void gotoSleep() {
  DEBUG_S("gotoSleep")

  // Reduce I/O current
  for (byte i = 0; i <= A7; i++) {
    if (i != BattV_Input) {
      pinMode (i, OUTPUT);
      digitalWrite (i, LOW);
    }
  }

#if defined(TRACE) || defined(DEBUG)
  Serial.flush();
#endif

  // disable ADC
  ADCSRA = 0;  

  // clear various "reset" flags
  MCUSR = 0;     
  // allow changes, disable reset
  WDTCSR = bit (WDCE) | bit (WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit (WDIE) | bit (WDP3) | bit (WDP0);    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog
  
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  noInterrupts ();           // timed sequence follows
  sleep_enable();
 
  // turn off brown-out enable in software
  MCUCR = bit (BODS) | bit (BODSE);
  MCUCR = bit (BODS); 
  interrupts ();             // guarantees next instruction executed
  sleep_cpu ();  

  // Resumes from here on wake up ...
  
  // cancel sleep as a precaution
  sleep_disable();

}
