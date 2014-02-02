# PRismino image

This serves as an example on how to make an image with a bootloader and an initial sketch for Arduino and upload it with the Stand-alone ISP Programmer sketch. This image was made for the [PRsimino](https://github.com/Robopoly/PRismino), the [Swiss Federal Institute of Technology](http://www.epfl.ch) robotics club ([Robopoly](http://robopoly.epfl.ch)) robotics platform based on [Arduino Leonardo](http://arduino.cc/en/Main/arduinoBoardLeonardo).

## Bootloader

To make a bootloader one can use a ready-made one and modify it, in case of [Arduino](https://github.com/arduino/Arduino) all source is open and available, all one has to do is modify it and compile it with `make`. Here the official [Caterina bootloader](https://github.com/arduino/Arduino/tree/master/hardware/arduino/bootloaders/caterina) was used with a couple changes and compiled (`image_bootloader.hex`).

## First sketch

To upload a bootloader and some code one has to merge both, this can be done by simply concatenating the code and the bootloader compiled file contents.

The compiled .hex files are stored in the temporary folder of the computer, the .hex file can be copied from the respective temporary folder of the system (see the Arduino IDE verbose output to know where it is exactly). In this example a sketch that blinks some morse code was made to be used as the initial sketch (`morse_code.hex`).

## Merging

To merge the the bootloader and the initial code one has to simply paste the bootloader code after the initial code in the Intel HEX format, see the example in `image_merged.hex`. The sketch has to be first.

## Refactoring

In this example a micro-controller with 32KB of program memory was used, unfortunately the Intel HEX format, in its original form, takes twice as much space for what's actually useful and the programmer code plus the merged bootloader and initial code take more than 32KB of program memory.

In order to remedy to that a Python script was made to transform that Intel HEX format to binary and merge nibbles together making the final size half of the original.

See the example of `image_merged.txt` refactored in `output.txt`. The refactoring Python code is in `refactor.py`.

To use the Python code call `$python refactor.py [filename]`.

## Programming

Finally when the code is ready, either in Intel HEX or binary format, one can paste it in the programmers sketch for it to be uploaded. See the example in the Stand-alone ISP Programmer sketch.

## Licence

This software is published under [LGPL](http://www.gnu.org/licenses/lgpl.html).