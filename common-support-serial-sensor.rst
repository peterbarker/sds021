.. _common-supporting-serial-sensor:

==========================
Supporting a Serial Sensor
==========================

ArduPilot supports attaching sensors by serveral different mechanisms; CANbus, SPI bus, i2C bus.

However, a vast number of sensors - particularly sensors which are not particularly high-rate - are connected via serial.  This document described several different ways you might attach such a sensor to your vehicle, read and process data from it, log that data and send that data to connected ground control stations.


The Sample Sampling Sensor
--------------------------

For this tutorial I will be dealing with the SDS021 particle sensor:

<image of sensor goes here>

These sensors are available online for very little money.

The SDS021 samples air particles at various sizes and sends you readings out on its serial port.  The protocol it speaks I found online in the form of this image:

<image of protocol goes here>

The protocol specification is sufficient to get the device working, but whereever possible you want the datasheet.  I found a datasheet for this device https://cdn.sparkfun.com/assets/parts/1/2/2/7/5/SDS021_laser_PM2.5_sensor_specification-V1.0.pdf - however, like most datasheets it is at the very least incomplete, if not misleading.

Step 1: signs of life
---------------------

Given this device came with a USB-to-TTL adapter, checking for signs of life on my laptop seems a good start.  If your device does not come with one, they are not expensive, and having a drawful of them isn't too difficult to arrange.

I used `screen` to connect to the device: screen /dev/ttyUSB0 57600

Yay!  Data is being received, and given this is a binary protocol, weird characters are all good.

Step 2: writing a parser on the desktop
---------------------------------------

A next sensible step is to write a parser, in C++, to run on your desktop.  So long as you structure it appropriately, it should be transferable into the ArduPilot code when it becomes time to do that.

Writing the parser to run on your desktop PC has the advantage that you can use all the modern tools for debugging the code - valgrind and gdb are popular examples.

sds021.cpp contains an example program which opens the serial port, parses the stream and emits readings on stdout.

Things to note about the structure of this program:
- the parser has an "update" method which may return and do nothing if there is no input
- the parser never sleeps and never blocks, as doing this may stop the vehicle from flying

Sample output from the program:

20170725185026: PM1.0=9.4 PM2.5=3.9 (bad=0 cksum fails=0)
20170725185027: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
20170725185028: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
20170725185029: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
20170725185030: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
20170725185031: PM1.0=8.6 PM2.5=3.8 (bad=0 cksum fails=0)
20170725185032: PM1.0=8.5 PM2.5=3.7 (bad=0 cksum fails=0)
20170725185033: PM1.0=8.5 PM2.5=3.6 (bad=0 cksum fails=0)


Step 3: moving the parser into ArduPilot
----------------------------------------
Ardupilot splits the operation of most peripheral devices into a "function" driver,
where data is manipulated to achieve the task that is wanted by a front end funtional library,
and a lower level "communications" driver, which manages receiving and transmitting data.
For peripherals possible to be called by more than one front end functional library, it may be
better for the device driver layer to be further abstracted into a class driver with more than 
one "function" driver calling it.
In Ardupilot serialmanager.cpp manages the allocation and rates of serial ports.  At present there
are 16 different serial settings which may be selected by the user to match the configuration of
their vehicle.  When introducing a new serial device to Ardupilot, it is best to review
serialmanager.cpp and if possible select an already implemented configuration.  You will need to 
note down the specifics of this serial type, as it is needed to implement your parser into a function
or class driver.
Once you have identified and noted down the serial configuration, you then need to identify the front end
functional library that best suits the purpose of your device.  These are found in the AP_Library folder in
the Ardupilot codebase.  AP_Rangefinder is an example where a number of different devices provide optional
back ends to the front end functional library.
The naming convention is AP_libraryname_devicename



Step 4: writing a SITL simulator for the device
------------------------------------------

Step 5: testing the parser using SITL
-------------------------------------

Step 6: testing the parser using SITL and a real device
-------------------------------------------------------

Step 7: dataflash logging
-------------------------

Step 8: sending the data to the GCS in real time
------------------------------------------------

Step 9: integration with the vehicle code
-----------------------------------------
