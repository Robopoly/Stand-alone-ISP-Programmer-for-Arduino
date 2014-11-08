/***************************************************************************************
 *
 * Title:       Stand-alone ISP Programmer for Arduino
 * Version:     v1.2
 * Date:        2014-11-08
 * Author:      Karl Kangur <karl.kangur@epfl.ch>
 * Website:     https://github.com/Robopoly/Stand-alone-ISP-Programmer-for-Arduino
 * Licence:     LGPL
 *
 ***************************************************************************************/
#ifndef ISPPROGRAMMER_H
#define ISPPROGRAMMER_H

// needed for type definitions
#include <Arduino.h>

#define FUSE_LOCK 0 // lock fuse
#define FUSE_LOW  1 // low fuse
#define FUSE_HIGH 2 // high fuse
#define FUSE_EXT  3 // extended fuse

#define FORMAT_HEX     0
#define FORMAT_COMPACT 1

// spi prescalers, the lower the faster
#define SPI128 0b011
#define SPI64  0b111
#define SPI32  0b110
#define SPI16  0b001
#define SPI8   0b101
#define SPI4   0b000
#define SPI2   0b100

// image object
typedef struct image {
    const char name[16];	 // name of the target
    const uint16_t signature;	 // two signature bytes for the mcu
    const uint8_t progfuses[4];  // fuses to set before programming
    const uint8_t normfuses[4];  // fuses to set after programming
    const uint8_t pagesize;	 // page size for flash programming in words
    const char *hexcode;         // pointer to intel hex format image
} image_t;

// forward image declarations
extern image_t PROGMEM image_prismino;
extern image_t PROGMEM image_camera;
extern image_t PROGMEM image_lcd;

#endif
