#include "stm8s.h"
#include "led.h"

#define   LED_delay      (1) // one digit emitting time
#define   ADC_delay      (50)// delay for reading voltage
#define   Display_delay (400)// delay for displaying value
// 1023 -> 3.3V - having 0.1ohm as shunt resistor for 1A -> ADC = 31
// so max value is about 33A
// send 999 to display_int when 9.99v
// testing shows that my shunt resistor is not exactly 0.1ohm, thus 29 value is more accurate
// using 29 instead of 31
#define K 29

uint32_t Global_time = 0L;    // global time in ms
uint32_t ADC_time = 0L;
uint32_t Display_time = 0L;
uint8_t waitforADC = 0;
uint8_t ADC_started = 0;
uint16_t ADC_values[3] = { 0, 0, 0};
uint8_t index = 0;

void simpleDelay(uint8_t how_much);

/*
DDR – Data Direction Register (pins are either inputs (0) or outputs (1))
ODR – Output Data Register (output data register is used for outputting digital values 1 -on, 0 off)
IDR – Input Data Register (IDR register can be used to read the digital value)
CR1 and CR2 – Control registers (different I/O features)
*/

uint16_t adc_read(void)
{
	uint16_t temph = 0;
  uint8_t templ = 0;
	
	ADC1->CR1 |= ADC1_CR1_ADON; //wake it up
	ADC1->CR1 |= ADC1_CR1_ADON; //start the conversion, wait few mils
	simpleDelay(1);
	while (!(ADC1->CSR & ADC1_CSR_EOC)); //wait for conversion to finish
	 /* Read LSB first */
	templ = ADC1->DRL;
	/* Then read MSB */
	temph = ADC1->DRH;    
	temph = (uint16_t)(templ | (uint16_t)(temph << (uint8_t)8));
	ADC1->CR1 &= (uint8_t)(~ADC1_CR1_ADON); //power it off
	return (uint16_t)temph;
}

void adc_read_start(void)
{
	ADC1->CR1 |= ADC1_CR1_ADON; //wake it up
	ADC1->CR1 |= ADC1_CR1_ADON; //start the conversion, wait few mils
	simpleDelay(1);
	waitforADC = 0;
	ADC_started = 1;
}

void wait_ADC(){
	if(ADC1->CSR & ADC1_CSR_EOC){ // the conversion is done!
		waitforADC = 1;
	}
}

uint16_t adc_read_end(void)
{
	uint16_t temph = 0;
  uint8_t templ = 0;
	 /* Read LSB first */
	templ = ADC1->DRL;
	/* Then read MSB */
	temph = ADC1->DRH;    
	temph = (uint16_t)(templ | (uint16_t)(temph << (uint8_t)8));
	ADC1->CR1 &= (uint8_t)(~ADC1_CR1_ADON); //power it off
	ADC_started = 0;
	waitforADC = 0;
	return (uint16_t)temph;
}

// store ADC value in a rolling array
void store(uint16_t value)
{
	ADC_values[index++] = value; // store value in array
	if(index == 3) index = 0; // check index overflow
}

// get median value
uint16_t median(uint16_t value1, uint16_t value2, uint16_t value3)
{
	if((value1 <= value2) && (value1 <= value3))
	{
		return (value2 <= value3) ? value2 : value3;
	}
	else
	{
		if((value2 <= value1) && (value2 <= value3))
		{
			return (value1 <= value3) ? value1 : value3;
		}
		else
		{
			return (value1 <= value2) ? value1 : value2;
		}
	}
}

// calculate average value
uint16_t average(uint16_t value1, uint16_t value2, uint16_t value3)
{
	uint16_t value = 0;
	uint8_t count = 0;
	
	if(value1 > 0)
	{
		value += value1;
		count++;
	}
	if(value2 > 0)
	{
		value += value2;
		count++;
	}	
	if(value3 > 0)
	{
		value += value3;
		count++;
	}	
	if (count > 0) value /= count;
	
	return value;
}

main()
{
	uint32_t T_LED = 0L;  // time of last digit update
	uint16_t ADC_value = 0;
	uint16_t value = 0, value1 = 0, value2 = 0, value3 = 0;
	uint8_t i, count = 0;

	//	Setup ADC on AIN6/PD6/PIN3 as AIN5/PD5/PIN2 is dead
	GPIOD->DDR &= (uint8_t)(0b10011111);				//make both as inputs
	GPIOD->CR1 &= (uint8_t)(0b10011111);				//make both floating inputs

	ADC1->CR1 &= (uint8_t)(~ADC1_CR1_CONT);			//Set the single conversion mode
	ADC1->CR1 &= (uint8_t)(~ADC1_CR1_SPSEL);		//	Clear the SPSEL bits (prescaler)
	ADC1->CR1 |= 0x40;													//	Select the prescaler division factor according to ADC1_PrescalerSelection values

	ADC1->CR2 |= ADC1_CR2_ALIGN;								//0x08 Configure right data aligment
	ADC1->CR2 &= (uint8_t)(~ADC1_CR2_EXTTRIG);	// Disable the selected external trigger
	ADC1->CR2 &= (uint8_t)(~ADC1_CR2_EXTSEL);		// Clear the external trigger selection bits
	ADC1->CR2 &= (uint8_t)(~ADC1_CR2_SCAN);			// Disable scan mode
	
  ADC1->CSR = 0b00000110; //disable everything, but select AIN6 channel for ADC
	
	// Setup Timer1
	// prescaler = f_{in}/f_{tim1} - 1
	// set Timer1 to 1MHz: 16/1 - 1 = 15
	TIM1->PSCRH = 0;
	TIM1->PSCRL = 15; // LSB should be written last as it updates prescaler
	// auto-reload each 1ms: TIM_ARR = 1000 = 0x03E8
	TIM1->ARRH = 0x03;
	TIM1->ARRL = 0xE8;
	// interrupts: update
	TIM1->IER = TIM1_IER_UIE;
	// auto-reload + interrupt on overflow + enable
	TIM1->CR1 = TIM1_CR1_ARPE | TIM1_CR1_URS | TIM1_CR1_CEN;	
	
	// Configure clocking
	CLK->CKDIVR = 0; // F_HSI = 16MHz, f_CPU = 16MHz
	// Configure pins
	CFG->GCR |= 1; // disable SWIM

	LED_init();
	set_display_buf("000");
	
	enableInterrupts();
	
	while (1) 
	{			
		if(((uint16_t)(Global_time - ADC_time) > ADC_delay) || (ADC_time > Global_time))
		{
			ADC_time = Global_time;
			//adc_read_start(); // non blocking read - comment out everything bellow this
			
			// read three values and get median
			value1 = adc_read();
			value2 = adc_read();
			value3 = adc_read();
			value = median(value1, value2, value3);
			
			store(value*(long)100/K);			
		}
		
		if((uint16_t)(Global_time - Display_time) > Display_delay)
		{
			Display_time = Global_time;
			
			value = average(ADC_values[0], ADC_values[1], ADC_values[2]);
			display_int(value, 1); // send "true" in second parameter to allow auto decimal point
		}
		
		if (Display_time > Global_time) Display_time = Global_time; // reset Display_time when Global_time overflows
		
		if((uint8_t)(Global_time - T_LED) > LED_delay)
		{
			T_LED = Global_time;
			show_next_digit();
		}
		
		// used for non-blocking read
		if(ADC_started) 
		{
			wait_ADC();
		}
		// used for non-blocking read
		if(waitforADC)
		{
			ADC_value = adc_read_end();	
			store(ADC_value*(long)100/K);
			
			waitforADC = 0;
		}
	}
}	

// keep the processor busy for some time
void simpleDelay(uint8_t how_much)
{
	unsigned int i, j;
	
	for(i = 0; i < how_much; i ++)
	{
		for(j=0; j < 40; j ++)
		{
		}
	}
}

// Timer1 Update/Overflow/Trigger/Break Interrupt
INTERRUPT_HANDLER(TIM1_UPD_OVF_TRG_BRK_IRQHandler, 11){
	if(TIM1->SR1 & TIM1_SR1_UIF){ // update interrupt
		Global_time++; // increase timer
	}
	TIM1->SR1 = 0; // clear all interrupt flags
}