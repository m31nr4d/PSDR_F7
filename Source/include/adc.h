/* hal.h
 *
 * misc. h/w interfaces definition
 *
 * Copyright 2013 RPH Engineering, VPI Engineering
 */

#pragma once

#ifndef ADC_H_
#define ADC_H_

    #include "hal.h"

#ifdef PSDR2
#include "stm32f4xx_hal_dma.h"
#include "stm32f4xx_hal_adc.h"
#include "stm32f4xx_hal_rcc.h"
#endif

#ifdef PSDR257
#include "stm32f7xx_hal_dma.h"
#include "stm32f7xx_hal_adc.h"
#include "stm32f7xx_hal_rcc.h"
#endif

extern __IO uint16_t uhADCxConvertedValue1;
extern __IO uint16_t uhADCxConvertedValue2;
extern __IO uint16_t uhADCxConvertedValue3;
extern uint8_t adcConfigured;
extern uint16_t sampleIndex;
extern volatile uint8_t sampleRun;
extern ADC_HandleTypeDef    AdcHandle1;
extern ADC_HandleTypeDef    AdcHandle2;
extern ADC_HandleTypeDef    AdcHandle3;
extern ADC_ChannelConfTypeDef sConfig1;
extern ADC_ChannelConfTypeDef sConfig2;
extern ADC_ChannelConfTypeDef sConfig3;
extern uint8_t wrongThings;

	/* Definition for ADCx clock resources */
	#define ADC_RX_Q            ADC3
	#define ADC_RX_I						ADC2
	#define ADC_MIC							ADC1
	#define ADCx_CLK_ENABLE()               __ADC1_CLK_ENABLE()
	#define ADCx_CHANNEL_GPIO_CLK_ENABLE()  __GPIOA_CLK_ENABLE()

	#define ADCx_FORCE_RESET()              __ADC_FORCE_RESET()
	#define ADCx_RELEASE_RESET()            __ADC_RELEASE_RESET()

	/* Definition for ADCx Channel Pin */
	//#define ADC_RX-Q_PIN	                GPIO_PIN_3
	//#define ADCx_CHANNEL_GPIO_PORT          GPIOA

	/* Definition for ADCx's Channel */
	#define ADC_RX_Q_CHANNEL                    ADC_CHANNEL_2
	#define ADC_RX_I_CHANNEL					ADC_CHANNEL_3
	#define ADC_MIC_CHANNEL						ADC_CHANNEL_9

#define ADC_LIPO_VOLTAGE					ADC_CHANNEL_15
#define ADC_VNA_PHASE						ADC_CHANNEL_7
#define ADC_VNA_MAGNITUTDE					ADC_CHANNEL_14



	/* Definition for ADCx's NVIC */
	#define ADCx_IRQn                      ADC_IRQn


	void initAdc(void);


#endif /* ADC_H_ */


void adcGetConversion(void);
void adcStartConversion(void);

