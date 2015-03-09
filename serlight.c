/* serlight.c: firmware for a flashlight running on an attiny13a. 
 *
 * This is based on ToyKeeper's fantastically commented code [tk]. 
 *
 * This is licensed under the GPL Version 3. You should have received
 * a copy of the GPL with this software.
 *
 * Design & Schematics
 * ===================
 *
 * TODO
 *
 * How modes are switched
 * ----------------------
 * TODO 
 *
 * How PWM is Used
 * ---------------
 * TODO
 * 
 *
 * References
 * ==========
 * 
 * These documents were helpful in writing this program.
 * 
 * [amtel]      http://www.atmel.com/Images/doc8126.pdf
 *    (if that link rots, check at http://www.atmel.com/devices/ATTINY13A.aspx)
 * [libc]       http://www.nongnu.org/avr-libc/user-manual
 * [pwm]        http://github.com/JCapSolutions/blf-firmware/wiki/PWM-Frequency
 * [wk]         http://flashlightwiki.com/AVR_Drivers#Download_settings
 * [tk]         http://bazaar.launchpad.net/~toykeeper/flashlight-firmware/trunk/view/head:/ToyKeeper/starry-offtime/starry-offtime.c
 */



#include <avr/pgmspace.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/sleep.h>


/* typedefs (including enums) */

// note: we don't do 'typedef enum' because that wastes too much space by
// creating an int which is 32 bits, despite avr being an 8-bit architecture

// The lights in the system
enum {
  LED1,
  LED2,
  LIGHTS //not a light; rather the number of lights in the system (since this is in the n+1'th position it gets value n when counting from 0)
} /*light_t*/;
typedef uint8_t light_t;

// The modes we support
// What the modes actually are are defined below in structs
// but we want integer names for each so we can store the mode to EEPROM
enum {
  M_NULL,   //simply used to mark the end of lists, c-str style
  M_MOON,   //"equivalent to a full moon"
  M_LOW,    //low power
  M_MEDIUM, //medium power
  M_HIGH,   //high power
  M_TURBO,  //full power; turns itself down to M_HIGH after 30s; only used by LED1
  // blink modes; only used for LED2
  M_BLINK,  //blink on and off in a 1:4 ratio
  M_BLINK2, 
  M_BLINK3, 
  M_OFF,    // currently unused
  M_INVALID = 0xF, //make sure we can't misinterpret an "empty" EEPROM cell as valid states
  // this is only 0xF because we store exactly 4 bits per mode to the cells
} /*mode_t*/;
typedef uint8_t mode_t;

//a power level, given on a scale from 0 to 255
typedef uint8_t power_t;


// an individual segment of a mode's routine
// (think of this as a minimal representation of subroutine on a CPU with exactly two operations: set output and sleep)
struct mode_step {
  uint8_t output;  //the portion of each cycle (on a scale of 0 to 255) to power the light
  uint16_t timeout; //how long to stay in this step before moving on. 0 means infinity.
};

// a mode that a light can be in:
//  simply a (fixed) list of the power levels to cycle through and at what rate to do so.
struct mode {
  uint8_t n_steps;
  struct mode_step steps[];
};

/* constants memory */
// Mode definitions
// these are individual variables because they are variable length
PROGMEM const struct mode moon = {1, {{3, 0}}};
PROGMEM const struct mode low = {1, {{15, 0}}};
PROGMEM const struct mode medium = {1, {{128, 0}}};
PROGMEM const struct mode high = {1, {{180, 0}}};
PROGMEM const struct mode turbo = {2, {{255, 30000}, {180, 0}}};
  //TODO: when turbo switchs to high
  //      make it actually switch the mode setting (by special-casing in the interrupt handler),
  //      so that you can cycle into turbo again.
  //      as written, this will instead cycle back to the low mode, which is probbbably alright
  //  To do this sort of special exception more generally requires redesigning these data structures
  //  and that's dangerous because we're already super close to the limit.
PROGMEM const struct mode blink = {2, {{170, 100}, {0, 400}}};
PROGMEM const struct mode off = {1, {{3, 0}}};


struct modegroup {
  uint8_t n_modes;
  mode_t modes[];
};

PROGMEM const struct modegroup LED1_modegroup = {
  5, {M_MOON, M_LOW, M_MEDIUM, M_HIGH, M_TURBO}
};
PROGMEM const struct modegroup LED2_modegroup = {
  5, {M_MOON, M_LOW, M_MEDIUM, M_HIGH, M_BLINK}
};

// This list needs to match the mode_t enum
// We use a pointer because a single mode is variable sized
const struct mode* modes[] = {
  NULL,     //M_NULL
  &moon,    //M_MOON
  &low,     //M_LOW
  &medium,  //M_MEDIUM
  &high,    //M_HIGH
  &turbo,   //M_TURBO
  &blink,   //M_BLINK  
  NULL,     //M_BLINK2, 
  NULL,     //M_BLINK3, 
  &off,     //M_OFF
};

// This needs to match the lights enum
const struct modegroup* modegroup[LIGHTS] = {
  &LED1_modegroup,
  &LED2_modegroup
};


/* working memory */

light_t curlight; // the current light we are processing
                  // instead of passing this as a parameter, using a global saves instructions
mode_t mode[LIGHTS]; //TODO: use a struct with bitfields to make this only one byte long, and compare if the savings from this in save()/restore() offset the extra ops everywhere else needs to access it
uint8_t mode_step[LIGHTS];
uint16_t timer[LIGHTS];


// Size of globals (.bss):
//sizeof(curlight) + LIGHTS*(sizeof(mode_t) + sizeof(uint8_t) + sizeof(uint16_t)) + sizeof(eepos) =
// = 1 + 2*(1 + 1 + 2) + 1
// = 1 + 8 + 1
// = 10

// Additionally, because of the use of the & operator,
// the modes and modegroup lists are stuck in the .bss section too:
// (10*sizeof(struct mode*) + LIGHTS*sizeof(struct modegroup*))
// = 10*2 + 2*2   ;(since pointers are word sized == 2 bytes on avr)
// = 24

// For a total of 34 bytes, leaving 30 bytes free.
// Indeed, this is what `avr-size -C --mcu=attiny13` reports
//
// I'm not sure if the stack has to share this space.
// If it does, 30 bytes is probably enough, but it's tight.

// ------------------ utilities --------------------

/* cycle: a counter by one, wrapping around at the end
 *
 * it is /slightly/ less space to use this than '%'
 * this works on bytes because that saves about 20 instructions (or 1/50th of our total space) over working on words
 */
 //TODO: see if making this uint8_t saves space
 //TODO: see if making this return via refernece saves space
uint8_t cycle(uint8_t n, uint8_t count) {
  n+=1;
  if(n==count)
    n = 0;
  return n;
}


// ----------------------- persistence ------------------------------

#define EEPROM_SIZE 64
#define NO_DATA 0xFF
uint8_t eepos = 0; //address in EEPROM where we last wrote data

/* save: write persisted state to EEPROM
 *
 * this does wear-levelling by advancing and always ensuring there is at most one 
 *
 * XXX 
 */
void save() {  //central method for writing 
    // first: erase (if somehow the CPU crashes between writing and reading, it'll be a glitch
    // but a small one: the flashlight'll just reset to defaults next time you turn it on)
    // eeprom_*() wants pointers, which are word sized. But this is silly because the EEPROM is only 64 bytes long:
    // you'd never need 16 bits to address all of it, even if you addressed each bit individually 
    // So, to save space, eepos is a uint8 and we force a cast (which gcc complains about).
    eeprom_write_byte((uint8_t*)eepos, NO_DATA);
    
    // now, write the new state
    //wear leveling, use next cell, wrapping around at EEPROM_SIZE
    //eepos=(eepos+1)%EEPROM_SIZE;  //NB: gcc's optimizer is smart enough to turn mods by a power of two into bitwise ANDs by one less.
                                    //however, it still ends up with two extra instructions (which is 1/512th of our total available space)
    eepos=cycle(eepos, EEPROM_SIZE);
    uint8_t buf = mode[LED1] << 4 | mode[LED2];
    eeprom_write_byte((uint8_t*)eepos, buf);
}

/* restore: load persisted state from EEPROM.
 *
 * side effect: find the location of the last store and remember it in eepos
 */
inline void restore() {
    uint8_t buf = NO_DATA;
    
    //NB: if we ever have more than one byte of storage we'll need to use eeprom_read_block and change the loop
    
    for(; (buf == NO_DATA) && ((uint16_t)eepos < EEPROM_SIZE); eepos++) {
        // see comments above about why gcc is wrong for complaining about this cast.
        buf = eeprom_read_byte((uint8_t*)eepos);
    }
    
    // restore the found data
    // but only if we actually found it
    // (if no data is restored, the defaults defined above rule)
    if((uint16_t)eepos < EEPROM_SIZE) {
        mode[LED1] = buf & 0xF0;
        mode[LED2] = buf & 0x0F;
    } else {
      // reset eepos to the start, since we don't know where the data was, if there was any
      eepos = 0;
    }
    
}

// ------------------------- hardware -----------------------------

#define MS_PER_TICK 500 //this is only an estimate. the CPU varies according to how much current it's getting. see [amtel:TODO]


inline void ISR_on() {
    // Setup watchdog timer to only interrupt, not reset
    cli();                          // Disable interrupts
    wdt_reset();                    // Reset the WDT
    WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
    
    // See [amtel:43] for the definition of WDTCR
    WDTCR = (1<<WDTIE) | (1<<WDP2) | (1<<WDP0); // Enable interrupt every 500ms
    sei();                          // Enable interrupts
}

inline void ISR_off()
{
    cli();                          // Disable interrupts
    wdt_reset();                    // Reset the WDT
    MCUSR &= ~(1<<WDRF);            // Clear Watchdog reset flag
    WDTCR |= (1<<WDCE) | (1<<WDE);  // Start timed sequence
    WDTCR = 0x00;                   // Disable WDT
    sei();                          // Enable interrupts
}


// PRESSED values
// If press() < PRESSED_${v} then we consider ourselves pressed for that amount of time.
enum {
  //TODO: tweak these as needed to match the hardware.
  PRESSED_COLD = 255,
  PRESSED_LONG = 150,
  PRESSED_MEDIUM = 70,
  PRESSED_SHORT = 30,
};

#define ADC_PRSCL   0x06    // clk/64
/* press: determine how long the power button was pressed.
 *
 * Since we lose power when the button is tapped, we measure this by
 * measuring a capacitor that is wired to one to the "ADC" pin, at boot.
 * If the capacitor has a lot of power, we were off for a short time.
 * If it has a little power, we were off for a medium or long time.
 * If it has no power, we are cold booting.
 *
 * This is our one single piece of UI. Barring getting out a soldering iron or a programmer,
 * there is no other way to communicate with the microcontroller.
 *
 * returns: the length of time we were off for on a scale from 0 to 255, with.
 */  
inline uint8_t press() {
    // Start up ADC to read capacitor pin
    DIDR0 |= (1 << ADC3D);                           // disable digital input on ADC pin to reduce power consumption
    ADMUX  = (1 << REFS0) | (1 << ADLAR) | 0x03; // 1.1v reference, left-adjust, ADC3/PB3
    ADCSRA = (1 << ADEN ) | (1 << ADSC ) | ADC_PRSCL;   // enable, start, prescale

    
    while (ADCSRA & (1 << ADSC)); // Wait for the ADC to be completion
    // Start again as datasheet says first result is unreliable
    ADCSRA |= (1 << ADSC);
    // Wait for completion
    while (ADCSRA & (1 << ADSC));
    uint8_t r = 0xFF - ADCH; //switch the scale around: instead of measuring power, measure length of time
    
    // Turn off ADC
    ADCSRA &= ~(1<<7);
    
    return r;
}

/* output: set power level of the current light.
 *
 */
void output(power_t level) {
    // the way output levels are controlled is that, when PWM is activated,
    // a special register is read to find out when the cycle
    // There are only two circuits; I am 99% sure that hardcoding this is less instructions than trying to be clever.
    if(curlight == LED1) {
        OCR0B = level;
    } else {
        OCR0A = level;
    }
}

// --------------------- modes and mode groups --------------------


void goto_mode(mode_t m) {
  //TODO: pointers?
  mode[curlight] = m;
  mode_step[curlight] = 0;
  timer[curlight] = 1; //run "immediately"
  save();
}

/* cycle_mode: step
 *
 * if curlight's current mode is not in its modegroup, reset to defaults
 */
void cycle_mode() {
    // because we store the raw modes instead of an index into the mode group,
    // we need to walk the mode group searching for the current mode
    //uint8_t max = modes[ //TODO: doe sthis save bytes?
    // TODO: we could probably save instructions by advancing a pointer into ->modes simultaneously with i++
    //       and maybe even better: we could replace the n_modes with ending the mode list with M_NULL, and then we don't even need i at all
    mode_t c_mode;
    for(uint8_t i = 0; i < modegroup[curlight]->n_modes; i++) {
        uint8_t n_modes = pgm_read_byte(&(modegroup[curlight]->n_modes));
        c_mode = pgm_read_byte(&(modegroup[curlight]->modes[i]));
        if(mode[curlight] == c_mode) {
            // we found the mode we are currently at
            // advance the mode "pointer", look it up again
            i = cycle(i, n_modes);
            c_mode = pgm_read_byte(&(modegroup[curlight]->modes[i]));
            goto_mode(c_mode);
            return;
        }
    }

    // if the search failed, reset to default
    c_mode = pgm_read_byte(&(modegroup[curlight]->modes[0]));
    goto_mode(c_mode);
}



// --------------------------- main -------------------------------
/* concurrency on an avr:

This program implements a stripped down sort of threading without having a threading library.
It exploits the timer interrupt--just like a typical OS--to run.
but unlike a normal OS, code *only* runs during the timer interrupt.
We also assume (and do not waste code checking) that the interrupt handlers always run in much less time than the interrupt quantum,
 (if this turns out to be false and a series of interrupts stack up, bad things will probably happen, like a melting flashlight).

How
*/


// TODO:
// combine mode and mode_step into proc[LIGHTS].{mode,step}
// measure the difference in code size from this.

/* run the process for the current light 
 * this runs once per interrupt timer tick.
 * 
 */
void run(void) {
    if(timer[curlight] < MS_PER_TICK) {
        // trigger!
        
        // aliases
        // (also, hopefully, save instructions by avoiding repeated array lookups?)
        // TODO: test by replacing stack vars with macros
        mode_t cur_mode = mode[curlight];
        uint8_t cur_step = mode_step[curlight];
        
        // Read the mode definition into local variables.
        // This process is subtle:
        // the modes array is in RAM but the things it points
        // to are in PROGMEM, which we we cannot read directly.
        // Instead we need to carefully use pgm_read*() which
        // copies data from a pointer into the PROGMEM area to our local RAM.
        // 
        // Now, it might look like we're accessing things we can't,
        // because we index and then apply the & operator,
        // but & and * should be magic operators that the compiler
        // translates into additions--so the indexing
        // If that turns out not to be true, we can replace the &[idx] pair with +idx 
        // (indeed, this is what [tk] does)
        
        uint8_t n_steps = pgm_read_byte(&(modes[cur_mode]->n_steps));
        uint8_t level = pgm_read_byte(&(modes[cur_mode]->steps[cur_step].output));
        uint16_t timeout = pgm_read_word(&(modes[cur_mode]->steps[cur_step].timeout));
        
        output(level);
        timer[curlight] = timeout;
        
        // cycle the step forward
        mode_step[curlight] = cycle(mode_step[curlight], n_steps);
    } else {
        // continue to count down
        timer[curlight] -= MS_PER_TICK;
    }
}

/* ISR: the interrupt service routine. Called by the timer clock (even when the CPU is asleep)
 * 
 * This "schedules" and "runs" the "threads" for each "concurrent process" (i.e. light)
 */
ISR(WDT_vect) {
    curlight = LED1; run();
    curlight = LED2; run();
}

/* init: configure the hardware at boot
 *
 */
inline void boot() {
    
    // All ports default to input, (so?) turn on pull-up resistors for the unusused ports
    PORTB = (1 << PB4);
    
    // configure the PWM pins
    TCCR0A = (1<<COM0A1) | (1<<COM0B1) | (1<<WGM01)                                    | (1<<WGM00);
    //         ^enable pin A ^ and B        ^ use Fast PWM (as opposed to 'phase')           ^enable PWM
    
    restore();
}


void //returning void saves a couple bytes (it's not like there's anyone to return to)
     // though GCC complains about it
main() { 
    boot();
    
    uint8_t pressed = press();
    if(pressed < PRESSED_SHORT) {
        // short press: cycle LED1
        curlight = LED1;
        cycle_mode();
    } else if(pressed < PRESSED_MEDIUM) {
        // medium press: cycle LED2
        curlight = LED2;
        cycle_mode();
    } else /*if(pressed < PRESSED_COLD)*/ {
        // a cold boot: use default state
        // we could do goto_mode() twice, but then we're doing two save()s
        mode[LED1] = pgm_read_byte(&modegroup[LED1]->modes[0]);
        mode[LED2] = pgm_read_byte(&modegroup[LED2]->modes[0]);
        mode_step[LED1] = mode_step[LED2] = 0;
        timer[LED1] = timer[LED2] = 1;
        save();
    }
    //TODO: test for if it's possible to boot the flashlight without. 
    //      the very first time you boot the flashlight
    //      if you somehow manage to turn it off right here before goto_mode() happens
    //      do bad things happen?
    
    // turn on the ISR() routine and put main() to sleep so that we only use CPU as needed.
    ISR_on();
    while(1) {
        // Power down as many components as possible.
        // This is in a loop because every timer
        // interrupt wakes us up again.
        set_sleep_mode(SLEEP_MODE_PWR_DOWN);
        sleep_mode();
    }

}