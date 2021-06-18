#include <avr/io.h>
#include <string.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>

#include "_main.h"
#include "_glcd2.h"
#include "_glcd.h"
#include "_bmp_hex.h"

#define F_CPU 14745600UL
#define usePoint 100
#define bosstimeSetting 200
#define EndTime 180
#define EndPoint 360

enum gameStates{ waiting = 0, running = 1, win = 2, lose = 3, bosstime = 4 };
enum characterCodes{ birld_L = 1, birld_H = 2, cactus_ = 3, dino_ = 4, bossattack_L = 5,bossattack_M = 6,bossattack_H = 7,};
enum boardSize{x_ = 51, y_ = 140};

// game background data
byte gameBoard[y_] = {0,};
byte backupBoard[y_] = {0,};

uint8_t gameState = waiting;
unsigned int point = 0;
unsigned int times = 0;
unsigned int speed = 2;
unsigned char cnt;
byte bosscounter = 3;
byte jumpFlag = 0x00;

// init 함수
void Init_Timer0(void);		//timer 초기화 변수
void Port_init(void);		//포트 초기화
void Interrupt_init(void);	//인터럽트 초기화
void init_devices(void);	//디바이스 초기화

// game 기능 함수
void gameInit(void);
void backgroundMove(void);
void makeBlock(void);
void copy(byte *arr1, byte *arr2); //arr1의 값을 arr2에 복사한다.
void EndCheck(void);
void speedDelay(void);

// UI 표현 기능 함수
void printInitScreen(void);
void printDino(uint8_t jump);
void printBoss(uint8_t y);
void printBirld(uint8_t x, uint8_t y);
void printCactus(uint8_t x);
void printDinoAttack(void);
void printBossAttack(uint8_t x, uint8_t y);
void printPoint(void);
void printEndPage(void);

void UI_Update(uint8_t jump);
void blockRouter(uint8_t block, int index);

//----------------------------------main---------------------------------------------------
int main(void){
	init_devices(); //cli(), Port_init(), Adc_init(), lcd_init(), sei(), inturrupt_init()
	printInitScreen();

	while(1) {
		while((PIND & 0x80) == 0x80); // 게임 시작 대기상태
		
		gameInit();
		byte flag = 0x00;
		while(gameState == running || gameState == bosstime){
			backgroundMove();
			if(gameState != bosstime){
				if((flag / 44)){
					makeBlock();	// 연속해서 block이 생성되지 않게 함 연속일 경우 피할 수 없이 종료됨
					flag /= 44;
				}
				flag++;
			}
			else{
				printBoss(0);
				if((flag / 44)){
					gameBoard[y_-4] = (rand() % 3) + 5;	// 연속해서 block이 생성되지 않게 함 연속일 경우 피할 수 없이 종료됨
					flag /= 44;
				}
				flag++;
			}
			UI_Update(0);
			speedDelay();
			
			if(times == 50)
				speed = 1;
			else if(times == 100)
				speed = 2;
			else if(times == 200)
				speed = 3;
			
		}
		
		// End
		printEndPage();
	}
	
}
//------------------------------------------------------------------------------------------
void makeBlock(void){
	// 0~3까지의 random한 characterCode를 고른다.
	// 0일 경우 장애물 생성하지 않음
	gameBoard[y_-1] = rand() % 4;
}
void backgroundMove(void){
	for(size_t y = 0; y< (y_ - 1); y++){
		backupBoard[y] = gameBoard[y+1];
	}
	memcpy(gameBoard, backupBoard, sizeof(backupBoard));
	memset(backupBoard, 0, sizeof(backupBoard));
}
void speedDelay(void){
	// speed가 올라갈 수록 delay텀이 짧아지면서 속도가 향상됨
	if(speed == 0)
	_delay_ms(400);
	else if(speed == 1)
	_delay_ms(200);
	else if(speed == 2)
	_delay_ms(100);
	else
	_delay_ms(50);
}
void EndCheck(void){
	byte code = gameBoard[23];
	switch(code){
		// 점프 안해야 할 때
		case birld_H:
		case bossattack_H:
		if(jumpFlag != 0x00){
			gameState = lose;
			return;
		}
		break;
		// 점프 해야 할 때
		case birld_L:
		case cactus_:
		case bossattack_L:
		case bossattack_M:
		if(jumpFlag != 0xFF){
			gameState = lose;
			return;
		}
		break;
		default:
		if (times == EndTime && point == EndPoint){
			gameState = win;
			point += 500;
		}
	}
}

//timer 초기화 변수
void Init_Timer0(void){
	// 인터럽트 발생주기   = 1/(14.7456M)  * 256 * 1024(분주)= 17.8 ms	// 17.8ms * 56 = 1 sec
	TCCR0=0x07;		//00000111  , normal mode, 1024 분주
	TCNT0=0;
	TIMSK=0x01;
}
ISR(TIMER0_OVF_vect){	if(gameState == running || gameState == bosstime){		cnt++;		if(cnt==45){			times++;			point+=2;			printPoint();			cnt=0;						if(times == bosstimeSetting){				gameState = bosstime;			}		}	}}
//포트 초기화
void Port_init(void)
{
	PORTA=0x00; DDRA=0xFF;
	PORTB=0xFF; DDRB=0xFF;
	PORTC=0x00; DDRC=0xF0;
	PORTD=0x00; DDRD=0x00;
	PORTE=0x00; DDRE=0xFF;
	PORTF=0x00; DDRF=0x00;
}

//인터럽트 초기화
void Interrupt_init(void){
	EIMSK = 0x07;  //0번, 1번
	EICRA = 0x2A;  //falling trigger
	SREG |= 0x80;
}

//디바이스 초기화
void init_devices(void)
{
	cli();  //모든 인터럽트 금지
	Port_init();
	lcd_init();
	Init_Timer0();
	Interrupt_init();
	sei(); //모든 인터럽트 허가

}

// 외부 인터럽트 0 jump 기능
SIGNAL(INT0_vect)
{
	if(gameState == running || gameState == bosstime){
		jumpFlag = ~jumpFlag;	//jump flag on
		for(byte i = 0; i< 23 ; i++){
			backgroundMove();
			UI_Update(i);
			speedDelay();
		}
		UI_Update(23);
		speedDelay();
		for(byte i = 22; i > 0 ;i--){
			backgroundMove();
			UI_Update(i);
			speedDelay();
		}
		jumpFlag = ~jumpFlag;	// jump flag off
	}
}

// 필살기 사용
SIGNAL(INT1_vect)
{
	if(gameState == running || gameState == bosstime){
		if(point < usePoint)
		return;
		point -= usePoint;
		printPoint();
		printDinoAttack();
		_delay_ms(30);
		memset(gameBoard, 0, sizeof(gameBoard));
		lcd_clear();
		UI_Update(0);
		if(gameState == bosstime)
		if(!(--bosscounter)){
			 gameState = win;
			 point += 1000;
		}
	}
}

// 게임 강제 종료
SIGNAL(INT2_vect)
{
	if(gameState == running || gameState == bosstime){
		gameState = lose;
	}
}

void printInitScreen(void){
	lcd_clear();
	lcd_string(1, 0,"Hello, Travler Dino");
	lcd_string(2, 0, "====================");
	lcd_string(3, 10, "Press PD7");
	GLCD_Line(33, 62, 33, 123);
	printDino(0);
	printCactus(70);
	GLCD_Line(60, 0, 60, 140);
}

void gameInit(){
	
	cnt = 0;
	times = 0;
	point = 0;
	bosscounter = 3;
	jumpFlag = 0x00;
	
	memset(gameBoard, 0, sizeof(gameBoard));
	
	lcd_clear();
	ScreenBuffer_clear();
	printPoint();
	printDino(0);
	GLCD_Line(60, 0, 60, 140);
	
	gameState = running;
}
void printPoint(void){
	lcd_string(0, 0, "Time:");
	GLCD_4DigitDecimal(times);
	lcd_string(0, 10, "Point:");
	GLCD_4DigitDecimal(point);
	lcd_string(1, 0, "====================");
}
void printEndPage(void){
	// You need point calculate
	lcd_clear();
	if(gameState == win){
		lcd_string(1, 1, "Congratulation");
		lcd_string(2, 4, "You Win!");
		lcd_string(3, 0, "====================");
	}
	else{
		lcd_string(2, 4, "You lose...");
		lcd_string(3, 0, "====================");
	}
	lcd_string(4, 2, "Your Point:");
	GLCD_4DigitDecimal(point);
	lcd_string(7, 4, "New Game: PD7");
	gameState = waiting;
}
// 참고 glcd_draw_bitmap( bmp, y좌표, x좌표, x사이즈, y사이즈);
void printCactus(uint8_t x){
	//glcd_draw_bitmap(cactus,40, (130-x), 27, 16);
	glcd_draw_bitmap(cactus,40, x, 27, 16);
}
void printBirld(uint8_t x, uint8_t y){
	uint8_t updown[] = {0, 22};
	//glcd_draw_bitmap(birld2,(36-updown[1]), x, 20, 24);
	glcd_draw_bitmap(birld,(36-updown[y]), x, 20,24);
}
void printDino(uint8_t jump){
	glcd_draw_bitmap(dino,(38-jump),0, 26,24);
}
void printBoss(uint8_t y){
	uint8_t yy[] = {27, 16, 38};
	glcd_draw_bitmap(boss, yy[y], 90, 30, 40);
}
void printDinoAttack(void){
	glcd_draw_bitmap(attack, 32, 25, 32, 16);
	
	uint8_t length = 40;
	for(int k = 0; k < 80; k++){
		GLCD_Line(30, length, 70, length);
		length++;
		_delay_ms(130);
	}
}
void printBossAttack(uint8_t x, uint8_t y){
	uint8_t updown[] = {11, 24, 0};
	//GLCD_Circle(55-updown[y], (88-x), 5);
	GLCD_Circle(55-updown[y], x, 5);
}

void blockRouter(uint8_t block, int index){
	switch(block){
		case birld_L:
		printBirld(index, 0);
		break;
		case birld_H:
		printBirld(index, 1);
		break;
		case cactus_:
		printCactus(index);
		break;
		case bossattack_L:
		printBossAttack(index, 0);
		break;
		case bossattack_M:
		printBossAttack(index, 1);
		break;
		case bossattack_H:
		printBossAttack(index, 2);
		break;
		default:
		break;
	}
}
void UI_Update(uint8_t jump){
	lcd_clear();
	_delay_ms(10);
	printPoint();
	GLCD_Line(60, 0, 60, 140);
	printDino(jump);
	uint8_t pre = 0;
	for(int i = 0; i<y_;i++){
		if(0 == gameBoard[i])
		continue;
		pre = gameBoard[i];
		blockRouter(pre, i);
	}
	EndCheck();
}