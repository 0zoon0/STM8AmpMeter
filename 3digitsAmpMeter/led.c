#include "stm8s.h"
#include "led.h"

#define GPIO_PIN3 0b00001000

/*
 *   ***A***                   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  -  h
 *   *     *         (F) PB5   0  1  1  1  0  0  0  1  0  0  0  0  0  1  0  0  1  0
 *   F     B         (B) PC5   0  0  0  0  0  1  1  0  0  0  0  1  1  0  1  1  1  1
 *   *     *         (A) PB4   0  1  0  0  1  0  0  0  0  0  0  1  0  1  0  0  1  1
 *   ***G***         (G) PC6   1  1  0  0  0  0  0  1  0  0  0  0  1  0  0  0  0  0
 *   *     *         (C) PC7   0  0  1  0  0  0  0  0  0  0  0  0  1  0  1  1  1  0
 *   E     C         (DP)PD3   1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1  1
 *   *     *   **    (D) PD2   0  1  0  0  1  0  0  1  0  0  1  0  0  0  0  1  1  1
 *   ***D***  *DP*   (E) PD1   0  1  0  1  1  1  0  1  0  1  0  0  0  0  0  0  1  0
 *             **
 */

static uint8_t display_buffer[3] = {' ',' ',' '};	// blank by default
static uint8_t N_current = 0;											// current digit to display

/*
 * Number of digit on indicator with common anode
 * digits 0..2: PC3, PC4, PA3
 */
#define CLEAR_ANODES() do{GPIOC->ODR &= ~(0x18);GPIOA->ODR &= ~(0x08);}while(0)

/************* arrays for ports *************/
// PB, mask: 0x30 (dec: 48), PB4:0x10=16, PB5:0x20=32
// To light up a segment we should setup it as PPout -> this arrays are inverse!
#define PB_BLANK 0x30
static uint8_t PB_bits[18] = {48,0,16,16,32,48,48,16,48,48,48,32,48,0,48,48,0,32};

// PC, mask: 0xE0 (dec: 224), PC5:0x20=32, PC6:0x40=64, PC7:0x80=128
#define PC_BLANK 0xE0
static uint8_t PC_bits[18] = {160,160,96,224,224,192,192,160,224,224,224,192,0,224,64,64,64,192};

// PD, mask: 0x0E (dec: 14), PD1:0x02=2, PD2:0x4=4, PD3:0x8=8
static uint8_t PD_bits[18] = {6,0,6,4,0,4,6,0,6,4,2,6,6,6,6,2,0,2};
#define PD_BLANK 0x0E

/** 
 * Initialize ports
 * anodes (PC3, PC4, PA3) are push-pull outputs,
 * cathodes (PB4, PB5, PD1, PD2, PD3, PC5..PC7) are ODouts in active mode, pullup inputs in buttons reading and floating inputs in inactive
 * PA3, PB4|5, PC3|4|5|6|7, PD1|2|3
*/
void LED_init()
{
	GPIOB->DDR |= 0b00110000; // set output for PB4-5 cathodes
	GPIOC->DDR |= 0b11111000; // set output for PC3 as A1 anode, PC4 as A2 anode, PC5-7 cathodes
	GPIOD->DDR |= 0b00001110; // set output for PD1-3 cathodes
	GPIOA->DDR |= 0b00001000; // set output for PA3 as A3 anode
	
	GPIOA->CR1 |= 0b00001000; GPIOC->CR1 |= 0b00011000; // anodes should be push-pull outputs
	
	// set cathodes low
	GPIOB->ODR &= ~PB_BLANK; GPIOC->ODR &= ~PC_BLANK; GPIOD->ODR &= ~PD_BLANK;
	// set anodes low
	CLEAR_ANODES();
}

/*
 ********************* GPIO ********************
 * Px_ODR - Output data register bits
 * Px_IDR - Pin input values
 * Px_DDR - Data direction bits (1 - output)
 * Px_CR1 - DDR=0: 0 - floating, 1 - pull-up input; DDR=1: 0 - pseudo-open-drain, 1 - push-pull output [not for "T"]
 * Px_CR2 - DDR=0: 0/1 - EXTI disabled/enabled; DDR=1: 0/1 - 2/10MHz
 *
 */

/**
 * Show next digit - function calls from main() by some system time value amount
 */
void show_next_digit(){
	uint8_t L = display_buffer[N_current] & 0x7f;
	// first turn all off
	CLEAR_ANODES();
  // set all cathodes high (so that they are not light up when anodes are high)
	GPIOB->ODR |= PB_BLANK; GPIOC->ODR |= PC_BLANK; GPIOD->ODR |= PD_BLANK;
	
	if(L < 18){ // letter
	  // set required cathodes to low to light up the letter
		GPIOB->ODR &= ~PB_bits[L];
		GPIOC->ODR &= ~PC_bits[L];
		GPIOD->ODR &= ~PD_bits[L];
	}
	
	// display decimal point if required
	if(display_buffer[N_current] & 0x80){ // DP
		GPIOD->ODR &= ~GPIO_PIN3;
	}
	else 
	{ 
		GPIOD->ODR |= GPIO_PIN3;
	}

	// set anode up for current digit to light it up
	switch(N_current){
		case 0:
			GPIOC->ODR |= 0x08;
		break;
		case 1:
			GPIOC->ODR |= 0x10;
		break;
		case 2:
			GPIOA->ODR |= 0x08;
		break;
	}

	if(++N_current > 2) N_current = 0;
}

/**
 * fills buffer to display
 * @param str - string to display, contains "0..f" for digits, " " for space, "." for DP
 * 				for example: " 1.22" or "h1ab" (something like "0...abc" equivalent to "0.abc"
 * 				register independent!
 * 			any other letter would be omitted
 * 			if NULL - fill buffer with spaces
 */
void set_display_buf(char *str){
	uint8_t B[3];
	signed char ch, M = 0, i;
	N_current = 0; // refresh current digit number
	// empty buffer
	for(i = 0; i < 3; i++)
		display_buffer[i] = ' ';
	if(!str) return;
	i = 0;
	for(;(ch = *str) && (i < 3); str++){
		M = 0;
		if(ch > '/' && ch < ':'){ // digit
			M = '0';
		}else if(ch > '`' & ch < 'g'){ // a..f
			M = 'a' - 10;
		}else if(ch > '@' & ch < 'G'){ // A..F
			M = 'A' - 10;
		}else if(ch == '-'){ // minus
			M = '-' - 16;
		}else if(ch == 'h'){ // hex
			M = 'h' - 17;
		}else if(ch == 'H'){ // hex
			M = 'H' - 17;
		}else if(ch == '.'){ // DP, set it to previous char
			if(i == 0){ // word starts from '.' - make a space with point
				B[0] = 0xff;
			}else{ // set point for previous character
				B[i-1] |= 0x80;
			}
			continue;
		}else if(ch != ' '){ // bad character - continue
			continue;
		}
		B[i] = ch - M;
		i++;
	}
	// now make align to right
	ch = 2;
	for(M = i-1; M > -1; M--, ch--){
		display_buffer[ch] = B[M];
	}
}

/**
 * convert integer value i into string and display it
 * @param i - value to display, -99 <= i <= 999, if wrong, displays "--E"
 */
void display_int(uint16_t I, char voltmeter){
	int rem;
	uint8_t pos = 0; //DP position
	char N = 2, sign = 0, i;
	if(I < -99 || I > 999){
		set_display_buf("--E");
		return;
	}
	// prepare buffer for voltmeter's values
	if(voltmeter){
		for(i = 0; i < 3; i++)
			display_buffer[i] = 0;
		if(I>999){
			I /= 10;
			pos = 1; // DP is in 2nd position - voltage more than 9.99V
		}
	}else{
		for(i = 0; i < 3; i++)
			display_buffer[i] = ' ';
	}
	
	if(I == 0){ // just show zero
		display_buffer[2] = 0;
		return;
	}
	if(I < 0){
		sign = 1;
		I *= -1;
	}
	do{
		rem = I % 10;
		display_buffer[N] = rem;
		I /= 10;
	}while(--N > -1 && I);
	if(sign && N > -1) display_buffer[N] = 16; // minus sign
	if(voltmeter) display_buffer[pos] |= 0x80;
	N_current = 0;
}

/**
 * displays digital point at position i
 * @param i - position to display DP, concequent calls can light up many DPs
 */
void display_DP_at_pos(uint8_t i){
	if(i > 2) return;
	display_buffer[i] |= 0x80;
}
