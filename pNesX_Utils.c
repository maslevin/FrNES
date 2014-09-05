/*===================================================================*/
/*                                                                   */
/*  pNesX_Utils.h : Utility Functions                                */
/*                                                                   */
/*  2001/12/22  ReGex   FrNES 0.60 Source Release                    */
/*                                                                   */
/*===================================================================*/

#include <kos.h>
#include "pNesX_Utils.h"

char pNesX_nibtoc(uint32 nibble)
{
	switch(nibble)
	{
		case 0x0:
			return '0';
			break;
		case 0x1:
			return '1';
			break;
		case 0x2:
			return '2';
			break;
		case 0x3:
			return '3';
			break;
		case 0x4:
			return '4';
			break;
		case 0x5:
			return '5';
			break;
		case 0x6:
			return '6';
			break;
		case 0x7:
			return '7';
			break;
		case 0x8:
			return '8';
			break;
		case 0x9:
			return '9';
			break;
		case 0xA:
			return 'A';
			break;
		case 0xB:
			return 'B';
			break;
		case 0xC:
			return 'C';
			break;
		case 0xD:
			return 'D';
			break;
		case 0xE:
			return 'E';
			break;
		case 0xF:
			return 'F';
			break;
		default:
			return ((char)NULL);
			break;
	}
}

int pNesX_ctoi (char character)
{
	switch (character)
	{
		case '0':
			return 0;
			break;
		case '1':
			return 1;
			break;
		case '2':
			return 2;
			break;
		case '3':
			return 3;
			break;
		case '4':
			return 4;
			break;
		case '5':
			return 5;
			break;
		case '6':
			return 6;
			break;
		case '7':
			return 7;
			break;
		case '8':
			return 8;
			break;
		case '9':
			return 9;
			break;
	}
	return -1;
}

void pNesX_i32toa(uint32 i, char* buffer)
{
	char b[9];

	uint32 temp;

	temp = i >> 28;
	b[0] = pNesX_nibtoc(temp);
	temp = (i >> 24) & 0x0000000F;
	b[1] = pNesX_nibtoc(temp);
	temp = (i >> 20) & 0x0000000F;
	b[2] = pNesX_nibtoc(temp);
	temp = (i >> 16) & 0x0000000F;
	b[3] = pNesX_nibtoc(temp);
	temp = (i >> 12) & 0x0000000F;
	b[4] = pNesX_nibtoc(temp);
	temp = (i >> 8) & 0x0000000F;
	b[5] = pNesX_nibtoc(temp);
	temp = (i >> 4) & 0x0000000F;
	b[6] = pNesX_nibtoc(temp);
	temp = (i >> 0) & 0x0000000F;
	b[7] = pNesX_nibtoc(temp);
	b[8] = 0;

	strcpy(buffer, b);
}

int pNesX_pow10(int order)
{
	int accum;
	if (order)
	{
		accum = 10;
		for (order -= 1; order; order--)
			accum *= 10;
		return accum;
	}
	else
		return 1;
}

void pNesX_itoa(uint32 i, char* buffer)
{
	int index;
	int maxindex;
	uint32 temp;
	int divisor;

	index = 0;
	temp = i;

	if (temp == 0)
	{
		buffer[0] = '0';
		buffer[1] = (char)NULL;
		return;
	}

	while (temp > 0)
	{
		temp = temp / 10;
		index++;
	}
	
	maxindex = index;
	temp = i;

	for (index = 0; index < maxindex; index++)
	{
		divisor = pNesX_pow10(maxindex - index - 1);
		buffer[index] = pNesX_nibtoc((temp / divisor) % 10);
	}

	buffer[index] = (char)NULL;
}

int pNesX_atoi(char* numbuffer)
{
	int order;
	int index;
	int accum;
	int temp;
	order = strlen(numbuffer);
	accum = 0;

	for (index = 0; index < order; index++)
	{
		temp = pNesX_ctoi(numbuffer[index]);
		if (temp != -1)
			accum += temp * pNesX_pow10(order - 1 - index);
		else
			return -1;
	}
	
	return accum;
}
