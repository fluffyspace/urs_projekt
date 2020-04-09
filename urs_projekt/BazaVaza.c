/*
 * urs_projekt.c
 *
 * Created: March 2020.
 * Authors (alphabetically) : Ingo Kodba, Martin Luk, Anamaria Vargi?
 */ 
#define F_CPU 7372800UL

#define SAMPLE_RATE 5
#define COUNTER_INCREMENT_TIME 100 // in milliseconds
#define MAX_TIMER_VALUE 65536 // 2^16 because 2 * 8 bit registers is used to store max CTC value
#define AVERAGE_ARRAY_SIZE 10

#define BUTTON_SWITCH_MODE 0
#define BUTTON_LEFT 1
#define BUTTON_RIGHT 3
#define BUTTON_CONFIRM 2

#define ALCOTEST_MODE 0
#define SWITCH_USER_MODE 1
#define RESULTS_MODE 2

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

uint8_t debounce_buttons_array[8];
uint16_t global_counter;

uint16_t average_array[AVERAGE_ARRAY_SIZE] = {0};
uint8_t average_array_counter = 0;

uint8_t is_calibrated = 0;
uint16_t last_sample = 0;
uint16_t max_value = 0;
uint16_t ref_value = 0;

uint8_t user_number = 0;
uint8_t user_pointer = 0;

uint8_t menu_active = 0;
uint8_t menu_pointer = 0;

char adcStr[128]; // string that outputs to LCD when function write_to_lcd() is called

int mode;




void delete_database(){
	// TODO
}

int calculate_average() {
	uint8_t j;
	uint16_t sum = 0;
	for (j = 0; j < AVERAGE_ARRAY_SIZE; ++j) {
		sum += average_array[j];
	}
	return sum/AVERAGE_ARRAY_SIZE;
}

void write_to_lcd(){
	lcd_clrscr();
	lcd_puts(adcStr);
}

void push_q(uint16_t val) {
	average_array_counter %= AVERAGE_ARRAY_SIZE;
	average_array[average_array_counter++] = val;
	last_sample = val;
	
	if (val > max_value) {
		max_value = val;
	}
}

void take_sensor_sample(){
	push_q(ADC);
}

ISR(TIMER1_COMPA_vect){	// global counter iteration
	// increment global counter
	if(global_counter + 1 > sizeof(uint16_t)){
		global_counter = 0;
		} else {
		global_counter++;
	}
	
	
	
	// if in alcotest mode, take sensor sample
	if(mode == ALCOTEST_MODE && global_counter % SAMPLE_RATE == 0){
		take_sensor_sample();
	}
	
	// debouncing
	uint8_t i;
	for(i = 0; i < 8; i++){
		if(bit_is_set(PINB, i)) { // ako gumb nije pritisnut
			if(debounce_buttons_array[i] > 0) debounce_buttons_array[i]--;
		}
	}
}

void setup_global_timer(uint16_t milliseconds){
	// global_timer = F_CPU / (2 * N * (1/DEBOUNCE_TIME)) - 1
	// N is lowest possible prescaler value so that debounce_timer is less than 2^16
	int tmp_debounce = F_CPU / (2 * (1/milliseconds));
	if(tmp_debounce - 1 < MAX_TIMER_VALUE){
		TCCR1B = _BV(WGM12);
		OCR1A = tmp_debounce - 1;
		} else if(tmp_debounce * 8 - 1 < MAX_TIMER_VALUE){
		TCCR1B = _BV(WGM12) | _BV(CS11);
		OCR1A = tmp_debounce * 8 - 1;
		} else if(tmp_debounce * 64 - 1 < MAX_TIMER_VALUE){
		TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);
		OCR1A = tmp_debounce * 64 - 1;
		} else if(tmp_debounce * 256 - 1 < MAX_TIMER_VALUE){
		TCCR1B = _BV(WGM12) | _BV(CS12);
		OCR1A = tmp_debounce * 256 - 1;
		} else {
		TCCR1B = _BV(WGM12) | _BV(CS10) | _BV(CS12);
		OCR1A = tmp_debounce * 1024 - 1;
	}
	TCCR1A = 0;
	TIMSK = _BV(OCIE1A);
}

void debounce_button(int button){
	debounce_buttons_array[button] = 3;
}

int is_button_pressed(int button){
	if(bit_is_clear(PINB, button) && debounce_buttons_array[button] == 0) {
		debounce_button(button);
		return 1;
	}
	return 0;
}

void initialize(){
	ref_value = eeprom_read_word((uint16_t*)0);
	user_number = eeprom_read_word((uint16_t*)16);
	global_counter = 0;
	mode = ALCOTEST_MODE;
	
	// buttons
	PORTB = _BV(0) | _BV(1) | _BV(2) | _BV(3);
	DDRB = 0;
	
	setup_global_timer(COUNTER_INCREMENT_TIME);
	sei();
}

void calibrate()
{
	while(global_counter < 300){
		uint8_t new_sample = ADC;
		uint8_t count = 0;
		uint8_t tmp_abs = new_sample - last_sample > 0 ? new_sample - last_sample : last_sample - new_sample;
		last_sample = new_sample;
		if (tmp_abs > 10) {
			snprintf(adcStr, 128, "Calibrating %hhu%c \nElapsed: %ds", count*20 '%', global_counter/10);
			write_to_lcd();
			} else {
			count++;
			if(count == 5) {
				is_calibrated = 1;
				break;
			}
		}
		_delay_ms(200);
	}
}



int main()
{
	initialize();
	calibrate();
	
	// calibrating
	
    /* Replace with your beautiful code */
    while (1) 
    {
		
		if(is_button_pressed(BUTTON_SWITCH_MODE)) {
			if(menu_active){
				menu_active = 0;
			} else {
				menu_active = 1;
			}
		}
		
		if(menu_active){
			if(is_button_pressed(BUTTON_CONFIRM)) {
				mode = menu_pointer;
				menu_active = 0;
			}
			
			if(is_button_pressed(BUTTON_LEFT)) {
				menu_pointer--;
				if(menu_pointer < 0) menu_pointer = 3;
			}
			
			if(is_button_pressed(BUTTON_RIGHT)) {
				menu_pointer++;
				if(menu_pointer > 3) menu_pointer = 0;
			}
			
			switch(menu_pointer){
				case 0:
					snprintf(adcStr, 128, "?Alcotest");
					break;
				case 1:
					snprintf(adcStr, 128, "?Change\nuser");
					break;
				case 2:
					snprintf(adcStr, 128, "?Results");
					break;
				case 3:
					snprintf(adcStr, 128, "?Delete\ndatabase");
					break;
			}
			write_to_lcd();
			continue;
		}
		
		
		
		switch(mode){
			case ALCOTEST_MODE:
				snprintf(adcStr, 128, "%d, avg:%d", last_sample, calculate_average());
				if(is_button_pressed(BUTTON_CONFIRM)) {
					/*save_result();
					mode = RESULTS_MODE;*/
				}
				break;
			case SWITCH_USER_MODE:
				snprintf(adcStr, 128, "TODO:\nusers");
				break;
			case RESULTS_MODE:
				snprintf(adcStr, 128, "TODO:\nresults");
				break;
		}
		write_to_lcd();
    }
}

