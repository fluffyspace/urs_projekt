/*
 * urs_projekt.c
 *
 * Created: March 2020.
 * Authors (alphabetically) : Ingo Kodba, Martin Luk, Anamaria Vargi?
 */ 
#define F_CPU 7372800UL

#define AVERAGE_ARRAY_SIZE 10
#define SINGLE_USER_MEMORY_SIZE 18
#define USER_NAME_SIZE 6
#define USER_SLOT_SIZE 1

#define BUTTON_SWITCH_MODE 0
#define BUTTON_LEFT 1
#define BUTTON_RIGHT 3
#define BUTTON_CONFIRM 2

#define ALCOTEST_MODE 0
#define SWITCH_USER_MODE 1
#define RESULTS_MODE 2
#define ADD_USER_MODE 3
#define CLEAR_DATABASE_MODE 4

#define R2 660 //otpor r2 otpornika u djelilu napona

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "lcd.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

volatile float ref_value = 0;
volatile int R0 = 1200; //veci broj = brze raste funkcija (unutarnji otpor senzora kad je cista arija)
volatile float ALCsample; 
uint16_t PROMILI;

uint8_t yes_no_button = 0;
char ime[USER_NAME_SIZE] = "";
uint8_t choose_name_pointer = 0;
char name_current_letter = ' ';

uint8_t debounce_buttons_array[4];

uint16_t average_array[AVERAGE_ARRAY_SIZE] = {0};
uint8_t average_array_counter = 0;

uint8_t is_calibrated = 0;
uint16_t last_sample = 0;
uint16_t max_value = 0;

uint8_t user_number = 0;

uint8_t number_of_users = 0;

uint8_t result_pointer = 0;
float result = 0.0f;

uint8_t menu_active = 0;
uint8_t menu_pointer = 0;

char Ustr[64];
char Dstr[64];
// strings that outputs to LCD when function write_to_lcd() is called

uint8_t mode;




void write_to_lcd(){
	char str[128];
	snprintf(str, 128, "%s\n%s", Ustr, Dstr);
	lcd_clrscr();
	lcd_puts(str);
}

void delete_database(){
	uint8_t i;
	uint8_t j;
	for(i = 0; i < 4; i++){ // for each user
		for(j = 0; j < USER_NAME_SIZE; j++){
			eeprom_write_byte(i*SINGLE_USER_MEMORY_SIZE+j, '\0'); // delete name
		}
		eeprom_write_byte(i*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE, 0); // delete slot pointer
		for(j = 0; j < 4; j++){
			eeprom_write_float(i*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE + USER_SLOT_SIZE + j*4, 0.0f);
		}
	}
	eeprom_write_byte(0, 'A');
	eeprom_write_byte(1, '\0');
	/*eeprom_write_byte(SINGLE_USER_MEMORY_SIZE, 'B');
	eeprom_write_byte(SINGLE_USER_MEMORY_SIZE*2, 'C');
	eeprom_write_byte(SINGLE_USER_MEMORY_SIZE*3, 'D');*/
	eeprom_write_byte(SINGLE_USER_MEMORY_SIZE*4, 1); // how many users are in the database? only one after initialization
	number_of_users = 1;
}

int calculate_average() {
	uint8_t j;
	uint16_t sum = 0;
	for (j = 0; j < AVERAGE_ARRAY_SIZE; ++j) {
		ADCSRA |= _BV(ADSC);
		while (!(ADCSRA & _BV(ADIF)));
		sum += ADC;
	}
	return sum/AVERAGE_ARRAY_SIZE;
}


void push_q(uint16_t val) {
	average_array_counter %= AVERAGE_ARRAY_SIZE;
	average_array[average_array_counter++] = val;
	last_sample = val;
	
	if (val > max_value) {
		max_value = val;
	}
}


void writeADC(float BAC)
{
	// BAC = 2.56 mgl = 25.6 u float
	float PROM = BAC * 2.09;
	snprintf(Dstr, 64, "%d.%d%dmg/L  %d.%d%d%c.", (int)BAC/10, (int)BAC%10, (int)(BAC*10)%10, (int)PROM/10, (int)PROM%10, (int)(PROM*10)%10, '%');
}

float ADCpretvorba()
{
	//ucitava stanje na senzoru
	int average = calculate_average();
	//pretvorba ADC u BAC
	float sens_volt = (float)average / 1024 * 5.0;
	float RS = ((5 * R2) / sens_volt) - R2;
	float omjer = RS / R0;
	double x = 0.3934 * omjer;
	float BAC = pow(x, -1.504) - ref_value;
	
	return BAC;
}


void debounce_buttons(){
	uint8_t i;
	for(i = 0; i < 4; i++){
		if(bit_is_set(PINB, i)) { // ako gumb nije pritisnut
			if(debounce_buttons_array[i] > 0) debounce_buttons_array[i] = debounce_buttons_array[i] - 1;
		}
	}
}

uint8_t is_button_pressed(int button){
	if(bit_is_clear(PINB, button) && debounce_buttons_array[button] == 0) {
		// make debounce increase to 30;
		debounce_buttons_array[button] = 1;
		return 1;
	}
	return 0;
}

void initialize(){
	//user_number = eeprom_read_word((uint16_t*)16);
	mode = ALCOTEST_MODE;
	menu_active = 1;
	
	// buttons
	PORTB = _BV(0) | _BV(1) | _BV(2) | _BV(3);
	DDRB = 0;	
	
	DDRA |=_BV(1);
	PORTA &= ~_BV(1);
	
	number_of_users = eeprom_read_byte(4*SINGLE_USER_MEMORY_SIZE);
	if(number_of_users < 1 || number_of_users > 4){
		delete_database();
	}
	read_user_name();
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
	//ukljuci ADC i prescaler od 64
	ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1);
}


void calibrate()
{
	uint8_t count = 0;
	while(!is_calibrated){
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
				R0 *= 1 + ref_value/420; //veci broj = brze raste funkcija
				snprintf(Dstr, 64, "");
			}
		}
		snprintf(Ustr, 64, "Calibrating:%hhu%c", count*5, '%');
		write_to_lcd();
		_delay_ms(50); //ako je vec dovoljno zagrijan senzor kalibracija ce bit gotova u jednu sekundu
	}
}

void save_result(float BAC){
	float max_tmp;
	max_tmp = eeprom_read_float((uint8_t)user_number*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE + USER_SLOT_SIZE + 12);
	if(max_tmp < BAC){
		eeprom_write_float((uint8_t)user_number*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE + USER_SLOT_SIZE + 12, BAC);
	}
	
	uint8_t save_offest = eeprom_read_byte((uint8_t)user_number*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE);
	eeprom_write_float((uint8_t) user_number*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE + USER_SLOT_SIZE + 4*save_offest, BAC);
	if(save_offest >= 3){
		save_offest = 0;
	} else {
		save_offest++;
	}
	eeprom_write_byte(user_number*SINGLE_USER_MEMORY_SIZE+USER_NAME_SIZE, save_offest);
}

void read_user_name(){
	eeprom_read_block(ime, user_number*SINGLE_USER_MEMORY_SIZE, USER_NAME_SIZE);
}

void get_user_result(){
	result = eeprom_read_float(user_number*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE + USER_SLOT_SIZE + result_pointer*4);
}

float average_result() {
	uint8_t j;
	float sum = 0.0f;
	for (j = 0; j < 3; ++j) {
		sum += eeprom_read_float(user_number*SINGLE_USER_MEMORY_SIZE+USER_NAME_SIZE+USER_SLOT_SIZE+j*4);
	}
	return sum/(float)3;
}

void add_new_user_update_screen(){
	snprintf(Ustr, 128, "User:%s", ime);
	write_to_lcd();
	lcd_gotoxy(5+choose_name_pointer, 0);
	lcd_putc(name_current_letter);
	lcd_gotoxy(5+choose_name_pointer, 0);
}

void add_new_user(){
	if(number_of_users >= 4) return;
	uint8_t j;
	for (j = 0; j < USER_NAME_SIZE-1; ++j) {
		eeprom_write_byte(number_of_users*SINGLE_USER_MEMORY_SIZE + j, ime[j]);
	}
	//eeprom_write_block(ime, number_of_users*SINGLE_USER_MEMORY_SIZE, USER_NAME_SIZE-1); // write name
	//eeprom_write_byte(number_of_users*SINGLE_USER_MEMORY_SIZE + USER_NAME_SIZE - 1, '\0'); // write name
	number_of_users++;
	eeprom_write_byte(SINGLE_USER_MEMORY_SIZE*4, number_of_users); // how many users are in the database?
}

int main()
{
	initialize();
	LCDinit();
	ADCinit();
	calibrate();
	
	// calibrating
	
    /* Replace with your beautiful code */
    while (1) 
    {
		debounce_buttons();

		if(is_button_pressed(BUTTON_SWITCH_MODE)) {
			if(menu_active){
				//menu_active = 0;
			} else {
				menu_active = 1;
			}
		}
		
		if(menu_active){
			if(mode == ADD_USER_MODE){
				lcd_command(LCD_DISP_ON);
			}
			
			if(is_button_pressed(BUTTON_CONFIRM)) {
				// odabrano nesto iz menu-a
				if(menu_pointer == ADD_USER_MODE){
					if(number_of_users > 4) continue;
					snprintf(ime, USER_NAME_SIZE, ""); // ocisti varijablu ime
					snprintf(Dstr, 128, "");
					lcd_command(LCD_DISP_ON_CURSOR_BLINK); // napravi da cursor blinka
					choose_name_pointer = 0;
					name_current_letter = 'A';
					add_new_user_update_screen();
				} else if(menu_pointer == RESULTS_MODE){
					result_pointer = 0;
					get_user_result();
				} else if(menu_pointer == SWITCH_USER_MODE){
					read_user_name();
				}
				mode = menu_pointer;
				menu_active = 0;
				//snprintf(Ustr, 128, "User:%s", ime);
				
			}
			
			if(is_button_pressed(BUTTON_LEFT)) {
				if(menu_pointer == 0){
					menu_pointer = 4;
				} else {
					menu_pointer--;
				}
				
			}
			
			if(is_button_pressed(BUTTON_RIGHT)) {
				menu_pointer++;
				if(menu_pointer > 4) menu_pointer = 0;
			}
			
			
			switch(menu_pointer){
				case ALCOTEST_MODE:
					snprintf(Ustr, 64, "?Alcotest");
					break;
				case SWITCH_USER_MODE:
					snprintf(Ustr, 64, "?Switch\nuser");
					break;
				case RESULTS_MODE:
					snprintf(Ustr, 64, "?Results");
					break;
				case ADD_USER_MODE:
					snprintf(Ustr, 64, "?Add new\nusers");
					break;
				case CLEAR_DATABASE_MODE:
					snprintf(Ustr, 64, "?Clear\ndatabase");
					break;
			}
			snprintf(Dstr, 64, "");
			write_to_lcd();
			continue;
		}
		
		
		
		switch(mode){
			case ALCOTEST_MODE:
			{
				ALCsample = ADCpretvorba();
				snprintf(Ustr, 128, "User:%s", ime);
				writeADC(ALCsample);
				if(ALCsample > 2.5) PORTA |= _BV(1);
				else PORTA &= ~_BV(1);
				_delay_ms(150);
				if(is_button_pressed(BUTTON_CONFIRM)) {
					save_result(ALCsample);
					mode = RESULTS_MODE;
				}
				write_to_lcd();
				break;
			}
			case SWITCH_USER_MODE:
			
				if(is_button_pressed(BUTTON_CONFIRM)) {
					mode = ALCOTEST_MODE;
					menu_active = 1;
				}
				
				if(is_button_pressed(BUTTON_LEFT)) {
					if(user_number == 0){
						user_number = number_of_users-1;
						} else {
						user_number--;
					}
					read_user_name();
				}
				
				if(is_button_pressed(BUTTON_RIGHT)) {
					user_number++;
					if(user_number > number_of_users-1) user_number = 0;
					read_user_name();
				}

				snprintf(Ustr, 128, "User:%s", ime);
				snprintf(Dstr, 128, "ID:%d of %d", user_number+1, number_of_users);
				write_to_lcd();
				break;
			case RESULTS_MODE:
				get_user_result();
				if(is_button_pressed(BUTTON_LEFT)) {
					if(result_pointer == 0){
						result_pointer = 4;
						} else {
						result_pointer--;
					}
					get_user_result();
				}
				
				if(is_button_pressed(BUTTON_RIGHT)) {
					if(result_pointer >= 4){
						result_pointer = 0;
						} else {
						result_pointer++;
					}
					get_user_result();
				}
				
				if(result_pointer < 3) {
					snprintf(Ustr, 128, "User:%s  %d.", ime, result_pointer+1);
					writeADC(result);
				} else if(result_pointer == 3) {
					snprintf(Ustr, 128, "User:%s  MAX:", ime);
					writeADC(result);
				} else if(result_pointer == 4) {
					snprintf(Ustr, 128, "User:%s  AVG:", ime);
					writeADC(average_result());					
				}
				write_to_lcd();
				break;
			case ADD_USER_MODE:
				add_new_user_update_screen();
			
				if(is_button_pressed(BUTTON_LEFT)) {
					if(name_current_letter == 65){
						name_current_letter = 32;
					} else if(name_current_letter == 32){
						name_current_letter = 90;
					} else {
						name_current_letter--;
				}
				add_new_user_update_screen();
				}
				
				if(is_button_pressed(BUTTON_RIGHT)) {
					if(name_current_letter >= 90){
						name_current_letter = 32;
					} else if(name_current_letter == 32){
						name_current_letter = 65;
					} else {
						name_current_letter++;
					}
					add_new_user_update_screen();
				}
				
				if(is_button_pressed(BUTTON_CONFIRM)) {
					if(name_current_letter == 32){
						// save ime to eeprom as new user
						add_new_user();
						mode = ALCOTEST_MODE;
						menu_active = 1;
						lcd_command(LCD_DISP_ON);
					} else {
						snprintf(ime, USER_NAME_SIZE, "%s%c\0", ime, name_current_letter);
						name_current_letter = 32;
						choose_name_pointer++;
					}
					add_new_user_update_screen();
				}
				break;
			case CLEAR_DATABASE_MODE:
				snprintf(Ustr, 128, "Clear db?");
				if(is_button_pressed(BUTTON_LEFT) || is_button_pressed(BUTTON_RIGHT)) {
					if(!yes_no_button){
						yes_no_button = 1;
					} else {
						yes_no_button = 0;
					}
				}
				
				if(!yes_no_button){
					snprintf(Dstr, 128, "No");
				} else {
					snprintf(Dstr, 128, "Yes");
					if(is_button_pressed(BUTTON_CONFIRM)) {
						snprintf(Ustr, 128, "Clearing db...");
						snprintf(Dstr, 128, "Wait...");
						write_to_lcd();
						delete_database();
						mode = ALCOTEST_MODE;
						menu_active = 1;
					}
				}
				write_to_lcd();
				break;
		}
		_delay_ms(50);
    }
}