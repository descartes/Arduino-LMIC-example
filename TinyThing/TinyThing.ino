#include "settings.h"


// -------------------------------------------------------------------------------------------------
// Application

// Check data task
bool checkData() {
  TRACE_S("checkData")

  setupIO();

  bool sendAble = false;

  int v = Batt_Read();
  if (v < eeData.Batt_Low) {
    TRACE_SX("Battery!", v)
    sendAble = true;
  }

  int x = 0;
  if (eeData.TMP_Mode) {  // Inside range - bizzare but catered for ...
    if ((x >= eeData.TMP_Low) && (x <= eeData.TMP_High)) {
      TRACE_SX("Inside!", v)
      sendAble = true;
    }
  } else {                // Outside range
    if (x < eeData.TMP_Low) {
      TRACE_SX("Low!", x)
      TRACE_SX("TMP_Low", eeData.TMP_Low)
      sendAble = true;
    } else if (x > eeData.TMP_High) {
      TRACE_SX("High!", x)
      TRACE_SX("TMP_High", eeData.TMP_High)
      sendAble = true;
    }
  }

  return sendAble;
}


// Send task
void doSend() {
  TRACE_PRINTLN("")
  TRACE_S("doSend")
  // Check if there is not a current TX/RX job running
  if (LMIC.opmode & OP_TXRXPEND) {
    TRACE_S("OP_TXRXPEND, not sending")

  } else {
    BuildPayload();
    // Prepare upstream data transmission at the next possible time.
    LMIC_setTxData2(1, mydata, sizeof(mydata) - 1, 0);
    DEBUG_S("Packet queued")
  }
  // Next TX is scheduled after TX_COMPLETE event.
}



// Respond to incoming data
void processIncoming() {

  if ((LMIC.dataLen == 9) && LMIC.frame[LMIC.dataBeg]) { // Nine bytes, first one not zero, must be an update
    TRACE_S("eeData update")

    eeData.flags    = LMIC.frame[LMIC.dataBeg];
    eeData.Batt_Low = LMIC.frame[LMIC.dataBeg + 1] * 256 + LMIC.frame[LMIC.dataBeg + 2];
    eeData.TMP_Low = LMIC.frame[LMIC.dataBeg + 3] * 256 + LMIC.frame[LMIC.dataBeg + 4];
    eeData.TMP_High  = LMIC.frame[LMIC.dataBeg + 5] * 256 + LMIC.frame[LMIC.dataBeg + 6];
    eeData.TMP_Mode = LMIC.frame[LMIC.dataBeg + 7];
    eeData.TMP_Hys  = LMIC.frame[LMIC.dataBeg + 8];

    EEPROM.put(0, eeData);

    TRACE_SX("flags", eeData.flags)
    TRACE_SX("Batt_Low", eeData.Batt_Low)
    TRACE_SX("TMP_Low", eeData.TMP_Low)
    TRACE_SX("TMP_High", eeData.TMP_High)
    TRACE_SX("TMP_Mode", eeData.TMP_Mode)
    TRACE_SX("TMP_Hys", eeData.TMP_Hys)

  } else if (LMIC.frame[LMIC.dataBeg]) {
    TRACE_SX("Unknown", LMIC.frame[LMIC.dataBeg])

  } else {
    TRACE_S("0x00 rec'd")

  }
}


// -------------------------------------------------------------------------------------------------
// Read sensors & build payload

int Batt_Read() {
  digitalWrite(BattV_Pwr, HIGH);    // Turn on power to sensor
  analogReference(INTERNAL);
  delay(10);
  int v = analogRead(BattV_Input);  // Read sensor a few times to get the analogRef to settle
  for (byte c = 0; c < 3; c++) {
    //DEBUG_SX("BattV", v)
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

int DS18_Read() {
  digitalWrite(oneWire_Pwr, HIGH);    // Turn on power to sensor
  delay(10);
  DS18B20.requestTemperatures();
  int v = (int) (DS18B20.getTempCByIndex(0) * 100.0);
  digitalWrite(oneWire_Pwr, LOW);     // Turn it off
  return v;
}

void DHT_Start() {
  digitalWrite(DHT_Pwr, HIGH);    // Turn on power to sensor
  delay(100);
  dht.begin();
}

int DHT_Temp() {
  int v = (int)dht.readTemperature();
  //DEBUG_SX("Temp", v)
  return v;
}

int DHT_Humidity() {
  int v = (int)dht.readHumidity();
  //DEBUG_SX("Humidity", v)
  return v;
}

void DHT_Stop() {
  digitalWrite(DHT_Pwr, LOW);     // Turn it off
}



void BuildPayload() {
  int v = 0;

  byte flags = 0; // No swith on this device !digitalRead(Switch_Input);
  if (justBooted) {
    flags += 2;
    justBooted = 0;
  }

  mydata[0] = flags;

  DHT_Start();  // because it takes time to get it's readings together

  v = Batt_Read();
  TRACE_SX("BattV_Value", v)
  mydata[1] = highByte(v);
  mydata[2] = lowByte(v);

  v = DS18_Read();
  TRACE_SX("Temp_Value", v)
  mydata[3] = highByte(v);
  mydata[4] = lowByte(v);

  v = LDR_Read();
  TRACE_SX("LDR_Value", v)
  mydata[5] = highByte(v);
  mydata[6] = lowByte(v);


  v = DHT_Temp();
  TRACE_SX("DHT_Temp", v)
  mydata[7] = v;

  v = DHT_Humidity();
  TRACE_SX("DHT_Humidity", v)
  mydata[8] = v;

  DHT_Stop();

}

// -------------------------------------------------------------------------------------------------
// We turn off all IO when we sleep, this turns the relevant IO back on
void setupIO() {  
  DEBUG_S("setupIO")

  ADCSRA = 134; // Enable ADC
  analogReference(DEFAULT);

  pinMode(LDR_Pwr, OUTPUT);
  pinMode(LDR_Input, INPUT);

  pinMode(BattV_Pwr, OUTPUT);
  pinMode(BattV_Input, INPUT);

  pinMode(DHT_Pwr, OUTPUT);

  pinMode(oneWire_Pwr, OUTPUT);
}


// -------------------------------------------------------------------------------------------------
// Standard lmic event handler
void onEvent (ev_t ev) {
  DEBUG_SX("Time", os_getTime())
  switch (ev) {
    case EV_TXCOMPLETE:
      TRACE_S("EV_TXCOMPLETE")

      if (LMIC.txrxFlags & TXRX_ACK) {
        TRACE_S("Received ack")
      }

      DEBUG_SX("Bytes received", LMIC.dataLen)

      if (LMIC.dataLen) {
        processIncoming();
      }

      DEBUG_PRINTLN("")  // Bit of white space between tx/rx entries

      sleepOK = true;

      // The MCCI LMIC example would normally schedule the next send in it's mini-RTOS
      // But the call to sendJob is now done in the loop as a function of the number of sleeps
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
  
  // Setup the I/O
  setupIO();

  // Setup / initalise DS18B20 and grab serial number
  digitalWrite(oneWire_Pwr, HIGH);    // Turn on power to sensor

  DS18B20.begin();
  Serial.print("DS18B20 serial number: ");
  if (DS18B20.getAddress(theDS18B20, 0)) {
    for (uint8_t i = 0; i < 8; i++) {
      if (theDS18B20[i] < 16) Serial.print("0"); // zero pad the address if necessary
      Serial.print(theDS18B20[i], HEX);
    }
    Serial.println("");
  } else {
    Serial.println("Unable to find address for Device 0");
  }

  DS18B20.requestTemperatures();  // Read to ignore as first temperature is junk
  delay(1000);
  DS18B20.requestTemperatures();
  int v = (int) (DS18B20.getTempCByIndex(0) * 100.0);
  DEBUG_SX("Temp at Setup", v)

  digitalWrite(oneWire_Pwr, LOW);    // Turn off power to sensor


  // LoRa setup .....
  os_init();      // LMIC init
  LMIC_reset();   // Reset the MAC state. Session and pending data transfers will be discarded.

  // Set static session parameters for ABO
  uint8_t appskey[sizeof(APPSKEY)];
  uint8_t nwkskey[sizeof(NWKSKEY)];
  memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
  memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
  LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);

// Show address / keys as required
//  SERIAL_PRINTF("DEVADDR: ")
//  SERIAL_PRINTLN(DEVADDR)
//
//  SERIAL_PRINTF("NWKSKEY: ")
//  for (uint8_t i = 0; i < 16; i++) {
//    // zero pad the address if necessary
//    if (nwkskey[i] < 16) Serial.print("0");
//    Serial.print(nwkskey[i], HEX);
//  }
//  Serial.println("");
//
//  SERIAL_PRINTF("APPSKEY: ")
//  for (uint8_t i = 0; i < 16; i++) {
//    // zero pad the address if necessary
//    if (appskey[i] < 16) Serial.print("0");
//    Serial.print(appskey[i], HEX);
//  }
//  Serial.println("");


  LMIC_setLinkCheckMode(0);   // Disable link check validation
  LMIC_setAdrMode(false);     // Disable ADR
  LMIC.dn2Dr = DR_SF9;        // TTN uses SF9 for its RX2 window.
  LMIC_setDrTxpow(DR_SF7, 14); // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)


  // Setup Alarms
  EEPROM.get(0, eeData);
  TRACE_SX("flags", eeData.flags)
  TRACE_SX("Batt_Low", eeData.Batt_Low)
  TRACE_SX("TMP_Low", eeData.TMP_Low)
  TRACE_SX("TMP_High", eeData.TMP_High)
  TRACE_SX("TMP_Mode", eeData.TMP_Mode)
  TRACE_SX("TMP_Hys", eeData.TMP_Hys)

  TRACE_S("Setup complete")

  doSend(); // Start LoRaWAN job
}

// -------------------------------------------------------------------------------------------------
// Arduino loop

void loop() {

  bool shouldSend = false;

  os_runloop_once();  // Allow the LMIC to do its stuff, even between each sleep cycle

  // if we've moved forward enough loops of 8 seconds to exceed the txInterval
  if (((loops - lastSentLoop) * 8) > txInterval) {    // 8 being the number of seconds asleep
    shouldSend = true;
    TRACE_S("Time to send")
  }


// Uncomment this if you want to use the alarms
// Just be warned you should be adding in some flags to stop it constantly sending at the dataCheckInterval!

//  if (((loops - lastDataCheckLoop) * 8) > dataCheckInterval) {    // 8 being the number of seconds asleep
//    lastDataCheckLoop = loops;
//    if (checkData()) {
//      shouldSend = true;
//      TRACE_S("Alarm!")
//    }
//  }

  // We are using 'shouldSend' so that we can add a push button check in here
  // Plus anything else that you want to trigger a send ...

  if (shouldSend) {
    shouldSend = false;   // Reset flag
    sleepOK = false;      // Do not sleep whilst we are queuing a send and waiting for the receive window - it is reset when complete event is run
    lastSentLoop = loops;
    setupIO();
    doSend();
  }

  if (sleepOK) {
    loops++;
    TRACE_SX("Loops", loops)
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

  // Reduce current by setting the IO in to an appropriate state
  for (byte i = 0; i <= A7; i++) {
    if (i != BattV_Input) {   // Do all but the battery input, for reasons I don't recall right now
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

  // cancel sleep as a precaution
  sleep_disable();

  DEBUG_S("Woken up")

}
