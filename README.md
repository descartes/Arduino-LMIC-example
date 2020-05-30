# Arduino-LMIC-example

This is a working setup for an ATmega328 with a RFM95 LoRa radio using the MCCI LoRaWAN LMIC library version 2.2.2 compiled on my iMac using Arduino 1.8.9. It's for our temp & humidity sensor. I've included more complex examples as well.

# Contents

* [A bit more info](#a-bit-more-info)
* [More examples](#more-examples)
* [Work in progress](#work-in-progress)
* [Why I am not mad](#this-is-madness)
* [Is it reliable?](#is-it-reliable)
* [I still use this](#do-you-still-use-this)
* [Can you help?](#can-i-help)

## A bit more info

I use a small PCB to mount the RFM95 with an Arduino Pro Mini connected on the other side. Minimum connections are:

3.3V, Gnd, Antenna, SPI plus DIO0 and DIO1. You can set which pins the two DIO's are connected to.

I have a number of gateways from TTN (indoor), RAK (833 & 7246, both Pi based) and Yandex (Russian built Semtech chipset on OrangePi) that the nodes are currently talking to, using The Things Community Network as the back end.

The only thing that's changed in this source code from an actual working compile are the ABP keys, so if you use your own, use the Arduino 1.8.9 IDE with the MCCI LMIC v2.2.2, you should have a working node.

## More examples

The TinyThingInABottle is the first PCB based node we built and has a rudimentary alarm system - being LoRaWAN we didn't get as far as alarm ack or having it throttle back it's uplink if it hits an alarm situation.

The TinyThingOutside has a string of DS18B20's attached.

## Work in progress

Looking at the code as I prepare to upload to GitHub, I can see some tidy up that's needed and I should really finish off the alarms.

And my favourite bugbear of GitHub, I need to sort the missing design/roadmap/building blocks document so you don't have to read all the code to see how it fits together. And the how's & why's of turning the sensors on & off via IO.

If I can tidy up the mess that Kicad makes when you import an Eagle project, I will share the PCB. Or at least the v1 PCB from Eagle.

## This is madness

Context is everything, so before writing this repository & me off as a random piece of madness, read this:

I launch High Altitude Balloons with schools, so I was already doing point to point LoRa with a Pro Mini, a RFM95 & a GPS module as a tracker.

So it was a natural progression to change the code over to use a LoRaWAN stack for some random monitoring where I wasn't going to lose sleep over the nodes going missing, being trodden on or forgotten.

I found the MCCI LMIC v2 worked well and worked quickly.

Subsequently, I spent DAYS testing different LMIC libraries & their different versions. I've read all of the source code & put debug printf's in to most versions of the  MCCI library to see what's going on. Mostly I found that v2.2.2 just worked and it went a bit complicated after that.

It was all working fine mid-2019 and then I found that the Arduino IDE updates caused the compile to exceed the flash on the ATmega328.

As has been pointed out, 32K flash and 4K RAM is a bit of a constraint for running a proper LoRaWAN stack and for less money you can buy better micro-controllers. This is entirely correct, but Arduino's are upbiquitous and there is an endless amount of support resources & tutorials on the internet for those looking to make a simple low-cost node.

There is the new Arduino Every Nano that looks & talks like an Arduino Uno/Nano/Pro Mini but with an ATmega4809 it has 48K flash and 6K RAM. I have used this on a co-developer project to very good effect, but still using the same software combo. This gave room for a three channel current sensor INA3221, multiple DS18B20's, a small OLED display, ultrasonic water depth sensor, light level, generator RPM and more!

## Is it reliable?

Short answer: Yes.

The first PCB based node had a TMP36 (analog temp), a LDR and a LED, was put in small plastic bottle with an upturned yogurt pot on top, started 23 Oct 2019 15:17:05 and stuck outside. Battery reading from two Lithium AA cells previously used on a HAB flight was 3.302V.

As of 30 May 2020, it has sent 120,630 messages, has never sent a reboot flag and the dashboard reports last weeks average battery voltage of 3.202V.

As this node flashes an LED (very very briefly) every 8 seconds (the maximum sleep duration for an ATmega328), a couple of times when it's checking sensors (every 60s) and 3 times when it sends (every 5m) and I appear to have left the serial port running, I'd consider this a win.

There is a rudimentary alarm facility built in and on a few occasions it's been triggered when the bush that it is sat in was trimmed and the sun cooked the temp sensor, resulting in an uplink every 60s.

There are two temp/humidity sensors in the office area, each has sent over 49,500 uplinks (as above, 30 May 2020).

The outdoor sensor has four DS18B20's, a LDR and a soil moisture sensor and is at 45,678 (again, 30 May 2020).  

## Do you still use this?

Yes. For small simple nodes that aren't system critical and easy to get to to replace if neccessary, why not.

Also good for throw-away or items unlikely to come back if sent out 'on loan'.

And great for a gateway monitor aka a canary (in a coal mine) - a small node that send a very small message every so often, to prove that the whole gateway from radio to back-haul is fully operational.

At less than 10GBP for parts and a few minutes of the production managers soldering time, it's proved useful.

However, the software is a bit "me only" - so for co-development of Proof of Concept work, I use Arduino with a RAK4200 module. The RAK module provides a serial interface (works great over SoftSerial) that takes all the issues of the LMIC in software away. This way the client can hack away at their ideal sensor setup using all the Arduino libraries and sensible websites as resource.

## Can I help?

Please post on TTN forum, tag me (@descartes) and we'll take it from there. 

Serious flaws highlighted via an issue will be addressed when I can. Feel free to fork, fix & send a pull request.

If I have a compelling commercial reason to, I may enhance & extend this setup, get in touch and we can see what makes sense.
