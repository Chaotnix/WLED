/*
  WS2812FX.cpp contains all effect methods
  Harm Aldick - 2016
  www.aldick.org
  LICENSE
  The MIT License (MIT)
  Copyright (c) 2016  Harm Aldick
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  Modified heavily for WLED
*/

#include "FX.h"

#define IBN 5100
#define PALETTE_SOLID_WRAP (paletteBlend == 1 || paletteBlend == 3)


/*
 * No blinking. Just plain old static light.
 */
uint16_t WS2812FX::mode_static(void) {
  fill(SEGCOLOR(0));
  return (SEGMENT.getOption(SEG_OPTION_TRANSITIONAL)) ? FRAMETIME : 500; //update faster if in transition
}


/*
 * Blink/strobe function
 * Alternate between color1 and color2
 * if(strobe == true) then create a strobe effect
 * NOTE: Maybe re-rework without timer
 */
uint16_t WS2812FX::blink(uint32_t color1, uint32_t color2, bool strobe, bool do_palette) {
  uint16_t stateTime = SEGENV.aux1;
  uint32_t cycleTime = (255 - SEGMENT.speed)*20;
  uint32_t onTime = 0;
  uint32_t offTime = cycleTime;

  if (!strobe) {
    onTime = (cycleTime * SEGMENT.intensity) >> 8;
    offTime = cycleTime - onTime;
  }

  stateTime = ((SEGENV.aux0 & 1) == 0) ? onTime : offTime;
  stateTime += 20;

  if (now - SEGENV.step > stateTime)
  {
    SEGENV.aux0++;
    SEGENV.aux1 = stateTime;
    SEGENV.step = now;
  }

  uint32_t color = ((SEGENV.aux0 & 1) == 0) ? color1 : color2;
  if (color == color1 && do_palette)
  {
    for(uint16_t i = 0; i < SEGLEN; i++) {
      setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  } else fill(color);

  return FRAMETIME;
}


/*
 * Normal blinking. 50% on/off time.
 */
uint16_t WS2812FX::mode_blink(void) {
  return blink(SEGCOLOR(0), SEGCOLOR(1), false, true);
}


/*
 * Classic Blink effect. Cycling through the rainbow.
 */
uint16_t WS2812FX::mode_blink_rainbow(void) {
  return blink(color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), false, false);
}


/*
 * Classic Strobe effect.
 */
uint16_t WS2812FX::mode_strobe(void) {
  return blink(SEGCOLOR(0), SEGCOLOR(1), true, true);
}


/*
 * Classic Strobe effect. Cycling through the rainbow.
 */
uint16_t WS2812FX::mode_strobe_rainbow(void) {
  return blink(color_wheel(SEGENV.call & 0xFF), SEGCOLOR(1), true, false);
}


/*
 * Color wipe function
 * LEDs are turned on (color1) in sequence, then turned off (color2) in sequence.
 * if (bool rev == true) then LEDs are turned off in reverse order
 */
uint16_t WS2812FX::color_wipe(bool rev, bool useRandomColors) {
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed)*150;
  uint32_t perc = now % cycleTime;
  uint16_t prog = (perc * 65535) / cycleTime;
  bool back = (prog > 32767);
  if (back) {
    prog -= 32767;
    if (SEGENV.step == 0) SEGENV.step = 1;
  } else {
    if (SEGENV.step == 2) SEGENV.step = 3; //trigger color change
  }

  if (useRandomColors) {
    if (SEGENV.call == 0) {
      SEGENV.aux0 = random8();
      SEGENV.step = 3;
    }
    if (SEGENV.step == 1) { //if flag set, change to new random color
      SEGENV.aux1 = get_random_wheel_index(SEGENV.aux0);
      SEGENV.step = 2;
    }
    if (SEGENV.step == 3) {
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux1);
      SEGENV.step = 0;
    }
  }

  uint16_t ledIndex = (prog * SEGLEN) >> 15;
  uint16_t rem = 0;
  rem = (prog * SEGLEN) * 2; //mod 0xFFFF
  rem /= (SEGMENT.intensity +1);
  if (rem > 255) rem = 255;

  uint32_t col1 = useRandomColors? color_wheel(SEGENV.aux1) : SEGCOLOR(1);
  for (uint16_t i = 0; i < SEGLEN; i++)
  {
    uint16_t index = (rev && back)? SEGLEN -1 -i : i;
    uint32_t col0 = useRandomColors? color_wheel(SEGENV.aux0) : color_from_palette(index, true, PALETTE_SOLID_WRAP, 0);

    if (i < ledIndex)
    {
      setPixelColor(index, back? col1 : col0);
    } else
    {
      setPixelColor(index, back? col0 : col1);
      if (i == ledIndex) setPixelColor(index, color_blend(back? col0 : col1, back? col1 : col0, rem));
    }
  }
  return FRAMETIME;
}


/*
 * Lights all LEDs one after another.
 */
uint16_t WS2812FX::mode_color_wipe(void) {
  return color_wipe(false, false);
}

/*
 * Lights all LEDs one after another. Turns off opposite
 */
uint16_t WS2812FX::mode_color_sweep(void) {
  return color_wipe(true, false);
}


/*
 * Turns all LEDs after each other to a random color.
 * Then starts over with another color.
 */
uint16_t WS2812FX::mode_color_wipe_random(void) {
  return color_wipe(false, true);
}


/*
 * Random color introduced alternating from start and end of strip.
 */
uint16_t WS2812FX::mode_color_sweep_random(void) {
  return color_wipe(true, true);
}


/*
 * Lights all LEDs in one random color up. Then switches them
 * to the next random color.
 */
uint16_t WS2812FX::mode_random_color(void) {
  uint32_t cycleTime = 200 + (255 - SEGMENT.speed)*50;
  uint32_t it = now / cycleTime;
  uint32_t rem = now % cycleTime;
  uint16_t fadedur = (cycleTime * SEGMENT.intensity) >> 8;

  uint32_t fade = 255;
  if (fadedur) {
    fade = (rem * 255) / fadedur;
    if (fade > 255) fade = 255;
  }

  if (SEGENV.call == 0) {
    SEGENV.aux0 = random8();
    SEGENV.step = 2;
  }
  if (it != SEGENV.step) //new color
  {
    SEGENV.aux1 = SEGENV.aux0;
    SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0); //aux0 will store our random color wheel index
    SEGENV.step = it;
  }

  fill(color_blend(color_wheel(SEGENV.aux1), color_wheel(SEGENV.aux0), fade));
  return FRAMETIME;
}


/*
 * Lights every LED in a random color. Changes all LED at the same time
// * to new random colors.
 */
uint16_t WS2812FX::mode_dynamic(void) {
  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed

  if(SEGENV.call == 0) {
    for (uint16_t i = 0; i < SEGLEN; i++) SEGENV.data[i] = random8();
  }

  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*15;
  uint32_t it = now / cycleTime;
  if (it != SEGENV.step && SEGMENT.speed != 0) //new color
  {
    for (uint16_t i = 0; i < SEGLEN; i++) {
      if (random8() <= SEGMENT.intensity) SEGENV.data[i] = random8();
    }
    SEGENV.step = it;
  }

  for (uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_wheel(SEGENV.data[i]));
  }
  return FRAMETIME;
}


/*
 * Does the "standby-breathing" of well known i-Devices.
 */
uint16_t WS2812FX::mode_breath(void) {
  uint16_t var = 0;
  uint16_t counter = (now * ((SEGMENT.speed >> 3) +10));
  counter = (counter >> 2) + (counter >> 4); //0-16384 + 0-2048
  if (counter < 16384) {
    if (counter > 8192) counter = 8192 - (counter - 8192);
    var = sin16(counter) / 103; //close to parabolic in range 0-8192, max val. 23170
  }

  uint8_t lum = 30 + var;
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_blend(SEGCOLOR(1), color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

  return FRAMETIME;
}


/*
 * Fades the LEDs between two colors
 */
uint16_t WS2812FX::mode_fade(void) {
  uint16_t counter = (now * ((SEGMENT.speed >> 3) +10));
  uint8_t lum = triwave16(counter) >> 8;

  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_blend(SEGCOLOR(1), color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), lum));
  }

  return FRAMETIME;
}


/*
 * Scan mode parent function
 */
uint16_t WS2812FX::scan(bool dual)
{
  uint32_t cycleTime = 750 + (255 - SEGMENT.speed)*150;
  uint32_t perc = now % cycleTime;
  uint16_t prog = (perc * 65535) / cycleTime;
  uint16_t size = 1 + ((SEGMENT.intensity * SEGLEN) >> 9);
  uint16_t ledIndex = (prog * ((SEGLEN *2) - size *2)) >> 16;

  fill(SEGCOLOR(1));

  int led_offset = ledIndex - (SEGLEN - size);
  led_offset = abs(led_offset);

  if (dual) {
    for (uint16_t j = led_offset; j < led_offset + size; j++) {
      uint16_t i2 = SEGLEN -1 -j;
      setPixelColor(i2, color_from_palette(i2, true, PALETTE_SOLID_WRAP, (SEGCOLOR(2))? 2:0));
    }
  }

  for (uint16_t j = led_offset; j < led_offset + size; j++) {
    setPixelColor(j, color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
}


/*
 * Runs a single pixel back and forth.
 */
uint16_t WS2812FX::mode_scan(void) {
  return scan(false);
}


/*
 * Runs two pixel back and forth in opposite directions.
 */
uint16_t WS2812FX::mode_dual_scan(void) {
  return scan(true);
}


/*
 * Cycles all LEDs at once through a rainbow.
 */
uint16_t WS2812FX::mode_rainbow(void) {
  uint16_t counter = (now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  if (SEGMENT.intensity < 128){
    fill(color_blend(color_wheel(counter),WHITE,128-SEGMENT.intensity));
  } else {
    fill(color_wheel(counter));
  }

  return FRAMETIME;
}


/*
 * Cycles a rainbow over the entire string of LEDs.
 */
uint16_t WS2812FX::mode_rainbow_cycle(void) {
  uint16_t counter = (now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;

  for(uint16_t i = 0; i < SEGLEN; i++) {
    //intensity/29 = 0 (1/16) 1 (1/8) 2 (1/4) 3 (1/2) 4 (1) 5 (2) 6 (4) 7 (8) 8 (16)
    uint8_t index = (i * (16 << (SEGMENT.intensity /29)) / SEGLEN) + counter;
    setPixelColor(i, color_wheel(index));
  }

  return FRAMETIME;
}


/*
 * theater chase function
 */
uint16_t WS2812FX::theater_chase(uint32_t color1, uint32_t color2, bool do_palette) {
  byte gap = 2 + ((255 - SEGMENT.intensity) >> 5);
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*2;
  uint32_t it = now / cycleTime;
  if (it != SEGENV.step) //new color
  {
    SEGENV.aux0 = (SEGENV.aux0 +1) % gap;
    SEGENV.step = it;
  }

  for(uint16_t i = 0; i < SEGLEN; i++) {
    if((i % gap) == SEGENV.aux0) {
      if (do_palette)
      {
        setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
      } else {
        setPixelColor(i, color1);
      }
    } else {
      setPixelColor(i, color2);
    }
  }
  return FRAMETIME;
}


/*
 * Theatre-style crawling lights.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX::mode_theater_chase(void) {
  return theater_chase(SEGCOLOR(0), SEGCOLOR(1), true);
}


/*
 * Theatre-style crawling lights with rainbow effect.
 * Inspired by the Adafruit examples.
 */
uint16_t WS2812FX::mode_theater_chase_rainbow(void) {
  return theater_chase(color_wheel(SEGENV.step), SEGCOLOR(1), false);
}


/*
 * Running lights effect with smooth sine transition base.
 */
uint16_t WS2812FX::running_base(bool saw) {
  uint8_t x_scale = SEGMENT.intensity >> 2;
  uint32_t counter = (now * SEGMENT.speed) >> 9;

  for(uint16_t i = 0; i < SEGLEN; i++) {
    uint8_t s = 0;
    uint8_t a = i*x_scale - counter;
    if (saw) {
      if (a < 16)
      {
        a = 192 + a*8;
      } else {
        a = map(a,16,255,64,192);
      }
    }
    s = sin8(a);
    setPixelColor(i, color_blend(color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), s));
  }
  return FRAMETIME;
}


/*
 * Running lights effect with smooth sine transition.
 */
uint16_t WS2812FX::mode_running_lights(void) {
  return running_base(false);
}


/*
 * Running lights effect with sawtooth transition.
 */
uint16_t WS2812FX::mode_saw(void) {
  return running_base(true);
}


/*
 * Blink several LEDs in random colors on, reset, repeat.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t WS2812FX::mode_twinkle(void) {
  fill(SEGCOLOR(1));

  uint32_t cycleTime = 20 + (255 - SEGMENT.speed)*5;
  uint32_t it = now / cycleTime;
  if (it != SEGENV.step)
  {
    uint16_t maxOn = map(SEGMENT.intensity, 0, 255, 1, SEGLEN); // make sure at least one LED is on
    if (SEGENV.aux0 >= maxOn)
    {
      SEGENV.aux0 = 0;
      SEGENV.aux1 = random16(); //new seed for our PRNG
    }
    SEGENV.aux0++;
    SEGENV.step = it;
  }

  uint16_t PRNG16 = SEGENV.aux1;

  for (uint16_t i = 0; i < SEGENV.aux0; i++)
  {
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 13849; // next 'random' number
    uint32_t p = (uint32_t)SEGLEN * (uint32_t)PRNG16;
    uint16_t j = p >> 16;
    setPixelColor(j, color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }

  return FRAMETIME;
}


/*
 * Dissolve function
 */
uint16_t WS2812FX::dissolve(uint32_t color) {
  bool wa = (SEGCOLOR(1) != 0 && _brightness < 255); //workaround, can't compare getPixel to color if not full brightness

  for (uint16_t j = 0; j <= SEGLEN / 15; j++)
  {
    if (random8() <= SEGMENT.intensity) {
      for (uint8_t times = 0; times < 10; times++) //attempt to spawn a new pixel 5 times
      {
        uint16_t i = random16(SEGLEN);
        if (SEGENV.aux0) { //dissolve to primary/palette
          if (getPixelColor(i) == SEGCOLOR(1) || wa) {
            if (color == SEGCOLOR(0))
            {
              setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
            } else { setPixelColor(i, color); }
            break; //only spawn 1 new pixel per frame per 50 LEDs
          }
        } else { //dissolve to secondary
          if (getPixelColor(i) != SEGCOLOR(1)) { setPixelColor(i, SEGCOLOR(1)); break; }
        }
      }
    }
  }

  if (SEGENV.call > (255 - SEGMENT.speed) + 15)
  {
    SEGENV.aux0 = !SEGENV.aux0;
    SEGENV.call = 0;
  }

  return FRAMETIME;
}


/*
 * Blink several LEDs on and then off
 */
uint16_t WS2812FX::mode_dissolve(void) {
  return dissolve(SEGCOLOR(0));
}


/*
 * Blink several LEDs on and then off in random colors
 */
uint16_t WS2812FX::mode_dissolve_random(void) {
  return dissolve(color_wheel(random8()));
}


/*
 * Blinks one LED at a time.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t WS2812FX::mode_sparkle(void) {
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }
  uint32_t cycleTime = 10 + (255 - SEGMENT.speed)*2;
  uint32_t it = now / cycleTime;
  if (it != SEGENV.step)
  {
    SEGENV.aux0 = random16(SEGLEN); // aux0 stores the random led index
    SEGENV.step = it;
  }

  setPixelColor(SEGENV.aux0, SEGCOLOR(0));
  return FRAMETIME;
}


/*
 * Lights all LEDs in the color. Flashes single white pixels randomly.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t WS2812FX::mode_flash_sparkle(void) {
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  if(random8(5) == 0) {
    SEGENV.aux0 = random16(SEGLEN); // aux0 stores the random led index
    setPixelColor(SEGENV.aux0, SEGCOLOR(1));
    return 20;
  }
  return 20 + (uint16_t)(255-SEGMENT.speed);
}


/*
 * Like flash sparkle. With more flash.
 * Inspired by www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
 */
uint16_t WS2812FX::mode_hyper_sparkle(void) {
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  if(random8(5) < 2) {
    for(uint16_t i = 0; i < MAX(1, SEGLEN/3); i++) {
      setPixelColor(random16(SEGLEN), SEGCOLOR(1));
    }
    return 20;
  }
  return 20 + (uint16_t)(255-SEGMENT.speed);
}


/*
 * Strobe effect with different strobe count and pause, controlled by speed.
 */
uint16_t WS2812FX::mode_multi_strobe(void) {
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }
  //blink(SEGCOLOR(0), SEGCOLOR(1), true, true);

  uint16_t delay = 50 + 20*(uint16_t)(255-SEGMENT.speed);
  uint16_t count = 2 * ((SEGMENT.speed / 10) + 1);
  if(SEGENV.step < count) {
    if((SEGENV.step & 1) == 0) {
      for(uint16_t i = 0; i < SEGLEN; i++) {
        setPixelColor(i, SEGCOLOR(0));
      }
      delay = 20;
    } else {
      delay = 50;
    }
  }
  SEGENV.step = (SEGENV.step + 1) % (count + 1);
  return delay;
}

/*
 * Android loading circle
 */
uint16_t WS2812FX::mode_android(void) {

  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  }

  if (SEGENV.aux1 > ((float)SEGMENT.intensity/255.0)*(float)SEGLEN)
  {
    SEGENV.aux0 = 1;
  } else
  {
    if (SEGENV.aux1 < 2) SEGENV.aux0 = 0;
  }

  uint16_t a = SEGENV.step;

  if (SEGENV.aux0 == 0)
  {
    if (SEGENV.call %3 == 1) {a++;}
    else {SEGENV.aux1++;}
  } else
  {
    a++;
    if (SEGENV.call %3 != 1) SEGENV.aux1--;
  }

  if (a >= SEGLEN) a = 0;

  if (a + SEGENV.aux1 < SEGLEN)
  {
    for(int i = a; i < a+SEGENV.aux1; i++) {
      setPixelColor(i, SEGCOLOR(0));
    }
  } else
  {
    for(int i = a; i < SEGLEN; i++) {
      setPixelColor(i, SEGCOLOR(0));
    }
    for(int i = 0; i < SEGENV.aux1 - (SEGLEN -a); i++) {
      setPixelColor(i, SEGCOLOR(0));
    }
  }
  SEGENV.step = a;

  return 3 + ((8 * (uint32_t)(255 - SEGMENT.speed)) / SEGLEN);
}

/*
 * color chase function.
 * color1 = background color
 * color2 and color3 = colors of two adjacent leds
 */
uint16_t WS2812FX::chase(uint32_t color1, uint32_t color2, uint32_t color3, bool do_palette) {
  uint16_t counter = now * ((SEGMENT.speed >> 2) + 1);
  uint16_t a = counter * SEGLEN  >> 16;

  bool chase_random = (SEGMENT.mode == FX_MODE_CHASE_RANDOM);
  if (chase_random) {
    if (a < SEGENV.step) //we hit the start again, choose new color for Chase random
    {
      SEGENV.aux1 = SEGENV.aux0; //store previous random color
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0);
    }
    color1 = color_wheel(SEGENV.aux0);
  }
  SEGENV.step = a;

  // Use intensity setting to vary chase up to 1/2 string length
  uint8_t size = 1 + (SEGMENT.intensity * SEGLEN >> 10);

  uint16_t b = a + size; //"trail" of chase, filled with color1
  if (b > SEGLEN) b -= SEGLEN;
  uint16_t c = b + size;
  if (c > SEGLEN) c -= SEGLEN;

  //background
  if (do_palette)
  {
    for(uint16_t i = 0; i < SEGLEN; i++) {
      setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
    }
  } else fill(color1);

  //if random, fill old background between a and end
  if (chase_random)
  {
    color1 = color_wheel(SEGENV.aux1);
    for (uint16_t i = a; i < SEGLEN; i++)
      setPixelColor(i, color1);
  }

  //fill between points a and b with color2
  if (a < b)
  {
    for (uint16_t i = a; i < b; i++)
      setPixelColor(i, color2);
  } else {
    for (uint16_t i = a; i < SEGLEN; i++) //fill until end
      setPixelColor(i, color2);
    for (uint16_t i = 0; i < b; i++) //fill from start until b
      setPixelColor(i, color2);
  }

  //fill between points b and c with color2
  if (b < c)
  {
    for (uint16_t i = b; i < c; i++)
      setPixelColor(i, color3);
  } else {
    for (uint16_t i = b; i < SEGLEN; i++) //fill until end
      setPixelColor(i, color3);
    for (uint16_t i = 0; i < c; i++) //fill from start until c
      setPixelColor(i, color3);
  }

  return FRAMETIME;
}


/*
 * Bicolor chase, more primary color.
 */
uint16_t WS2812FX::mode_chase_color(void) {
  return chase(SEGCOLOR(1), (SEGCOLOR(2)) ? SEGCOLOR(2) : SEGCOLOR(0), SEGCOLOR(0), true);
}


/*
 * Primary running followed by random color.
 */
uint16_t WS2812FX::mode_chase_random(void) {
  return chase(SEGCOLOR(1), (SEGCOLOR(2)) ? SEGCOLOR(2) : SEGCOLOR(0), SEGCOLOR(0), false);
}


/*
 * Primary, secondary running on rainbow.
 */
uint16_t WS2812FX::mode_chase_rainbow(void) {
  uint8_t color_sep = 256 / SEGLEN;
  uint8_t color_index = SEGENV.call & 0xFF;
  uint32_t color = color_wheel(((SEGENV.step * color_sep) + color_index) & 0xFF);

  return chase(color, SEGCOLOR(0), SEGCOLOR(1), false);
}


/*
 * Primary running on rainbow.
 */
uint16_t WS2812FX::mode_chase_rainbow_white(void) {
  uint16_t n = SEGENV.step;
  uint16_t m = (SEGENV.step + 1) % SEGLEN;
  uint32_t color2 = color_wheel(((n * 256 / SEGLEN) + (SEGENV.call & 0xFF)) & 0xFF);
  uint32_t color3 = color_wheel(((m * 256 / SEGLEN) + (SEGENV.call & 0xFF)) & 0xFF);

  return chase(SEGCOLOR(0), color2, color3, false);
}


/*
 * Red - Amber - Green - Blue lights running
 */
uint16_t WS2812FX::mode_colorful(void) {
  uint32_t cols[]{0x00FF0000,0x00EEBB00,0x0000EE00,0x000077CC,0x00FF0000,0x00EEBB00,0x0000EE00};
  if (SEGMENT.intensity < 127) //pastel (easter) colors
  {
    cols[0] = 0x00FF8040;
    cols[1] = 0x00E5D241;
    cols[2] = 0x0077FF77;
    cols[3] = 0x0077F0F0;
    for (uint8_t i = 4; i < 7; i++) cols[i] = cols[i-4];
  }

  uint32_t cycleTime = 50 + (15 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = now / cycleTime;
  if (it != SEGENV.step)
  {
    if (SEGMENT.speed > 0) SEGENV.aux0++;
    if (SEGENV.aux0 > 3) SEGENV.aux0 = 0;
    SEGENV.step = it;
  }

  uint16_t i = 0;
  for (i; i < SEGLEN -3; i+=4)
  {
    setPixelColor(i, cols[SEGENV.aux0]);
    setPixelColor(i+1, cols[SEGENV.aux0+1]);
    setPixelColor(i+2, cols[SEGENV.aux0+2]);
    setPixelColor(i+3, cols[SEGENV.aux0+3]);
  }
  if(i < SEGLEN)
  {
    setPixelColor(i, cols[SEGENV.aux0]);

    if(i+1 < SEGLEN)
    {
      setPixelColor(i+1, cols[SEGENV.aux0+1]);

      if(i+2 < SEGLEN)
      {
        setPixelColor(i+2, cols[SEGENV.aux0+2]);
      }
    }
  }

  return FRAMETIME;
}


/*
 * Emulates a traffic light.
 */
uint16_t WS2812FX::mode_traffic_light(void) {
  for(uint16_t i=0; i < SEGLEN; i++)
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 1));
  uint32_t mdelay = 500;
  for (int i = 0; i < SEGLEN-2 ; i+=3)
  {
    switch (SEGENV.aux0)
    {
      case 0: setPixelColor(i, 0x00FF0000); mdelay = 150 + (100 * (uint32_t)(255 - SEGMENT.speed));break;
      case 1: setPixelColor(i, 0x00FF0000); mdelay = 150 + (20 * (uint32_t)(255 - SEGMENT.speed)); setPixelColor(i+1, 0x00EECC00); break;
      case 2: setPixelColor(i+2, 0x0000FF00); mdelay = 150 + (100 * (uint32_t)(255 - SEGMENT.speed));break;
      case 3: setPixelColor(i+1, 0x00EECC00); mdelay = 150 + (20 * (uint32_t)(255 - SEGMENT.speed));break;
    }
  }

  if (now - SEGENV.step > mdelay)
  {
    SEGENV.aux0++;
    if (SEGENV.aux0 > 3) SEGENV.aux0 = 0;
    SEGENV.step = now;
  }

  return FRAMETIME;
}


/*
 * Sec flashes running on prim.
 */
#define FLASH_COUNT 4
uint16_t WS2812FX::mode_chase_flash(void) {
  uint8_t flash_step = SEGENV.call % ((FLASH_COUNT * 2) + 1);

  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
  }

  uint16_t delay = 10 + ((30 * (uint16_t)(255 - SEGMENT.speed)) / SEGLEN);
  if(flash_step < (FLASH_COUNT * 2)) {
    if(flash_step % 2 == 0) {
      uint16_t n = SEGENV.step;
      uint16_t m = (SEGENV.step + 1) % SEGLEN;
      setPixelColor( n, SEGCOLOR(1));
      setPixelColor( m, SEGCOLOR(1));
      delay = 20;
    } else {
      delay = 30;
    }
  } else {
    SEGENV.step = (SEGENV.step + 1) % SEGLEN;
  }
  return delay;
}


/*
 * Prim flashes running, followed by random color.
 */
uint16_t WS2812FX::mode_chase_flash_random(void) {
  uint8_t flash_step = SEGENV.call % ((FLASH_COUNT * 2) + 1);

  for(uint16_t i = 0; i < SEGENV.step; i++) {
    setPixelColor(i, color_wheel(SEGENV.aux0));
  }

  uint16_t delay = 1 + ((10 * (uint16_t)(255 - SEGMENT.speed)) / SEGLEN);
  if(flash_step < (FLASH_COUNT * 2)) {
    uint16_t n = SEGENV.step;
    uint16_t m = (SEGENV.step + 1) % SEGLEN;
    if(flash_step % 2 == 0) {
      setPixelColor( n, SEGCOLOR(0));
      setPixelColor( m, SEGCOLOR(0));
      delay = 20;
    } else {
      setPixelColor( n, color_wheel(SEGENV.aux0));
      setPixelColor( m, SEGCOLOR(1));
      delay = 30;
    }
  } else {
    SEGENV.step = (SEGENV.step + 1) % SEGLEN;

    if(SEGENV.step == 0) {
      SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0);
    }
  }
  return delay;
}


/*
 * Alternating pixels running function.
 */
uint16_t WS2812FX::running(uint32_t color1, uint32_t color2) {
  uint8_t pxw = 1 + (SEGMENT.intensity >> 5);
  uint32_t cycleTime = 35 + (255 - SEGMENT.speed);
  uint32_t it = now / cycleTime;
  if (SEGMENT.speed == 0) it = 0;

  for(uint16_t i = 0; i < SEGLEN; i++) {
    if((i + SEGENV.aux0) % (pxw*2) < pxw) {
      if (color1 == SEGCOLOR(0))
      {
        setPixelColor(SEGLEN -i -1, color_from_palette(SEGLEN -i -1, true, PALETTE_SOLID_WRAP, 0));
      } else
      {
        setPixelColor(SEGLEN -i -1, color1);
      }
    } else {
      setPixelColor(SEGLEN -i -1, color2);
    }
  }

  if (it != SEGENV.step )
  {
    SEGENV.aux0 = (SEGENV.aux0 +1) % (pxw*2);
    SEGENV.step = it;
  }
  return FRAMETIME;
}

/*
 * Alternating color/sec pixels running.
 */
uint16_t WS2812FX::mode_running_color(void) {
  return running(SEGCOLOR(0), SEGCOLOR(1));
}


/*
 * Alternating red/blue pixels running.
 */
uint16_t WS2812FX::mode_running_red_blue(void) {
  return running(RED, BLUE);
}


/*
 * Alternating red/green pixels running.
 */
uint16_t WS2812FX::mode_merry_christmas(void) {
  return running(RED, GREEN);
}


/*
 * Alternating orange/purple pixels running.
 */
uint16_t WS2812FX::mode_halloween(void) {
  return running(PURPLE, ORANGE);
}


/*
 * Random colored pixels running.
 */
uint16_t WS2812FX::mode_running_random(void) {
  uint32_t cycleTime = 25 + (3 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = now / cycleTime;
  if (SEGENV.aux1 == it) return FRAMETIME;

  for(uint16_t i=SEGLEN-1; i > 0; i--) {
    setPixelColor( i, getPixelColor( i - 1));
  }

  if(SEGENV.step == 0) {
    SEGENV.aux0 = get_random_wheel_index(SEGENV.aux0);
    setPixelColor(0, color_wheel(SEGENV.aux0));
  }

  SEGENV.step++;
  if (SEGENV.step > ((255-SEGMENT.intensity) >> 4))
  {
    SEGENV.step = 0;
  }

  SEGENV.aux1 = it;
  return FRAMETIME;
}


/*
 * K.I.T.T.
 */
uint16_t WS2812FX::mode_larson_scanner(void){
  return larson_scanner(false);
}

uint16_t WS2812FX::larson_scanner(bool dual) {
  uint16_t counter = now * ((SEGMENT.speed >> 2) +8);
  uint16_t index = counter * SEGLEN  >> 16;

  fade_out(SEGMENT.intensity);

  if (SEGENV.step > index && SEGENV.step - index > SEGLEN/2) {
    SEGENV.aux0 = !SEGENV.aux0;
  }

  for (uint16_t i = SEGENV.step; i < index; i++) {
    uint16_t j = (SEGENV.aux0)?i:SEGLEN-1-i;
    setPixelColor( j, color_from_palette(j, true, PALETTE_SOLID_WRAP, 0));
  }
  if (dual) {
    uint32_t c;
    if (SEGCOLOR(2) != 0) {
      c = SEGCOLOR(2);
    } else {
      c = color_from_palette(index, true, PALETTE_SOLID_WRAP, 0);
    }

    for (uint16_t i = SEGENV.step; i < index; i++) {
      uint16_t j = (SEGENV.aux0)?SEGLEN-1-i:i;
      setPixelColor(j, c);
    }
  }

  SEGENV.step = index;
  return FRAMETIME;
}


/*
 * Firing comets from one end. "Lighthouse"
 */
uint16_t WS2812FX::mode_comet(void) {
  uint16_t counter = now * ((SEGMENT.speed >>2) +1);
  uint16_t index = counter * SEGLEN >> 16;
  if (SEGENV.call == 0) SEGENV.aux0 = index;

  fade_out(SEGMENT.intensity);

  setPixelColor( index, color_from_palette(index, true, PALETTE_SOLID_WRAP, 0));
  if (index > SEGENV.aux0) {
    for (uint16_t i = SEGENV.aux0; i < index ; i++) {
       setPixelColor( i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  } else if (index < SEGENV.aux0 && index < 10) {
    for (uint16_t i = 0; i < index ; i++) {
       setPixelColor( i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
    }
  }
  SEGENV.aux0 = index++;

  return FRAMETIME;
}


/*
 * Fireworks function.
 */
uint16_t WS2812FX::mode_fireworks() {
  fade_out(0);
  if (SEGENV.call == 0) {
    SEGENV.aux0 = UINT16_MAX;
    SEGENV.aux1 = UINT16_MAX;
  }
  bool valid1 = (SEGENV.aux0 < SEGLEN);
  bool valid2 = (SEGENV.aux1 < SEGLEN);
  uint32_t sv1 = 0, sv2 = 0;
  if (valid1) sv1 = getPixelColor(SEGENV.aux0);
  if (valid2) sv2 = getPixelColor(SEGENV.aux1);
  blur(255-SEGMENT.speed);
  if (valid1) setPixelColor(SEGENV.aux0 , sv1);
  if (valid2) setPixelColor(SEGENV.aux1, sv2);

  for(uint16_t i=0; i<MAX(1, SEGLEN/20); i++) {
    if(random8(129 - (SEGMENT.intensity >> 1)) == 0) {
      uint16_t index = random(SEGLEN);
      setPixelColor(index, color_from_palette(random8(), false, false, 0));
      SEGENV.aux1 = SEGENV.aux0;
      SEGENV.aux0 = index;
    }
  }
  return FRAMETIME;
}


//Twinkling LEDs running. Inspired by https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Rain.h
uint16_t WS2812FX::mode_rain()
{
  SEGENV.step += FRAMETIME;
  if (SEGENV.step > SPEED_FORMULA_L) {
    SEGENV.step = 0;
    //shift all leds right
    uint32_t ctemp = getPixelColor(SEGLEN -1);
    for(uint16_t i = SEGLEN -1; i > 0; i--) {
      setPixelColor(i, getPixelColor(i-1));
    }
    setPixelColor(0, ctemp);
    SEGENV.aux0++;
    SEGENV.aux1++;
    if (SEGENV.aux0 == 0) SEGENV.aux0 = UINT16_MAX;
    if (SEGENV.aux1 == 0) SEGENV.aux0 = UINT16_MAX;
    if (SEGENV.aux0 == SEGLEN) SEGENV.aux0 = 0;
    if (SEGENV.aux1 == SEGLEN) SEGENV.aux1 = 0;
  }
  return mode_fireworks();
}


/*
 * Fire flicker function
 */
uint16_t WS2812FX::mode_fire_flicker(void) {
  uint32_t cycleTime = 40 + (255 - SEGMENT.speed);
  uint32_t it = now / cycleTime;
  if (SEGENV.step == it) return FRAMETIME;

  byte w = (SEGCOLOR(0) >> 24) & 0xFF;
  byte r = (SEGCOLOR(0) >> 16) & 0xFF;
  byte g = (SEGCOLOR(0) >>  8) & 0xFF;
  byte b = (SEGCOLOR(0)        & 0xFF);
  byte lum = (SEGMENT.palette == 0) ? MAX(w, MAX(r, MAX(g, b))) : 255;
  lum /= (((256-SEGMENT.intensity)/16)+1);
  for(uint16_t i = 0; i < SEGLEN; i++) {
    byte flicker = random8(lum);
    if (SEGMENT.palette == 0) {
      setPixelColor(i, MAX(r - flicker, 0), MAX(g - flicker, 0), MAX(b - flicker, 0), MAX(w - flicker, 0));
    } else {
      setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0, 255 - flicker));
    }
  }

  SEGENV.step = it;
  return FRAMETIME;
}


/*
 * Gradient run base function
 */
uint16_t WS2812FX::gradient_base(bool loading) {
  uint16_t counter = now * ((SEGMENT.speed >> 2) + 1);
  uint16_t pp = counter * SEGLEN >> 16;
  if (SEGENV.call == 0) pp = 0;
  float val; //0.0 = sec 1.0 = pri
  float brd = loading ? SEGMENT.intensity : SEGMENT.intensity/2;
  if (brd <1.0) brd = 1.0;
  int p1 = pp-SEGLEN;
  int p2 = pp+SEGLEN;

  for(uint16_t i = 0; i < SEGLEN; i++)
  {
    if (loading)
    {
      val = abs(((i>pp) ? p2:pp) -i);
    } else {
      val = MIN(abs(pp-i),MIN(abs(p1-i),abs(p2-i)));
    }
    val = (brd > val) ? val/brd * 255 : 255;
    setPixelColor(i, color_blend(SEGCOLOR(0), color_from_palette(i, true, PALETTE_SOLID_WRAP, 1), val));
  }

  return FRAMETIME;
}


/*
 * Gradient run
 */
uint16_t WS2812FX::mode_gradient(void) {
  return gradient_base(false);
}


/*
 * Gradient run with hard transition
 */
uint16_t WS2812FX::mode_loading(void) {
  return gradient_base(true);
}


//American Police Light with all LEDs Red and Blue
uint16_t WS2812FX::police_base(uint32_t color1, uint32_t color2, bool all)
{
  uint16_t counter = now * ((SEGMENT.speed >> 2) +1);
  uint16_t idexR = (counter * SEGLEN) >> 16;
  if (idexR >= SEGLEN) idexR = 0;

  uint16_t topindex = SEGLEN >> 1;
  uint16_t idexB = (idexR > topindex) ? idexR - topindex : idexR + topindex;
  if (SEGENV.call == 0) SEGENV.aux0 = idexR;
  if (idexB >= SEGLEN) idexB = 0; //otherwise overflow on odd number of LEDs

  if (all) { //different algo, ensuring immediate fill
    if (idexB > idexR) {
      fill(color2);
      for (uint16_t i = idexR; i < idexB; i++) setPixelColor(i, color1);
    } else {
      fill(color1);
      for (uint16_t i = idexB; i < idexR; i++) setPixelColor(i, color2);
    }
  } else { //regular dot-only mode
    uint8_t size = 1 + SEGMENT.intensity >> 3;
    if (size > SEGLEN/2) size = 1+ SEGLEN/2;
    for (uint8_t i=0; i <= size; i++) {
      setPixelColor(idexR+i, color1);
      setPixelColor(idexB+i, color2);
    }
    if (SEGENV.aux0 != idexR) {
      uint8_t gap = (SEGENV.aux0 < idexR)? idexR - SEGENV.aux0:SEGLEN - SEGENV.aux0 + idexR;
      for (uint8_t i = 0; i <= gap ; i++) {
        if ((idexR - i) < 0) idexR = SEGLEN-1 + i;
        if ((idexB - i) < 0) idexB = SEGLEN-1 + i;
        setPixelColor(idexR-i, color1);
        setPixelColor(idexB-i, color2);
      }
      SEGENV.aux0 = idexR;
    }
  }

  return FRAMETIME;
}


//American Police Light with all LEDs Red and Blue
uint16_t WS2812FX::mode_police_all()
{
  return police_base(RED, BLUE, true);
}


//Police Lights Red and Blue
uint16_t WS2812FX::mode_police()
{
  fill(SEGCOLOR(1));

  return police_base(RED, BLUE, false);
}


//Police All with custom colors
uint16_t WS2812FX::mode_two_areas()
{
  return police_base(SEGCOLOR(0), SEGCOLOR(1), true);
}


//Police Lights with custom colors
uint16_t WS2812FX::mode_two_dots()
{
  fill(SEGCOLOR(2));
  uint32_t color2 = (SEGCOLOR(1) == SEGCOLOR(2)) ? SEGCOLOR(0) : SEGCOLOR(1);

  return police_base(SEGCOLOR(0), color2, false);
}


/*
 * Tricolor chase function
 */
uint16_t WS2812FX::tricolor_chase(uint32_t color1, uint32_t color2) {
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed)*2;
  uint32_t it = now / cycleTime;
  uint8_t width = (1 + SEGMENT.intensity/32) * 3; //value of 1-8 for each colour
  uint8_t index = it % width;

  for(uint16_t i = 0; i < SEGLEN; i++, index++) {
    if(index > width-1) index = 0;

    uint32_t color = color1;
    if(index > width*2/3-1) color = color_from_palette(i, true, PALETTE_SOLID_WRAP, 1);
    else if(index > width/3-1) color = color2;

    setPixelColor(SEGLEN - i -1, color);
  }

  return FRAMETIME;
}


/*
 * Alternating white/red/black pixels running. PLACEHOLDER
 */
uint16_t WS2812FX::mode_circus_combustus(void) {
  return tricolor_chase(RED, WHITE);
}


/*
 * Tricolor chase mode
 */
uint16_t WS2812FX::mode_tricolor_chase(void) {
  return tricolor_chase(SEGCOLOR(2), SEGCOLOR(0));
}


/*
 * ICU mode
 */
uint16_t WS2812FX::mode_icu(void) {
  uint16_t dest = SEGENV.step & 0xFFFF;
  uint8_t space = (SEGMENT.intensity >> 3) +2;

  fill(SEGCOLOR(1));

  byte pindex = map(dest, 0, SEGLEN-SEGLEN/space, 0, 255);
  uint32_t col = color_from_palette(pindex, false, false, 0);

  setPixelColor(dest, col);
  setPixelColor(dest + SEGLEN/space, col);

  if(SEGENV.aux0 == dest) { // pause between eye movements
    if(random8(6) == 0) { // blink once in a while
      setPixelColor(dest, SEGCOLOR(1));
      setPixelColor(dest + SEGLEN/space, SEGCOLOR(1));
      return 200;
    }
    SEGENV.aux0 = random16(SEGLEN-SEGLEN/space);
    return 1000 + random16(2000);
  }

  if(SEGENV.aux0 > SEGENV.step) {
    SEGENV.step++;
    dest++;
  } else if (SEGENV.aux0 < SEGENV.step) {
    SEGENV.step--;
    dest--;
  }

  setPixelColor(dest, col);
  setPixelColor(dest + SEGLEN/space, col);

  return SPEED_FORMULA_L;
}


/*
 * Custom mode by Aircoookie. Color Wipe, but with 3 colors
 */
uint16_t WS2812FX::mode_tricolor_wipe(void)
{
  uint32_t cycleTime = 1000 + (255 - SEGMENT.speed)*200;
  uint32_t perc = now % cycleTime;
  uint16_t prog = (perc * 65535) / cycleTime;
  uint16_t ledIndex = (prog * SEGLEN * 3) >> 16;
  uint16_t ledOffset = ledIndex;

  for (uint16_t i = 0; i < SEGLEN; i++)
  {
    setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 2));
  }

  if(ledIndex < SEGLEN) { //wipe from 0 to 1
    for (uint16_t i = 0; i < SEGLEN; i++)
    {
      setPixelColor(i, (i > ledOffset)? SEGCOLOR(0) : SEGCOLOR(1));
    }
  } else if (ledIndex < SEGLEN*2) { //wipe from 1 to 2
    ledOffset = ledIndex - SEGLEN;
    for (uint16_t i = ledOffset +1; i < SEGLEN; i++)
    {
      setPixelColor(i, SEGCOLOR(1));
    }
  } else //wipe from 2 to 0
  {
    ledOffset = ledIndex - SEGLEN*2;
    for (uint16_t i = 0; i <= ledOffset; i++)
    {
      setPixelColor(i, SEGCOLOR(0));
    }
  }

  return FRAMETIME;
}


/*
 * Fades between 3 colors
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/TriFade.h
 * Modified by Aircoookie
 */
uint16_t WS2812FX::mode_tricolor_fade(void)
{
  uint16_t counter = now * ((SEGMENT.speed >> 3) +1);
  uint32_t prog = (counter * 768) >> 16;

  uint32_t color1 = 0, color2 = 0;
  byte stage = 0;

  if(prog < 256) {
    color1 = SEGCOLOR(0);
    color2 = SEGCOLOR(1);
    stage = 0;
  } else if(prog < 512) {
    color1 = SEGCOLOR(1);
    color2 = SEGCOLOR(2);
    stage = 1;
  } else {
    color1 = SEGCOLOR(2);
    color2 = SEGCOLOR(0);
    stage = 2;
  }

  byte stp = prog; // % 256
  uint32_t color = 0;
  for(uint16_t i = 0; i < SEGLEN; i++) {
    if (stage == 2) {
      color = color_blend(color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), color2, stp);
    } else if (stage == 1) {
      color = color_blend(color1, color_from_palette(i, true, PALETTE_SOLID_WRAP, 2), stp);
    } else {
      color = color_blend(color1, color2, stp);
    }
    setPixelColor(i, color);
  }

  return FRAMETIME;
}


/*
 * Creates random comets
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/MultiComet.h
 */
uint16_t WS2812FX::mode_multi_comet(void)
{
  uint32_t cycleTime = 10 + (uint32_t)(255 - SEGMENT.speed);
  uint32_t it = now / cycleTime;
  if (SEGENV.step == it) return FRAMETIME;
  if (!SEGENV.allocateData(sizeof(uint16_t) * 8)) return mode_static(); //allocation failed

  fade_out(SEGMENT.intensity);

  uint16_t* comets = reinterpret_cast<uint16_t*>(SEGENV.data);

  for(uint8_t i=0; i < 8; i++) {
    if(comets[i] < SEGLEN) {
      uint16_t index = comets[i];
      if (SEGCOLOR(2) != 0)
      {
        setPixelColor(index, i % 2 ? color_from_palette(index, true, PALETTE_SOLID_WRAP, 0) : SEGCOLOR(2));
      } else
      {
        setPixelColor(index, color_from_palette(index, true, PALETTE_SOLID_WRAP, 0));
      }
      comets[i]++;
    } else {
      if(!random(SEGLEN)) {
        comets[i] = 0;
      }
    }
  }

  SEGENV.step = it;
  return FRAMETIME;
}


/*
 * Creates two Larson scanners moving in opposite directions
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/DualLarson.h
 */
uint16_t WS2812FX::mode_dual_larson_scanner(void){
  return larson_scanner(true);
}


/*
 * Running random pixels
 * Custom mode by Keith Lord: https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/RandomChase.h
 */
uint16_t WS2812FX::mode_random_chase(void)
{
  uint32_t cycleTime = 25 + (3 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = now / cycleTime;
  if (SEGENV.step == it) return FRAMETIME;

  for(uint16_t i = SEGLEN -1; i > 0; i--) {
    setPixelColor(i, getPixelColor(i-1));
  }
  uint32_t color = getPixelColor(0);
  if (SEGLEN > 1) color = getPixelColor( 1);
  uint8_t r = random8(6) != 0 ? (color >> 16 & 0xFF) : random8();
  uint8_t g = random8(6) != 0 ? (color >> 8  & 0xFF) : random8();
  uint8_t b = random8(6) != 0 ? (color       & 0xFF) : random8();
  setPixelColor(0, r, g, b);

  SEGENV.step = it;
  return FRAMETIME;
}


typedef struct Oscillator {
  int16_t pos;
  int8_t  size;
  int8_t  dir;
  int8_t  speed;
} oscillator;

/*
/  Oscillating bars of color, updated with standard framerate
*/
uint16_t WS2812FX::mode_oscillate(void)
{
  uint8_t numOscillators = 3;
  uint16_t dataSize = sizeof(oscillator) * numOscillators;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Oscillator* oscillators = reinterpret_cast<Oscillator*>(SEGENV.data);

  if (SEGENV.call == 0)
  {
    oscillators[0] = {SEGLEN/4,   SEGLEN/8,  1, 1};
    oscillators[1] = {SEGLEN/4*3, SEGLEN/8,  1, 2};
    oscillators[2] = {SEGLEN/4*2, SEGLEN/8, -1, 1};
  }

  uint32_t cycleTime = 20 + (2 * (uint32_t)(255 - SEGMENT.speed));
  uint32_t it = now / cycleTime;

  for(uint8_t i = 0; i < numOscillators; i++) {
    // if the counter has increased, move the oscillator by the random step
    if (it != SEGENV.step) oscillators[i].pos += oscillators[i].dir * oscillators[i].speed;
    oscillators[i].size = SEGLEN/(3+SEGMENT.intensity/8);
    if((oscillators[i].dir == -1) && (oscillators[i].pos <= 0)) {
      oscillators[i].pos = 0;
      oscillators[i].dir = 1;
      // make bigger steps for faster speeds
      oscillators[i].speed = SEGMENT.speed > 100 ? random8(2, 4):random8(1, 3);
    }
    if((oscillators[i].dir == 1) && (oscillators[i].pos >= (SEGLEN - 1))) {
      oscillators[i].pos = SEGLEN - 1;
      oscillators[i].dir = -1;
      oscillators[i].speed = SEGMENT.speed > 100 ? random8(2, 4):random8(1, 3);
    }
  }

  for(uint16_t i=0; i < SEGLEN; i++) {
    uint32_t color = BLACK;
    for(uint8_t j=0; j < numOscillators; j++) {
      if(i >= oscillators[j].pos - oscillators[j].size && i <= oscillators[j].pos + oscillators[j].size) {
        color = (color == BLACK) ? SEGCOLOR(j) : color_blend(color, SEGCOLOR(j), 128);
      }
    }
    setPixelColor(i, color);
  }

  SEGENV.step = it;
  return FRAMETIME;
}


uint16_t WS2812FX::mode_lightning(void)
{
  uint16_t ledstart = random16(SEGLEN);               // Determine starting location of flash
  uint16_t ledlen = random16(SEGLEN -1 -ledstart);    // Determine length of flash (not to go beyond SEGLEN-1)
  uint8_t bri = 255/random8(1, 3);

  if (SEGENV.step == 0)
  {
    SEGENV.aux0 = random8(3, 3 + SEGMENT.intensity/20); //number of flashes
    bri = 52;
    SEGENV.aux1 = 1;
  }

  fill(SEGCOLOR(1));

  if (SEGENV.aux1) {
    for (int i = ledstart; i < ledstart + ledlen; i++)
    {
      if (SEGMENT.palette == 0)
      {
//        setPixelColor(i,bri,bri,bri,bri);
        setPixelColor(i, SEGCOLOR(0));
      } else {
        setPixelColor(i,color_from_palette(i, true, PALETTE_SOLID_WRAP, 0, bri));
      }
    }
    SEGENV.aux1 = 0;
    SEGENV.step++;
    return random8(4, 10);                                    // each flash only lasts 4-10 milliseconds
  }

  SEGENV.aux1 = 1;
  if (SEGENV.step == 1) return (200);                       // longer delay until next flash after the leader

  if (SEGENV.step <= SEGENV.aux0) return (50 + random8(100));  // shorter delay between strokes

  SEGENV.step = 0;
  return (random8(255 - SEGMENT.speed) * 100);                            // delay between strikes
}


// Pride2015
// Animated, ever-changing rainbows.
// by Mark Kriegsman: https://gist.github.com/kriegsman/964de772d64c502760e5
uint16_t WS2812FX::mode_pride_2015(void)
{
  uint16_t duration = 10 + SEGMENT.speed;
  uint16_t sPseudotime = SEGENV.step;
  uint16_t sHue16 = SEGENV.aux0;

  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);

  sPseudotime += duration * msmultiplier;
  sHue16 += duration * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  CRGB fastled_col;

  for (uint16_t i = 0 ; i < SEGLEN; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 >> 8;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = CHSV( hue8, sat8, bri8);
    fastled_col = col_to_crgb(getPixelColor(i));
    nblend(fastled_col, newcolor, 64);

    if (SEGMENT.palette == 0) {
      setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
    } else {
      fastled_col = ColorFromPalette(currentPalette, hue8, bri8);
      setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
    }

  }
  SEGENV.step = sPseudotime;
  SEGENV.aux0 = sHue16;
  return FRAMETIME;
}


//eight colored dots, weaving in and out of sync with each other
uint16_t WS2812FX::mode_juggle(void){
  fade_out(SEGMENT.intensity);
  CRGB fastled_col;
  byte dothue = 0;
  for ( byte i = 0; i < 8; i++) {
    uint16_t index = 0 + beatsin88((128 + SEGMENT.speed)*(i + 7), 0, SEGLEN -1);
    fastled_col = col_to_crgb(getPixelColor(index));
    fastled_col |= (SEGMENT.palette==0)?CHSV(dothue, 220, 255):ColorFromPalette(currentPalette, dothue, 255);
    setPixelColor(index, fastled_col.red, fastled_col.green, fastled_col.blue);
    dothue += 32;
  }
  return FRAMETIME;
}


uint16_t WS2812FX::mode_palette()
{
  uint16_t counter = 0;
  if (SEGMENT.speed != 0)
  {
    counter = (now * ((SEGMENT.speed >> 3) +1)) & 0xFFFF;
    counter = counter >> 8;
  }

  bool noWrap = (paletteBlend == 2 || (paletteBlend == 0 && SEGMENT.speed == 0));
  for (uint16_t i = 0; i < SEGLEN; i++)
  {
    uint8_t colorIndex = (i * 255 / SEGLEN) - counter;

    if (noWrap) colorIndex = map(colorIndex, 0, 255, 0, 240); //cut off blend at palette "end"

    setPixelColor(i, color_from_palette(colorIndex, false, true, 255));
  }
  return FRAMETIME;
}


// WLED limitation: Analog Clock overlay will NOT work when Fire2012 is active
// Fire2012 by Mark Kriegsman, July 2012
// as part of "Five Elements" shown here: http://youtu.be/knWiGsmgycY
////
// This basic one-dimensional 'fire' simulation works roughly as follows:
// There's a underlying array of 'heat' cells, that model the temperature
// at each point along the line.  Every cycle through the simulation,
// four steps are performed:
//  1) All cells cool down a little bit, losing heat to the air
//  2) The heat from each cell drifts 'up' and diffuses a little
//  3) Sometimes randomly new 'sparks' of heat are added at the bottom
//  4) The heat from each cell is rendered as a color into the leds array
//     The heat-to-color mapping uses a black-body radiation approximation.
//
// Temperature is in arbitrary units from 0 (cold black) to 255 (white hot).
//
// This simulation scales it self a bit depending on NUM_LEDS; it should look
// "OK" on anywhere from 20 to 100 LEDs without too much tweaking.
//
// I recommend running this simulation at anywhere from 30-100 frames per second,
// meaning an interframe delay of about 10-35 milliseconds.
//
// Looks best on a high-density LED setup (60+ pixels/meter).
//
//
// There are two main parameters you can play with to control the look and
// feel of your fire: COOLING (used in step 1 above) (Speed = COOLING), and SPARKING (used
// in step 3 above) (Effect Intensity = Sparking).


uint16_t WS2812FX::mode_fire_2012()
{
  uint32_t it = now >> 5; //div 32

  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed

  byte* heat = SEGENV.data;

  if (it != SEGENV.step)
  {
    // Step 1.  Cool down every cell a little
    for (uint16_t i = 0; i < SEGLEN; i++) {
      SEGENV.data[i] = qsub8(heat[i],  random8(0, (((20 + SEGMENT.speed /3) * 10) / SEGLEN) + 2));
    }

    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for (uint16_t k= SEGLEN -1; k > 1; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }

    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if (random8() <= SEGMENT.intensity) {
      uint8_t y = random8(7);
      if (y < SEGLEN) heat[y] = qadd8(heat[y], random8(160,255));
    }
    SEGENV.step = it;
  }

  // Step 4.  Map from heat cells to LED colors
  for (uint16_t j = 0; j < SEGLEN; j++) {
    CRGB color = ColorFromPalette(currentPalette, MIN(heat[j],240), 255, LINEARBLEND);
    setPixelColor(j, color.red, color.green, color.blue);
  }
  return FRAMETIME;
}


// ColorWavesWithPalettes by Mark Kriegsman: https://gist.github.com/kriegsman/8281905786e8b2632aeb
// This function draws color waves with an ever-changing,
// widely-varying set of parameters, using a color palette.
uint16_t WS2812FX::mode_colorwaves()
{
  uint16_t duration = 10 + SEGMENT.speed;
  uint16_t sPseudotime = SEGENV.step;
  uint16_t sHue16 = SEGENV.aux0;

  uint8_t brightdepth = beatsin88(341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  // uint16_t hueinc16 = beatsin88(113, 300, 1500);
  uint16_t hueinc16 = beatsin88(113, 60, 300)*SEGMENT.intensity*10/255;  // Use the Intensity Slider for the hues

  sPseudotime += duration * msmultiplier;
  sHue16 += duration * beatsin88(400, 5, 9);
  uint16_t brightnesstheta16 = sPseudotime;
  CRGB fastled_col;

  for ( uint16_t i = 0 ; i < SEGLEN; i++) {
    uint8_t hue8 = hue16 >> 8;
    uint16_t h16_128 = hue16 >> 7;
    if ( h16_128 & 0x100) {
      hue8 = 255 - (h16_128 >> 1);
    } else {
      hue8 = h16_128 >> 1;
    }

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);

    CRGB newcolor = ColorFromPalette(currentPalette, hue8, bri8);
    fastled_col = col_to_crgb(getPixelColor(i));

    nblend(fastled_col, newcolor, 128);
    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }
  SEGENV.step = sPseudotime;
  SEGENV.aux0 = sHue16;
  return FRAMETIME;
}


// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
uint16_t WS2812FX::mode_bpm()
{
  CRGB fastled_col;
  uint32_t stp = (now / 20) & 0xFF;
  uint8_t beat = beatsin8(SEGMENT.speed, 64, 255);
  for (uint16_t i = 0; i < SEGLEN; i++) {
//    fastled_col = ColorFromPalette(currentPalette, stp + (i * 2), beat - stp + (i * 10));
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);

    setPixCol(i, stp + (i * 2), beat - stp + (i * 10));

  }
  return FRAMETIME;
}


uint16_t WS2812FX::mode_fillnoise8()
{
  if (SEGENV.call == 0) SEGENV.step = random16(12345);
//  CRGB fastled_col;
  for (uint16_t i = 0; i < SEGLEN; i++) {
    uint8_t index = inoise8(i * SEGLEN, SEGENV.step + i * SEGLEN);

    setPixCol(i, index, sin8(index));

//    fastled_col = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }
//  SEGENV.step += beatsin8(SEGMENT.speed, 1, 6); //10,1,4
  SEGENV.step += (SEGMENT.speed >> 4);

  return FRAMETIME;
}


uint16_t WS2812FX::mode_noise16_1()
{
  uint16_t scale = 320;                                      // the "zoom factor" for the noise
  CRGB fastled_col;
  SEGENV.step += (1 + SEGMENT.speed/16);

  for (uint16_t i = 0; i < SEGLEN; i++) {

    uint16_t shift_x = beatsin8(11);                           // the x position of the noise field swings @ 17 bpm
    uint16_t shift_y = SEGENV.step/42;             // the y position becomes slowly incremented


    uint16_t real_x = (i + shift_x) * scale;                  // the x position of the noise field swings @ 17 bpm
    uint16_t real_y = (i + shift_y) * scale;                  // the y position becomes slowly incremented
    uint32_t real_z = SEGENV.step;                          // the z position becomes quickly incremented

    uint8_t noise = inoise16(real_x, real_y, real_z) >> 8;   // get the noise data and scale it down

    uint8_t index = sin8(noise * 3);                         // map LED color based on noise data

    setPixCol(i, index, sin8(index));

//    fastled_col = ColorFromPalette(currentPalette, index, 255, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }

  return FRAMETIME;
}


uint16_t WS2812FX::mode_noise16_2()
{
  uint16_t scale = 1000;                                       // the "zoom factor" for the noise
  CRGB fastled_col;
  SEGENV.step += (1 + (SEGMENT.speed >> 1));

  for (uint16_t i = 0; i < SEGLEN; i++) {

    uint16_t shift_x = SEGENV.step >> 6;                         // x as a function of time
    uint16_t shift_y = SEGENV.step/42;

    uint32_t real_x = (i + shift_x) * scale;                  // calculate the coordinates within the noise field

    uint8_t noise = inoise16(real_x, 0, 4223) >> 8;    // get the noise data and scale it down

    uint8_t index = sin8(noise * 3);                          // map led color based on noise data

    setPixCol(i, index, sin8(index));
//    fastled_col = ColorFromPalette(currentPalette, index, noise, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }

  return FRAMETIME;
}


uint16_t WS2812FX::mode_noise16_3()
{
  uint16_t scale = 800;                                       // the "zoom factor" for the noise
  CRGB fastled_col;
  SEGENV.step += (1 + SEGMENT.speed);

  for (uint16_t i = 0; i < SEGLEN; i++) {

    uint16_t shift_x = 4223;                                  // no movement along x and y
    uint16_t shift_y = 1234;

    uint32_t real_x = (i + shift_x) * scale;                  // calculate the coordinates within the noise field
    uint32_t real_y = (i + shift_y) * scale;                  // based on the precalculated positions
    uint32_t real_z = SEGENV.step*8;

    uint8_t noise = inoise16(real_x, real_y, real_z) >> 8;    // get the noise data and scale it down

    uint8_t index = sin8(noise * 3);                          // map led color based on noise data

    setPixCol(i, index, sin8(index));
//    fastled_col = ColorFromPalette(currentPalette, index, noise, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }

  return FRAMETIME;
}


//https://github.com/aykevl/ledstrip-spark/blob/master/ledstrip.ino
uint16_t WS2812FX::mode_noise16_4()
{
  CRGB fastled_col;
  uint32_t stp = (now * SEGMENT.speed) >> 7;
  for (uint16_t i = 0; i < SEGLEN; i++) {
    int16_t index = inoise16(uint32_t(i) << 12, stp);

    setPixCol(i, index, sin8(index));
//    fastled_col = ColorFromPalette(currentPalette, index);
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
  }
  return FRAMETIME;
}


//based on https://gist.github.com/kriegsman/5408ecd397744ba0393e
uint16_t WS2812FX::mode_colortwinkle()
{
  uint16_t dataSize = (SEGLEN+7) >> 3; //1 bit per LED
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  CRGB fastled_col, prev;
  fract8 fadeUpAmount = 8 + (SEGMENT.speed/4), fadeDownAmount = 5 + (SEGMENT.speed/7);
  for (uint16_t i = 0; i < SEGLEN; i++) {
    fastled_col = col_to_crgb(getPixelColor(i));
    prev = fastled_col;
    uint16_t index = i >> 3;
    uint8_t  bitNum = i & 0x07;
    bool fadeUp = bitRead(SEGENV.data[index], bitNum);

    if (fadeUp) {
      CRGB incrementalColor = fastled_col;
      incrementalColor.nscale8_video( fadeUpAmount);
      fastled_col += incrementalColor;

      if (fastled_col.red == 255 || fastled_col.green == 255 || fastled_col.blue == 255) {
        bitWrite(SEGENV.data[index], bitNum, false);
      }
      setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);

      if (col_to_crgb(getPixelColor(i)) == prev) //fix "stuck" pixels
      {
        fastled_col += fastled_col;
        setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
      }
    } else {
      fastled_col.nscale8( 255 - fadeDownAmount);
      setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
    }
  }

  for (uint16_t j = 0; j <= SEGLEN / 50; j++)
  {
    if (random8() <= SEGMENT.intensity) {
      for (uint8_t times = 0; times < 5; times++) //attempt to spawn a new pixel 5 times
      {
        int i = random16(SEGLEN);
        if(getPixelColor(i) == 0) {
          fastled_col = ColorFromPalette(currentPalette, random8(), 64, NOBLEND);
          uint16_t index = i >> 3;
          uint8_t  bitNum = i & 0x07;
          bitWrite(SEGENV.data[index], bitNum, true);
          setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
          break; //only spawn 1 new pixel per frame per 50 LEDs
        }
      }
    }
  }
  return FRAMETIME;
}


//Calm effect, like a lake at night
uint16_t WS2812FX::mode_lake() {
  uint8_t sp = SEGMENT.speed/10;
  int wave1 = beatsin8(sp +2, -64,64);
  int wave2 = beatsin8(sp +1, -64,64);
  uint8_t wave3 = beatsin8(sp +2,   0,80);
  CRGB fastled_col;

  for (uint16_t i = 0; i < SEGLEN; i++)
  {
    int index = cos8((i*15)+ wave1)/2 + cubicwave8((i*23)+ wave2)/2;
    uint8_t lum = (index > wave3) ? index - wave3 : 0;

//    fastled_col = ColorFromPalette(currentPalette, map(index,0,255,0,240), lum, LINEARBLEND);
//    setPixelColor(i, fastled_col.red, fastled_col.green, fastled_col.blue);
    setPixCol(i,map(index,0,255,0,240), lum);
  }
  return FRAMETIME;
}


// meteor effect
// send a meteor from begining to to the end of the strip with a trail that randomly decays.
// adapted from https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/#LEDStripEffectMeteorRain
uint16_t WS2812FX::mode_meteor() {
  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed

  byte* trail = SEGENV.data;

  byte meteorSize= 1+ SEGLEN / 10;
  uint16_t counter = now * ((SEGMENT.speed >> 2) +8);
  uint16_t in = counter * SEGLEN >> 16;

  // fade all leds to colors[1] in LEDs one step
  for (uint16_t i = 0; i < SEGLEN; i++) {
    if (random8() <= 255 - SEGMENT.intensity)
    {
      byte meteorTrailDecay = 128 + random8(127);
      trail[i] = scale8(trail[i], meteorTrailDecay);
      setPixelColor(i, color_from_palette(trail[i], false, true, 255));
    }
  }

  // draw meteor
  for(int j = 0; j < meteorSize; j++) {
    uint16_t index = in + j;
    if(index >= SEGLEN) {
      index = (in + j - SEGLEN);
    }

    trail[index] = 240;
    setPixelColor(index, color_from_palette(trail[index], false, true, 255));
  }

  return FRAMETIME;
}


// smooth meteor effect
// send a meteor from begining to to the end of the strip with a trail that randomly decays.
// adapted from https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/#LEDStripEffectMeteorRain
uint16_t WS2812FX::mode_meteor_smooth() {
  if (!SEGENV.allocateData(SEGLEN)) return mode_static(); //allocation failed

  byte* trail = SEGENV.data;

  byte meteorSize= 1+ SEGLEN / 10;
  uint16_t in = map((SEGENV.step >> 6 & 0xFF), 0, 255, 0, SEGLEN -1);

  // fade all leds to colors[1] in LEDs one step
  for (uint16_t i = 0; i < SEGLEN; i++) {
    if (trail[i] != 0 && random8() <= 255 - SEGMENT.intensity)
    {
      int change = 3 - random8(12); //change each time between -8 and +3
      trail[i] += change;
      if (trail[i] > 245) trail[i] = 0;
      if (trail[i] > 240) trail[i] = 240;
      setPixelColor(i, color_from_palette(trail[i], false, true, 255));
    }
  }

  // draw meteor
  for(int j = 0; j < meteorSize; j++) {
    uint16_t index = in + j;
    if(in + j >= SEGLEN) {
      index = (in + j - SEGLEN);
    }
    setPixelColor(index, color_blend(getPixelColor(index), color_from_palette(240, false, true, 255), 48));
    trail[index] = 240;
  }

  SEGENV.step += SEGMENT.speed +1;
  return FRAMETIME;
}


//Railway Crossing / Christmas Fairy lights
uint16_t WS2812FX::mode_railway()
{
  uint16_t dur = 40 + (255 - SEGMENT.speed) * 10;
  uint16_t rampdur = (dur * SEGMENT.intensity) >> 8;
  if (SEGENV.step > dur)
  {
    //reverse direction
    SEGENV.step = 0;
    SEGENV.aux0 = !SEGENV.aux0;
  }
  uint8_t pos = 255;
  if (rampdur != 0)
  {
    uint16_t p0 = (SEGENV.step * 255) / rampdur;
    if (p0 < 255) pos = p0;
  }
  if (SEGENV.aux0) pos = 255 - pos;
  for (uint16_t i = 0; i < SEGLEN; i += 2)
  {
    setPixelColor(i, color_from_palette(255 - pos, false, false, 255));
    if (i < SEGLEN -1)
    {
      setPixelColor(i + 1, color_from_palette(pos, false, false, 255));
    }
  }
  SEGENV.step += FRAMETIME;
  return FRAMETIME;
}


//Water ripple
//propagation velocity from speed
//drop rate from intensity

//4 bytes
typedef struct Ripple {
  uint8_t state;
  uint8_t color;
  uint16_t pos;
} ripple;

uint16_t WS2812FX::ripple_base(bool rainbow)
{
  uint16_t maxRipples = 1 + (SEGLEN >> 2);
  if (maxRipples > 100) maxRipples = 100;
  uint16_t dataSize = sizeof(ripple) * maxRipples;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Ripple* ripples = reinterpret_cast<Ripple*>(SEGENV.data);

  // ranbow background or chosen background, all very dim.
  if (rainbow) {
    if (SEGENV.call ==0) {
      SEGENV.aux0 = random8();
      SEGENV.aux1 = random8();
    }
    if (SEGENV.aux0 == SEGENV.aux1) {
      SEGENV.aux1 = random8();
    }
    else if (SEGENV.aux1 > SEGENV.aux0) {
      SEGENV.aux0++;
    } else {
      SEGENV.aux0--;
    }
    fill(color_blend(color_wheel(SEGENV.aux0),BLACK,235));
  } else {
    fill(SEGCOLOR(1));
  }

  //draw wave
  for (uint16_t i = 0; i < maxRipples; i++)
  {
    uint16_t ripplestate = ripples[i].state;
    if (ripplestate)
    {
      uint8_t rippledecay = (SEGMENT.speed >> 4) +1; //faster decay if faster propagation
      uint16_t rippleorigin = ripples[i].pos;
      uint32_t col = color_from_palette(ripples[i].color, false, false, 255);
      uint16_t propagation = ((ripplestate/rippledecay -1) * SEGMENT.speed);
      int16_t propI = propagation >> 8;
      uint8_t propF = propagation & 0xFF;
      int16_t left = rippleorigin - propI -1;
      uint8_t amp = (ripplestate < 17) ? triwave8((ripplestate-1)*8) : map(ripplestate,17,255,255,2);

      for (int16_t v = left; v < left +4; v++)
      {
        uint8_t mag = scale8(cubicwave8((propF>>2)+(v-left)*64), amp);
        if (v < SEGLEN && v >= 0)
        {
          setPixelColor(v, color_blend(getPixelColor(v), col, mag));
        }
        int16_t w = left + propI*2 + 3 -(v-left);
        if (w < SEGLEN && w >= 0)
        {
          setPixelColor(w, color_blend(getPixelColor(w), col, mag));
        }
      }
      ripplestate += rippledecay;
      ripples[i].state = (ripplestate > 254) ? 0 : ripplestate;
    } else //randomly create new wave
    {
      if (random16(IBN + 10000) <= SEGMENT.intensity)
      {
        ripples[i].state = 1;
        ripples[i].pos = random16(SEGLEN);
        ripples[i].color = random8(); //color
      }
    }
  }
  return FRAMETIME;
}

uint16_t WS2812FX::mode_ripple(void) {
  return ripple_base(false);
}

uint16_t WS2812FX::mode_ripple_rainbow(void) {
  return ripple_base(true);
}



//  TwinkleFOX by Mark Kriegsman: https://gist.github.com/kriegsman/756ea6dcae8e30845b5a
//
//  TwinkleFOX: Twinkling 'holiday' lights that fade in and out.
//  Colors are chosen from a palette. Read more about this effect using the link above!

// If COOL_LIKE_INCANDESCENT is set to 1, colors will
// fade out slighted 'reddened', similar to how
// incandescent bulbs change color as they get dim down.
#define COOL_LIKE_INCANDESCENT 1

CRGB WS2812FX::twinklefox_one_twinkle(uint32_t ms, uint8_t salt, bool cat)
{
  // Overall twinkle speed (changed)
  uint16_t ticks = ms / (32 - (SEGMENT.speed >> 3));
  uint8_t fastcycle8 = ticks;
  uint16_t slowcycle16 = (ticks >> 8) + salt;
  slowcycle16 += sin8(slowcycle16);
  slowcycle16 = (slowcycle16 * 2053) + 1384;
  uint8_t slowcycle8 = (slowcycle16 & 0xFF) + (slowcycle16 >> 8);

  // Overall twinkle density.
  // 0 (NONE lit) to 8 (ALL lit at once).
  // Default is 5.
  uint8_t twinkleDensity = (SEGMENT.intensity >> 5) +1;

  uint8_t bright = 0;
  if (((slowcycle8 & 0x0E)/2) < twinkleDensity) {
    uint8_t ph = fastcycle8;
    // This is like 'triwave8', which produces a
    // symmetrical up-and-down triangle sawtooth waveform, except that this
    // function produces a triangle wave with a faster attack and a slower decay
    if (cat) //twinklecat, variant where the leds instantly turn on
    {
      bright = 255 - ph;
    } else { //vanilla twinklefox
      if (ph < 86) {
      bright = ph * 3;
      } else {
        ph -= 86;
        bright = 255 - (ph + (ph/2));
      }
    }
  }

  uint8_t hue = slowcycle8 - salt;
  CRGB c;
  if (bright > 0) {
    c = ColorFromPalette(currentPalette, hue, bright, NOBLEND);
    if(COOL_LIKE_INCANDESCENT == 1) {
      // This code takes a pixel, and if its in the 'fading down'
      // part of the cycle, it adjusts the color a little bit like the
      // way that incandescent bulbs fade toward 'red' as they dim.
      if (fastcycle8 >= 128)
      {
        uint8_t cooling = (fastcycle8 - 128) >> 4;
        c.g = qsub8(c.g, cooling);
        c.b = qsub8(c.b, cooling * 2);
      }
    }
  } else {
    c = CRGB::Black;
  }
  return c;
}

//  This function loops over each pixel, calculates the
//  adjusted 'clock' that this pixel should use, and calls
//  "CalculateOneTwinkle" on each pixel.  It then displays
//  either the twinkle color of the background color,
//  whichever is brighter.
uint16_t WS2812FX::twinklefox_base(bool cat)
{
  // "PRNG16" is the pseudorandom number generator
  // It MUST be reset to the same starting value each time
  // this function is called, so that the sequence of 'random'
  // numbers that it generates is (paradoxically) stable.
  uint16_t PRNG16 = 11337;

  // Set up the background color, "bg".
  // if AUTO_SELECT_BACKGROUND_COLOR == 1, and the first two colors of
  // the current palette are identical, then a deeply faded version of
  // that color is used for the background color
  CRGB bg;
  bg = col_to_crgb(SEGCOLOR(1));
  uint8_t bglight = bg.getAverageLight();
  if (bglight > 64) {
    bg.nscale8_video(16); // very bright, so scale to 1/16th
  } else if (bglight > 16) {
    bg.nscale8_video(64); // not that bright, so scale to 1/4th
  } else {
    bg.nscale8_video(86); // dim, scale to 1/3rd.
  }

  uint8_t backgroundBrightness = bg.getAverageLight();

  for (uint16_t i = 0; i < SEGLEN; i++) {

    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    uint16_t myclockoffset16= PRNG16; // use that number as clock offset
    PRNG16 = (uint16_t)(PRNG16 * 2053) + 1384; // next 'random' number
    // use that number as clock speed adjustment factor (in 8ths, from 8/8ths to 23/8ths)
    uint8_t myspeedmultiplierQ5_3 =  ((((PRNG16 & 0xFF)>>4) + (PRNG16 & 0x0F)) & 0x0F) + 0x08;
    uint32_t myclock30 = (uint32_t)((now * myspeedmultiplierQ5_3) >> 3) + myclockoffset16;
    uint8_t  myunique8 = PRNG16 >> 8; // get 'salt' value for this pixel

    // We now have the adjusted 'clock' for this pixel, now we call
    // the function that computes what color the pixel should be based
    // on the "brightness = f( time )" idea.
    CRGB c = twinklefox_one_twinkle(myclock30, myunique8, cat);

    uint8_t cbright = c.getAverageLight();
    int16_t deltabright = cbright - backgroundBrightness;
    if (deltabright >= 32 || (!bg)) {
      // If the new pixel is significantly brighter than the background color,
      // use the new color.
      setPixelColor(i, c.red, c.green, c.blue);
    } else if (deltabright > 0) {
      // If the new pixel is just slightly brighter than the background color,
      // mix a blend of the new color and the background color
      setPixelColor(i, color_blend(crgb_to_col(bg), crgb_to_col(c), deltabright * 8));
    } else {
      // if the new pixel is not at all brighter than the background color,
      // just use the background color.
      setPixelColor(i, bg.r, bg.g, bg.b);
    }
  }
  return FRAMETIME;
}

uint16_t WS2812FX::mode_twinklefox()
{
  return twinklefox_base(false);
}

uint16_t WS2812FX::mode_twinklecat()
{
  return twinklefox_base(true);
}


//inspired by https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/#LEDStripEffectBlinkingHalloweenEyes
#define HALLOWEEN_EYE_SPACE 3
#define HALLOWEEN_EYE_WIDTH 1

uint16_t WS2812FX::mode_halloween_eyes()
{
  uint16_t eyeLength = (2*HALLOWEEN_EYE_WIDTH) + HALLOWEEN_EYE_SPACE;
  if (eyeLength > SEGLEN) return mode_static(); //bail if segment too short

  fill(SEGCOLOR(1)); //fill background

  uint8_t state = SEGENV.aux1 >> 8;
  uint16_t stateTime = SEGENV.call;
  if (stateTime == 0) stateTime = 2000;

  if (state == 0) { //spawn eyes
    SEGENV.aux0 = random16(0, SEGLEN - eyeLength); //start pos
    SEGENV.aux1 = random8(); //color
    state = 1;
  }

  if (state < 2) { //fade eyes
    uint16_t startPos    = SEGENV.aux0;
    uint16_t start2ndEye = startPos + HALLOWEEN_EYE_WIDTH + HALLOWEEN_EYE_SPACE;

    uint32_t fadestage = (now - SEGENV.step)*255 / stateTime;
    if (fadestage > 255) fadestage = 255;
    uint32_t c = color_blend(color_from_palette(SEGENV.aux1 & 0xFF, false, false, 0), SEGCOLOR(1), fadestage);

    for (uint16_t i = 0; i < HALLOWEEN_EYE_WIDTH; i++)
    {
      setPixelColor(startPos    + i, c);
      setPixelColor(start2ndEye + i, c);
    }
  }

  if (now - SEGENV.step > stateTime)
  {
    state++;
    if (state > 2) state = 0;

    if (state < 2)
    {
      stateTime = 100 + (255 - SEGMENT.intensity)*10; //eye fade time
    } else {
      uint16_t eyeOffTimeBase = (255 - SEGMENT.speed)*10;
      stateTime = eyeOffTimeBase + random16(eyeOffTimeBase);
    }
    SEGENV.step = now;
    SEGENV.call = stateTime;
  }

  SEGENV.aux1 = (SEGENV.aux1 & 0xFF) + (state << 8); //save state

  return FRAMETIME;
}


//Speed slider sets amount of LEDs lit, intensity sets unlit
uint16_t WS2812FX::mode_static_pattern()
{
  uint16_t lit = 1 + SEGMENT.speed;
  uint16_t unlit = 1 + SEGMENT.intensity;
  bool drawingLit = true;
  uint16_t cnt = 0;

  for (uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, (drawingLit) ? color_from_palette(i, true, PALETTE_SOLID_WRAP, 0) : SEGCOLOR(1));
    cnt++;
    if (cnt >= ((drawingLit) ? lit : unlit)) {
      cnt = 0;
      drawingLit = !drawingLit;
    }
  }

  return FRAMETIME;
}

uint16_t WS2812FX::mode_tri_static_pattern()
{
  uint8_t segSize = (SEGMENT.intensity >> 5) +1;
  uint8_t currSeg = 0;
  uint16_t currSegCount = 0;

  for (uint16_t i = 0; i < SEGLEN; i++) {
    if ( currSeg % 3 == 0 ) {
      setPixelColor(i, SEGCOLOR(0));
    } else if( currSeg % 3 == 1) {
      setPixelColor(i, SEGCOLOR(1));
    } else {
      setPixelColor(i, (SEGCOLOR(2) > 0 ? SEGCOLOR(2) : WHITE));
    }
    currSegCount += 1;
    if (currSegCount >= segSize) {
      currSeg +=1;
      currSegCount = 0;
    }
  }

  return FRAMETIME;
}


uint16_t WS2812FX::spots_base(uint16_t threshold)
{
  fill(SEGCOLOR(1));

  uint16_t maxZones = SEGLEN >> 2;
  uint16_t zones = 1 + ((SEGMENT.intensity * maxZones) >> 8);
  uint16_t zoneLen = SEGLEN / zones;
  uint16_t offset = (SEGLEN - zones * zoneLen) >> 1;

  for (uint16_t z = 0; z < zones; z++)
  {
    uint16_t pos = offset + z * zoneLen;
    for (uint16_t i = 0; i < zoneLen; i++)
    {
      uint16_t wave = triwave16((i * 0xFFFF) / zoneLen);
      if (wave > threshold) {
        uint16_t index = 0 + pos + i;
        uint8_t s = (wave - threshold)*255 / (0xFFFF - threshold);
        setPixelColor(index, color_blend(color_from_palette(index, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), 255-s));
      }
    }
  }

  return FRAMETIME;
}


//Intensity slider sets number of "lights", speed sets LEDs per light
uint16_t WS2812FX::mode_spots()
{
  return spots_base((255 - SEGMENT.speed) << 8);
}


//Intensity slider sets number of "lights", LEDs per light fade in and out
uint16_t WS2812FX::mode_spots_fade()
{
  uint16_t counter = now * ((SEGMENT.speed >> 2) +8);
  uint16_t t = triwave16(counter);
  uint16_t tr = (t >> 1) + (t >> 2);
  return spots_base(tr);
}


//each needs 12 bytes
//Spark type is used for popcorn and 1D fireworks
typedef struct Ball {
  unsigned long lastBounceTime;
  float impactVelocity;
  float height;
} ball;

/*
*  Bouncing Balls Effect
*/
uint16_t WS2812FX::mode_bouncing_balls(void) {
  //allocate segment data
  uint16_t maxNumBalls = 16;
  uint16_t dataSize = sizeof(ball) * maxNumBalls;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Ball* balls = reinterpret_cast<Ball*>(SEGENV.data);

  // number of balls based on intensity setting to max of 7 (cycles colors)
  // non-chosen color is a random color
  uint8_t numBalls = int(((SEGMENT.intensity * (maxNumBalls - 0.8f)) / 255) + 1);

  float gravity                           = -9.81; // standard value of gravity
  float impactVelocityStart               = sqrt( -2 * gravity);

  unsigned long time = millis();

  if (SEGENV.call == 0) {
    for (uint8_t i = 0; i < maxNumBalls; i++) balls[i].lastBounceTime = time;
  }

  bool hasCol2 = SEGCOLOR(2);
  fill(hasCol2 ? BLACK : SEGCOLOR(1));

  for (uint8_t i = 0; i < numBalls; i++) {
    float timeSinceLastBounce = (time - balls[i].lastBounceTime)/((255-SEGMENT.speed)*8/256 +1);
    balls[i].height = 0.5 * gravity * pow(timeSinceLastBounce/1000 , 2.0) + balls[i].impactVelocity * timeSinceLastBounce/1000;

    if (balls[i].height < 0) { //start bounce
      balls[i].height = 0;
      //damping for better effect using multiple balls
      float dampening = 0.90 - float(i)/pow(numBalls,2);
      balls[i].impactVelocity = dampening * balls[i].impactVelocity;
      balls[i].lastBounceTime = time;

      if (balls[i].impactVelocity < 0.015) {
        balls[i].impactVelocity = impactVelocityStart;
      }
    }

    uint32_t color = SEGCOLOR(0);
    if (SEGMENT.palette) {
      color = color_wheel(i*(256/MAX(numBalls, 8)));
    } else if (hasCol2) {
      color = SEGCOLOR(i % NUM_COLORS);
    }

    uint16_t pos = round(balls[i].height * (SEGLEN - 1));
    setPixelColor(pos, color);
  }

  return FRAMETIME;
}


/*
* Sinelon stolen from FASTLED examples
*/
uint16_t WS2812FX::sinelon_base(bool dual, bool rainbow=false) {
  fade_out(SEGMENT.intensity);
  uint16_t pos = beatsin16(SEGMENT.speed/10,0,SEGLEN-1);
  if (SEGENV.call == 0) SEGENV.aux0 = pos;
  uint32_t color1 = color_from_palette(pos, true, false, 0);
  uint32_t color2 = SEGCOLOR(2);
  if (rainbow) {
    color1 = color_wheel((pos & 0x07) * 32);
  }
  setPixelColor(pos, color1);
  if (dual) {
    if (!color2) color2 = color_from_palette(pos, true, false, 0);
    if (rainbow) color2 = color1; //rainbow
    setPixelColor(SEGLEN-1-pos, color2);
  }
  if (SEGENV.aux0 != pos) {
    if (SEGENV.aux0 < pos) {
      for (uint16_t i = SEGENV.aux0; i < pos ; i++) {
        setPixelColor(i, color1);
        if (dual) setPixelColor(SEGLEN-1-i, color2);
      }
    } else {
      for (uint16_t i = SEGENV.aux0; i > pos ; i--) {
        setPixelColor(i, color1);
        if (dual) setPixelColor(SEGLEN-1-i, color2);
      }
    }
    SEGENV.aux0 = pos;
  }

  return FRAMETIME;
}

uint16_t WS2812FX::mode_sinelon(void) {
  return sinelon_base(false);
}

uint16_t WS2812FX::mode_sinelon_dual(void) {
  return sinelon_base(true);
}

uint16_t WS2812FX::mode_sinelon_rainbow(void) {
  return sinelon_base(true, true);
}


//Rainbow with glitter, inspired by https://gist.github.com/kriegsman/062e10f7f07ba8518af6
uint16_t WS2812FX::mode_glitter()
{
  mode_palette();

  if (SEGMENT.intensity > random8())
  {
    setPixelColor(random16(SEGLEN), ULTRAWHITE);
  }

  return FRAMETIME;
}



//each needs 12 bytes
//Spark type is used for popcorn, 1D fireworks, and drip
typedef struct Spark {
  float pos;
  float vel;
  uint16_t col;
  uint8_t colIndex;
} spark;

/*
*  POPCORN
*  modified from https://github.com/kitesurfer1404/WS2812FX/blob/master/src/custom/Popcorn.h
*/
uint16_t WS2812FX::mode_popcorn(void) {
  //allocate segment data
  uint16_t maxNumPopcorn = 24;
  uint16_t dataSize = sizeof(spark) * maxNumPopcorn;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  Spark* popcorn = reinterpret_cast<Spark*>(SEGENV.data);

  float gravity = -0.0001 - (SEGMENT.speed/200000.0); // m/s/s
  gravity *= SEGLEN;

  bool hasCol2 = SEGCOLOR(2);
  fill(hasCol2 ? BLACK : SEGCOLOR(1));

  uint8_t numPopcorn = SEGMENT.intensity*maxNumPopcorn/255;
  if (numPopcorn == 0) numPopcorn = 1;

  for(uint8_t i = 0; i < numPopcorn; i++) {
    bool isActive = popcorn[i].pos >= 0.0f;

    if (isActive) { // if kernel is active, update its position
      popcorn[i].pos += popcorn[i].vel;
      popcorn[i].vel += gravity;
      uint32_t col = color_wheel(popcorn[i].colIndex);
      if (!SEGMENT.palette && popcorn[i].colIndex < NUM_COLORS) col = SEGCOLOR(popcorn[i].colIndex);

      uint16_t ledIndex = popcorn[i].pos;
      if (ledIndex < SEGLEN) setPixelColor(ledIndex, col);
    } else { // if kernel is inactive, randomly pop it
      if (random8() < 2) { // POP!!!
        popcorn[i].pos = 0.01f;

        uint16_t peakHeight = 128 + random8(128); //0-255
        peakHeight = (peakHeight * (SEGLEN -1)) >> 8;
        popcorn[i].vel = sqrt(-2.0 * gravity * peakHeight);

        if (SEGMENT.palette)
        {
          popcorn[i].colIndex = random8();
        } else {
          byte col = random8(0, NUM_COLORS);
          if (!hasCol2 || !SEGCOLOR(col)) col = 0;
          popcorn[i].colIndex = col;
        }
      }
    }
  }

  return FRAMETIME;
}


//values close to 100 produce 5Hz flicker, which looks very candle-y
//Inspired by https://github.com/avanhanegem/ArduinoCandleEffectNeoPixel
//and https://cpldcpu.wordpress.com/2016/01/05/reverse-engineering-a-real-candle/

uint16_t WS2812FX::candle(bool multi)
{
  if (multi)
  {
    //allocate segment data
    uint16_t dataSize = (SEGLEN -1) *3;
    if (!SEGENV.allocateData(dataSize)) return candle(false); //allocation failed
  }

  //max. flicker range controlled by intensity
  uint8_t valrange = SEGMENT.intensity;
  uint8_t rndval = valrange >> 1;

  //step (how much to move closer to target per frame) coarsely set by speed
  uint8_t speedFactor = 4;
  if (SEGMENT.speed > 252) { //epilepsy
    speedFactor = 1;
  } else if (SEGMENT.speed > 99) { //regular candle (mode called every ~25 ms, so 4 frames to have a new target every 100ms)
    speedFactor = 2;
  } else if (SEGMENT.speed > 49) { //slower fade
    speedFactor = 3;
  } //else 4 (slowest)

  uint16_t numCandles = (multi) ? SEGLEN : 1;

  for (uint16_t i = 0; i < numCandles; i++)
  {
    uint16_t d = 0; //data location

    uint8_t s = SEGENV.aux0, s_target = SEGENV.aux1, fadeStep = SEGENV.step;
    if (i > 0) {
      d = (i-1) *3;
      s = SEGENV.data[d]; s_target = SEGENV.data[d+1]; fadeStep = SEGENV.data[d+2];
    }
    if (fadeStep == 0) { //init vals
      s = 128; s_target = 130 + random8(4); fadeStep = 1;
    }

    bool newTarget = false;
    if (s_target > s) { //fade up
      s = qadd8(s, fadeStep);
      if (s >= s_target) newTarget = true;
    } else {
      s = qsub8(s, fadeStep);
      if (s <= s_target) newTarget = true;
    }

    if (newTarget) {
      s_target = random8(rndval) + random8(rndval);
      if (s_target < (rndval >> 1)) s_target = (rndval >> 1) + random8(rndval);
      uint8_t offset = (255 - valrange) >> 1;
      s_target += offset;

      uint8_t dif = (s_target > s) ? s_target - s : s - s_target;

      fadeStep = dif >> speedFactor;
      if (fadeStep == 0) fadeStep = 1;
    }

     if (i > 0) {
      setPixelColor(i, color_blend(SEGCOLOR(1), color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), s));

      SEGENV.data[d] = s; SEGENV.data[d+1] = s_target; SEGENV.data[d+2] = fadeStep;
    } else {
      for (uint16_t j = 0; j < SEGLEN; j++) {
        setPixelColor(j, color_blend(SEGCOLOR(1), color_from_palette(j, true, PALETTE_SOLID_WRAP, 0), s));
      }

      SEGENV.aux0 = s; SEGENV.aux1 = s_target; SEGENV.step = fadeStep;
    }
  }

  return FRAMETIME;
}

uint16_t WS2812FX::mode_candle()
{
  return candle(false);
}


uint16_t WS2812FX::mode_candle_multi()
{
  return candle(true);
}


/*
/ Fireworks in starburst effect
/ based on the video: https://www.reddit.com/r/arduino/comments/c3sd46/i_made_this_fireworks_effect_for_my_led_strips/
/ Speed sets frequency of new starbursts, intensity is the intensity of the burst
*/
#define STARBURST_MAX_FRAG 12

//each needs 64 byte
typedef struct particle {
  CRGB     color;
  uint32_t birth  =0;
  uint32_t last   =0;
  float    vel    =0;
  uint16_t pos    =-1;
  float    fragment[STARBURST_MAX_FRAG];
} star;

uint16_t WS2812FX::mode_starburst(void) {
  uint8_t numStars = 1 + (SEGLEN >> 3);
  if (numStars > 15) numStars = 15;
  uint16_t dataSize = sizeof(star) * numStars;

  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  uint32_t it = millis();

  star* stars = reinterpret_cast<star*>(SEGENV.data);

  float          maxSpeed                = 375.0f;  // Max velocity
  float          particleIgnition        = 250.0f;  // How long to "flash"
  float          particleFadeTime        = 1500.0f; // Fade out time

  for (int j = 0; j < numStars; j++)
  {
    // speed to adjust chance of a burst, max is nearly always.
    if (random8((144-(SEGMENT.speed >> 1))) == 0 && stars[j].birth == 0)
    {
      // Pick a random color and location.
      uint16_t startPos = random16(SEGLEN-1);
      float multiplier = (float)(random8())/255.0 * 1.0;

      stars[j].color = col_to_crgb(color_wheel(random8()));
      stars[j].pos = startPos;
      stars[j].vel = maxSpeed * (float)(random8())/255.0 * multiplier;
      stars[j].birth = it;
      stars[j].last = it;
      // more fragments means larger burst effect
      int num = random8(3,6 + (SEGMENT.intensity >> 5));

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        if (i < num) stars[j].fragment[i] = startPos;
        else stars[j].fragment[i] = -1;
      }
    }
  }

  fill(SEGCOLOR(1));

  for (int j=0; j<numStars; j++)
  {
    if (stars[j].birth != 0) {
      float dt = (it-stars[j].last)/1000.0;

      for (int i=0; i < STARBURST_MAX_FRAG; i++) {
        int var = i >> 1;

        if (stars[j].fragment[i] > 0) {
          //all fragments travel right, will be mirrored on other side
          stars[j].fragment[i] += stars[j].vel * dt * (float)var/3.0;
        }
      }
      stars[j].last = it;
      stars[j].vel -= 3*stars[j].vel*dt;
    }

    CRGB c = stars[j].color;

    // If the star is brand new, it flashes white briefly.
    // Otherwise it just fades over time.
    float fade = 0.0f;
    float age = it-stars[j].birth;

    if (age < particleIgnition) {
      c = col_to_crgb(color_blend(WHITE, crgb_to_col(c), 254.5f*((age / particleIgnition))));
    } else {
      // Figure out how much to fade and shrink the star based on
      // its age relative to its lifetime
      if (age > particleIgnition + particleFadeTime) {
        fade = 1.0f;                  // Black hole, all faded out
        stars[j].birth = 0;
        c = col_to_crgb(SEGCOLOR(1));
      } else {
        age -= particleIgnition;
        fade = (age / particleFadeTime);  // Fading star
        byte f = 254.5f*fade;
        c = col_to_crgb(color_blend(crgb_to_col(c), SEGCOLOR(1), f));
      }
    }

    float particleSize = (1.0 - fade) * 2;

    for (uint8_t index=0; index < STARBURST_MAX_FRAG*2; index++) {
      bool mirrored = index & 0x1;
      uint8_t i = index >> 1;
      if (stars[j].fragment[i] > 0) {
        float loc = stars[j].fragment[i];
        if (mirrored) loc -= (loc-stars[j].pos)*2;
        int start = loc - particleSize;
        int end = loc + particleSize;
        if (start < 0) start = 0;
        if (start == end) end++;
        if (end > SEGLEN) end = SEGLEN;
        for (int p = start; p < end; p++) {
          setPixelColor(p, c.r, c.g, c.b);
        }
      }
    }
  }
  return FRAMETIME;
}


/*
 * Exploding fireworks effect
 * adapted from: http://www.anirama.com/1000leds/1d-fireworks/
 */

uint16_t WS2812FX::mode_exploding_fireworks(void)
{
  //allocate segment data
  uint16_t numSparks = 2 + (SEGLEN >> 1);
  if (numSparks > 80) numSparks = 80;
  uint16_t dataSize = sizeof(spark) * numSparks;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  fill(BLACK);

  bool actuallyReverse = SEGMENT.getOption(SEG_OPTION_REVERSED);
  //have fireworks start in either direction based on intensity
  SEGMENT.setOption(SEG_OPTION_REVERSED, SEGENV.step);

  Spark* sparks = reinterpret_cast<Spark*>(SEGENV.data);
  Spark* flare = sparks; //first spark is flare data

  float gravity = -0.0004 - (SEGMENT.speed/800000.0); // m/s/s
  gravity *= SEGLEN;

  if (SEGENV.aux0 < 2) { //FLARE
    if (SEGENV.aux0 == 0) { //init flare
      flare->pos = 0;
      uint16_t peakHeight = 75 + random8(180); //0-255
      peakHeight = (peakHeight * (SEGLEN -1)) >> 8;
      flare->vel = sqrt(-2.0 * gravity * peakHeight);
      flare->col = 255; //brightness

      SEGENV.aux0 = 1;
    }

    // launch
    if (flare->vel > 12 * gravity) {
      // flare
      setPixelColor(int(flare->pos),flare->col,flare->col,flare->col);

      flare->pos += flare->vel;
      flare->pos = constrain(flare->pos, 0, SEGLEN-1);
      flare->vel += gravity;
      flare->col -= 2;
    } else {
      SEGENV.aux0 = 2;  // ready to explode
    }
  } else if (SEGENV.aux0 < 4) {
    /*
     * Explode!
     *
     * Explosion happens where the flare ended.
     * Size is proportional to the height.
     */
    int nSparks = flare->pos;
    nSparks = constrain(nSparks, 0, numSparks);
    static float dying_gravity;

    // initialize sparks
    if (SEGENV.aux0 == 2) {
      for (int i = 1; i < nSparks; i++) {
        sparks[i].pos = flare->pos;
        sparks[i].vel = (float(random16(0, 20000)) / 10000.0) - 0.9; // from -0.9 to 1.1
        sparks[i].col = 345;//abs(sparks[i].vel * 750.0); // set colors before scaling velocity to keep them bright
        //sparks[i].col = constrain(sparks[i].col, 0, 345);
        sparks[i].colIndex = random8();
        sparks[i].vel *= flare->pos/SEGLEN; // proportional to height
        sparks[i].vel *= -gravity *50;
      }
      //sparks[1].col = 345; // this will be our known spark
      dying_gravity = gravity/2;
      SEGENV.aux0 = 3;
    }

    if (sparks[1].col > 4) {//&& sparks[1].pos > 0) { // as long as our known spark is lit, work with all the sparks
      for (int i = 1; i < nSparks; i++) {
        sparks[i].pos += sparks[i].vel;
        sparks[i].vel += dying_gravity;
        if (sparks[i].col > 3) sparks[i].col -= 4;

        if (sparks[i].pos > 0 && sparks[i].pos < SEGLEN) {
          uint16_t prog = sparks[i].col;
          uint32_t spColor = (SEGMENT.palette) ? color_wheel(sparks[i].colIndex) : SEGCOLOR(0);
          CRGB c = CRGB::Black; //HeatColor(sparks[i].col);
          if (prog > 300) { //fade from white to spark color
            c = col_to_crgb(color_blend(spColor, WHITE, (prog - 300)*5));
          } else if (prog > 45) { //fade from spark color to black
            c = col_to_crgb(color_blend(BLACK, spColor, prog - 45));
            uint8_t cooling = (300 - prog) >> 5;
            c.g = qsub8(c.g, cooling);
            c.b = qsub8(c.b, cooling * 2);
          }
          setPixelColor(int(sparks[i].pos), c.red, c.green, c.blue);
        }
      }
      dying_gravity *= .99; // as sparks burn out they fall slower
    } else {
      SEGENV.aux0 = 6 + random8(10); //wait for this many frames
    }
  } else {
    SEGENV.aux0--;
    if (SEGENV.aux0 < 4) {
      SEGENV.aux0 = 0; //back to flare
      SEGENV.step = (SEGMENT.intensity > random8()); //decide firing side
    }
  }

  SEGMENT.setOption(SEG_OPTION_REVERSED, actuallyReverse);

  return FRAMETIME;
}


/*
 * Drip Effect
 * ported of: https://www.youtube.com/watch?v=sru2fXh4r7k
 */
uint16_t WS2812FX::mode_drip(void)
{
  //allocate segment data
  uint16_t numDrops = 4;
  uint16_t dataSize = sizeof(spark) * numDrops;
  if (!SEGENV.allocateData(dataSize)) return mode_static(); //allocation failed

  fill(SEGCOLOR(1));

  Spark* drops = reinterpret_cast<Spark*>(SEGENV.data);

  numDrops = 1 + (SEGMENT.intensity >> 6);

  float gravity = -0.001 - (SEGMENT.speed/50000.0);
  gravity *= SEGLEN;
  int sourcedrop = 12;

  for (int j=0;j<numDrops;j++) {
    if (drops[j].colIndex == 0) { //init
      drops[j].pos = SEGLEN-1;    // start at end
      drops[j].vel = 0;           // speed
      drops[j].col = sourcedrop;  // brightness
      drops[j].colIndex = 1;      // drop state (0 init, 1 forming, 2 falling, 5 bouncing)
    }

    setPixelColor(SEGLEN-1,color_blend(BLACK,SEGCOLOR(0), sourcedrop));// water source
    if (drops[j].colIndex==1) {
      if (drops[j].col>255) drops[j].col=255;
      setPixelColor(int(drops[j].pos),color_blend(BLACK,SEGCOLOR(0),drops[j].col));

      drops[j].col += map(SEGMENT.speed, 0, 255, 1, 6); // swelling

      if (random8() < drops[j].col/10) {               // random drop
        drops[j].colIndex=2;               //fall
        drops[j].col=255;
      }
    }
    if (drops[j].colIndex > 1) {           // falling
      if (drops[j].pos > 0) {              // fall until end of segment
        drops[j].pos += drops[j].vel;
        if (drops[j].pos < 0) drops[j].pos = 0;
        drops[j].vel += gravity;

        for (int i=1;i<7-drops[j].colIndex;i++) { // some minor math so we don't expand bouncing droplets
          setPixelColor(int(drops[j].pos)+i,color_blend(BLACK,SEGCOLOR(0),drops[j].col/i)); //spread pixel with fade while falling
        }

        if (drops[j].colIndex > 2) {       // during bounce, some water is on the floor
          setPixelColor(0,color_blend(SEGCOLOR(0),BLACK,drops[j].col));
        }
      } else {                             // we hit bottom
        if (drops[j].colIndex > 2) {       // already hit once, so back to forming
          drops[j].colIndex = 0;
          drops[j].col = sourcedrop;

        } else {

          if (drops[j].colIndex==2) {      // init bounce
            drops[j].vel = -drops[j].vel/4;// reverse velocity with damping
            drops[j].pos += drops[j].vel;
          }
          drops[j].col = sourcedrop*2;
          drops[j].colIndex = 5;           // bouncing
        }
      }
    }
  }
  return FRAMETIME;
}


/*
/ Plasma Effect
/ adapted from https://github.com/atuline/FastLED-Demos/blob/master/plasma/plasma.ino
*/
uint16_t WS2812FX::mode_plasma(void) {
  uint8_t thisPhase = beatsin8(6,-64,64);                       // Setting phase change for a couple of waves.
  uint8_t thatPhase = beatsin8(7,-64,64);

  for (int i = 0; i < SEGLEN; i++) {   // For each of the LED's in the strand, set color &  brightness based on a wave as follows:
    uint8_t colorIndex = cubicwave8((i*(1+ 3*(SEGMENT.speed >> 5)))+(thisPhase) & 0xFF)/2   // factor=23 // Create a wave and add a phase change and add another wave with its own phase change.
                             + cos8((i*(1+ 2*(SEGMENT.speed >> 5)))+(thatPhase) & 0xFF)/2;  // factor=15 // Hey, you can even change the frequencies if you wish.
    uint8_t thisBright = qsub8(colorIndex, beatsin8(6,0, (255 - SEGMENT.intensity)|0x01 ));

    setPixCol(i, colorIndex, thisBright);
//    CRGB color = ColorFromPalette(currentPalette, colorIndex, thisBright, LINEARBLEND);
//    setPixelColor(i, color.red, color.green, color.blue);
  }

  return FRAMETIME;
}



/*
 * Percentage display
 * Intesity values from 0-100 turn on the leds.
 */
uint16_t WS2812FX::mode_percent(void) {

	uint8_t percent = MAX(0, MIN(200, SEGMENT.intensity));
	uint16_t active_leds = (percent < 100) ? SEGLEN * percent / 100.0
                                         : SEGLEN * (200 - percent) / 100.0;

  uint8_t size = (1 + ((SEGMENT.speed * SEGLEN) >> 11));
  if (SEGMENT.speed == 255) size = 255;

  if (percent < 100) {
    for (uint16_t i = 0; i < SEGLEN; i++) {
      if (i < SEGENV.step) {
        setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
      }
      else {
        setPixelColor(i, SEGCOLOR(1));
      }
    }
  } else {
    for (uint16_t i = 0; i < SEGLEN; i++) {
      if (i < (SEGLEN - SEGENV.step)) {
        setPixelColor(i, SEGCOLOR(1));
      }
      else {
        setPixelColor(i, color_from_palette(i, true, PALETTE_SOLID_WRAP, 0));
      }
    }
  }

  if(active_leds > SEGENV.step) {  // smooth transition to the target value
    SEGENV.step += size;
    if (SEGENV.step > active_leds) SEGENV.step = active_leds;
  } else if (active_leds < SEGENV.step) {
    if (SEGENV.step > size) SEGENV.step -= size; else SEGENV.step = 0;
    if (SEGENV.step < active_leds) SEGENV.step = active_leds;
  }

  return FRAMETIME;
}

/*
/ Modulates the brightness similar to a heartbeat
*/
uint16_t WS2812FX::mode_heartbeat(void) {
  uint8_t bpm = 40 + (SEGMENT.speed >> 4);
  uint32_t msPerBeat = (60000 / bpm);
  uint32_t secondBeat = (msPerBeat / 3);

  uint32_t bri_lower = SEGENV.aux1;
  bri_lower = bri_lower * 2042 / (2048 + SEGMENT.intensity);
  SEGENV.aux1 = bri_lower;

  unsigned long beatTimer = millis() - SEGENV.step;
  if((beatTimer > secondBeat) && !SEGENV.aux0) { // time for the second beat?
    SEGENV.aux1 = UINT16_MAX; //full bri
    SEGENV.aux0 = 1;
  }
  if(beatTimer > msPerBeat) { // time to reset the beat timer?
    SEGENV.aux1 = UINT16_MAX; //full bri
    SEGENV.aux0 = 0;
    SEGENV.step = millis();
  }

  for (uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, color_blend(color_from_palette(i, true, PALETTE_SOLID_WRAP, 0), SEGCOLOR(1), 255 - (SEGENV.aux1 >> 8)));
  }

  return FRAMETIME;
}


//  "Pacifica"
//  Gentle, blue-green ocean waves.
//  December 2019, Mark Kriegsman and Mary Corey March.
//  For Dan.
//
//
// In this animation, there are four "layers" of waves of light.
//
// Each layer moves independently, and each is scaled separately.
//
// All four wave layers are added together on top of each other, and then
// another filter is applied that adds "whitecaps" of brightness where the
// waves line up with each other more.  Finally, another pass is taken
// over the led array to 'deepen' (dim) the blues and greens.
//
// The speed and scale and motion each layer varies slowly within independent
// hand-chosen ranges, which is why the code has a lot of low-speed 'beatsin8' functions
// with a lot of oddly specific numeric ranges.
//
// These three custom blue-green color palettes were inspired by the colors found in
// the waters off the southern coast of California, https://goo.gl/maps/QQgd97jjHesHZVxQ7
//
// Modified for WLED, based on https://github.com/FastLED/FastLED/blob/master/examples/Pacifica/Pacifica.ino
//
uint16_t WS2812FX::mode_pacifica()
{
  CRGBPalette16 pacifica_palette_1 =
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50 };
  CRGBPalette16 pacifica_palette_2 =
    { 0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
      0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F };
  CRGBPalette16 pacifica_palette_3 =
    { 0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
      0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF };

  if (SEGMENT.palette) {
    pacifica_palette_1 = currentPalette;
    pacifica_palette_2 = currentPalette;
    pacifica_palette_3 = currentPalette;
  }

  // Increment the four "color index start" counters, one for each wave layer.
  // Each is incremented at a different speed, and the speeds vary over time.
  uint16_t sCIStart1 = SEGENV.aux0, sCIStart2 = SEGENV.aux1, sCIStart3 = SEGENV.step, sCIStart4 = SEGENV.step >> 16;
  //static uint16_t sCIStart1, sCIStart2, sCIStart3, sCIStart4;
  uint32_t deltams = 26 + (SEGMENT.speed >> 3);

  uint16_t speedfactor1 = beatsin16(3, 179, 269);
  uint16_t speedfactor2 = beatsin16(4, 179, 269);
  uint32_t deltams1 = (deltams * speedfactor1) / 256;
  uint32_t deltams2 = (deltams * speedfactor2) / 256;
  uint32_t deltams21 = (deltams1 + deltams2) / 2;
  sCIStart1 += (deltams1 * beatsin88(1011,10,13));
  sCIStart2 -= (deltams21 * beatsin88(777,8,11));
  sCIStart3 -= (deltams1 * beatsin88(501,5,7));
  sCIStart4 -= (deltams2 * beatsin88(257,4,6));
  SEGENV.aux0 = sCIStart1; SEGENV.aux1 = sCIStart2;
  SEGENV.step = sCIStart4; SEGENV.step = (SEGENV.step << 16) + sCIStart3;

  // Clear out the LED array to a dim background blue-green
  //fill(132618);

  uint8_t basethreshold = beatsin8( 9, 55, 65);
  uint8_t wave = beat8( 7 );

  for( uint16_t i = 0; i < SEGLEN; i++) {
    CRGB c = CRGB(2, 6, 10);
    // Render each of four layers, with different scales and speeds, that vary over time
    c += pacifica_one_layer(i, pacifica_palette_1, sCIStart1, beatsin16(3, 11 * 256, 14 * 256), beatsin8(10, 70, 130), 0-beat16(301));
    c += pacifica_one_layer(i, pacifica_palette_2, sCIStart2, beatsin16(4,  6 * 256,  9 * 256), beatsin8(17, 40,  80),   beat16(401));
    c += pacifica_one_layer(i, pacifica_palette_3, sCIStart3,                         6 * 256 , beatsin8(9, 10,38)   , 0-beat16(503));
    c += pacifica_one_layer(i, pacifica_palette_3, sCIStart4,                         5 * 256 , beatsin8(8, 10,28)   ,   beat16(601));

    // Add extra 'white' to areas where the four layers of light have lined up brightly
    uint8_t threshold = scale8( sin8( wave), 20) + basethreshold;
    wave += 7;
    uint8_t l = c.getAverageLight();
    if (l > threshold) {
      uint8_t overage = l - threshold;
      uint8_t overage2 = qadd8(overage, overage);
      c += CRGB(overage, overage2, qadd8(overage2, overage2));
    }

    //deepen the blues and greens
    c.blue  = scale8(c.blue,  145);
    c.green = scale8(c.green, 200);
    c |= CRGB( 2, 5, 7);

    setPixelColor(i, c.red, c.green, c.blue);
  }

  return FRAMETIME;
}

// Add one layer of waves into the led array
CRGB WS2812FX::pacifica_one_layer(uint16_t i, CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
  uint16_t ci = cistart;
  uint16_t waveangle = ioff;
  uint16_t wavescale_half = (wavescale >> 1) + 20;

  waveangle += ((120 + SEGMENT.intensity) * i); //original 250 * i
  uint16_t s16 = sin16(waveangle) + 32768;
  uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
  ci += (cs * i);
  uint16_t sindex16 = sin16(ci) + 32768;
  uint8_t sindex8 = scale16(sindex16, 240);
  return ColorFromPalette(p, sindex8, bri, LINEARBLEND);
}

//Solid colour background with glitter
uint16_t WS2812FX::mode_solid_glitter()
{
  fill(SEGCOLOR(0));

  if (SEGMENT.intensity > random8())
  {
    setPixelColor(random16(SEGLEN), ULTRAWHITE);
  }
  return FRAMETIME;
}


/////////////////////////////////////////////////////////////////////////////
//    Start of Audio Reactive fork, beginning with Non-reactive routines   //
/////////////////////////////////////////////////////////////////////////////

uint16_t WS2812FX::phased_base(uint8_t moder) {                       // We're making sine waves here. By Andrew Tuline.


  float thisspeed = SEGMENT.speed/32.0;                               // You can change the speed of the wave.   AKA SPEED (was .4)
  uint8_t allfreq = 16;                                               // Base frequency.
  static float thisphase;                                             // Phase change value gets calculated.
  uint8_t thisrot = 64;                                               // Colour rotation speed.
  uint8_t cutOff = (255-SEGMENT.intensity);                           // You can change the number of pixels.  AKA INTENSITY (was 192).
  uint8_t modVal = SEGMENT.fft1/8+1;                                  // You can change the modulus. AKA FFT1 (was 5).

  uint8_t thisindex = millis()/thisrot;                               // Colour at leds[0].
  thisphase += thisspeed;                                             // Phase change at leds[0].

  for (int i=0; i<SEGLEN; i++) {
    if (moder == 1) modVal = (inoise8(i*10 + i*10) /16);         // Let's randomize our mod length with some Perlin noise.
    uint16_t val = (i+1)*allfreq;                                          // This sets the frequency of the waves. The +1 makes sure that leds[0] is used.
    val +=thisphase*(i%modVal+1)/2;                                   // This sets the varying phase change of the waves. By Andrew Tuline.
    uint8_t thisbright = cubicwave8(val);                                 // Now we make an 8 bit sinewave.
    thisbright = (thisbright > cutOff) ? (thisbright-cutOff) : 0;     // A ternary operator to cutoff the light.
    setPixCol(i, thisindex, thisbright);
    thisindex +=256/SEGLEN;
  }

  return FRAMETIME;
} // phased_base()



uint16_t WS2812FX::mode_phased(void) {
  return phased_base(0);
} // mode_phased()



uint16_t WS2812FX::mode_phased_noise(void) {
  return phased_base(1);
} // mode_phased_noise()



uint16_t WS2812FX::mode_twinkleup(void) {                 // A very short twinkle routine with fade-in and dual controls. By Andrew Tuline.

  random16_set_seed(535);                                 // The randomizer needs to be re-set each time through the loop in order for the same 'random' numbers to be the same each time through.

  for (int i = 0; i<SEGLEN; i++) {
    uint8_t ranstart = random8();                         // The starting value (aka brightness) for each pixel. Must be consistent each time through the loop for this to work.
    uint8_t pixBri = sin8(ranstart + 16 * millis()/(255-SEGMENT.speed));
    if (random8() > SEGMENT.intensity) pixBri = 0;
    setPixCol(i, i*20, pixBri);
  }

  return FRAMETIME;
} // mode_twinkleup()



// Peaceful noise that's slow and with gradually changing palettes. Does not support WLED palettes or default colours or controls.
uint16_t WS2812FX::mode_noisepal(void) {                                    // Slow noise palette by Andrew Tuline.

  #define scale 30                                                          // Don't change this programmatically or everything shakes.

  static CRGBPalette16 thisPalette;
  static CRGBPalette16 thatPalette = RainbowColors_p;                       // An initial palette so the led's aren't black.

  static uint16_t dist;                                                     // Our distance from 0.
  CRGB color;

  EVERY_N_MILLIS(10) {
    nblendPaletteTowardPalette(thisPalette, thatPalette, 48);               // Blend towards the target palette over 48 iterations.
  }

  EVERY_N_SECONDS(5) {                                                      // Change the target palette to a random one every 5 seconds.
    uint8_t baseI = random8();
    thatPalette = CRGBPalette16(CHSV(baseI+random8(64), 255, random8(128,255)), CHSV(baseI+128, 255, random8(128,255)), CHSV(baseI+random8(92), 192, random8(128,255)), CHSV(baseI+random8(92), 255, random8(128,255)));
  }

  for(int i = 0; i < SEGLEN; i++) {
    uint8_t index = inoise8(i*scale, dist+i*scale);                       // Get a value from the noise function. I'm using both x and y axis.
    color = ColorFromPalette(thisPalette, index, 255, LINEARBLEND);       // Use the my own palette.
    setPixelColor(i, color.red, color.green, color.blue);
  }

  dist += beatsin8(10,1,4);                                                // Moving along the distance. Vary it a bit with a sine wave.

  return FRAMETIME;
} // mode_noisepal()


// Sine waves that have controllable phase change speed, frequency and cutoff. By Andrew Tuline.
// SEGMENT.speed ->Speed, SEGMENT.intensity -> PWM cutoff, SEGMENT.fft1 -> Color change, SEGMENT.fft2 -> Frequency
uint16_t WS2812FX::mode_sinewave(void) {             // Adjustable sinewave. By Andrew Tuline.

  #define qsuba(x, b)  ((x>b)?x-b:0)                 // Analog Unsigned subtraction macro. if result <0, then => 0

  static uint16_t thisPhase = 0;
  uint16_t thisFreq;
  uint16_t colorIndex = millis() / (256 - SEGMENT.fft1);  // Amount of colour change.

  thisPhase += SEGMENT.speed/16;                   // Speed of animation.
  thisFreq = SEGMENT.fft2/8;                       // Frequency of the signal.

  for (int i=0; i<SEGLEN; i++) {                   // For each of the LED's in the strand, set a brightness based on a wave as follows:
    int pixBri = qsuba(cubicwave8((i*thisFreq)+thisPhase), (255-SEGMENT.intensity));    // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
    setPixCol(i, i*colorIndex/255, pixBri);
  }

  return FRAMETIME;
} // mode_sinewave()



//////////////////////////////////////////
//   Pixel assignment helper routine    //
//////////////////////////////////////////

// This helper function displays the RGBW SEGCOLOR(0) if no palette has been loaded. Index must be 32 bit because I use millis().
void WS2812FX::setPixCol(uint16_t location, uint32_t index, uint8_t intensity) {

  CRGB color;

  if (SEGMENT.palette == 0) {                                             // No palette loaded, so let's use the first colour. . . and white.
    uint32_t myClr = color_blend(SEGCOLOR(1), SEGCOLOR(0), intensity);    // Scale the brightness of the colour.
    setPixelColor(location, myClr);                                       // This supports RGBW.
  } else {
    color = ColorFromPalette(currentPalette, index, intensity);           // This just uses the palettes and just RGB ones at that.
    setPixelColor(location, color.red, color.green, color.blue);
  }

} // setPixCol()



////////////////////////////////
//   Begin volume routines    //
////////////////////////////////

//////////////////////
//     PIXELS       //
//////////////////////

uint16_t WS2812FX::mode_pixels(void) {                                   // Pixels. By Andrew Tuline.

  fade_out(4);

  for (int i=0; i <SEGMENT.intensity/16; i++) {
    uint16_t segLoc = random(SEGLEN);                                      // 16 bit for larger strands of LED's.
    setPixCol(segLoc, myVals[i%32]+i*4, sampleAgc);
  }

  return FRAMETIME;
} // mode_pixels()


//////////////////////
//     PIXELWAVE    //
//////////////////////

uint16_t WS2812FX::mode_pixelwave(void) {                                  // Pixelwave. By Andrew Tuline.

  EVERY_N_MILLISECONDS_I(pixTimer, SEGMENT.speed) {                       // Using FastLED's timer. You want to change speed? You need to . .

    pixTimer.setPeriod((256 - SEGMENT.speed) >> 2);                       // change it down here!!! By Andrew Tuline.
    int pixBri = sample * SEGMENT.intensity / 128;
    setPixCol(SEGLEN/2, millis(), pixBri);

    for (int i=SEGLEN-1; i>SEGLEN/2; i--) {                               // Move to the right.
      setPixelColor(i,getPixelColor(i-1));
    }

    for (int i=0; i<SEGLEN/2; i++) {                                      // Move to the left.
      setPixelColor(i,getPixelColor(i+1));
    }
  }

  return FRAMETIME;
} // mode_pixelwave()


//////////////////////
//     JUGGLES      //
//////////////////////

uint16_t WS2812FX::mode_juggles(void) {                                  // Juggles. By Andrew Tuline.

  static int thistime = 20;

  EVERY_N_MILLISECONDS_I(pixTimer, SEGMENT.speed) {                       // Using FastLED's timer. You want to change speed? You need to

    pixTimer.setPeriod((256 - SEGMENT.speed) >> 2);                       // change it down here!!! By Andrew Tuline.

    fade_out(224);

    for (int i=0; i<SEGMENT.intensity/32; i++) {
      setPixCol(beatsin16(thistime+i*2,0,SEGLEN-1), millis()/4+i*2, sampleAgc);
    }
  }

  return FRAMETIME;
} // mode_juggles()


//////////////////////
//     MATRIPIX     //
//////////////////////

uint16_t WS2812FX::mode_matripix(void) {                                  // Matripix. By Andrew Tuline.

  EVERY_N_MILLISECONDS_I(pixTimer, SEGMENT.speed) {                       // Using FastLED's timer. You want to change speed? You need to . .

    pixTimer.setPeriod((256 - SEGMENT.speed) >> 2);                       // change it down here!!!
    int pixBri = sample * SEGMENT.intensity / 128;
    setPixCol(SEGLEN-1, millis(), pixBri);
    for (int i=0; i<SEGLEN-1; i++) setPixelColor(i,getPixelColor(i+1));

  }

  return FRAMETIME;
} // mode_matripix()


///////////////////////
//     GRAVIMETER    //
///////////////////////

uint16_t WS2812FX::mode_gravimeter(void) {                               // Gravmeter. By Andrew Tuline.

  static int topLED;
  static int gravityCounter = 0;

  fade_out(240);

  sampleAvg = sampleAvg * SEGMENT.intensity / 255;

  int tempsamp = constrain(sampleAvg*2,0,SEGLEN-1);                       // Keep the sample from overflowing.
  uint8_t gravity = 8 - SEGMENT.speed/32;

  for (int i=0; i<tempsamp; i++) {
    uint8_t index = inoise8(i*sampleAvg+millis(), 5000+i*sampleAvg);
    setPixCol(i, index, sampleAvg*8);
  }

  if (tempsamp >= topLED)
    topLED = tempsamp;
  else if (gravityCounter % gravity == 0)
    topLED--;

  if (topLED > 0) setPixCol(topLED, millis(), 255);
  gravityCounter = (gravityCounter + 1) % gravity;

  return FRAMETIME;
} // mode_gravimeter()


//////////////////////
//     PLASMOID     //
//////////////////////

uint16_t WS2812FX::mode_plasmoid(void) {                                  // Plasmoid. By Andrew Tuline.

  static int16_t thisphase = 0;                                           // Phase of a cubicwave8.
  static int16_t thatphase = 0;                                           // Phase of the cos8.

  uint8_t thisbright;
  uint8_t colorIndex;

  thisphase += beatsin8(6,-4,4);                                          // You can change direction and speed individually.
  thatphase += beatsin8(7,-4,4);                                          // Two phase values to make a complex pattern. By Andrew Tuline.

  for (int i=0; i<SEGLEN; i++) {                                          // For each of the LED's in the strand, set a brightness based on a wave as follows.
    thisbright = cubicwave8((i*13)+thisphase)/2;
    thisbright += cos8((i*117)+thatphase)/2;                              // Let's munge the brightness a bit and animate it all with the phases.
    colorIndex=thisbright;

    if (sampleAvg * 8 * SEGMENT.intensity/256 > thisbright) {thisbright = 255;} else {thisbright = 0;}

    setPixCol(i, colorIndex, thisbright);
  }

  return FRAMETIME;
} // mode_plasmoid()


//////////////////////
//     PUDDLES      //
//////////////////////

uint16_t WS2812FX::mode_puddles(void) {                                  // Puddles. By Andrew Tuline.

  uint16_t size = 0;
  uint8_t fadeVal = map(SEGMENT.speed,0,255, 224, 255);
  uint16_t pos = random(SEGLEN);                                          // Set a random starting position.

  fade_out(fadeVal);

  if (sample>0 ) {
    size = sample * SEGMENT.intensity /256 /8 + 1;                        // Determine size of the flash based on the volume.
    if (pos+size>= SEGLEN) size=SEGLEN-pos;
  }

  for(int i=0; i<size; i++) {                                             // Flash the LED's.
    setPixCol(pos+i, millis(), 255);
  }

  return FRAMETIME;
} // mode_puddles()


//////////////////////
//     MIDNOISE     //
//////////////////////

uint16_t WS2812FX::mode_midnoise(void) {                                  // Midnoise. By Andrew Tuline.

  static uint16_t xdist;
  static uint16_t ydist;

  fade_out(224);

  uint16_t maxLen = sampleAvg * SEGMENT.intensity / 256;                  // Too sensitive.
  maxLen = maxLen * SEGMENT.intensity / 256;                              // Reduce sensitity/length.

  if (maxLen >SEGLEN/2) maxLen = SEGLEN/2;

  for (int i=(SEGLEN/2-maxLen); i<(SEGLEN/2+maxLen); i++) {
    uint8_t index = inoise8(i*sampleAvg+xdist, ydist+i*sampleAvg);        // Get a value from the noise function. I'm using both x and y axis.
    setPixCol(i, index, 255);
  }

  xdist=xdist+beatsin8(5,0,10);
  ydist=ydist+beatsin8(4,0,10);

  return FRAMETIME;
} // mode_midnoise()


///////////////////////
//     Noisemeter    //
///////////////////////

uint16_t WS2812FX::mode_noisemeter(void) {                                  // Noisemeter. By Andrew Tuline.

  static uint16_t xdist;
  static uint16_t ydist;

  fade_out(240);

  int maxLen = sampleAvg;
  if (sample > sampleAvg) maxLen = sample-sampleAvg;
  maxLen = maxLen * SEGMENT.intensity / 256;                              // Still a bit too sensitive.
  maxLen = maxLen * SEGMENT.intensity / 256;                              // Reduce sensitity/length.
  if (maxLen >SEGLEN) maxLen = SEGLEN;

  for (int i=0; i<maxLen; i++) {                                          // The louder the sound, the wider the soundbar. By Andrew Tuline.
    uint8_t index = inoise8(i*sampleAvg+xdist, ydist+i*sampleAvg);        // Get a value from the noise function. I'm using both x and y axis.
    setPixCol(i, index, 255);
  }

  xdist+=beatsin8(5,0,10);
  ydist+=beatsin8(4,0,10);

  return FRAMETIME;
} // mode_noisemeter()


//////////////////////
//     NOISEFIRE    //
//////////////////////

// I am the god of hellfire. . . Volume (only) reactive fire routine. Oh, look how short this is.
uint16_t WS2812FX::mode_noisefire(void) {                    // Noisefire. By Andrew Tuline.

  const uint8_t xscale = 20;            // How far apart they are
  const uint8_t yscale = 3;             // How fast they move

  CRGB color;
  uint16_t index;            // Current colour lookup value.

  currentPalette = CRGBPalette16(CHSV(0,255,2), CHSV(0,255,4), CHSV(0,255,8), CHSV(0, 255, 8),    // Fire palette definition. Lower value = darker.
                                 CHSV(0, 255, 16), CRGB::Red, CRGB::Red, CRGB::Red,
                                 CRGB::DarkOrange,CRGB::DarkOrange, CRGB::Orange, CRGB::Orange,
                                 CRGB::Yellow, CRGB::Orange, CRGB::Yellow, CRGB::Yellow);

  for (int i = 0; i < SEGLEN; i++) {
    index = inoise8(i*xscale,millis()*yscale*SEGLEN/255);                     // X location is constant, but we move along the Y at the rate of millis(). By Andrew Tuline.
    index = (255 - i*256/SEGLEN) * index/128;                                 // Now we need to scale index so that it gets blacker as we get close to one of the ends.
    color = ColorFromPalette(currentPalette, index, sampleAvg*2, LINEARBLEND);       // Use the my own palette.
    setPixelColor(i, color.red, color.green, color.blue);                       // This is a simple y=mx+b equation that's been scaled. index/128 is another scaling.
  }

  return FRAMETIME;
} // mode_noisefire()


///////////////////////
//     PUDDLEPEAK    //
///////////////////////

// Andrew's crappy peak detector. If I were 40+ years younger, I'd learn signal processing.
uint16_t WS2812FX::mode_puddlepeak(void) {                                // Puddlepeak. By Andrew Tuline.

  uint16_t size = 0;
  uint8_t fadeVal = map(SEGMENT.speed,0,255, 224, 255);
  uint16_t pos = random(SEGLEN);                                          // Set a random starting position.

  fade_out(fadeVal);

  if (samplePeak == 1 ) {
    size = sample * SEGMENT.intensity /256 /8 + 1;                        // Determine size of the flash based on the volume.
    if (pos+size>= SEGLEN) size=SEGLEN-pos;
    samplePeak = 0;
  }

  for(int i=0; i<size; i++) {                                             // Flash the LED's.
    setPixCol(pos+i, millis(), 255);
  }

  return FRAMETIME;
} // mode_puddlepeak()



///////////////////////////////
//     BEGIN FFT ROUTINES    //
///////////////////////////////

#ifndef ESP8266
extern double FFT_MajorPeak;
extern double FFT_Magnitude;
extern double fftBin[];           // raw FFT data
extern double fftResult[];        // pre-added result array 0 .. 15
extern double beat;
extern uint16_t lastSample;
double volume = 1;
uint32_t ledData[1500];
uint32_t dataStore[4096];										// we are declaring a storage area or 64 x 64 (4096) words.



double mapf(double x, double in_min, double in_max, double out_min, double out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif // ESP8266

//////////////////////
//     FREQWAVE     //
//////////////////////

// sound 10: assign a color to the central (starting pixels) based on the predominant frequencies and the volume. The color is being determined by mapping the MajorPeak from the FFT
// and then mapping this to the HSV color circle. Currently we are sampling at 10240 Hz, so the highest frequency we can look at is 5120Hz.
//
// SEGMENT.fft1: the lower cut off point for the FFT. (many, most time the lowest values have very little information since they are FFT conversion artifacts. Suggested value is close to but above 0
// SEGMENT.fft2: The high cut off point. This depends on your sound profile. Most music looks good when this slider is between 50% and 100%.
// SEGMENT.fft3: "preamp" for the audio signal for audio10.
//
// I suggest that for this effect you turn the brightness to 95%-100% but again it depends on your soundprofile you find yourself in.
// Instead of using colorpalettes, This effect works on the HSV color circle with red being the lowest frequency
//
// as a compromise between speed and accuracy we are currently sampling with 10240Hz, from which we can then determine with a 512bin FFT our max frequency is 5120Hz.
// Depending on the music stream you have you might find it useful to change the frequency mapping.

uint16_t WS2812FX::mode_freqwave(void) {          // Freqwave. By Andreas Pleschung.
// Instead of using colorpalettes, This effect works on the HSV color circle with red being the lowest frequency
//
// as a compromise between speed and accuracy we are currently sampling with 10240Hz, from which we can then determine with a 512bin FFT our max frequency is 5120Hz.
// Depending on the music stream you have you might find it useful to change the frequency mapping.

  #ifndef ESP8266
  static unsigned long prevMillis;
  unsigned long curMillis = millis();

  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t* leds = ledData;

    //uint8_t fade = SEGMENT.fft3;
    //uint8_t fadeval;

    double sensitivity = mapf(SEGMENT.fft3, 1, 255, 1, 10);
    int pixVal = sampleAvg * SEGMENT.intensity / 256 * sensitivity;
    if (pixVal > 255) pixVal = 255;

    double intensity = map(pixVal, 0, 255, 0, 100) / 100.0;            // make a brightness from the last avg

    CRGB color = 0;
    CHSV c;

    if (FFT_MajorPeak > 5120) FFT_MajorPeak = 0;
      // MajorPeak holds the freq. value which is most abundant in the last sample.
      // With our sampling rate of 10240Hz we have a usable freq range from roughtly 80Hz to 10240/2 Hz
      // we will treat everything with less than 65Hz as 0
      //Serial.printf("%5d ", FFT_MajorPeak, 0);
    if (FFT_MajorPeak < 80) {
      color = CRGB::Black;
    } else {
      int upperLimit = 20 * SEGMENT.fft2;
      int lowerLimit = 2 * SEGMENT.fft1;
      int i =  map(FFT_MajorPeak, lowerLimit, upperLimit, 0, 255);
      uint16_t b = 255 * intensity;
      if (b > 255) b=255;
      c = CHSV(i, 240, (uint8_t)b);
    }

    // Serial.println(color);
    leds[SEGLEN/2] =  (c.h << 16) + (c.s << 8)  + (c.v );

// shift the pixels one pixel outwards
    for (int i = SEGLEN; i > SEGLEN/2; i--) {                                 // Move to the right.
      leds[i] = leds[i-1];
    }
    for (int i = 0; i < SEGLEN/2; i++) {                                      // Move to the left.
      leds[i] = leds[i+1];
    }

    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  }

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_freqwave()


///////////////////////
//     Freqmatrix    //
///////////////////////

uint16_t WS2812FX::mode_freqmatrix(void) {        // Freqmatrix. By Andreas Pleschung.

#ifndef ESP8266
  static unsigned long prevMillis;
  unsigned long curMillis = millis();

  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

    double sensitivity = mapf(SEGMENT.fft3, 1, 255, 1, 10);
    int pixVal = sampleAvg * SEGMENT.intensity / 256 * sensitivity;
    if (pixVal > 255) pixVal = 255;

    double intensity = map(pixVal, 0, 255, 0, 100) / 100.0;            // make a brightness from the last avg

    CRGB color = 0;
    CHSV c;

    if (FFT_MajorPeak > 5120) FFT_MajorPeak = 0;
      // MajorPeak holds the freq. value which is most abundant in the last sample.
      // With our sampling rate of 10240Hz we have a usable freq range from roughtly 80Hz to 10240/2 Hz
      // we will treat everything with less than 65Hz as 0
      //Serial.printf("%5d ", FFT_MajorPeak, 0);
    if (FFT_MajorPeak < 80) {
      color = CRGB::Black;
    } else {
      int upperLimit = 20 * SEGMENT.fft2;
      int lowerLimit = 2 * SEGMENT.fft1;
      int i =  map(FFT_MajorPeak, lowerLimit, upperLimit, 0, 255);
      uint16_t b = 255 * intensity;
      if (b > 255) b=255;
      c = CHSV(i, 240, (uint8_t)b);
    }

    // Serial.println(color);
    leds[0] =  (c.h << 16) + (c.s << 8)  + (c.v );

// shift the pixels one pixel up
    for (int i = SEGLEN; i > 0; i--) {                                 // Move up
      leds[i] = leds[i-1];
    }

    //fadeval = fade;

    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  }

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_freqmatrix()


//////////////////////
//     SPECTRAL     //
//////////////////////

// This delivers a spectral "analysis" of the audio signal compressed into 16 bins which are supposed to be at least half way similar log (human ear is log as well)
//
// this effect is best being displayed on strips in multiples of 16 leds (and only in multiples of 16), you can use it on strips shorter than 16 leds but then the higher frequency bins are just cut off
//
// The 2 slider that is active in this effect is the general brightness slider, everything else is being computed on the fly.
// FFT3 sets the cutoff value below which we think its noise
//
uint16_t WS2812FX::mode_spectral(void) {        // Spectral. By Andreas Pleschutznig.

#ifndef ESP8266
  double maxVal = 0;
  CHSV c;
  CRGB color;

  if (SEGENV.call == 0)
    for (int i = 0; i < SEGLEN; i++)
      setPixelColor(i, 0,0,0);                              // turn off all leds

  uint16_t cutoff = 40 * SEGMENT.fft3;                      // read slider3

  // Determine max value in bins to normalize
  maxVal = 0;
  for (int i = 0; i < 16; i++) {
    if (fftResult[i] > maxVal) {
      maxVal = fftResult[i];
    }

  if (maxVal < cutoff)                                      // we assume this is noise
    for (int i = 0; i < 16; i++)
      fftResult[i] = 0;


  if (maxVal == 0) maxVal = 255;
  int ledsPerBin = SEGLEN/16;

  if (ledsPerBin > 0) {                                     // our led strip is longer or at least than 16 LEDS
    for (int i = 0; i < 16; i++ )                           // walk through all bins and display
      if (ledsPerBin > 1) {                                 // more than one led per bin
        for (int l = 0; l < ledsPerBin; l++)  {
          int pos = i*ledsPerBin+l;                                   // which led are we talking about -- Also which bin are we talking about
          uint8_t angle = map(i*ledsPerBin, 0, SEGLEN, 0, 255);            // the color we are going to display
          uint8_t bright = mapf(fftResult[i], 0, maxVal, 0, 255); // find the brightness in relation to max
          color = CHSV(angle, 240, bright);                   // colculate a color and convert it to RGB
          setPixelColor(pos, color.red, color.green, color.blue);
        }
      } else {                                              // only one led per bin
        int pos = i;                                        // which led are we talking about -- Also which bin are we talking about
        uint8_t angle = map(pos, 0, SEGLEN, 0, 255);            // the color we are going to display
        uint8_t bright = mapf(fftResult[i], 0, maxVal, 0, 255); // find the brightness in relation to max
        color = CHSV(angle, 240, bright);                   // calculate a color and convert it to RGB
        setPixelColor(pos, color.red, color.green, color.blue);
      }
    } else {                                                  // our led strip is shorter than 16LEDS
    for (int i = 0; i < SEGLEN; i++ )  {                                      // which led are we talking about -- Also which bin are we talking about
        uint8_t angle = map(i, 0, SEGLEN, 0, 255);            // the color we are going to display
        uint8_t bright = mapf(fftResult[i], 0, maxVal, 0, 255); // find the brightness in relation to max
        color = CHSV(angle, 240, bright);                   // colculate a color and convert it to RGB
        setPixelColor(i, color.red, color.green, color.blue);
      }
    }
  }

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_spectral()


///////////////////////
//     WATERFALL     //
///////////////////////

// Experimenting with volume only as a fallback if no FFT.
uint16_t WS2812FX::mode_waterfall(void) {                  // Waterfall. By: Andrew Tuline

  static unsigned long prevMillis;
  unsigned long curMillis = millis();

  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

#ifndef ESP8266
    uint8_t pixCol = (log10((int)FFT_MajorPeak) - 2.26) * 177;       // log10 frequency range is from 2.26 to 3.7. Let's scale accordingly.
#else
    uint8_t pixCol = sample * SEGMENT.intensity / 128;
#endif // ESP8266

    if (samplePeak) {
      samplePeak = 0;
      setPixelColor(SEGLEN-1,92,92,92);
    } else {

#ifndef ESP8266
    setPixCol(SEGLEN-1, pixCol+SEGMENT.intensity, (int)FFT_Magnitude>>8);
#else
    setPixCol(SEGLEN-1, millis(), pixCol);
#endif // ESP8266
    }

    for (int i=0; i<SEGLEN-1; i++) setPixelColor(i,getPixelColor(i+1));
  }

  return FRAMETIME;
} // mode_waterfall()


//////////////////////
//     FREQPIXEL    //
//////////////////////

uint16_t WS2812FX::mode_freqpixel(void) {                  // Freqpixel. By Andrew Tuline.

#ifndef ESP8266

  uint16_t fadeRate = 2*SEGMENT.speed - SEGMENT.speed*SEGMENT.speed/255;      // Get to 255 as quick as you can.
  fade_out(fadeRate);
  uint16_t locn = random16(0,SEGLEN);
  uint8_t pixCol = (log10((int)FFT_MajorPeak) - 2.26) * 177;                  // log10 frequency range is from 2.26 to 3.7. Let's scale accordingly.
  setPixCol(locn, SEGMENT.intensity+pixCol, (int)FFT_Magnitude>>8);           // Shift the colours so we start at blue.

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_freqpixel()


////////////////////
//     BINMAP     //
////////////////////

// Map bins 7 through 490 to the ENTIRE SEGLEN.
// For some reason, it seems to be mirroring itself. I really don't know why.
uint16_t WS2812FX::mode_binmap(void) {    // Binmap. Scale bins to SEGLEN. By Andrew Tuline

#ifndef ESP8266

  extern double fftBin[];                   // raw FFT data. He uses bins 7 through 470, so we'll limit to around there.
  extern double fftResult[];
  #define samples 490                       // Don't use the highest bins.

  double maxVal = 0;

  for (int i = 0; i < 16; i++) {            // apleshu's quickie method to to get the max volume.
    if (fftResult[i] > maxVal) {
      maxVal = fftResult[i];                // These values aren't normalized though.
    }
  }

  if (maxVal == 0) maxVal = 255;            // If maxVal is too low, we'll have a mapping issue.
  if (maxVal > (255-SEGMENT.intensity)*10) maxVal = (255- SEGMENT.intensity)*10;         // That maxVal may be too high, so let's cap it.

  for (int i=0; i<SEGLEN; i++) {

    uint16_t startBin = 7+i*(samples-8)/SEGLEN;      // Don't use the first 7 bins, and don't overshoot by 8.
    uint16_t   endBin = 7+(i+1)*(samples-8)/SEGLEN;  // Ditto.

    double sumBin = 0;
    for (int j=startBin; j<=endBin; j++) { sumBin += fftBin[j]; }
    sumBin = sumBin / (endBin-startBin+1);  // Normalize it

    if (sumBin > maxVal) sumBin = maxVal;   // Make sure our bin isn't higher than the max . . which we capped earlier.
    uint8_t bright = mapf(sumBin, 0, maxVal, 0, 255); // find the brightness in relation to max
    setPixCol(i, i*4, bright);               // colour is just an index in the palette. The FFT is the intensity.

  }

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_binmap()


//////////////////////
//     NOISEPEAK     //
//////////////////////

uint16_t WS2812FX::mode_noisepeak(void) {                  // Noisepeak.  Frequency noise beat (err. . . OK peak) to blast out palette based perlin noise across SEGLEN. By Andrew Tuline.

#ifndef ESP8266

  static CRGBPalette16 thisPalette;
  static uint16_t dist;
  CRGB color;

  uint16_t fadeRate = 2*SEGMENT.speed - SEGMENT.speed*SEGMENT.speed/255;   // Get to 255 as quick as you can.
  fade_out(fadeRate);

  uint8_t pixCol = SEGMENT.intensity+(log10((int)FFT_MajorPeak) - 2.26) * 177;    // log10 frequency range is from 2.26 to 3.7. Let's scale accordingly.

   if (samplePeak) {
      samplePeak = 0;

      // Palette defined on the fly that stays close to the base pixCol.
      thisPalette = CRGBPalette16(CHSV(pixCol,255,random8(128,255)),CHSV(pixCol+32,255,random8(128,255)),CHSV(pixCol+80,192,random8(128,255)),CHSV(pixCol+16,255,random8(128,255)));

      for (int i = 0; i < SEGLEN; i++) {
        uint8_t index = inoise8(i*30, dist+i*30);                       // Get a value from the noise function. I'm using both x and y axis.
        color = ColorFromPalette(thisPalette, index, 255, LINEARBLEND);       // Use the my own palette.
        setPixelColor(i, color.red, color.green, color.blue);
      }
    }
  dist += beatsin8(10,1,4);

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_noisepeak()


//////////////////////
//     NOISEMOVE    //
//////////////////////

uint16_t WS2812FX::mode_noisemove(void) {          // Noisemove    By: Andrew Tuline
#ifndef ESP8266

  fade_out(92);

  for (int i=0; i<3; i++) {     // DO NOT make this > 5 because we only have 15 FFTresult bins.
    uint16_t locn = inoise16(millis()*SEGMENT.speed+i*50000, millis()*SEGMENT.speed);           // Get a new pixel location from moving noise.
    locn = map(locn,0,65535,0,SEGLEN-1);                                         // Map that to the length of the strand, and ensure we don't go over.
    setPixCol(locn, i*64, fftResult[i*3]/256);
  }

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_noisemove()



#ifndef ESP8266
/////////////////////////////////
//     START of 2D ROUTINES    //
/////////////////////////////////                                // needs to become a variable that we can set from the UI

static uint16_t x = 0;
static uint16_t y = 0;
static uint16_t z = 0;
static int speed2D = 20;



// uint8_t colorLoop = 1;

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
static int scale_2d = 30; // scale is set dynamically once we've started up


#endif // ESP8266


// blur1d: one-dimensional blur filter. Spreads light to 2 line neighbors.
// blur2d: two-dimensional blur filter. Spreads light to 8 XY neighbors.
//
//           0 = no spread at all
//          64 = moderate spreading
//         172 = maximum smooth, even spreading
//
//         173..255 = wider spreading, but increasing flicker
//
//         Total light is NOT entirely conserved, so many repeated
//         calls to 'blur' will also result in the light fading,
//         eventually all the way to black; this is by design so that
//         it can be used to (slowly) clear the LEDs to black.
void WS2812FX::blur1d( CRGB* leds, uint16_t numLeds, fract8 blur_amount)
{
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;
    CRGB carryover = CRGB::Black;
    for( uint16_t i = 0; i < numLeds; i++) {
        CRGB cur = leds[i];
        CRGB part = cur;
        part.nscale8( seep);
        cur.nscale8( keep);
        cur += carryover;
        if( i) leds[i-1] += part;
        leds[i] = cur;
        carryover = part;
    }
}

void WS2812FX::blur2d( CRGB* leds, uint8_t width, uint8_t height, fract8 blur_amount)
{
    blurRows(leds, width, height, blur_amount);
    blurColumns(leds, width, height, blur_amount);
}

// blurRows: perform a blur1d on every row of a rectangular matrix
void WS2812FX::blurRows( CRGB* leds, uint8_t width, uint8_t height, fract8 blur_amount)
{
    for( uint8_t row = 0; row < height; row++) {
        CRGB* rowbase = leds + (row * width);
        blur1d( rowbase, width, blur_amount);
    }
}

// blurColumns: perform a blur1d on each column of a rectangular matrix
void WS2812FX::blurColumns(CRGB* leds, uint8_t width, uint8_t height, fract8 blur_amount)
{
    // blur columns
    uint8_t keep = 255 - blur_amount;
    uint8_t seep = blur_amount >> 1;
    for( uint8_t col = 0; col < width; col++) {
        CRGB carryover = CRGB::Black;
        for( uint8_t i = 0; i < height; i++) {
            CRGB cur = leds[XY(col,i)];
            CRGB part = cur;
            part.nscale8( seep);
            cur.nscale8( keep);
            cur += carryover;
            if( i) leds[XY(col,i-1)] += part;
            leds[XY(col,i)] = cur;
            carryover = part;
        }
    }
}



// Set 'matrixSerpentine' to false if your pixels are 
// laid out all running the same way, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//     .----<----<----<----'
//     |
//     5 >  6 >  7 >  8 >  9
//                         |
//     .----<----<----<----'
//     |
//    10 > 11 > 12 > 13 > 14
//                         |
//     .----<----<----<----'
//     |
//    15 > 16 > 17 > 18 > 19
//
// Set 'matrixSerpentine' to true if your pixels are 
// laid out back-and-forth, like this:
//
//     0 >  1 >  2 >  3 >  4
//                         |
//                         |
//     9 <  8 <  7 <  6 <  5
//     |
//     |
//    10 > 11 > 12 > 13 > 14
//                        |
//                        |
//    19 < 18 < 17 < 16 < 15
//
// Bonus vocabulary word: anything that goes one way
// in one row, and then backwards in the next row, and so on
// is call "boustrophedon", meaning "as the ox plows."


// This function will return the right 'led index number' for
// a given set of X and Y coordinates on your matrix.
// IT DOES NOT CHECK THE COORDINATE BOUNDARIES.
// That's up to you.  Don't pass it bogus values.
//
// Use the "XY" function like this:
//
//    for( uint8_t x = 0; x < matrixWidth; x++) {
//      for( uint8_t y = 0; y < matrixHeight; y++) {
//      
//        // Here's the x, y to 'led index' in action: 
//        leds[ XY( x, y) ] = CHSV( random8(), 255, 255);
//
//      }
//    }
//
//
uint16_t WS2812FX::XY( int x, int y) {

#ifndef ESP8266

uint16_t i;
  
  if( matrixSerpentine == false) {
    i = (y * matrixWidth) + x;
  }

  if( matrixSerpentine == true) {
    if( y & 0x01) {
      // Odd rows run backwards
      uint8_t reverseX = (matrixWidth - 1) - x;
      i = (y * matrixWidth) + reverseX;
    } else {
      // Even rows run forwards
      i = (y * matrixWidth) + x;
    }
  }

  return i;
#endif // ESP8266
}



//////////////////////
//     2D01         //
//////////////////////

// fft1 slider above 1/2 will shift the colors
// fft2 slider == scale (how far away are we from the plasma)
// fft3 slider determines the speed the 'plasma' wafts

uint16_t WS2812FX::mode_2DPlasma(void) {                 // By Andreas Pleschutznig. A work in progress.

#ifndef ESP8266

  static uint8_t ihue=0;
  uint8_t index;
  uint8_t bri;
  static unsigned long prevMillis;
  unsigned long curMillis = millis();

  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;
    speed2D = SEGMENT.fft3;
    scale_2d = SEGMENT.fft2;

    uint32_t *noise = ledData;                    // we use the set aside storage array for FFT routines to store temporary 2D data
    uint8_t MAX_DIMENSION = ((matrixWidth>matrixHeight) ? matrixWidth : matrixHeight);

    // If we're runing at a low "speed", some 8-bit artifacts become visible
    // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
    // The amount of data smoothing we're doing depends on "speed".

    uint8_t dataSmoothing = 0;
    if( speed2D < 50) {
      dataSmoothing = 200 - (speed2D * 4);
      }

    for(int i = 0; i < MAX_DIMENSION; i++) {
      int ioffset = scale * i;
      for(int j = 0; j < MAX_DIMENSION; j++) {
        int joffset = scale * j;

        uint8_t data = inoise8(x + ioffset,y + joffset,z);

          // The range of the inoise8 function is roughly 16-238.
          // These two operations expand those values out to roughly 0..255
          // You can comment them out if you want the raw noise data.
        data = qsub8(data,16);
        data = qadd8(data,scale8(data,39));

        if( dataSmoothing ) {
          uint8_t olddata = noise[i * matrixWidth + j];
          uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
          data = newdata;
        }

        noise[i * matrixWidth + j] = data;
      }
    }

    z += speed2D;

    // apply slow drift to X and Y, just for visual variation.
    x += speed2D / 8;
    y -= speed2D / 16;

 // ---
  
  for(int i = 0; i < matrixWidth; i++) {
    for(int j = 0; j < matrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j * matrixWidth + i];
      uint8_t bri =   noise[i * matrixWidth + j];

      // if this palette is a 'loop', add a slowly-changing base value
      if (SEGMENT.fft1 > 128) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the 
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( currentPalette, index, bri);
      setPixelColor(XY(i, j), color.red, color.green, color.blue);
      }
    }
  ihue+=1;
  }


#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_2D01()


//////////////////////
//     2D02         //
//////////////////////

uint16_t WS2812FX::mode_2D02(void) {    // somebody can take over hereits not what I envisioned it to be


#ifndef ESP8266
   static uint8_t ihue=0;
  uint8_t index;
  // uint8_t bri = SEGMENT.fft2;
  static unsigned long prevMillis;
  unsigned long curMillis = millis();

  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    int pixBri = sample * SEGMENT.intensity / 128;
    speed2D = SEGMENT.fft3;
    scale_2d = SEGMENT.fft2;
 
    uint32_t *noise = ledData;                    // we use the set aside storage array for FFT routines to store temporary 2D data
    uint8_t MAX_DIMENSION = ((matrixWidth>matrixHeight) ? matrixWidth : matrixHeight);

    // If we're runing at a low "speed", some 8-bit artifacts become visible
    // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
    // The amount of data smoothing we're doing depends on "speed".

    uint8_t dataSmoothing = 0;
    if( speed2D < 50) {
      dataSmoothing = 200 - (speed2D * 4);
      }

    for(int i = 0; i < MAX_DIMENSION; i++) {
      int ioffset = scale * i;
      for(int j = 0; j < MAX_DIMENSION; j++) {
        int joffset = scale * j;

        uint8_t data = inoise8(x + ioffset,y + joffset,z);

          // The range of the inoise8 function is roughly 16-238.
          // These two operations expand those values out to roughly 0..255
          // You can comment them out if you want the raw noise data.
        data = qsub8(data,16);
        data = qadd8(data,scale8(data,39));

        if( dataSmoothing ) {
          uint8_t olddata = noise[i * matrixWidth + j];
          uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
          data = newdata;
        }

        noise[i * matrixWidth + j] = data;
      }
    }

    z += speed2D;

    // apply slow drift to X and Y, just for visual variation.
    x += speed2D / 8;
    y -= speed2D / 16;

 // ---
  
  for(int i = 0; i < matrixWidth; i++) {
    for(int j = 0; j < matrixHeight; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j * matrixWidth + i];
      uint8_t bri =   noise[i * matrixWidth + j];

      // if this palette is a 'loop', add a slowly-changing base value
      if (SEGMENT.fft1 > 128) {
        index += ihue;
      }

      // brighten up, as the color palette itself often contains the 
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( currentPalette, index, bri);
      color.red = color.red * pixBri / 256;
      color.green = color.green * pixBri / 256;
      color.blue = color.blue * pixBri / 256;
      setPixelColor(XY(i, j),color.red, color.green, color.blue);
      
      }
    }
  ihue+=1;
  }
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_2D02()


//////////////////////
//     2D03         //
//////////////////////

uint16_t WS2812FX::mode_2D03(void) {
#ifndef ESP8266
  uint16_t counter = (now * ((SEGMENT.speed >> 2) +2)) & 0xFFFF;
  counter = counter >> 8;
  uint32_t color = color_blend(color_wheel(counter),WHITE,128-SEGMENT.intensity);
  fill(0);
  for( uint8_t y = 0; y < matrixHeight; y++) {
    setPixelColor(XY(0,y), 255,0,0);
    setPixelColor(XY(matrixWidth - 1,y), color);
  }

  for( uint8_t x = 0; x < matrixWidth; x++) {
    setPixelColor(XY(x,0), 255,0,0);
    setPixelColor(XY(x,matrixHeight - 1), color);
  }
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_2D03()



//////////////////////
//      A0          //
//////////////////////

uint16_t WS2812FX::mode_A0(void) {      // 16 bit noisemove. By Andrew Tuline. Use Perlin Noise instead of sinewaves for movement.
                                        // Controls are speed, # of pixels, faderate.
  fade_out(255-SEGMENT.fft1);
  for (int i=0; i<SEGMENT.intensity/16+1; i++) {
    uint16_t locn = inoise16(millis()*128/(260-SEGMENT.speed)+i*15000, millis()*128/(260-SEGMENT.speed));   // Get a new pixel location from moving noise.
    uint16_t pixloc = map(locn,50*256,192*256,0,SEGLEN)%(SEGLEN);                           // Map that to the length of the strand, and ensure we don't go over.
    setPixCol(pixloc, pixloc%255, 255);                                                     // Use that value for both the location as well as the palette index colour for the pixel.
  }  

  return FRAMETIME;
} // mode_a0()


//////////////////////
//      A1          //
//////////////////////

uint16_t WS2812FX::mode_A1(void) {                            // * Ripple peak. By Andrew Tuline.
                                                              // This currently has no controls.

  #define maxsteps 16                                         // Case statement wouldn't allow a variable.

  static uint8_t colour;                                      // Ripple colour is randomized.
  static uint16_t centre;                                     // Center of the current ripple.
  static int8_t steps = -1;                                   // -1 is the initializing step.
  static uint8_t ripFade = 255;                               // Starting brightness.

  fade_out(240);                                              // Lower frame rate means less effective fading than FastLED
  fade_out(240);

  EVERY_N_MILLIS(20) {

     if (samplePeak == 1) {samplePeak = 0; steps = -1;}
  
    switch (steps) {
  
      case -1:                                                // Initialize ripple variables.
        centre = random16(SEGLEN);
        colour = random8();
        steps = 0;
        break;
  
      case 0:
        setPixCol(centre, colour, ripFade); 
        steps ++;
        break;
  
      case maxsteps:                                                    // At the end of the ripples.
//        steps = -1;
        break;
  
      default:                                                          // Middle of the ripples.

        setPixCol((centre + steps + SEGLEN) % SEGLEN, colour, ripFade/steps*2); 
        setPixCol((centre - steps + SEGLEN) % SEGLEN, colour, ripFade/steps*2); 
        steps ++;                                                         // Next step.
        break;  
    } // switch step
  }

  return FRAMETIME;
} // mode_a1()


//////////////////////
//      A2          //
//////////////////////

uint16_t WS2812FX::mode_A2(void) {                                // firenoise2d. By Andrew Tuline. Yep, yet another short routine.
  
#ifndef ESP8266

  uint32_t xscale = 600;                                          // How far apart they are
  uint32_t yscale = 1000;                                         // How fast they move
  uint8_t indexx = 0;

  currentPalette = CRGBPalette16(  CHSV(0,255,2), CHSV(0,255,4), CHSV(0,255,8), CHSV(0, 255, 8),
                                   CHSV(0, 255, 16), CRGB::Red, CRGB::Red, CRGB::Red,                                   
                                   CRGB::DarkOrange,CRGB::DarkOrange, CRGB::Orange, CRGB::Orange,
                                   CRGB::Yellow, CRGB::Orange, CRGB::Yellow, CRGB::Yellow);

  for (int j=0; j < matrixWidth; j++) {
    for (int i=0; i < matrixHeight; i++) {
      indexx = inoise8(i*xscale+millis()/4,j*yscale*matrixWidth/255);                                                 // We're moving along our Perlin map.
//      leds[XY(j,i)] = ColorFromPalette(currentPalette, min(i*(indexx)>>4, 255), i*255/kMatrixWidth, LINEARBLEND);   // With that value, look up the 8 bit colour palette value and assign it to the current LED.      

      CRGB color = ColorFromPalette(currentPalette, min(i*(indexx)>>4, 255), i*255/matrixWidth, LINEARBLEND);          // Use the my own palette.
      setPixelColor(XY(j,i), color.red, color.green, color.blue);     
    } // for i
  } // for j

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a2()


//////////////////////
//      A3          //
//////////////////////

uint16_t WS2812FX::mode_A3(void) {              // squaredswirl  By: Mark Kriegsman. https://gist.github.com/kriegsman/368b316c55221134b160 Adapted by: Andrew Tuline
                                                // Speed affects the blur amount.
#ifndef ESP8266

  const uint8_t kBorderWidth = 0;

//  uint8_t blurAmount = dim8_raw( beatsin8(20,64,128) );  //3,64,192
    blur(255-SEGMENT.speed);
//  blur2d( leds, matrixWidth, matrixHeight, blurAmount);
  
  // Use two out-of-sync sine waves
  uint8_t  i = beatsin8(19, kBorderWidth, matrixWidth-kBorderWidth);
  uint8_t  j = beatsin8(22, kBorderWidth, matrixWidth-kBorderWidth);
  uint8_t  k = beatsin8(17, kBorderWidth, matrixWidth-kBorderWidth);
  uint8_t  m = beatsin8(18, kBorderWidth, matrixHeight-kBorderWidth); 
  uint8_t  n = beatsin8(15, kBorderWidth, matrixHeight-kBorderWidth);
  uint8_t  p = beatsin8(20, kBorderWidth, matrixHeight-kBorderWidth);
  
  // The color of each point shifts over time, each at a different speed.
  uint16_t ms = millis();  

  setPixCol(XY(i,m), ms/29, 255);
  setPixCol(XY(j,n), ms/41+96, 255);
  setPixCol(XY(k,p), ms/73+192, 255);

#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a3()


//////////////////////
//      A4          //
//////////////////////

uint16_t WS2812FX::mode_A4(void) {                 // Fire2012XY - Reference 3833
#ifndef ESP8266
/*

  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  CRGBPalette16 currentPalette  = CRGBPalette16( CRGB::Black, CRGB::Red, CRGB::Orange, CRGB::Yellow);
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

    const uint8_t COOLING = 40;
    const uint8_t SPARKING = 60;

// Array of temperature readings at each simulation cell
  static byte heat[matrixWidth][matrixHeight];

    for (int mw = 0; mw < matrixWidth; mw++) {        // Move along the width of the flame
  
    // Step 1.  Cool down every cell a little
      for (int mh = 0; mh < matrixHeight; mh++) {
        heat[mw][mh] = qsub8( heat[mw][mh],  random16(0, ((COOLING * 10) / matrixHeight) + 2));
      }
    
      // Step 2.  Heat from each cell drifts 'up' and diffuses a little
      for (int mh = matrixHeight - 1; mh >= 2; mh--) {
        heat[mw][mh] = (heat[mw][mh - 1] + heat[mw][mh - 2] + heat[mw][mh - 2] ) / 3;
      }
      
      // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
      if (random8(0,255) < SPARKING ) {
        int mh = random8(3);
        heat[mw][mh] = qadd8( heat[mw][mh], random8(160,255) );
      }
  
      // Step 4.  Map from heat cells to LED colors
      for (int mh = 0; mh < matrixHeight; mh++) {
        byte colorindex = scale8( heat[mw][mh], 240);
        leds[ XY(mh, mw)] = ColorFromPalette( currentPalette, colorindex);
      }
    } // for mw

    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if millis
*/

fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a4()


//////////////////////
//      A5          //
//////////////////////

uint16_t WS2812FX::mode_A5(void) {
#ifndef ESP8266
/*  
  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

// ADD FASTLED ROUTINE HERE and use matrixWidth and matrixHeight


    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if millis
*/
  fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a5()


//////////////////////
//      A6          //
//////////////////////

uint16_t WS2812FX::mode_A6(void) {
#ifndef ESP8266
/*  
  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

// ADD FASTLED ROUTINE HERE and use matrixWidth and matrixHeight


    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if
*/  
  fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a6()


//////////////////////
//      A7          //
//////////////////////

uint16_t WS2812FX::mode_A7(void) {
#ifndef ESP8266
/*  
  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

// ADD FASTLED ROUTINE HERE and use matrixWidth and matrixHeight


    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if
*/
  fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a7()


//////////////////////
//      A8          //
//////////////////////

uint16_t WS2812FX::mode_A8(void) {
#ifndef ESP8266
/*  
  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

// ADD FASTLED ROUTINE HERE and use matrixWidth and matrixHeight


    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if
*/
  fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a8()


//////////////////////
//      A9          //
//////////////////////

uint16_t WS2812FX::mode_A9(void) {
#ifndef ESP8266
/*  
  static unsigned long prevMillis;
  unsigned long curMillis = millis();
  
  if ((curMillis - prevMillis) >= ((256-SEGMENT.speed) >>2)) {
    prevMillis = curMillis;

    uint32_t *leds = ledData;

// ADD FASTLED ROUTINE HERE and use matrixWidth and matrixHeight


    // DISPLAY ARRAY
    for (int i= 0; i < SEGLEN; i++) {
      c.h = (leds[i] >> 16) & 0xFF;
      c.s = (leds[i] >> 8) &0xFF;
      c.v = leds[i] & 0xFF;
      color = c;                                                              // implicit conversion to RGB supplied by FastLED
      setPixelColor(i, color.red, color.green, color.blue);
    }
  } // if
*/
  fade_out(224);
#else
  fade_out(224);
#endif // ESP8266

  return FRAMETIME;
} // mode_a9()
