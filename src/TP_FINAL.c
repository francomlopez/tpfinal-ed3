/*
===============================================================================
 Name        : tpfinal.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "lpc17xx.h"
#endif

#include <stdio.h>

#include "lpc17xx_gpio.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_exti.h"
#include "lpc17xx_spi.h"
#include "lpc17xx_timer.h";

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')
#define SPI_DATABIT_SIZE 8
#define SSEL 16

#define DERECHA 1
#define IZQUIERDA 0;

uint8_t leds[8];
uint8_t dir;
int level = 0;
uint32_t match0_tim0 = 500000;
int flag = 0;
uint32_t PR_levels[8] = {25,25,20,20,15,15,10,5};

void llenar_leds();
void desplazar_fila(int n);
void confEInt(void);
void confPin(void);
void conf_spi();
void retardo (uint32_t tiempo);
void print_leds();
void llenar_win();
void llenar_lose();
void spi_send_leds(void);
void update_leds();
void conf_timers();

int main(){
	SystemInit();
	llenar_leds();
	dir = DERECHA;

	confPin();
	confEInt();
	conf_spi();
	conf_timers();
	update_leds();

	//print_leds();
	//spi_send_leds();

	while(1){

	}

	return 0;
}

void llenar_leds(){
	for(int i = 0; i < 8; i++){
		leds[i] = 0;
	}
	leds[0] = 0xe0>>1; // 01110000
}

void desplazar_fila(int n){
	if(dir == DERECHA){
		leds[n] = leds[n] >> 1;
		if(leds[n] & 1){
			dir ^= 1;
		}
	}
	else{
		leds[n] = leds[n] << 1;
		if(leds[n] & (1<<7)){
			dir ^= 1;
		}
	}
}

void confEInt(void){
	EXTI_InitTypeDef IntCfg;  //configuro EINT0
	IntCfg.EXTI_Line = EXTI_EINT0; //por flaco ascendente
	IntCfg.EXTI_Mode = EXTI_MODE_EDGE_SENSITIVE;
	IntCfg.EXTI_polarity = EXTI_POLARITY_LOW_ACTIVE_OR_FALLING_EDGE;
	EXTI_Config(&IntCfg);

	EXTI_ClearEXTIFlag(EXTI_EINT0);

	NVIC_EnableIRQ(EINT0_IRQn);
}

void confPin(void){
	//Para EINT0
	PINSEL_CFG_Type PinCfg;
		PinCfg.Funcnum = 1;
		PinCfg.OpenDrain = PINSEL_PINMODE_NORMAL;
		PinCfg.Pinmode = PINSEL_PINMODE_PULLUP;
		PinCfg.Pinnum = 10;
		PinCfg.Portnum = 2;
		PINSEL_ConfigPin(&PinCfg);
}

void EINT0_IRQHandler (void){

	if(flag){
		flag = 0;
		llenar_leds();
		level = 0;
	}
	else{
		if(level == 0){
			leds[1] = 0x70;
		}
		else if(level == 7){
			leds[7] = leds[7] & leds[6];
			if(leds[7] == 0){
				flag = 1;
				retardo(5000000);
				llenar_lose();
			}
			else{
				flag = 1;
				retardo(5000000);
				llenar_win();
			}
		}
		else{
			leds[level] &= leds[level - 1];
			if(leds[level] == 0){
				flag = 1;
				retardo(5000000);
				llenar_lose();
			}
			else{
				leds[level+1] = leds[level];
			}

		}
		level++;
		LPC_TIM0->PR = PR_levels[level];
	}

	retardo(4000000); // antirebote
	EXTI_ClearEXTIFlag(EXTI_EINT0); //limpio flag
	return;

}



void retardo (uint32_t tiempo){
	for (uint32_t conta=0; conta<tiempo; conta++){}
	return;
}

void print_leds(){
	for(int i = 0; i < 8; i++){
		printf(BYTE_TO_BINARY_PATTERN"\n", BYTE_TO_BINARY(leds[i]));
	}
	printf("\n");
}

void llenar_win(){
	leds[7] = 0xff;
	leds[6] = 0x81;
	leds[5] = 0xa5;
	leds[4] = 0x81;
	leds[3] = 0xa5;
	leds[2] = 0x99;
	leds[1] = 0x81;
	leds[0] = 0xff;
}

void llenar_lose(){
	leds[7] = 0xff;
	leds[6] = 0x81;
	leds[5] = 0xa5;
	leds[4] = 0x81;
	leds[3] = 0x99;
	leds[2] = 0xa5;
	leds[1] = 0x81;
	leds[0] = 0xff;
}


void conf_spi(){
	SPI_DATA_SETUP_Type xferConfig;
	uint32_t len=0;
	uint16_t Tx_Buf[2];
	uint16_t Rx_Buf[2];
    PINSEL_CFG_Type PinCfg;
        /*
         * Initialize SPI pin connect
         * P0.15 - SCK;
         * P0.16 - SSEL - used as GPIO
         * P0.17 - MISO
         * P0.18 - MOSI
         */
    PinCfg.Funcnum = 3;
    PinCfg.OpenDrain = 0;

    PinCfg.Pinmode = 0;
    PinCfg.Portnum = 0;
    PinCfg.Pinnum = 15;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 17;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 18;
    PINSEL_ConfigPin(&PinCfg);
    PinCfg.Pinnum = 16;
    PinCfg.Funcnum = 0;
    PINSEL_ConfigPin(&PinCfg);
    LPC_GPIO0->FIODIR |= (1<<SSEL); // setea como output el ssel
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    SPI_CFG_Type SPI_ConfigStruct;
    SPI_ConfigStruct.CPHA = SPI_CPHA_SECOND;
    SPI_ConfigStruct.CPOL = SPI_CPOL_LO;
    SPI_ConfigStruct.ClockRate = 1000000;
    SPI_ConfigStruct.DataOrder = SPI_DATA_MSB_FIRST;
    SPI_ConfigStruct.Databit = SPI_DATABIT_16;
    SPI_ConfigStruct.Mode = SPI_MASTER_MODE;
    // Initialize SPI peripheral with parameter given in structure above
    SPI_Init(LPC_SPI, &SPI_ConfigStruct);

    xferConfig.tx_data = Tx_Buf;
    xferConfig.rx_data = Rx_Buf;
    xferConfig.length = 2;

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0900;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);// modo sin decodificacion 7 seg
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0a0f;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);// selec brillo max
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0b07;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);// scan limit
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0c01; // modo normal (no shutdown)
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0f00;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    return;
}

void spi_send_leds(void){
	SPI_DATA_SETUP_Type xferConfig;
	uint32_t len=0;
	uint16_t Tx_Buf[2];
	uint16_t Rx_Buf[2];
	xferConfig.tx_data = Tx_Buf;
	xferConfig.rx_data = Rx_Buf;
	xferConfig.length = 2;

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x01ff;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0281;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x03a5;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0481;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x05a5;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0699;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x0781;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);

    LPC_GPIO0->FIOCLR |= (1<<SSEL);
    Tx_Buf[0] = 0x08ff;
    len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
    LPC_GPIO0->FIOSET |= (1<<SSEL);
}

void update_leds(){
	SPI_DATA_SETUP_Type xferConfig;
	uint32_t len=0;
	uint16_t Tx_Buf[2];
	uint16_t Rx_Buf[2];
	xferConfig.tx_data = Tx_Buf;
	xferConfig.rx_data = Rx_Buf;
	xferConfig.length = 2;

	for(int i = 0; i < 8; i++){
		LPC_GPIO0->FIOCLR |= (1<<SSEL);
		Tx_Buf[0] = (uint16_t) ((i+1)<<8) + leds[i];
		len = SPI_ReadWrite(LPC_SPI, &xferConfig, SPI_TRANSFER_POLLING);
		LPC_GPIO0->FIOSET |= (1<<SSEL);
	}

}

void conf_timers(){
	TIM_TIMERCFG_Type TIMERCFG;
	TIMERCFG.PrescaleOption=TIM_PRESCALE_TICKVAL;
	TIMERCFG.PrescaleValue=25;

	TIM_MATCHCFG_Type MatchTCFG;
	MatchTCFG.MatchChannel=0;
	MatchTCFG.MatchValue=match0_tim0;
	MatchTCFG.ResetOnMatch=ENABLE;
	MatchTCFG.IntOnMatch=ENABLE;
	MatchTCFG.ExtMatchOutputType=TIM_EXTMATCH_NOTHING;
	MatchTCFG.StopOnMatch=DISABLE;

	TIM_Init(LPC_TIM0, TIM_TIMER_MODE, &TIMERCFG); //Inicializa el periferico.
	TIM_ConfigMatch(LPC_TIM0, &MatchTCFG); //Configura Match0 en 0.5s.

	TIM_Cmd(LPC_TIM0, ENABLE); //Habilita el periferico.

	NVIC_EnableIRQ(TIMER0_IRQn); //Habilita interrupciones del periferico.
	return;
}

void TIMER0_IRQHandler(){
	if(!flag){
		desplazar_fila(level);
	}
	update_leds();
	TIM_ClearIntPending(LPC_TIM0,0);
}
