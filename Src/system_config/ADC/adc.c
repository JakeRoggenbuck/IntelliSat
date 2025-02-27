/*
 * adc.c
 *
 *  Created on: Feb 10, 2024
 *      Author: Chandrark Muddana
 */

#include "adc.h"

//USES ADC1

bool is_ADC_not_ready() { return (ADC1->CR & ADC_CR_ADCAL) != 0; }
//THE BELOW FUNCTION IN TIMEOUT HUNG FOREVER, COULD NOT TEST OTHER ADC
bool is_VREFBUF_not_ready() { return (VREFBUF->CSR & VREFBUF_CSR_VRR) != 0; }
void adc_init() {
	RCC->AHB2ENR |= RCC_AHB2ENR_ADCEN; //enables ADC clock
	//ADC1->CR |= ADC_CR_ADDIS; //Disables ADC
	RCC->CCIPR &= ~RCC_CCIPR_ADCSEL; //enables peripheral clock
	RCC->CCIPR |= RCC_CCIPR_ADCSEL_SYSCLK; //Sets ADC clock to system clock
	ADC1->CR   &= ~ADC_CR_DEEPPWD;//makes sure ADC isn't in deep power down mode
	ADC1->CR   |= ADC_CR_ADVREGEN; //enables ADC voltage regulator
	//Test what happens if voltage regulator is gone
	nop(10000); //waits a bit
	ADC1->CR &= ~(ADC_CR_ADCALDIF); //Sets it to single ended mode
	ADC1->CR |= ADC_CR_ADCAL; //Calibrates ADC
	wait_with_timeout(is_ADC_not_ready, DEFAULT_TIMEOUT_MS); //Waits until ADC is calibrated

	VREFBUF->CSR |= VREFBUF_CSR_ENVR; //Enables internal reference buffer
	VREFBUF->CSR &= ~(VREFBUF_CSR_HIZ); //IDK If this is needed or not

	wait_with_timeout(is_VREFBUF_not_ready, DEFAULT_TIMEOUT_MS); //Waits until voltage reference value reaches expected output

	VREFBUF->CSR |= VREFBUF_CSR_VRS; //Sets internal reference buffer to around 2.5V

	//Try changing ADC CCR prescaler
	//Try changing VREFBUF CSR register to enable vrefint since vref+ is decoupled

}

bool is_ADRDY_not_reset() { return (ADC1->ISR & ADC_ISR_ADRDY) == 0; }
void adc_enable(){
	ADC1->ISR |= ADC_ISR_ADRDY; // Set before enabling ADC
	ADC1->CR |= ADC_CR_ADEN; //Enables ADC
	wait_with_timeout(is_ADRDY_not_reset, DEFAULT_TIMEOUT_MS); //Waits until ADRDY is reset
}

void adc_configGPIO(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOCEN; //enables GPIO C
	wait_with_timeout(is_GPIOB_not_ready, DEFAULT_TIMEOUT_MS); //Waits until its done enabling

	GPIOC->OTYPER   &= ~(GPIO_OTYPER_OT0); //Reset C0 pin
	GPIOC->PUPDR    &= ~(GPIO_OSPEEDR_OSPEED0); //Reset C0 pin
	GPIOC->OSPEEDR  &= ~(GPIO_PUPDR_PUPD0); //Reset C0 pin
	GPIOC->MODER    &= ~(GPIO_MODER_MODE0); //Reset C0 pin
	//GPIOC->MODER  |= ( GPIO_MODER_MODE0_0 ); //Sets C0 pin to output mode (KEEP COMMENTED)
	GPIOC->ASCR	    |= ( GPIO_ASCR_ASC0 ); //Connects analog switch to ADC channel for C0
	GPIOC->MODER    |=  GPIO_MODER_MODE0_ANALOG; //Sets mode to analog
	//gpio_set(GPIOC, 0,0); //Testing where C0 pin was (KEEP COMMENTED)
}

void adc_setConstantGPIOValue(){
	RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; //enables GPIO B
	wait_with_timeout(is_GPIOB_not_ready, DEFAULT_TIMEOUT_MS); //Waits until its done enabling

	GPIOB->MODER &= ~(
		  GPIO_MODER_MODE3_Msk
		| GPIO_MODER_MODE4_Msk
		| GPIO_MODER_MODE5_Msk
		| GPIO_MODER_MODE6_Msk); //Resets config of B3 to B6

	GPIOB->MODER |=
	   	  GPIO_MODER_MODE3_0	// B3
		| GPIO_MODER_MODE4_0	// B4
		| GPIO_MODER_MODE5_0	// B5
		| GPIO_MODER_MODE6_0;	// B6 Sets to output mode

	gpio_set(GPIOB, 3, 0); //3-3
	gpio_set(GPIOB, 4, 0); //3-2
	gpio_set(GPIOB, 5, 1); //2-2
	gpio_set(GPIOB, 6, 1); //1-3
}

void adc_setChannel(){
	//leave sampling time at default
	ADC123_COMMON->CCR |= (ADC_CCR_VBATEN);
	ADC1->SQR1 &= ~( ADC_SQR1_L ); //Sets number of channels in the sequence of 1
	ADC1->SQR1 &= ~(ADC_SQR1_SQ1); //Resets the sequence
	ADC1->SQR1 |= ADC_SQR1_SQ1_CHAN18_AS_1st_CONV; //Sets the sequence to just channel 1 (PIN C0)
	ADC1->SMPR2 &= ~(ADC_SMPR2_SMP18); //Resets the sampling time of channel 18
	ADC1->SMPR2 |= ADC_CHAN18_640_5_CLK_CYC; //Sets the sampling time to max: 640.5 cycles per sample
}

// Perform a single ADC conversion.
// (Assumes that there is only one channel per sequence)
bool is_EOC_down() { return !(ADC1->ISR & ADC_ISR_EOC); }
bool is_EOS_down() { return !(ADC1->ISR & ADC_ISR_EOS); }
uint16_t adc_singleConversion() {
	// Start the ADC conversion.
	ADC1->CR  |=  ( ADC_CR_ADSTART );
	// Wait for the 'End Of Conversion' flag.
	wait_with_timeout(is_EOC_down, DEFAULT_TIMEOUT_MS);
	// Read the converted value (this also clears the EOC flag).
	uint16_t adc_val = ADC1->DR;
	// Wait for the 'End Of Sequence' flag and clear it.
	wait_with_timeout(is_EOS_down, DEFAULT_TIMEOUT_MS);
	ADC1->ISR |=  ( ADC_ISR_EOS );
	// Return the ADC value.
	return adc_val;
}

uint16_t adc_adcToVolt1(uint16_t adcVal){
	return (adcVal * 2062) / 4095; //Uses 2.048 volt reference (VREFBUF->CSR VRS bit is 0)
}

uint16_t adc_adcToVolt2(uint16_t adcVal){
	return (adcVal * 2532) / 4095; //Uses 2.048 volt reference (VREFBUF->CSR VRS bit is 0)
}

uint16_t adc_adcToBatVolt(uint16_t adcVal){
	return (adcVal * 1757) / 200; //Uses whatever im testing as a basis
}

void adc_printVolt(uint16_t volt){
	printMsg("%d", volt / 1000);
	printMsg(".");
	printMsg("%d", volt % 1000);
}

void adc_printMilliVolt(uint16_t volt){
	printMsg("%d", volt);
}
