#define F_CPU 7372800UL

#include <stdio.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "lcd.h"

volatile float ref_value = 0;
volatile int R0 = 1200; //veci broj = brze raste funkcija
static volatile int R2 = 660;


uint8_t is_calibrated = 0;
uint16_t last_sample = 0;
char adcStr[128];



void write_to_lcd(){
	lcd_clrscr();
	lcd_puts(adcStr);
}

void writeADC(float BAC)
{
	float PROM = BAC * 2.09;
	snprintf(adcStr, 128, "BAC: %d.%d%d mg/L\nPROMILI: %d.%d%d%c.",
	(int)BAC/10, (int)BAC%10, (int)(BAC*10)%10, (int)PROM/10, (int)PROM%10, (int)(PROM*10)%10, '%');
	write_to_lcd();
}

float ADCpretvorba()
{
	//ucitava stanje na senzoru
	ADCSRA |= _BV(ADSC);
	while (!(ADCSRA & _BV(ADIF)));
	//magija (pretvorba ADC u BAC)
	float sens_volt = (float)ADC / 1024 * 5.0;
	float RS = ((5 * R2) / sens_volt) - R2;
	float omjer = RS / R0;
	double x = 0.3934 * omjer;
	float BAC = pow(x, -1.504) - ref_value;
	
	return BAC;
}

void calibrate()
{
	uint8_t count = 0;
	while(!is_calibrated)
	{
		ADCSRA |= _BV(ADSC);
		while (!(ADCSRA & _BV(ADIF)));
		uint16_t new_sample = ADC;
		uint8_t tmp_abs = 0;
		if((new_sample - last_sample) > 0) tmp_abs = new_sample - last_sample;
		else tmp_abs = last_sample - new_sample;
		last_sample = new_sample;
		
		if (tmp_abs < 10) //mores smanjit kod
		{
			count++;
			if(count == 20) //mores povecat tu da se vise puta treba poklopit vrijednost da je manja od one u if uvjetu
			{				//ali onda promijeni i u ispisu postotke
				is_calibrated = 1;
				ref_value = ADCpretvorba();
				R0 *= 1 + ref_value/666; //veci broj = brze raste funkcija
				break;
			}
		}
		snprintf(adcStr, 128, "Calibrating: %hhu%c ", count*5, '%');
		write_to_lcd();
		_delay_ms(50); //ako je vec dovoljno zagrijan senzor kalibracija ce bit gotova u jednu sekundu
	}
}


void LCDinit()
{
	DDRD = _BV(4);

	TCCR1A = _BV(COM1B1) | _BV(WGM10);
	TCCR1B = _BV(WGM12) | _BV(CS11);
	OCR1B = 14;
	
	lcd_init(LCD_DISP_ON);
}

void ADCinit()
{
	//konfiguracija ADC referentne voltaze (REFS0) i ulaznog pina PA0!
	ADMUX = _BV(REFS0);
	
	//taka ADC i prescaler od 64
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
}

int main(void)
{
	LCDinit();
	ADCinit();
	calibrate();

	while (1)
	{
		writeADC(ADCpretvorba());
		_delay_ms(150);
	}
}
