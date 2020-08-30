# gbpxl (multi-tone)

**Game Boy Printer XL**

An *invisible* interface between Game Boy Camera and ESC/POS compatible printers with RS232 serial interface. The main project is maintained by Václav Mach (https://github.com/xx0x/gbpxl). A kit is available [at Tindie](https://www.tindie.com/products/xx0x/gbpxl-game-boy-printer-xl-kit/). 
Improvements, suggestions, experience with various printer models are welcome!

**This fork is enhanced to ouput 4-shade prints similar to the original Game Boy Printer using supported multi-tone printers.**

<img src="https://github.com/cristofercruz/gbpxl-multi-tone/raw/master/docs/gbpxl_multi.jpg" width="520" />

## How to use it

Build **gbpxl** using custom PCB kit available on Tindie **Sold Out** (https://www.tindie.com/products/xx0x/gbpxl-game-boy-printer-xl-kit/). The kit is currently sold out, but the gerber file is public, so you can have your PCBs made at PCBWAY, JLCPCB, Elecrow etc. Gerber file and BOM are available here: https://github.com/xx0x/gbpxl/tree/master/schematic-bom-pcb

<img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_1.jpg" width="260" /> <img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_2.jpg" width="260" />

*or*

**Download the code** from this repository and build a custom gbpxl yourself using **Arduino Nano Every** and **TTL-RS232** converter. See the schematic folder at main project repo for the breadboard design (https://github.com/xx0x/gbpxl/tree/master/schematic-bom-pcb).

## Tested with these printers
Unlike the main project, this fork has limited printer support. This fork is coded to work with the few EPSON thermal printers that support multi-tone printing. The following printers have support and or are confirmed working.

| Printer             | Supports Multi-Tone| Confirmed Working |
|:--------------------|:-------------------|:------------------|
|Epson TM-T88V        |Yes                 |Yes                |
|Epson TM-T88VI       |Yes                 |Untested           |
|Epson TM-T70II       |Yes                 |Untested           |

## gbpxl DIP switch settings

|     | DIP1: Scale | DIP2: Cut | DIP3: High Density | DIP4: Margins  |
|:----|:------------|:----------|:-------------------|:---------------|
| ON  |   2x        | Yes       | Increased Density  | Yes            |
| OFF |   1x        | No        | Normal Density     | No             |

### Scale
The original Game Boy Printer outputs at 150dpi. Compatible EPSON printers ouput at 180dpi. 1x outputs at about 83%, 2x outputs at about 166% compared to an original Game Boy Printer print.

### Cut
Cuts the paper when the end feed margin is 3 (typically after each photo).

### High Density
When enabled, increases the density/richness of the light tone. Useful to slow fading when printing on thicker paper like adhesive labels.

### Margins
Feeds paper based on margin specified on the Link > Print > Option screen on Game Boy Camera. See note on cuts when printing with margin 3.
 
 ## Programming gbpxl
 
Since gbpxl uses ATmega4809, the programming must be done via **UPDI** interface, you can't use just your regular USBasp or similar. The UPDI connector uses the pinout based on **[microUPDI project](https://github.com/MCUdude/microUPDI)**, which also includes TX/RX connections for easy communication with PC - useful for transfering images etc.

Programming this fork onto an Arduino Nano Every is straight forward through the Arduino IDE. Programming the custom project PCB can be done using a UDPI programmer. Sadly, since the UPDI is quite new, there are no cheap programmers available yet, but you can build the **[microUPDI](https://github.com/MCUdude/microUPDI)** or **[jtag2updi](https://github.com/ElTangas/jtag2updi)** quite easily by yourself. When uploading, specify "Arduino Nano Every" as your pinout.

<img src="https://github.com/xx0x/gbpxl/raw/master/docs/gbpxl_updi.jpg" width="500" />

**Don't forget to unplug the power before connecting UPDI, since the programmer usually powers the device!**
 
 ## Author for the main project
 
**Václav Mach**
* http://www.xx0x.cz
* http://www.vaclav-mach.cz
 
**gbpxl main repo**
* https://github.com/xx0x/gbpxl
