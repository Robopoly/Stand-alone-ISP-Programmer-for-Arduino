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
#include <avr/pgmspace.h>
#include "images.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD display instance: address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// set the spi bus speed, use the slowest speed, higher might not always work
#define SPISPEED SPI128

// low battery value, about 2V
#define LOW_BATTERY 800

// time a message is shown on LCD
#define MSG_TIME 2000

// pin definitions
#define RESET   A5
#define LED     13
#define BUTTON  0
#define BUZZER  8
#define BATTERY A0
// the isp pins are already defined by arduino

// global variables
uint16_t target_signature = 0;
image_t *target_image;
uint8_t imageFormat;

// array containing all the available images
image_t *images[] = {&image_prismino, &image_camera, &image_lcd};

// battery level measurement, initialize to 1023
uint16_t batteryLevel = 1023;

void setup()
{
  // to read battery level use the internal 2.56V reference
  analogReference(INTERNAL);
  // allow 3ms for the new voltage reference to settle
  delay(3);
  
  lcd.init();
  
  // average 16 values for battery reading level ()
  for(uint8_t i = 0; i < 16; i++)
  {
    batteryLevel += analogRead(BATTERY);
  }
  // divide result by 16
  batteryLevel >>= 4;
  
  // check battery state
  if(batteryLevel < LOW_BATTERY)
  {
    lcd.clear();
    lcd.setCursor(2,0);
    lcd.print("LOW BATTERY!");
    lcd.setCursor(0,1);
    lcd.print("<- 2xAA");
    delay(MSG_TIME);
  }
  
  Serial.begin(9600);
  
  pinMode(LED, OUTPUT);
  
  // enable button pull-up
  digitalWrite(BUTTON, HIGH);
}

void loop()
{
  static byte state = 0;
  static long stateTimeout = 0;
  
  // wait for button press (does not use interrupt vector)
  if(!digitalRead(BUTTON))
  {
    // turn on LED for swag
    digitalWrite(LED, HIGH);
  
    // wait for button release
    while(!digitalRead(BUTTON));
    
    // launch the programming sequence
    program();
  
    digitalWrite(LED, LOW);
  }
  
  // wait for user input
  if(Serial.available())
  {
    switch(Serial.read())
    {
      case 'p': program(); break;
      case 'r': read_flash_memory(); break;
      //case 'e': read_eeprom_memory(); break;
      //case 'm': program_memory_test(); break;
      //case 't': eeprom_memory_test(); break;
      case 'w': wipe_all_memory(); break;
      case 'f': read_fuse_bits(); break;
      default: Serial.print("Command not recognised");
    }
  }
  
  if(millis() > stateTimeout)
  {
    // show information on LCD when idle
    lcd.clear();
    switch(state)
    {
      case 0:
        lcd.setCursor(4,0);
        lcd.print("Robopoly");
        lcd.setCursor(3,1);
        lcd.print("programmer");
        break;
      case 1:
        lcd.setCursor(0,0);
        lcd.print("Connect MCU and");
        lcd.setCursor(0,1);
        lcd.print("press \"Prog\"");
        break;
      case 2:
        lcd.setCursor(2,0);
        lcd.print("LOW BATTERY!");
        lcd.setCursor(0,1);
        lcd.print("<- 2xAA");
        break;
    }
    // go to next state
    state++;
    
    // if the battery level is low show it
    if(batteryLevel < LOW_BATTERY)
    {
      state %= 3;
    }
    else
    {
      state %= 2;
    }
    
    // timeout between states
    stateTimeout = millis() + MSG_TIME;
  }
    
  // running average for battery level for low-pass because of boost circuit ripple
  batteryLevel = (batteryLevel * 15 + analogRead(BATTERY)) >> 4;
}

// returns the low, high, extended and lock fuse bits
void read_fuse_bits()
{
  char buffer[40];
  
  Serial.print("Reading fuse bits: ");
  
  enable_memory_access();
  uint8_t low = spi_transaction(0x50, 0x00, 0x00, 0x00);
  uint8_t high = spi_transaction(0x58, 0x08, 0x00, 0x00);
  uint8_t ext = spi_transaction(0x50, 0x08, 0x00, 0x00);
  uint8_t lock = spi_transaction(0x58, 0x00, 0x00, 0x00);
  reset_spi();
  
  sprintf(buffer, "low: %02X, high: %02X, ext: %02X, lock: %02X", low, high, ext, lock);
  Serial.println(buffer);
}

void wipe_all_memory()
{
  if(!enable_memory_access())
  {
    reset_spi();
    return;
  }
  if(!target_identify())
  {
    reset_spi();
    return;
  }
  
  target_erase();
  
  reset_spi();
}

void program()
{
  boolean success = false;
  while(1)
  {
    // first check for connected target and enable memory access
    Serial.println("Enabling memory access");
    if(!enable_memory_access()) break;
    // read the target device code to identify it
    Serial.println("Identifying target");
    if(!target_identify()) break;
    // see if any images are available for this target
    Serial.println("Searching for image");
    if(!target_findimage()) break;
    // erase chip contents
    Serial.println("Erasing memory");
    if(!target_erase()) break;
    // set progamming fuses, lock fuse doesn't matter as it's wiped
    Serial.println("Setting programming fuse bits");
    if(!target_setfuses(target_image->progfuses)) break;
    // actually program the target
    Serial.println("Starting programming");
    if(!target_program())
    {
      break;
    }
    else
    {
      success = true;
    }
    // load final fuse bits
    Serial.println("Setting final fuse bits");
    target_setfuses(target_image->normfuses);
    break;
  }
  
  target_signature = 0;
  
  // reset pin modes
  reset_spi();
  
  if(success)
  {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Programming");
    lcd.setCursor(0,1);
    lcd.print("complete");
    delay(MSG_TIME); 
  }
}

// as per the datasheet the only first command that can be sent is programming enable
boolean enable_memory_access()
{
  // configure spi: enable, master mode, divide clock by a prescaler
  SPCR = (1 << SPE) | (1 << MSTR) | (((SPISPEED >> 1) & 1) << SPR1) | ((SPISPEED & 1) << SPR0);
  // super mega spi light speed boost times two (only if you dare)
  SPSR = ((SPISPEED >> 2) << SPI2X);
  
  // check for target reset pin state
  pinMode(RESET, INPUT);
  if(digitalRead(RESET) != HIGH)
  {
    // the target isn't activly pulling the reset pin to 1
    Serial.println("Error: reset pin not pulled to 1 by target");
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ERROR: MCU not");
    lcd.setCursor(0,1);
    lcd.print("detected");
    delay(MSG_TIME);
        
    return false;
  }
  
  // setup spi pins
  pinMode(RESET, OUTPUT);
  digitalWrite(RESET, HIGH);
  pinMode(SCK, OUTPUT);
  digitalWrite(SCK, LOW);
  delay(50);
  
  // enable memory access by pulling reset pin low and sending programming enable command
  digitalWrite(RESET, LOW);
  // datasheet says to wait at least 20ms at this point
  delay(50);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);
  
  // actually send the programming enable command
  spi_transaction(0xAC, 0x53, 0x00, 0x00);
  
  // now the target device can be messed with
  return true;
}

void reset_spi()
{
  SPCR = 0;
  digitalWrite(MISO, LOW);
  pinMode(MISO, INPUT);
  digitalWrite(MOSI, LOW);
  pinMode(MOSI, INPUT);
  digitalWrite(SCK, LOW);
  pinMode(SCK, INPUT);
  digitalWrite(RESET, LOW);
  pinMode(RESET, INPUT);
}

// example target id
// 95 87
// -+--+
//  |  |
//  |  +- Family, 0x87: ATmega32U4 device (see datasheet)
//  +---- Number, 2^5 (32KB) of memory

boolean target_identify()
{
  // to read a device code send "30 00 ?? 00" data with ?? being 00, 01 or 02
  // 00: vendor code, 1E indicates Atmel, but that's not interesting
  // 01: part family and flash size: "9n" shows 2^n [KB] flash memory
  // 02: part number: the actual interesting part, the mcu signature
  uint8_t family = spi_transaction(0x30, 0x00, 0x01, 0x00);
  uint8_t number = spi_transaction(0x30, 0x00, 0x02, 0x00);
  target_signature = ((family << 8) + number);
  if(target_signature == 0 || target_signature == 0xFFFF)
  {
    Serial.println("Error: target could not be identified");
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ERROR: cannot");
    lcd.setCursor(0,1);
    lcd.print("identify MCU");
    delay(MSG_TIME);
    
    return false;
  } 
  return true;
}

// dump all program memory to serial
void read_flash_memory()
{
  if(!enable_memory_access())
  {
    reset_spi();
    return;
  }
  if(!target_identify())
  {
    reset_spi();
    return;
  }
  
  // the chip has 2^n [kb] of memory, the n is defined in the target signature
  // for the atmega32u4: n = 5 (signature is 0x9587), so it has 2^(5+9) = 16384 * 2B = 32768KB of memory
  const uint16_t memory_end = 1 << (((target_signature >> 8) & 0xF) + 9);
  
  uint8_t high, low;
  char buffer[8];
  
  for(uint16_t i = 0; i < memory_end; i++)
  {
    // show address only every 8 words
    if(!(i % 8))
    {
      // multipy by 2 as each read shows 2 byes (or 1 word)
      sprintf(buffer, "\n%04X : ", i << 1);
      Serial.print(buffer);
    }
    
    // read low and high byte of a program memory address
    low = spi_transaction(0x20, (i >> 8), (i & 0xFF), 0x00);
    high = spi_transaction(0x28, (i >> 8), (i & 0xFF), 0x00);
    sprintf(buffer, "%02X%02X ", low, high);
    Serial.print(buffer);
  }
  Serial.println();
  
  reset_spi();
}

boolean target_findimage()
{
  char name[16] = {0};
      
  // iterate trough all images and find the one matching the signature
  for(uint8_t i = 0; i < sizeof(images) / sizeof(images[0]); i++)
  {
    // read image signature from program memory
    if(pgm_read_word(&(images[i])->signature) == target_signature)
    {
      // found a compatible image
      target_image = images[i];
      Serial.println("Found image for target:");
      Serial.print("\tSignatue: 0x");
      Serial.println(pgm_read_word(&target_image->signature), HEX);
      Serial.print("\tFuses: ");
      Serial.println(pgm_read_dword(&target_image->normfuses), HEX);
      Serial.print("\tPage size: ");
      Serial.println(pgm_read_byte(&target_image->pagesize));
      
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Target found:");
      lcd.setCursor(0,1);
      // Fetch the name of the target from program memory
      for(uint8_t j = 0; j < 16; j++)
      {
        name[j] = (char) pgm_read_byte(&target_image->name[j]);
      }
      lcd.print(name);
      delay(MSG_TIME);
      
      return true;
    }
  }
  Serial.println("Error: no image found for this target");
  
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ERROR: target ");
  lcd.setCursor(0,1);
  lcd.print("not recognised");
  delay(MSG_TIME);
      
  return false;
}

boolean target_erase()
{
  // chip erase erases all current program memory, eeprom and lock bits (not fuse bits)
  spi_transaction(0xAC, 0x80, 0x00, 0x00);
  // poll the rdy/bsy flag to know when memory wipe is done
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  return true;
}

boolean target_setfuses(const uint8_t *fuses)
{
  uint8_t f;
  
  // wait for the target to finish whatever it's doing
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  spi_transaction(0xAC, 0xA0, 0x00, f);
  
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  spi_transaction(0xAC, 0xA8, 0x00, f);
  
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  spi_transaction(0xAC, 0xA4, 0x00, f);
  
  // wait for the fuses to be written before writing the lock bits (this has been an issue)
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  // the lock bits must be programmed last (datasheet p. 348)
  f = pgm_read_byte(&fuses[FUSE_LOCK]);
  spi_transaction(0xAC, 0xE0, 0x00, 0xC0 | f);
  
  while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
  
  return true;
}

// Intel HEX format example, values in hexadecimal (char)
// inefficient data packing: one whole byte (8 bits) for a nibble (4 bits)
// : 10 7000 00 5FC0000078C0000076C0000074C00000 BF
// +--+----+--+--------------------------------+--+
// |  |    |  |                                |  |
// |  |    |  |                                |  +- Checksum (useless)
// |  |    |  |                                +---- Data: 128 bits, 32 nibbles, 16 bytes, 8 words
// |  |    |  +------------------------------------- Record type (only accept 00)
// |  |    +---------------------------------------- Address of the target (in words)
// |  +--------------------------------------------- Byte count, here there are 16 bytes in data
// +------------------------------------------------ Colon, line start marker

// this reads the image and immediately dumps it to the target memory
boolean target_program()
{
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Programming...");
      
  uint16_t address;
  // fetch the starting address of the image
  char *cursor = (char*) pgm_read_word(&target_image->hexcode);
  uint8_t i, len;
  
  const uint16_t pagesize = pgm_read_byte(&target_image->pagesize);
  
  // a trick for initialising the first page address value
  uint16_t current_page = -1;
  
  // this will mask all the words addesses inside a page
  // an example with a page size of 128:
  // 128 = 0b0000 0000 1000 0000
  // 128 - 1 = 127 = 0b0000 0000 0111 1111
  // 127 >> 1 = 63 = 0b0000 0000 0011 1111
  // ~63 = 0b1111 1111 1100 0000 (masks 6 LSB)
  // if the 7th bit changes then it means it's a new page
  const uint16_t pagemask = ~((pagesize - 1) >> 1);
  
  // clock programming time
  const uint32_t time = millis();
  
  // select format, compact mode doesn't use colons as line delimiters
  imageFormat = pgm_read_byte(cursor) == ':' ? FORMAT_HEX : FORMAT_COMPACT;
  
  Serial.println("Programming memory");
  // for each byte in the image
  while(pgm_read_byte(cursor) != '\0')
  {
    // skip the colon marking the start of a line in case of intel hex format
    if(imageFormat == FORMAT_HEX)
    {
      cursor++;
    }
    
    // get length of current line data
    // divide length by 2 as we're going to write words and not bytes to memory
    len = hex2char(cursor) >> 1;
    
    // get the address of where the current data must be written on the target
    address = (hex2char(cursor) << 8) + hex2char(cursor);
    // divide adress by 2 as it indicates words in the program memory
    address >>= 1;
    
    // get the record type
    if(hex2char(cursor))
    {
      // skip all data and checksum if record type is other than data (0x00)
      cursor += imageFormat == FORMAT_HEX ? 4 * len + 2 : 2 * len + 1;
      continue;
    }
    
    // read the data part
    for(i = 0; i < len; i++)
    { 
      // see if we switched to a new page, commits must be done here because sometimes
      // compiled code is non-word-aligned and so a check must be done after each word
      
      // initialise current page variable at start
      if(current_page == -1)
      {
        current_page = (address + i) & pagemask;
      }
      // add local offset to address making it the absolute word address
      // mask all adress bits inside of the page, compare value to current page variable
      else if(current_page != ((address + i) & pagemask))
      {
        // if the current address is the next multiple of a page: commit current page
        spi_transaction(0x4C, current_page >> 8, current_page & 0xFF, 0x00);
        
        // poll the rdy/bsy flag and continue as soon as the page is committed
        while(spi_transaction(0xF0, 0x00, 0x00, 0x00) & 1);
        
        // current page can only be equal to a multiple of a page
        // take care of the mid-line commits with the relative "i"
        current_page = (address + i) & pagemask;
      }
      
      // read the data and immediately send data to target
      spi_transaction(0x40, 0x00, address + i, hex2char(cursor));
      
      // read another byte, transmit to same address, but high byte
      spi_transaction(0x48, 0x00, address + i, hex2char(cursor));
    }
    
    // skip the checksum byte and go to the next line, checksums are for the weak
    cursor += imageFormat == FORMAT_HEX ? 2 : 1;
  }
  
  // commit last (probably unfinished) page
  spi_transaction(0x4C, current_page >> 8, current_page & 0xFF, 0x00);
  
  Serial.print("Time: ");
  Serial.print(millis() - time);
  Serial.println("ms");
  
  return true;
}

// transfer 4 bytes via spi and return the last reply
uint8_t spi_transaction(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
  // send 4 bytes and get a reply
  uint8_t n, m;
  spi_send(a);
  n = spi_send(b);
  m = spi_send(c);
  
  // 2nd byte is always echoed in the 3rd reply byte
  if(m != b)
  {
    Serial.println("Error: spi transaction failed");
    return 0;
  }
  
  return spi_send(d);
}

// send one byte via spi
uint8_t spi_send(uint8_t b)
{
  uint8_t reply;
  // write data to spi
  SPDR = b;
  // wait for the reply
  while(!(SPSR & (1 << SPIF)));
  reply = SPDR;
  return reply;
}

// depending on image format read the contents and increment cursor on address accordingly
uint8_t hex2char(char *&addr)
{
  uint8_t c;
  if(imageFormat == FORMAT_HEX)
  {
    // intel hex is coded on 2 bytes, but only half of which is useful data
    // merge 2 nibbles to get a byte
    c = hexton(pgm_read_byte(addr++));
    c = (c << 4) + hexton(pgm_read_byte(addr++));
  }
  else
  {
    // the compact format stores the image in binary and takes half the space of intel hex
    c = pgm_read_byte(addr++);
  }
  return c;
}

// transform a char to a nibble in case of intel hex format
uint8_t hexton(uint8_t h)
{
  if(h >= '0' && h <= '9')
  {
    return(h - '0');
  }
  if(h >= 'A' && h <= 'F')
  {
    return((h - 'A') + 10);
  }
  return 0;
}

