#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <softPwm.h> 
#include <lcd.h>

/*GPIO �� ����
�Ƴ��α� �� ��ȣ ����
***ADC ��� �� ���� //5,7,9,10,11***
TEXT LCD //RS => 6, E => 4, D4 => 20, D5 => 21, D6 => 12, D7 => 16
STEP MOTOR // A => 13, A_ => 17, B => 22, B_ => 27
FAN //		EN => 25, P => 19, N => 26

LED //		LED => 24
�������� // LED => 18	ADCä�� => 3
���μ��� //				ADCä�� => 4
�������� //				ADCä�� => 0
*/

#define CS_MCP3208 8
#define SPI_CHANNEL 0
#define SPI_SPEED 1000000

//TLCD
#define PIN_TEXTLCD_RS 6
#define PIN_TEXTLCD_E 4
#define PIN_TEXTLCD_D6 12
#define PIN_TEXTLCD_D7 16
#define PIN_TEXTLCD_D4 20
#define PIN_TEXTLCD_D5 21

//���ܸ���
#define PIN_STEP_A 13
#define PIN_STEP_A_ 17
#define PIN_STEP_B 22
#define PIN_STEP_B_ 27


//LED
#define PIN_LED 24

//�Ƴ��α� -> ���� ���� ���
#define LED_OFF -1
#define LED_ON 1
#define WIPER_OFF -1
#define WIPER_ON 1
#define FAN_OFF -1
#define FAN_ON 1

//FAN
#define PIN_FAN_EN 25	//pwm
#define PIN_FAN_P 19
#define PIN_FAN_N 26

//�������� LED
#define PIN_DUST_LED 18      

//STEP���� ���� �Լ�
void setsteps(int w1, int w2, int w3, int w4)
{
	digitalWrite(PIN_STEP_A, w1);
	digitalWrite(PIN_STEP_A_, w2);
	digitalWrite(PIN_STEP_B, w3);
	digitalWrite(PIN_STEP_B_, w4);
}

//ADC �Ƴ��αװ� �о����
int ReadMcp3208ADC(unsigned char adcChannel)
{
	unsigned char buff[3];   
	int nAdcValue = 0;

	buff[0] = 0x06 | ((adcChannel & 0x07) >> 2);  
	buff[1] = ((adcChannel & 0x07) << 6);   
	buff[2] = 0x00;

	digitalWrite(CS_MCP3208, 0);   

	wiringPiSPIDataRW(SPI_CHANNEL, buff, 3);
	buff[1] = 0x0F & buff[1];
	nAdcValue = (buff[1] << 8) | buff[2];
	digitalWrite(CS_MCP3208, 1);

	return nAdcValue;
}

//�� �ʱ�ȭ �Լ�
void Init_A(void) {
	pinMode(CS_MCP3208, OUTPUT);
	pinMode(PIN_STEP_A, OUTPUT);
	pinMode(PIN_STEP_A_, OUTPUT);
	pinMode(PIN_STEP_B, OUTPUT);
	pinMode(PIN_STEP_B_, OUTPUT);
	pinMode(PIN_LED, OUTPUT);
	digitalWrite(PIN_LED, LOW);
	pinMode(PIN_FAN_P, OUTPUT);
	pinMode(PIN_FAN_N, OUTPUT);
	softPwmCreate(PIN_FAN_EN, 0, 100);
	softPwmWrite(PIN_FAN_EN, 100);
	pinMode(PIN_DUST_LED, OUTPUT);
	FanOff();
}

float CheckDust(int ndustChannel);		//���� ���� �Լ�
void FanOn(void);						//FanOn �Լ�
void FanOff(void);						//FanOff �Լ�
void WiperOn(int rain);					//WiperOn �Լ�
void WiperOff(void);					//WiperOff �Լ�

//LCD ���� �Լ�
void lcd_FAN(int disp1, int cmd, float dust);
void lcd_Wiper(int disp1, int cmd);
void lcd_Light(int disp1, int cmd);
void lcd_Main(int disp1, int led, int wiper, int fan, float dust);

int main(void)
{	
	//����
	int nCdsChannel = 0;  //ä��
	int nCdsValue = 0;  //��
	//����
	int nRainChannel = 4;
	int nRainValue = 0;
	//����
	int ndustChannel = 3;
	int ndustValue = 0; 

	if (wiringPiSetupGpio() == -1) {
		fprintf(stdout, "Not start wiringPi: %s\n",
			strerror(errno));
		return 1;
	}
	if (wiringPiSPISetup(SPI_CHANNEL, SPI_SPEED) == -1) {
		fprintf(stdout, "wiringPiSPISetup Failed: %s\n", strerror(errno));
		return 1;
	}

	Init_A(); //�� �ʱ�ȭ �Լ�

	//LCD �ʱ�ȭ
	int disp1;
	disp1 = lcdInit(2, 16, 4, PIN_TEXTLCD_RS, PIN_TEXTLCD_E, PIN_TEXTLCD_D4, PIN_TEXTLCD_D5, PIN_TEXTLCD_D6, PIN_TEXTLCD_D7, 0, 0, 0, 0);

	int wiper = WIPER_OFF;	//������ ���� ���� -1 => �������� / 1 => ��������
	int fan = FAN_OFF;		//FAN ���� ���� -1 => �������� / 1 => ��������
	int led = LED_OFF;		//LED ���� ���� -1 => �������� / 1 => ��������
	int rain = -1;			//0 => �� ��ħ, 1 => 1�ܰ�, 2 => 2�ܰ�, 3 => 3�ܰ�
	   	  
	while (1)
	{
		//�Ƴ��α� ���� �� �о����
		ndustValue = CheckDust(ndustChannel);
		nRainValue = ReadMcp3208ADC(nRainChannel); 
		nCdsValue = ReadMcp3208ADC(nCdsChannel);  

		//����Ʈ ������ Ȯ���ϱ�
		/****���� printf �� CheckDust() ���ο��� ���****/
		printf("Rain Sensor Value = % u\n", nRainValue);
		printf("Cds Sensor Value = % u\n", nCdsValue);

		//���� ���� �� �б�
		if (ndustValue > 100) {  //�̼����� 100 �ʰ� ����
			FanOn();
			if (fan == FAN_OFF) {	//ó�� �ѹ��� ����
				printf("Fan On!!!!\n");
				lcd_FAN(disp1, FAN_ON, ndustValue);
			}
			fan = FAN_ON;		//���� 100�̻��̾ ��¹� ����X
		}
		else if(fan==FAN_ON){	//�̼����� 100���� && FAN_ON ���� 
			FanOff();			//FanOff
			printf("Fan OFF....\n");
			lcd_FAN(disp1, FAN_OFF, ndustValue);		//���� �� ����� ���� ndustValue ���ڷ� �Ѱ���
			fan = FAN_OFF;		//FAN_OFF�� ���� ���� ����
		}
		
		//����
		if (nCdsValue <= 500 && led == LED_OFF) //LED�� �����ְ� ���� ���� ����
		{
			digitalWrite(PIN_LED, HIGH);
			lcd_Light(disp1, LED_ON);
			led = LED_ON;
		}
		else if (nCdsValue > 500 && led == LED_ON) //LED�� �����ְ� ���� ���� ����
		{
			digitalWrite(PIN_LED, LOW);
			lcd_Light(disp1, LED_OFF);
			led = LED_OFF;
		}

		//����
		if (nRainValue < 3500) { //�񰡿��� �⺻�� 4096 => �� ���� �� �� ����
			wiper = WIPER_ON;	//WIPER_ON ���� ����
			if (nRainValue < 3500 && nRainValue > 2500) //2500~3500
			{
				rain = 1;
				printf("������ 1�ܰ�!\n");
			}
			else if (nRainValue < 2500 && nRainValue > 2000) //2000~2500
			{
				rain = 2;
				printf("������ 2�ܰ�!!\n");
			}
			else if (nRainValue < 2000 && nRainValue >0) //0~2000
			{
				rain = 3;
				printf("������ 3�ܰ�!!!\n");
			}
			WiperOn(rain);		//rain���� ������ ���� (���ܸ��� ������ ����)
			lcd_Wiper(disp1, rain);
		}
		else if (wiper == WIPER_ON) {	//��X && WIPER_ON����
				wiper = WIPER_OFF;	//WIPER_OFF ���� ����
				WiperOff();			//WiperOFF�� ���ܸ��� ����X
				rain = 0;			//rain ���� ������ ���� 0
				printf("������ OFF\n");
				lcd_Wiper(disp1, rain);
		}
		
		lcd_Main(disp1, led, wiper, fan, ndustValue);		//�� �ݺ����� ����� Ȯ��
		delay(1000); //���� �� �д� �ֱ� �� ������ ���� �ֱ�
	}
	return 0;
}


//�̼����� ���� �Լ�
float CheckDust(int ndustChannel) {
	float Vo_Val = 0;
	float Voltage = 0;
	float dustDensity = 0;

	int samplingTime = 280;     //0.28ms
	int delayTime = 40;         //0.04ms
	int offTime = 9680;         //9.68ms

	digitalWrite(PIN_DUST_LED, LOW);
	delayMicroseconds(samplingTime);
	Vo_Val = ReadMcp3208ADC(ndustChannel);
	delayMicroseconds(delayTime);
	digitalWrite(PIN_DUST_LED, HIGH);
	delayMicroseconds(offTime);

	Voltage = (Vo_Val * 3.3 / 1024.0) / 3.0;
	dustDensity = (0.172 * (Voltage)-0.0999) * 1000;
	printf("Voltage : %f, ", Voltage);
	printf("Value : %f(ug/m3)\n", dustDensity);

	return dustDensity;
}


void FanOn(void) {
	digitalWrite(PIN_FAN_P, HIGH);
	digitalWrite(PIN_FAN_N, LOW);
}

void FanOff(void) {
	digitalWrite(PIN_FAN_P, LOW);
	digitalWrite(PIN_FAN_N, LOW);
}

//FAN���� rain�� ���� �ݺ� �ֱ� ª���� 1��, 0.5��, 0.3��
void WiperOn(int rain) {
	int del = 5;
	int step = 2000 / (rain * 2);
	for (int i = 0; i < 128; i++) {
		setsteps(1, 1, 0, 0);
		delay(del);
		setsteps(0, 1, 1, 0);
		delay(del);
		setsteps(0, 0, 1, 1);
		delay(del);
		setsteps(1, 0, 0, 1);
		delay(del);
	}
	for (int k = 0; k < 128; k++) {
		setsteps(1, 0, 0, 1);
		delay(del);
		setsteps(0, 0, 1, 1);
		delay(del);
		setsteps(0, 1, 1, 0);
		delay(del);
		setsteps(1, 1, 0, 0);
		delay(del);
	}
	delay(step);
}

void WiperOff(void) {
	setsteps(0, 0, 0, 0);
}

//fan ���� ������ ���� �� ����
void lcd_FAN(int disp1,int cmd, float dust) {
	char buf[100];
	lcdClear(disp1);
	switch (cmd)
	{
	case FAN_OFF:
		lcdPosition(disp1, 0, 0);
		lcdPuts(disp1, "Dust is Good");			//Dust is Good
		lcdPosition(disp1, 0, 1);				//FAN OFF
		lcdPuts(disp1, "FAN OFF");
		break;
	case FAN_ON:
		sprintf(buf, "Dust %.1f ug/m3", dust);	//�� �����Ͽ� ���ڿ� ��ȭ
		lcdPosition(disp1, 0, 0);
		lcdPuts(disp1, buf);				//������ LCD ���
		lcdPosition(disp1, 0, 1);			//Dust 33.2 ug/m3
		lcdPuts(disp1, "FAN ON");			//FAN ON
		break;
	}
	delay(2000);
}

//wiper ���� ������ ���� �� ����
void lcd_Wiper(int disp1, int cmd) {
	lcdClear(disp1);
	lcdPosition(disp1, 0, 0);
	lcdPuts(disp1, "It's Raining");
	switch (cmd)
	{
	case 0:
		lcdClear(disp1);
		lcdPosition(disp1, 0, 0);
		lcdPuts(disp1, "Stop Raining");			//Stop Raining
		lcdPosition(disp1, 0, 1);				//WIPER OFF
		lcdPuts(disp1, "WIPER OFF");
		break;
	case 1:
		lcdPosition(disp1, 0, 1);				//It's Raining
		lcdPuts(disp1, "WIPER STEP 1");			//WIPER STEP 1
		break;
	case 2:
		lcdPosition(disp1, 0, 1);				//It's Raining
		lcdPuts(disp1, "WIPER STEP 2");			//WIPER STEP 2
		break;
	case 3:
		lcdPosition(disp1, 0, 1);				//It's Raining
		lcdPuts(disp1, "WIPER STEP 3");			//WIPER STEP 3
		break;
	}
	delay(2000);
}

//led ���� ������ ���� �� ����
void lcd_Light(int disp1, int cmd) {
	lcdClear(disp1);
	lcdPosition(disp1, 0, 0);
	lcdPuts(disp1, "Light control");
	switch (cmd)
	{
	case LED_OFF:
		lcdPosition(disp1, 0, 1);				//Light control
		lcdPuts(disp1, "Light OFF");			//Light OFF
		break;
	case LED_ON:
		lcdPosition(disp1, 0, 1);				//Light control
		lcdPuts(disp1, "Light ON");				//Light ON
		break;
	}
	delay(2000);
}

//led,wiper,fan �ǽð� ���� ���� �� ���� ������ �ǽð� ���
void lcd_Main(int disp1, int led, int wiper, int fan, float dust) {
	char buf[100];
	lcdClear(disp1);
	if (led == LED_OFF) {
		if (wiper == WIPER_OFF) {
			lcdPosition(disp1, 0, 0);
			lcdPuts(disp1, "LED X/WIPER X");		//LED X/WIPER X
		}
		else if (wiper == WIPER_ON) {
			lcdPosition(disp1, 0, 0);
			lcdPuts(disp1, "LED X/WIPER O");		//LED X/WIPER O
		}
	}
	else if (led == LED_ON) {
		if (wiper == WIPER_OFF) {
			lcdPosition(disp1, 0, 0);
			lcdPuts(disp1, "LED O/WIPER X");		//LED O/WIPER X
		}
		else if (wiper == WIPER_ON) {
			lcdPosition(disp1, 0, 0);
			lcdPuts(disp1, "LED O/WIPER O");		//LED O/WIPER O
		}
	}
	lcdPosition(disp1, 0, 1);
	switch (fan)
	{
	case FAN_OFF:
		sprintf(buf, "D %.1f/FAN X", dust);
		lcdPuts(disp1, buf);						//D 33.3/FAN X
		break;
	case FAN_ON:
		sprintf(buf, "D %.1f/FAN O", dust);
		lcdPuts(disp1, buf);						//D 102.3/FAN X
		break;
	default:
		break;
	}
}