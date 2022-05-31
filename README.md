# Dot Matrix Game Boy Printer
I [(Matt)](https://mattg.co.uk) Have forked [gbpxl](https://github.com/xx0x/gbpxl) (a Game Boy Printer to ESC/POS converter) and messed around with it to get it working with my [Epson LX-350 Dot Matrix Printer](https://www.epson.co.uk/en_GB/products/printers/dot-matrix/lx-350/p/11849), which can accept [ESC/P commands](https://files.support.epson.com/htmldocs/lx350_/lx350_ug/html/apspe_3.htm#S-00900-00300-00100).

I've done this in order to display at [Electromagnetic Field 2022](https://emfcamp.org/)

##Curent Setup
Game Boy Link Cable -> Arduino Nano Every -> TTL-RS232 Converter -> Null Modem Cable -> Epson LX-350 Serial Port.

No DIP Switches or buttons have been installed unlike the original build. Options are manually set in the code, and the update dip function has been commented out.

![Dot Matrix Printer, with Game Boy Camera plugged into it.](https://github.com/MattGrayYes/gbpxl/raw/master/dot%20matrix%20images/full%20setup.jpg)

![EMF 2022 Poster: Matt Gray's Dot Matrix Photo Printer](https://github.com/MattGrayYes/gbpxl/raw/master/dot%20matrix%20images/poster.jpg)

## Issues
This code works completely fine on my normal mini receipt printer. I'm trying to use it on a 9-pin dot-matrix printer, which only supports a subset of the ESC/P commands.

I'm hacking this together with only the slightest amount of decades-old C knowledge, so I don't expect to be able to magically figure it out.

When using the ESC * 3x print function, I can get it to print. I can change values in that function to scale the width, but the height remains fixed at 144 lines. I can then change the 8-dot print density modes to fix the aspect ratio.

* With the default line spacing of 24, there's a gap between each line.
	* Reducing the line spacing to 13 removes the gap.

I'd like to scale it bigger, so it fits the whole width of a page, but in my poking about I couldn't get it to scale the height. I realise i probably "just" need it to repeat each line, but its all using bitwise stuff that I don't understand. Here's what I've worked out so far about how it works:

This is printing using the `ESC *` Select bit image command. See [ESC/P Reference (pages C-177 - C-179)](https://files.support.epson.com/pdf/general/escp2ref.pdf).

There appears to be two versions of this function. one for `ESC/P` and one for `9-pin ESC/P`

* My printer is a 9-pin printer, which only supports `ESC *` Bit Image printing in 8-dot columns.
* 3x printing function uses 8-dot columns, so can print OK.
	* This function only has a scaling value to repeat columns, and scale the width. The height remains constant at 144 lines.
	* The data appears to come 8 lines at a time. I don't know enough about bitwise stuff to get it to repeat lines to increase the height. The best i could do by wanging different numbers into the code was to repeat 8 lines at a time. Which isn't what i wanted :D
* 2x printing function uses 24-dot columns, so doesn't work.
	* Telling this function to print in an 8-dot mode as part of the `ESC *` command doesn't help, It just prints out gobbledegook.
	* I assume this is because the data is structured expecting it to be 24 dot density not.
	* I don't know enough about bitwise stuff to make it work


### Changing the 8-dot density mode
I've ended up using a 3x width scale and density mode 7, (144x72). This provides the closest approximation to the proper aspect ratio

#### Other density modes

`ESC * m` for values (1,5,3) of m

![3 printouts, all of the same height but different width](https://github.com/MattGrayYes/gbpxl/raw/master/dot%20matrix%20images/dot%20matrix%20mode%20153.jpg)





---

# gbpxl - Original Documentation

**Game Boy Printer XL**

An *invisible* interface between Game Boy Camera and ESC/POS compatible printers with RS232 serial interface. A kit available [at Tindie](https://www.tindie.com/products/xx0x/gbpxl-game-boy-printer-xl-kit/). 
Improvements, suggestions, experience with various printer models are welcome!

## Introduction video

[![Video introduction](https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_video.jpg)](http://www.youtube.com/watch?v=J6ziFhM0pQw "Video introduction")

## How to use it

Build your **gbpxl** using kit (see below).

*or*

**Download the code** from this repository and build it yourself using **Arduino Nano Every** and **TTL-RS232** converter. See the schematic folder for the breadboard design. 

## gbpxl kit

**Available here:**

https://www.tindie.com/products/xx0x/gbpxl-game-boy-printer-xl-kit/

**Build instructions:**

https://github.com/xx0x/gbpxl/blob/master/build-instructions/BUILD_INSTRUCTIONS.md

**Gerber file and BOM are available here, if you want to make your own PCBs:**

https://github.com/xx0x/gbpxl/tree/master/schematic-bom-pcb

**How it looks:**

<img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_1.jpg" width="260" /> <img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_2.jpg" width="260" />

## Tested with these printers
Originally intended only for Epson TM-T88 family, but it should work with *most* ESC/POS printers by selecting different DIP settings.

| Printer             | DIP3  | DIP4  |                            
|:--------------------|:-----:|:-----:|
|Epson TM-T88III      |   OFF | ON    |
|Epson TM-T88IV       |   ON  | ON    |
|Wincor Nixdorf TH230 |   ON  | OFF   |
|HPRT PPTII-A         |   OFF | OFF   |


## gbpxl DIP switch settings

|     | DIP1: Scale | DIP2: Cut | DIP3: Baud rate | DIP4: Method   |
|-----|:-----------:|-----------|-----------------|----------------|
| ON  |   3x        | Yes       | 38400           | GS             |
| OFF |   2x        | No        | 9600            | ESC \*         |

### Scale
 * 2x scale is intended for 58 mm thermal printers
 * 3x scale is intended for 80 mm thermal printers

### Cut
Cuts the paper after printing each photo. (Some cheaper printers don't support it.)

### Baud rate
Must be the same as selected by the printer (see the manual). If you are not sure, try 9600.
 * Epson TM-T88 family: DIP switches under metal cover on the bottom.
 * Wincor Nixdorf TH230: Selected in the menu (the menu is activated by holding Feed button while powering the printer on).

### GS method
 * "Gs v 0" if baudrate 9600
 * "Gs ( L" if baudrate 38400


## How to wire the board

The link cable contains VCC as well, so in theory it could be used to power gbpxl. But in reality, the provided voltage/current is not enough to run gbpxl without issues, mostly since the MAX232 chip boosts TTL logic levels to higher voltages used by RS232 interface. That's why I decided to include buck regulator and power the device from printer's accessory port. If you want to power gbpxl directly with 5V, use VCC pad located in UPDI.

### Game Boy Link Connector
Cable at the end which plugs into the Game Boy.

```
  ___________
 |  6  4  2  |                
  \_5__3__1_/     
                            
2. SO (serial output)       |  BROWN / GREEN*
3. SI (serial input)        |  GREEN / BROWN*
5. SC (serial clock)        |  BLUE*
6. GND                      |  RED*
```

\*Color codes for cheap Game Boy Color link cable, which you can buy on eBay etc.
GREEN/BROWN color are swapped in each cable - you must try both variants or use your multimeter.
  
### RJ-12 connector (power from printer's "DK" port)
Cable at the end which plugs into the printer.

```
  ____===____
 |           |
 |___________|
  | | | | | |
  1 2 3 4 5 6 

4. VIN (24 V)
6. GND 
```
Gbpxl board supports 10-30 V as VIN voltage.
 
### ...if using Arduino Every
  
  * SO = Arduino pin 4
  * SI = Arduino pin 3
  * SC = Arduino pin 2
  
 Don't connect RJ-12 to Arduino Nano Every, it's VIN input supports 21 V max!
 
 ## Programming gbpxl
 
Since gbpxl uses ATmega4809, the programming must be done via **UPDI** interface, you can't use just your regular USBasp or similar. The UPDI connector uses the pinout based on **[microUPDI project](https://github.com/MCUdude/microUPDI)**, which also includes TX/RX connections for easy communication with PC - useful for transfering images etc.

Sadly, since the UPDI is quite new, there are no cheap programmers available yet, but you can build the **[microUPDI](https://github.com/MCUdude/microUPDI)** or **[jtag2updi](https://github.com/ElTangas/jtag2updi)** quite easily by yourself. When uploading, specify "Arduino Nano Every" as your pinout.

<img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_updi.jpg" width="500" />

**Don't forget to unplug the power before connecting UPDI, since the programmer usually powers the device!**

 ## Additional stuff
 
 ### BTN
As you can see in the photos and in the schematic, gbpxl also includes a *hidden button*. It's not populated by default, since there was no space for a microswitch, but you can use tweezers or a wired button for more permanent solution. Its' function is to print the last photo (stored in the buffer) again. It's a useful feature for selecting various settings with DIP switches and for development - since you don't have to wait for the transfer between Game Boy and gbpxl.

If you trigger the button when nothing has been printed yet, it will print the photo of the gbpxl author (stored in test_image_custom_frame.h) :-)

 ### LED
 The LED may look kind of useless, since it's hidden inside the case, but it's useful for development and problem solving with data transfer, since it blinks when recieving or sending data. If you want, you can always drill a hole in the plastic connector case to see the LED blinking...
 
 ## Author
 
**Václav Mach**
* http://www.xx0x.cz
* http://www.vaclav-mach.cz
 
 ## Thanks!
 
 Big thanks to Brian Khuu for decoding the GBP protocol
 and making Arduino Game Boy Printer Emulator
 https://github.com/mofosyne/arduino-gameboy-printer-emulator 
