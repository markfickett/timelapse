# Outdoor Battery Powered dSLR Time Lapse

## Parts

This is based around a dSLR for the high-quality image sensor and lenses. A Nikon D90 costs around $200 used in 2015, and has connections for a wired remote and external power supply.

### Control board

The control board is Arduino-based, using a bare ATMega328p, with a MOSFET to control camera power and an opto-isolater to control camera exposure.

#### Components

Many of these parts may be found cheaper at Mouser or DigiKey, but SparkFun or Adafruit makes finding the right component and learning to use it easy.

 * ATMega328p [$6 from Sparkfun](https://www.sparkfun.com/products/10524) and 28-DIP socket [$1 from SparkFun](https://www.sparkfun.com/products/7942).
 * optoisolator (2+ channel) [$2 from Mouser](http://www.mouser.com/ProductDetail/Vishay-Semiconductors/ILD2/?qs=sGAEpiMZZMteimceiIVCBwfsK9X9U0O6VGEot9Q9ETk%3d)
 * Chrono Dot real time clock module [$20 from Adafruit](https://www.adafruit.com/products/255)
 * MOSFET [$1 from Sparkfun](https://www.sparkfun.com/products/10213) for camera power control
 * NPN transistor [$1 from Sparkfun](https://www.sparkfun.com/products/521) for LED display power control
 * photoresistor (CdS) [$1 from Adafruit](http://www.adafruit.com/product/161), to sense the camera's activity LED and ambient light levels
 * 5V 1A buck converter for regulated power [$15 from Adafruit](https://www.adafruit.com/product/1065). Idle current of under 2mA.
 * Small tactile switches [$5 for 20 from Adafruit](https://www.adafruit.com/product/1489) for a reset and debug buttons.
 * I2C 7-segment display [$20 from Adafruit](https://www.adafruit.com/product/878), though I might go for the [alphanumeric version](https://www.adafruit.com/product/1911) were I starting again.
 * assorted resistors
 * 22 uF electrolytic capacitor for filtering

I used [SparkFun's $20 Tiny AVR Programmer](https://www.sparkfun.com/products/11801) to [upload to the bare ATMega328](https://www.sparkfun.com/products/11801#comment-561f19cbce395f7a0d8b4567). I soldered [female headers](https://www.adafruit.com/product/598) on the programmer, and used Adafruit's [$4 female-female jumpers](https://www.adafruit.com/product/266) along with [extra-long headers](https://www.adafruit.com/product/400) to jump to the ISP header on the board.

#### Circuit

See the `eagle/` subdirectory for circuit and PCB details including [schematic PDF](eagle/timelapse.sch.pdf).

I designed PCBs using EAGLE's non-commercial free license and autolayout, following [SparkFun's tutorial](https://learn.sparkfun.com/tutorials/using-eagle-schematic), and had them printed by [OSH Park](https://oshpark.com/).

### Off-Board Parts and Connectors

 * 12v to 9v DC regulator [$8 on Amazon](http://www.amazon.com/gp/product/B00A71E52G) for power to camera
 * AC adapter plug (3D printed plug for Nikon D90; [photo](nikond90acplug/151021nikond90acplugendquarter.jpg), [STL file](nikond90acplug/NikonD90AcPlugReducedH.stl), [Thingiverse](https://www.thingiverse.com/thing:1107374/apps/print/))
 * XT60s (power connections, [$2 from Sparkfun](https://www.sparkfun.com/products/10474), compare at HobbyKing)
 * servo cables (peripheral/control connections)
 * lamp cord (battery connection)
 * wired dSLR remote [$6 MC-DC2 clone on Amazon](http://www.amazon.com/gp/product/B003JR8GCU)

### Hardware

A Pelican case provides a sturdy, watertight enclosure which is easy to open and close; and the fins on the hinge side facilitate mounting.

 * Pelican case 1300 NF ($60 on eBay)
 * glass (from a small thrift-store picture frame; attached with JB weld)
 * silica gel (dried and reused)
 * treated wood and screws for mounting
 * 5W solar panel ([$35 on Amazon](https://www.amazon.com/gp/product/B004FOEUI0)) for locations with a fair amount of direct sun, or as overkill, 30W panel ([$60 on Amazon](https://www.amazon.com/gp/product/B00JDRG69K))
 * SLA solar charge controller (this [$10 CMP12 on Amazon](https://www.amazon.com/gp/product/B010FNO9NU) works well, but draws 10mA when idle, even with its indicator LEDs clipped)
 * 12v sealed lead-acid battery (a 4Ah version for home security systems works well; [$20 on Amazon](https://www.amazon.com/dp/B00GYHBACA))
