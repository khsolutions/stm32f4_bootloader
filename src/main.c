/*
 * This file is part of the µOS++ distribution.
 *   (https://github.com/micro-os-plus)
 * Copyright (c) 2014 Liviu Ionescu.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

// ----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stm32f4xx.h>
#include "diag/Trace.h"

#include "flash.h"

// ----------------------------------------------------------------------------
//
// Standalone STM32F4 empty sample (trace via DEBUG).
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_ITM, OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"


#define getbit(n,b)		((n  &  (1 << b)) >> b)

void enable_irq();

void flash_erase();
void flash_write(uint8_t);
void flash_read(uint32_t);

void recieve();
void printhex();
void printval(int);
void printdata(char *);

uint32_t getsector(int);

int data;				//Memory for storing User Input
int flag = 0;			//Used while Taking Input Data

USART_TypeDef *priv;
FLASH_TypeDef *fp = FLASH;

int
main(int argc, char* argv[])
{
	USART_TypeDef *u6 = USART6;
	GPIO_TypeDef  *pc = GPIOC;

	RCC->AHB1ENR	|=	RCC_AHB1ENR_GPIOCEN;
	RCC->APB2ENR	|=	RCC_APB2ENR_USART6EN;

	pc->MODER	|=	GPIO_MODER_MODER6_1;
	pc->MODER	|=	GPIO_MODER_MODER7_1;

	pc->AFR[0]	|=	(8 << 24)|(8 << 28);

	u6->BRR	=	(84000000/115200);

	u6->CR1	|=	USART_CR1_UE;
	u6->CR1	|=	USART_CR1_TE|USART_CR1_RE;
	u6->CR1	|=	USART_CR1_RXNEIE;

	priv = u6;

	enable_irq();

	u6->DR	=  '\0';
	while(!getbit(u6->SR,6));

	printdata("###################################################\n");
	printdata("#         Welcome To STM32F4  Bootloader          #\n");
	printdata("###################################################\n");

	play:

	printdata("\nSelect one of the following Operations:\n\n");

	printdata("0) To Erase Chip\n");
	printdata("1) To Read  Chip\n");
	printdata("2) To Check Code length\n");
	printdata("3) To Write Data into Chip\n");
	printdata("4) To Jump to Application Mode\n");

	printdata("\n");
	optn:
	printdata("\nSelect an Option: ");

	recieve();

	switch(data-'0')
	{

	case 0:
		flash_erase();
		goto play;
		break;

	case 1:
		flash_read(20);
		goto play;
		break;

	case 2:
		//code_length();
		goto play;
		break;

	case 3:
		flash_write(5);
		goto play;
		break;

	case 4:
		trace_printf("Im in reset function\n");
		//app_reset();
		goto play;
		break;

	default:
		printdata("\rUnknown Option\r\n");
		goto optn;

	}

	while(1);
}

void recieve()
{
	while(flag == 0);
	printval(data);
	flag = 0;
}

void printdata(char *p)
{

	priv->DR	=  '\r';

	while(!getbit(priv->SR,6));

	while(*p != '\0')
	{
		priv->DR	=  *p;
		while(!getbit(priv->SR,6));

		priv->SR	&=	~USART_SR_TC;
		p++;
	}
}

void printval(int a)
{
	priv->DR	=  a;
	while(!getbit(priv->SR,6));

	priv->DR	=  '\r';
	while(!getbit(priv->SR,6));

	priv->DR	=  '\n';
	while(!getbit(priv->SR,6));

}

void enable_irq()
{
	HAL_NVIC_EnableIRQ(USART6_IRQn);
}

void USART6_IRQHandler()
{
	//trace_printf("Im in Handler..\n");
	data = priv->DR;
	flag = 1;
	priv->SR &= ~USART_SR_RXNE;
}

void flash_read(uint32_t bytes)
{
	uint32_t *p;

	printdata("\nFlash :\n\r");
	sec:
	printdata("\nSelect a Sector to be Read: ");

	recieve();

	p = (uint32_t *) getsector(data - '0');

	if(((data - '0') > 11) || ((data - '0') < 0))
	{
		printdata("Error Sector Selection..\n");
		goto sec;
	}

	printdata("\nReading Sector: ");
	//trace_printf("Sector to be Read: 0x%X\n",p);
	printhex(p);
	printdata("\n\n\r");

	while(bytes--)
	{
		printhex(p);

		priv->DR	=  ':';
		while(!getbit(priv->SR,6));
		priv->DR	=  ' ';
		while(!getbit(priv->SR,6));

		printhex(*p);
		printdata("\n");
		p++;
	}

}

void printhex(uint32_t p)
{
	uint8_t *pckt = 0x00;

	int i = 0;

	priv->DR	=  '0';
	while(!getbit(priv->SR,6));

	priv->DR	=  'x';
	while(!getbit(priv->SR,6));

	while(i++ < 8)
	{
		*pckt = (p & 0xF0000000) >> 28;
		p	<<= 4;

		if(*pckt < 10)
		{
			priv->DR	=  *pckt + '0';
			while(!getbit(priv->SR,6));
		}
		else
		{
			priv->DR	=  *pckt + 55;
			while(!getbit(priv->SR,6));
		}
	}

}


void flash_erase()
{

	erase:

	printdata("\nSelect a Sector to be Erased: ");

	recieve();

	if(data < '2')
	{
		printdata("Data Cannot be Erased..\n");
		goto erase;
	}
	else
	{
		//trace_printf("Sector to be erased: %d\n",data-'0');

		if(getbit(fp->SR,16))
		{
			printdata("\r\nMemory Busy\n\r");
			return;
		}

		fp->KEYR	 =	FLASH_KEY_1;
		fp->KEYR	 =	FLASH_KEY_2;

		fp->CR	&=	~FLASH_CR_SNB_0;
		fp->CR	&=	~FLASH_CR_SNB_1;
		fp->CR	&=	~FLASH_CR_SNB_2;
		fp->CR	&=	~FLASH_CR_SNB_3;


		fp->CR	|=	FLASH_CR_SER;
		fp->CR	|=	((data - '0') << 3);
		fp->CR	|=	FLASH_CR_STRT;

		printdata("\nErasing...\r\n");

		//Busy Bit
		while(getbit(fp->SR,16));

		fp->CR	|=	FLASH_CR_LOCK;

		printdata("\nFinished Erasing..\n\r");

	}
}


void flash_write(uint8_t sector)
{
	uint8_t *p;

	fp->KEYR		=	FLASH_KEY_1;
	fp->KEYR		=	FLASH_KEY_2;

	fp->CR	&=	~FLASH_CR_SNB_0;
	fp->CR	&=	~FLASH_CR_SNB_1;
	fp->CR	&=	~FLASH_CR_SNB_2;
	fp->CR	&=	~FLASH_CR_SNB_3;

	printdata("Enter Sector to Write: ");
	recieve();

	p = (uint8_t *) getsector(data - '0');

	if(((data - '0') > 11) || ((data - '0') <0))
	{
		printdata("Error No such Sector\n");
		return;
	}

	if(getbit(fp->SR,16))
	{
		printdata("Flash Busy\n\r");
		return;
	}

	while(getbit(fp->SR,16));

	fp->CR	|=	FLASH_CR_PG;
	fp->CR	|=	((data-'0') << 3);

	int i = 20;

	while(i--)
	{
		*p = 0xA5;
		p++;
		while(getbit(fp->SR,16));
	}

	fp->CR	|=	FLASH_CR_LOCK;
	fp->CR	&= ~FLASH_CR_PG;
}

uint32_t getsector(int i)
{
	switch(i)
	{
		case 0:
			return sector0;

		case 1:
			return sector1;

		case 2:
			return sector2;

		case 3:
			return sector3;

		case 4:
			return sector4;

		case 5:
			return sector5;

		case 6:
			return sector6;

		case 7:
			return sector7;

		case 8:
			return sector8;

		case 9:
			return sector9;

		case 10:
			return sector10;

		case 11:
			return sector11;
	}

	return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
