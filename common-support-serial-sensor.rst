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

.. note:

   The repository https://github.com/peterbarker/sds021 contains the code references in this section.

A next sensible step is to write a parser, in C++, to run on your desktop.  So long as you structure it appropriately, it should be transferable into the ArduPilot code when it becomes time to do that.

Writing the parser to run on your desktop PC has the advantage that you can use all the modern tools for debugging the code - valgrind and gdb are popular examples.

sds021.cpp contains an example program which opens the serial port, parses the stream and emits readings on stdout.

Things to note about the structure of this program:
- the parser has an "update" method which may return and do nothing if there is no input
- the parser never sleeps and never blocks, as doing this may stop the vehicle from flying

Sample output from the program:

::

   20170725185026: PM1.0=9.4 PM2.5=3.9 (bad=0 cksum fails=0)
   20170725185027: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   20170725185028: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   20170725185029: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   20170725185030: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   20170725185031: PM1.0=8.6 PM2.5=3.8 (bad=0 cksum fails=0)
   20170725185032: PM1.0=8.5 PM2.5=3.7 (bad=0 cksum fails=0)
   20170725185033: PM1.0=8.5 PM2.5=3.6 (bad=0 cksum fails=0)

Step 3: preparing to work with ArduPilot
----------------------------------------

Follow the instructions at http://ardupilot.org/dev/docs/where-to-get-the-code.html to get a working ArduPilot development environment.

Create a topic branch for your driver (e.g.):

::

   git checkout -b sds021

Step 4: writing a SITL simulator for the device
-----------------------------------------------

Why a simulator?

 - many developers can debug the driver
 - changes to driver infrastructure can be tested
 - cheaper to crash a simulated vehicle than a real vehicle
 - faster than testing against a real device
 - allows fake inputs from the fake device to test higher levels in the stack (e.g. "bloodhound mode" in this example)

The SDS021 is a serially-connected device.  ArduPilot's SITL has the facility to specify these on the command-line.  The patch titled `SITL: add a simulated sds021 particle sensor` adds a simulated Particle Sensor.  You can attach a simulated device to SITL with a commandline option to `sim_vehicle.py`, and you should find debug information appearing on SITL's `stderr`.

::

  SIM connection ParticleSensor_SDS021:(null)


At this point the simulated device's output looks nothing like the actual device output, it is a "skeleton" of what it will become.  So long as it makes some data available to the device driver we will be writing, it is sufficient; later patches will flesh it out.  At this stage it doesn't need to be pretty, only functional.



Step 5: creating a driver skeleton
----------------------------------

.. note:

   Ardupilot splits the operation of most peripheral devices into a
   "function" driver, where data is manipulated to achieve the task that
   is wanted by a front end funtional library, and a lower level
   "communications" driver, which manages receiving and transmitting
   data.  For peripherals possible to be called by more than one front
   end functional library, it may be better for the device driver layer
   to be further abstracted into a class driver with more than one
   "function" driver calling it.

Your first step is to choose a Frontend driver to create your serial device under.  Examples of Frontend drivers that have serial backends include:

  - AP_RangeFinder
  - AP_ADSB
  - AP_GPS
  - AP_PrecLand

Choose a frontend that most closely matches the functionality of your device.

Step 5a: creating a frontend/backend structure for a new device type
....................................................................

.. note:

   if you have found a Frontend which matches your vehicle type you can skip this step!

Since there was no existing "ParticleSensor" frontend in ArduPilot, I created one.  The frontend defines the access points to the library, so must be generic enough to cover the possible use cases while staying as clean as possible.

.. note:

   For the time being I decided supporting a single particle sensor would be sufficient.  Attempting to get the interface correct for multiple sensors would be adding extra work, and without another sample device would almost certainly be incorrect.

The commit `AP_ParticleSensor: support for particle sensors` shows the creation of the frontend/backend structure.  Note that at this point there's no method to retrieve the data through the interface - that comes later!  The methods implemented in this commit are the bare minimum for a working driver.

Step 5b: creating the skeleton backend driver
.............................................

The commit `SerialManager: add SDS021 serial protocol` adds a unique identifier for the SDS021 protocol.  This number is used by the user to specify the protocol to be spoken on any particular UART.  This is done by setting a `SERIALn_PROTOCOL` - in this tutorial `SERIAL4_PROTOCOL` is the relevant parameter.  So, in this case, the user will need to set `SERIAL4_PROTOCOL` to 14 to enabble the SDS021 particle sensor driver on the simulated uartE port.

The commit `AP_ParticleSensor: support for sds021 particle sensors` adds a skeleton backend driver for the SDS021 particle sensor.

The important parts of this commit are:

  - add detection of the serial device in the frontend's `init` function.  Note that this uses the enumeation value created in the previous commit.
  - a constructor which takes a reference to the serial port UARTDriver
  - port setup (note that in this case the driver is ignoring the other SERIALn_PARAMETERS as the actual physical device can not be reconfigured)
  - an `update()` function which is periodically called by the frontend, and is responsible for parsing input from the device (n.b. this must be fast!  Challenge/response protocols must be done in separate steps.  Fast in this instance means hundreds of microseconds)
  - an `update()` method which (for the time being) simply emits the characters being received from the device

Step 6: Adding the driver to a vehicle
--------------------------------------

`Rover: AP_ParticleSensor support` adds the ParticleSensor frontend to the Rover vehicle.

Important parts of the patch are:

  - having Rover's scheduler call the ParticleSensor frontend's update function at 10Hz
  - Declaration of the ParticleSensor frontend
  - A `#define` which enabled or disabled the feature at compile-time
  - a call to the `init` function of the ParticleSensor frontend; the frontend callsthe backend `init` functions
  - Addition to the ArduPilot build systems

Step 7: testing the parser using SITL
-------------------------------------

At this point there is sufficient structure to test that the simulated device and the device driver can talk to one-another.

::

   ./Tools/autotest/sim_vehicle.py -v APMrover2 --gdb --debug -A --uartE=sim:ParticleSensor_SDS021:

.. note:

   Remember you must set the SERIALn_PROTOCOL parameter to enable the driver!

All going well, when you run the above the ParticleSensor driver should report:

::

   SIM connection ParticleSensor_SDS021:(null)
   SDS021: 4393: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 4493: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 4893: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 5892: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 6893: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 7892: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 8893: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 9892: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)
   SDS021: 10893: PM1.0=8.7 PM2.5=3.9 (bad=0 cksum fails=0)

on the console.

Step 8: testing the parser using SITL and a real device
-------------------------------------------------------

After step 7 our driver is reading from a simulated device which should be speaking the same protocol as the real device.  SITL allows you to make the simple substitution of a real device for that fake device!

When I plug the real device into my laptop I get a serial device:

::

   pbarker@bluebottle:~/rc/ardupilot(sds021)$ ls -l /dev/serial/by-id/*
   lrwxrwxrwx 1 root root 13 Sep  6 16:05 /dev/serial/by-id/usb-1a86_USB2.0-Serial-if00-port0 -> ../../ttyUSB0
   pbarker@bluebottle:~/rc/ardupilot(sds021)$ 

This device can be passed into SITL in place of our fake device:

::

   DEVICE=/dev/serial/by-id/usb-1a86_USB2.0-Serial-if00-port0
   ./Tools/autotest/sim_vehicle.py -v APMrover2 --gdb --debug -A --uartE=uart:$DEVICE

This should produce output which is analagous to the simulator - but with real numbers!

::

   Opened /dev/serial/by-id/usb-1a86_USB2.0-Serial-if00-port0
   SDS021: 4393: PM1.0=1.1 PM2.5=1.0 (bad=0 cksum fails=0)
   SDS021: 4793: PM1.0=1.1 PM2.5=1.0 (bad=0 cksum fails=0)
   SDS021: 5692: PM1.0=1.1 PM2.5=1.0 (bad=0 cksum fails=0)
   SDS021: 6693: PM1.0=1.1 PM2.5=1.0 (bad=0 cksum fails=0)
   SDS021: 7692: PM1.0=1.1 PM2.5=1.0 (bad=0 cksum fails=0)

Naturally these diagnostics will be removed before any pull request against ArduPilot is issued, but printf debugging is still a valid technique!

Step 7: dataflash logging
-------------------------

Step 8: sending the data to the GCS in real time using MAVLink
--------------------------------------------------------------

Step 8.25: creating a simple autotest test
------------------------------------------

Step 8.5: a MAVProxy module to display data received in real-time
-----------------------------------------------------------------

Step 9: integration with the vehicle code - "blood-hound mode"
------------------------------------------------------------

Step 10: Contributing the code back to the ArduPilot community
--------------------------------------------------------------

Another Step 10: Using guided mode based on sensor data gathered in a CC
------------------------------------------------------------------------

Using data gathered directly a companion computer, guided mode is used
via dronekit-python to move the vehicle around

Step 10: integrating an EKF
---------------------------

Step 11: World Domination
-------------------------
