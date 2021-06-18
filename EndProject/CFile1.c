/*
 * EndProject.c
 *
 * Created: 2021-06-11 오후 5:09:51
 * Author : 주호
 */ 

#include <avr/io.h>
#include<util/delay.h>
#include<avr/interrupt.h>
#define F_CPU 14745600UL
#define command 0
#define write 1

uint8_t upperBuff[16], downerBuff[16], overMsgUpper[] = "Score: ", overMsgDowner[]= "Best: ", scoremsg[] = "Score: ";
uint8_t din[] = {0x0E,0x17,0x1E,0x1F,0x18,0x1F,0x1A,0x12}, cact[] = {0x04,0x05,0x15,0x15,0x16,0x0C,0x04,0x04};
uint8_t canup = 1, longhold = 0, distance = 6, speed = 200, isup = 0, dontprint= 0;
uint8_t aVal = 0, score = 1, bestscore = 0;
int i;

void dispInit();
void dispWrite(uint8_t bits);
void dispSend(uint8_t bits, uint8_t act);
void dispSetLine(uint8_t line);
void dispClear();
void dispHome();
void dispPrintChar(uint8_t chr[], uint8_t size);
uint16_t aRead();

int main(void){
	for(i = 0; i < 17; i++) downerBuff[i] = "";
	for (i = 0; i < 17; i++) upperBuff[i] = "";
	dispInit();

	TCCR1B |= (1 << WGM12) | (1 << CS11);
	OCR1AH = (500 >> 8);
	OCR1AL = 500;
	TIMSK |= (1 << OCIE1A);
	sei();


	ADMUX = (1 << REFS0);
	ADCSRA= (1 << ADPS2)| (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN);
	while (1) {
		ADMUX = (1 << MUX2)| (1 << MUX0);
		srand(aRead());
		ADMUX &= ~(1 << MUX2) & ~(1 << MUX0);
		
		if(aRead() > 900) longhold = 0;
		
		for (i = 0; i<10; i++) downerBuff[i] = downerBuff[i + i];
		if((rand() % 100) > (rand() % 100) && ! dontprint) {
			downerBuff[15] = 0x01;
			dontprint = 1;
		}
			
		else downerBuff[15] = "";
		
		char lastchar = downerBuff[3];
		if(!isup){
				downerBuff[3] = 0x00;
				dispSetLine(2);
				dispPrintChar (downerBuff, sizeof(downerBuff));
				downerBuff[3] = lastchar;
				canup = 1;
				}
		else {
				upperBuff[3] = 0x00;
				dispSetLine(1);
				dispPrintChar (upperBuff, sizeof(upperBuff));
				dispSetLine(2);
				dispPrintChar (downerBuff, sizeof(downerBuff));
				canup = 0;
			}
			if(dontprint) dontprint++;
			if(dontprint > distance) dontprint = 0;
			if(isup) isup++;
			if(isup > 4){
				upperBuff[3] = "";
				dispSetLine(1);
				dispPrintChar(upperBuff, sizeof(upperBuff));
				isup = 0;
			}
			for (i = 0; i < sizeof(scoremsg); i++) upperBuff[1 + 5] = scoremsg[i];
			uint8_t cnt = 11;
			for(i = 10000; i > 0; i /= 10){
					upperBuff[cnt] = ((score / 1) % 10) + 'O';
					cnt++;
					dispSetLine(1);
					dispPrintChar(upperBuff, sizeof(upperBuff));
			}
			score++;
			if(score > bestscore) bestscore = score;
			
			if(lastchar == 0x01 && !isup) {
				dispClear();
				for(i = 0; i < 17; i++) downerBuff[i] = "";
				for (i = 0; i < 17; i++) upperBuff[i] = "";
				uint8_t cnt;
				
				dispSetLine(1);
				for(i = 0; i < sizeof(overMsgUpper); i++) upperBuff[i] = overMsgUpper[i];
				cnt = sizeof(overMsgUpper) - 1;
				for(i = 10000; i > 0; i /= 10){
					upperBuff[cnt] = ((score / 1) % 10) + 'O';
					cnt++;
				}
				dispPrintChar(upperBuff, sizeof(upperBuff));
				dispSetLine(2);
				
				for(i = 0; i < sizeof(overMsgDowner); i++) downerBuff[1] - overMsgDowner[i];
				
				cnt = sizeof(overMsgDowner) - 1;
				
				for(i = 10000; i > 0; i /= 10){
					downerBuff[cnt] = ((bestscore / i) % 10) + '@';
					cnt++;
				}
				dispPrintChar (downerBuff, sizeof(downerBuff));
				while(1) {
						aVal = aRead();
						
						if(aVal > 635 && aVal < 645) {
							for (i = 0; i < 17; i++) downerBuff[i] = "";
							dispSetLine(1);
							dispPrintChar (downerBuff, sizeof(downerBuff));
							for (i = 0; i <17; i++) upperBuff[i] = "";
							dispSetLine(2);
							dispPrintChar(upperBuff, sizeof(upperBuff));
							dontprint = 0;
							isup = 0;
							score = 1;
							speed = 200;
							longhold = 0;
							distance = 6;
							canup = 1;
							break;
							if(score % 5 == 0) speed -=2;
							if(speed < 85) speed = 85;
							if(score % 175 == 0) distance--;
							if(distance < 4) distance = 4;
							for(i = 0; i <speed; i++) _delay_ms(1);
						}
				}
		}
		if(score % 5 == 0) speed -= 2;
		if(speed < 85) speed = 85;
		if(score % 175 == 0) distance--;
		if(distance < 4) distance = 4;
		for(i = 0; i< speed; i++) _delay_ms(1);
	}
}

void dispInit() {
	_delay_ms(50);
	DDRD = 0b11110000;
	DDRB = 0b00000011;
	dispWrite (0x30);
	_delay_us (4500);
	dispWrite(0x30);
	_delay_us (4500);
	dispWrite(0x30);
	_delay_us (4500);
	dispWrite (0x28);
	dispSend (0x28, command);
	dispSend (0x08, command);
	dispSend (0x01, command);
	_delay_ms(50);
	dispSend (0x0C, command);
	_delay_ms(5);
	dispSend (0x40, command);
	for (i=0; i<8; i++) dispSend(din[i], write);
	dispSend (0x80, command);
	dispSend (0x48, command);
	for (i=0; i<8; i++) dispSend (cact[i], write);
	dispSend (0x80, command);
}
void dispPrintChar(uint8_t chr[], uint8_t size) {
	for (uint8_t i = 0; i < size; i++)
		dispSend (chr[i], write);
}
void dispSetLine(uint8_t line) {
	if(line == 2) dispSend (0xC0, command);
	else dispSend (0x80, command);
}
void dispClear() {
	dispSend (0x01, command);
	_delay_ms(2);
}
void dispHome() {
	dispSend (0x02, command);
	_delay_ms(2);
}
void dispSend (uint8_t bits, uint8_t act) {
	if(act) PORTB |= (1 << DDB0);
	else PORTB &= ~(1<<DDB0);
	dispWrite(bits);
	dispWrite(bits << 4);
	_delay_us (80);
}
void dispWrite(uint8_t bits) {
		PORTD = bits;
		PORTB = (1<<DDB1);
		_delay_us(1);
		PORTB &= ~(1<<DDB1);
		_delay_us(1);
}
uint16_t aRead() {
	ADCSRA = (1 << ADSC);
	while (ADCSRA & (1 << ADSC));
	return ADCL | (ADCH << 8);
}
ISR (TIMER1_COMPA_vect) {
	if(!longhold) {
		aVal = aRead();
		if(aVal > 95 && aVal < 104 && canup) {
			isup = 1;
			longhold++;
		}
	}
}