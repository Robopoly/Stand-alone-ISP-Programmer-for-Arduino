/***************************************************************************************
 *
 * Title:       Morse code blinking the on-board LED spelling "robopoly"
 * Version:     v1.0
 * Date:        2014-02-01
 * Author:      Karl Kangur <karl.kangur@gmail.com>
 * Website:     https://github.com/Robopoly/Stand-alone-ISP-Programmer-for-Arduino
 * Licence:     LGPL
 *
 ***************************************************************************************/
// first "1" indicates start position, 0 is dot, 1 is dash
byte morse[] = {
  0b00001010, // r
  0b00001111, // o
  0b00011000, // b
  0b00001111, // o
  0b00010110, // p
  0b00001111, // o
  0b00010100, // l
  0b00011011  // y
};

#define LED 13

// see http://en.wikipedia.org/wiki/Morse_code for reference
#define MORSE_UNIT    100 // in milliseconds
#define MORSE_DOT     MORSE_UNIT
#define MORSE_DASH    MORSE_UNIT * 3
#define MORSE_LETTER  MORSE_UNIT * 3
#define MORSE_WORD    MORSE_UNIT * 7

int8_t letter, unit, start;

void setup()
{
  pinMode(LED, OUTPUT);
}

void loop()
{
  for(letter = 0; letter < sizeof(morse) / sizeof(byte); letter++)
  {
    // get to the start position
    start = 0;
    for(unit = 7; unit >= 0; unit--)
    {
      if(start)
      {
        digitalWrite(LED, HIGH);
        delay((morse[letter] >> unit) & 1 ? MORSE_DASH : MORSE_DOT);
        digitalWrite(LED, LOW);
        // each letter separated by a dot
        delay(MORSE_UNIT);
      }
      
      // skip the starting indicator
      if(!start && morse[letter] >> unit)
      {
        start = 1;
      }
    }
    delay(MORSE_LETTER);
  }
  delay(MORSE_WORD);
}

