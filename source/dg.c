#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <softPwm.h>
#include <lcd.h>

/*GPIO �� ����
������ �� ��ȣ ����
������ //TP => 5, EP => 6
DC���� //1��EN => 25, 1��P => 23, 1�� N => 24, 2��EN => 26, 2��P => 13, 2��N => 19
����ġ //B1 => 17, B2 => 27, B3 => 22, B4 => 10, B5 => 18
*/

//DC����
#define MOTOR_1_EN_PIN 25
#define MOTOR_1_MT_P_PIN 23
#define MOTOR_1_MT_N_PIN 24
#define MOTOR_2_EN_PIN 26
#define MOTOR_2_MT_P_PIN 13
#define MOTOR_2_MT_N_PIN 19
//������
#define TP 5
#define EP 6
//����ġ
#define KEYPAD_PB1 17
#define KEYPAD_PB2 27
#define KEYPAD_PB_3 22  
#define KEYPAD_PB_4 10  
#define KEYPAD_PB_5 18
#define MAX_KEY_BT_NUM 5  
const int KeypadTable[5] = {
     KEYPAD_PB1,KEYPAD_PB2,KEYPAD_PB_3,KEYPAD_PB_4,KEYPAD_PB_5 };

int KeypadRead(void)
{
    int i, nKeypadstate;
    nKeypadstate = 0;
    for (i = 0; i < MAX_KEY_BT_NUM; i++) {
        if (!digitalRead(KeypadTable[i])) {
            nKeypadstate |= (1 << i);
        }
    }
    return nKeypadstate;
}

float getDistance(void)   //�����ķ� �Ÿ� ����
{
    float fDistance;
    int nStartTime, nEndTime;
    digitalWrite(TP, LOW);
    delayMicroseconds(2);
    digitalWrite(TP, HIGH);
    delayMicroseconds(10);
    digitalWrite(TP, LOW);
    while (digitalRead(EP) == LOW) {
        nStartTime = micros();
    }
    while (digitalRead(EP) == HIGH) {
        nEndTime = micros();
    }
    fDistance = (nEndTime - nStartTime) * 0.034 / 2;
    return fDistance;
}

void MotorStop()
{
    digitalWrite(MOTOR_1_MT_P_PIN, LOW);
    digitalWrite(MOTOR_1_MT_N_PIN, LOW);
    digitalWrite(MOTOR_2_MT_P_PIN, LOW);
    digitalWrite(MOTOR_2_MT_N_PIN, LOW);
}

//���� �ӵ����� EN�� pwm����
void MotorControl(unsigned char speed)
{
    softPwmWrite(MOTOR_1_EN_PIN, speed);
    digitalWrite(MOTOR_1_MT_P_PIN, HIGH);
    digitalWrite(MOTOR_1_MT_N_PIN, LOW);
    softPwmWrite(MOTOR_2_EN_PIN, speed);
    digitalWrite(MOTOR_2_MT_P_PIN, HIGH);
    digitalWrite(MOTOR_2_MT_N_PIN, LOW);
}

//�� �ʱ�ȭ
void Init_D() {
    pinMode(TP, OUTPUT);
    pinMode(EP, INPUT);
    pinMode(MOTOR_1_MT_P_PIN, OUTPUT);
    pinMode(MOTOR_1_MT_N_PIN, OUTPUT);
    pinMode(MOTOR_2_MT_P_PIN, OUTPUT);
    pinMode(MOTOR_2_MT_N_PIN, OUTPUT);
    softPwmCreate(MOTOR_1_EN_PIN, 0, 100);
    softPwmCreate(MOTOR_2_EN_PIN, 0, 100);
    
    for (int i = 0; i < MAX_KEY_BT_NUM; i++) {      //Ǫ�ù�ư �Է��� ���
        pinMode(KeypadTable[i], INPUT);
    }
}

void lcd_Speed(int disp1, int step);
void lcd_Light(int disp1, int cmd);
void lcd_Danger(int disp1);

int main(void)
{
    if (wiringPiSetupGpio() == -1) {
        fprintf(stdout, "Not start wiringPi: %s\n",
            strerror(errno));
        return 1;
    }
    //int disp1;
    //disp1 = lcdInit(2, 16, 4, TEXTLCD_PIN_RS, TEXTLCD_PIN_E, TEXTLCD_PIN_D4, TEXTLCD_PIN_D5, TEXTLCD_PIN_D6, TEXTLCD_PIN_D7, 0, 0, 0, 0);

    int current_btn;
    int i;
    int nKeypadstate;
    float fDistance = 0;   //������ �Ÿ�
    unsigned char duty = 0;

    Init_D();	//���ʱ�ȭ

    while (1)
    {
        if (KeypadRead() != 0) {   //Ű�� ��������
            nKeypadstate = KeypadRead();   //Ű�������� �޾ƿ´�.
            for (i = 0; i < MAX_KEY_BT_NUM; i++) {
                if (nKeypadstate & (1 << i)) {
                    current_btn = i;                  // �Է�Ű current_btn�� ����
                }
            }
            printf("key push STEP%d\n", current_btn+1);
            lcd_Speed(disp1, current_btn);
            if (current_btn == 0) {
                duty = 25;
            }
            else if (current_btn == 1) {
                duty = 40;
            }
            else if (current_btn == 2) {
                duty = 70;
            }
            else if (current_btn == 3) {
                duty = 100;
            }
            else if (current_btn == 4) {  //�극��ũ ��ư
                duty = 0;
            }
            MotorControl(duty);		//�ӵ��� ���� ���� ����
            delay(500);
        }

        if (duty >= 70) {   //�ӵ� 70 �̻����� ��� ������ ����
            fDistance = getDistance();

            if (fDistance < 200.0) {    //�����Ÿ� 2m���� �ִٸ� �ӵ� �ٿ�
                printf("�Ÿ�����!!\n");
                lcd_Danger(disp1);

                MotorControl(70);
                delay(300);
                MotorControl(40);
                delay(300);
                MotorControl(20);
                delay(2000);
            }
        }
    }
    return 0;
}

void lcd_Speed(int disp1, int step) {
    
   lcdClear(disp1);
   lcdPosition(disp1, 0, 0);
   lcdPuts(disp1, "Speed control");
    switch (step)
    {
    case 0:
//        printf("Speed control\n");
//        printf("Step 1\n\n");
        lcdPosition(disp1, 0, 1);
        lcdPuts(disp1, "Step 1");
        break;
    case 1:
//        printf("Speed control\n");
//        printf("Step 2\n\n");
        lcdPosition(disp1, 0, 1);
        lcdPuts(disp1, "Step 2");
        break;
    case 2:
//        printf("Speed control\n");
//        printf("Step 3\n\n");
        lcdPosition(disp1, 0, 1);
        lcdPuts(disp1, "Step 3");
        break;
    case 3:
//        printf("Speed control\n");
//        printf("Step 4\n\n");
        lcdPosition(disp1, 0, 1);
        lcdPuts(disp1, "Step 4");
        break;
    case 4:
//        printf("Speed 0\n");
//        printf("Engine is stop\n\n");
        lcdClear(disp1);
        lcdPosition(disp1, 0, 0);
        lcdPuts(disp1, "Speed 0");
        lcdPosition(disp1, 0, 1);
        lcdPuts(disp1, "Engine is stop");
        break;
    }
}

void lcd_Danger(int disp1) {
    printf("Watch OUt!!\n");
   /* lcdClear(disp1);
    lcdPosition(disp1, 0, 0);
    lcdPuts(disp1, "Watch Out!!");*/
}