#include "stm8s.h"
#include "led.h"

#define   LED_delay      (1) // one digit emitting time
#define   ADC_delay   (200)

uint32_t Global_time = 0L;    // global time in ms
uint32_t ADC_time = 0L;
U8 waitforADC = 0;
U8 ADC_started = 0;

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
	//unsigned int temp,adc_value;
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
	return ((uint16_t)temph);
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
	return ((uint16_t)temph);
}

main()
{
	uint32_t T_LED = 0L;  // time of last digit update
	uint16_t ADC_value = 0;
	uint16_t value = 0;

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
			adc_read_start();
		}
		
		if((U8)(Global_time - T_LED) > LED_delay)
		{
			T_LED = Global_time;
			show_next_digit();
		}
		
		if(ADC_started) 
		{
			wait_ADC();
		}
		
		if(waitforADC)
		{
			ADC_value = adc_read_end();	
			//1023 -> 3.3V - having 0.01ohm as shunt resistor for 1A -> ADC = 31
			//so max value is 33A
			if (ADC_value >= 310)
			{
				// here we have more than 10A, show one decimal
				value=(uint16_t)((long)ADC_value*(long)10/31);
				display_int(value, 0);
				display_DP_at_pos(1);								
			}
			else if (ADC_value > 0)
			{
				// less than 10A, show two decimals
				value=(uint16_t)((long)ADC_value*(long)100/31);
				display_int(value, 0);
				display_DP_at_pos(0);						
			}
			else
			{
				set_display_buf("000");
				display_DP_at_pos(0);
			}
								
			waitforADC = 0;
		}
	}
}	

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