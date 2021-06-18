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
void gameInit(void);		// 게임 시작전 초기화
void backgroundMove(void);	// 배경 장애물 움직이기 위한 함수
void makeBlock(void);		// 일정 시간동안 랜덤한 장애물을 생성하는 함수
void copy(byte *arr1, byte *arr2); //arr1의 값을 arr2에 복사한다.
void EndCheck(void);		// 게임이 종료되었는지 판단하는 함수
void speedDelay(void);		// 게임 진행 속도를 결정하는 함수

// UI 표현 기능 함수
void printInitScreen(void);				// 초기화면 출력
void printDino(uint8_t jump);			// Dino 출력
void printBoss(uint8_t y);				// Boss Monster 출력
void printBirld(uint8_t x, uint8_t y);	// 장애물1 새 출력
void printCactus(uint8_t x);			// 장애물2 선인장 출력
void printDinoAttack(void);				// Dino의 필살기 출력
void printBossAttack(uint8_t x, uint8_t y);	// Boss Monster 공격 출력
void printPoint(void);					// 게임 진행 점수 및 시간 출력
void printEndPage(void);				// 게임 종료 시 표현할 페이지 출력

void UI_Update(uint8_t jump);			// 게임 진행 중 전체 화면 갱신 메소드
void blockRouter(uint8_t block, int index);	// Background에 동작하는 장애물들을 UI에 출력하는 메소드

//----------------------------------main---------------------------------------------------
int main(void){
	init_devices(); //cli(), Port_init(), Adc_init(), lcd_init(), sei(), inturrupt_init()
	printInitScreen();

	while(1) {
		while((PIND & 0x80) == 0x80); // 게임 시작 대기상태, PIND.7 입력이 들어올 경우 루프를 벗어나 게임을 시작한다.
		
		gameInit();					// 게임 초기화
		byte flag = 0x00;			// 연속하지 않은 장애물을 생성하기 위한 FLAG
		while(gameState == running || gameState == bosstime){// 게임 시작
			backgroundMove();		// 매 순간마다 배경이 움직인다.
			if(gameState != bosstime){	// boss time일 때 장애물은 나오지 않고 보스의 공격만 나온다.
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
	// 왼쪽으로 데이터를 한칸씩 민다.
	for(size_t y = 0; y< (y_ - 1); y++){
		backupBoard[y] = gameBoard[y+1];
	}
	memcpy(gameBoard, backupBoard, sizeof(backupBoard));
	memset(backupBoard, 0, sizeof(backupBoard));
}
void speedDelay(void){
	// speed가 올라갈 수록 delay텀이 짧아지면서 속도가 향상됨
	if(speed == 0)
	_delay_ms(300);
	else if(speed == 1)
	_delay_ms(150);
	else if(speed == 2)
	_delay_ms(100);
	else
	_delay_ms(50);
}
void EndCheck(void){
	byte code = gameBoard[23];
	switch(code){
		// 점프 안해야 할 때 점프할 경우 게임 종료
		case birld_H:
		case bossattack_H:
		if(jumpFlag != 0x00){
			gameState = lose;
			return;
		}
		break;
		// 점프 해야 할 때 점프를 하지 않을 경우 게임 종료
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
ISR(TIMER0_OVF_vect){	if(gameState == running || gameState == bosstime){		cnt++;		if(cnt==17){			times++;			point+=2;			printPoint();			cnt=0;						if(times == bosstimeSetting){ // boss 출격				gameState = bosstime;			}		}	}}
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
			backgroundMove();	// jump하는 경우에도 배경은 움직임
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
		if(gameState == bosstime){
			printBoss(0);
			if(!(--bosscounter)){
				// boss monster 퇴치
				 gameState = win;
				 point += 1000;
			}
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
	// End and point 출력
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
	EndCheck();	// 게임이 끝났는지를 확인
}