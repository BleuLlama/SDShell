SDShell
=======

SDShell is meant to be a nice serial interface, a command line
shell, for manipulating files on SD storage cards via a serial
connection.  The intended use would be to hook up your Arduino to
the serial port on a classic computer, like a Tandy 100 "Model T"
Laptop, for example.  From there, you can use direct serial transfers
to type out files, or to capture your files being sent from the
device.

You need to just format a memory card (on your computer, using "SD
Card Formatter", or in your digital camera), and drop it in, and
start it up.

Commands
========

There are quite a few commands currently implemented which you can
use to interact with the SD card, as well as the small (1kbyte)
EEProm in the Micro. (This can be used to transfer files from one
microSD to another.)

- cd ..
- cd /DIRNAME
- cd DIRNAME
 - These will let you go up a directory, to a specific absolute directory, or to a relative directory.
- pwd
 - displays the current directory
- dir
- ls
 - display contents of the current directory
- mkdir DIRNAME
 - create the specified directory
- rmdir DIRNAME
 - remove the specified directory (Not currently working)
- type FILENAME
 - dump the full contents of a file out (binary or ascii, sent raw)
 - the "break" button on the device will cancel this operation in case you type the wrong thing
- hex FILENAME
 - dump the full contents of a file out as a hex dump
 - the "break" button on the device will cancel this operation in case you type the wrong thing
- capt FILENAME
 - capture your typing (or content send) into the specified file.
 - will go until the "break" button is pressed
 - will go until you type a '.' at the beginning of a line.  [return] [.]
 - if file exists, it gets overwritten
- onto FILENAME
 - same as capt, but appends to an existing file.
- rm FILENAME
 - remove the specified file
- etype
 - dump the full contents of the EEPROM out
 - the "break" button on the device will cancel this operation in case you type the wrong thing
- ehex
 - dump the full contents of the EEPROM out as a hex dump
 - the "break" button on the device will cancel this operation in case you type the wrong thing
- ecapt
 - capture your typing (or content send) into the EEPROM
 - will go until the "break" button is pressed
 - starts at the begining of EEPROM space
 - will go until the EEProm fills up
- eonto
 - same as ecapt, but appends to existing content
- erm
 - clear the EEPROM.  Fills it with 0s.
- eload FILENAME
 - load the EEPROM with the contents of FILENAME (will stop when the EEPROM gets filled)
- esave FILENAME
 - save the EEPROM contents out to a file, in its entirety.  If you only have 5 bytes, it fills in the rest with 0s.
- ?
- help
 - display a list of available commands
- vers
 - display SDSH version information
- reset
 - performs a full reset


Swapping Cards
==============

Due to the way the SD card library operates, swapping cards while
the device is running is not supported.  There is a workaround for
this though.  Once all write operations have completed, remove the
card from the slot.  Insert the new card in the slot.  Then press
the RESET button on the Arduino, or type the "reset" command.

This will restart using the new memory card.

If you have small files (1kbyte or so) you can copy them from one
card to another by putting them into the EEProm temporarily.  For
example:

- Insert card A
- Press reset
- "eload FILENAME"
- Wait for it to finish
- Remove card A
- Insert card B
- Press reset
- "esave FILENAME"
- Wait for it to finish

Your Hardware
=============

In order to use this you need to build a little bit of a hardware
stack.  At minimum, you need:

- Arduino with ATmega 328 (168 is too small for the FAT library)
- SD or MicroSD shield with memory card formatted with a FAT filesystem
- RS232 (DB 9 or DB15) to TTL level adapter
- Power Source

The reference version I've used is:
- Arduino Pro 5v '328 (Sparkfun) ($20)
- Seeed Studio SD Card Shield ($15)
- Generic RS232 interface from ebay ($3)
- 4AA Battery Pack ($5)
- 1 Gig MicroSD card ($old)

If your Arduino is a 5v model, your SD sheild needs to have 5v-3.3v
conversion hardware.

I have a slightly modifed version that instead uses the Sparkfun
microsd shield, but I've added the missing 3.3v circuitry, as well
as a button for "break" and "end editing", along with two LEDs for
read/write/status indication.  I wanted to use D13 for indication
of read/write, but it's used for SD card functions, unfortunately.

Another current test setup configuration:
- Arduino Pro Mini 328 (3.3v) from ebay ($3)
- SD to microSD adapter (with wires soldered to it
- RS232 Interface ($3)
- CR2032 battery for power

Which will get the price right around $10 for the hardware.  That,
plus a board to mount it all on, and wire it all up together will
give you the finished device.

Wiring Details
==============

Here's how to hook it all up.

If you're using a SD shield, just plug it in.
- You may need to chage the kSDPin define to use the right CS pin for your hardware. Common values for this are 10 or 8 or 4.

Next is the RS232 interface
- Hook up the RS232 interface's power and ground to the Arduino
- Hook up the RS232's TX to pin D1 of the Arduino
- Hook up the RS232's RX to pin D2 of the Arduino

Now, just hook up the interface to a 9/25, null modem, gender changer
as necessary to connect it to your host computer.


Optional Hardware Wiring
========================

Next you may want to add a break button.
- From pin 7 of the arduino, tie it to +5v/+3.3v through a 10k ohm resistor.
- From pin 7 of the arduino, tie it to one side of a pushbutton switch
- From the other side of the pushbutton switch, tie it to Ground

                     +---\/\/\/---(VCC)
                     |    ___
    (Arduino D7)-----+----o o-----(GND)

Next are the two indicator LEDs, red and green.
- From pin 5, tie it to the anode (longer wire) of the green led
- from the other lead of the green led, tie it to a 1kohm resistor
- from the other lead of the resistor, tie it to ground

    (Arduino D5/6)----|>|----\/\/\/---(GND)

Do likewise for the red led, but connect it to pin 6 of the Arduino
