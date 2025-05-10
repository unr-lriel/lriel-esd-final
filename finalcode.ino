// Liam Riel
// CPE 301-1001
// 2025-05-09
// Final Project

#include <LiquidCrystal.h>
#include <RTClib.h>
#include "DHT.h"

// DIGITAL IO SETUP //

volatile unsigned char *myPORTA = (unsigned char*) 0x22;
volatile unsigned char *myDDRA  = (unsigned char*) 0x21;
volatile unsigned char *myPINA  = (unsigned char*) 0x20;

volatile unsigned char *myPORTB = (unsigned char*) 0x25;
volatile unsigned char *myDDRB  = (unsigned char*) 0x24;
volatile unsigned char *myPINB  = (unsigned char*) 0x23;

volatile unsigned char *myPORTD = (unsigned char*) 0x2B;
volatile unsigned char *myDDRD  = (unsigned char*) 0x2A;
volatile unsigned char *myPIND  = (unsigned char*) 0x29;

volatile unsigned char *myPORTH = (unsigned char*) 0x102;
volatile unsigned char *myDDRH = (unsigned char*) 0x101;
volatile unsigned char *myPINH = (unsigned char*) 0x100;


// USART SETUP //

#define RDA 0x80
#define TBE 0x20
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;


// ADC SETUP //

volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;


// LCD SETUP //

#define LCD_RS 11
#define LCD_EN 12
#define LCD_D4 2
#define LCD_D5 3
#define LCD_D6 4 
#define LCD_D7 5
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);


// SENSOR SETUP //

#define WATER_SENSOR_POW 7
#define WATER_SENSOR_SIG A0
#define WATER_THRESHOLD 100

#define DHT_PIN 2
DHT dht(DHT_PIN, DHT11);


// COOLER/OTHER SETUP //

RTC_DS1307 rtc;

#define DISABLED 0
#define IDLE 1
#define ERROR 2
#define RUNNING 3

volatile unsigned char coolerState = DISABLED;

volatile unsigned char triggerStateChange = 0;

volatile int coolerTemp = 25;

unsigned long pastUpdateTime = 0;

// MAIN CONTROL FUNCTIONS //

void setup() {
  // put your setup code here, to run once:
  *myDDRA |= 0x0F;
  *myPORTA &= 0xF0;

  *myDDRD &= 0xF3;
  *myPORTD &= 0xF3;

  U0init(9600);
  adc_init();
  lcd.begin(16, 2);
  dht.begin();

  if (!rtc.begin()) {
    char errorMsg[18] = "Couldn't find RTC";
    printString(errorMsg);
    while(1);
  }

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  attachInterrupt(digitalPinToInterrupt(19), coolerOnOff, FALLING);

  triggerStateChange = 1;

}

void loop() {
  // put your main code here, to run repeatedly:
  if(triggerStateChange) {
    coolerStateChange(coolerState);
  }

  if (coolerState == IDLE) {
    coolerIdleState();
  }
  else if (coolerState == ERROR) {
    coolerErrorState();
  }
  else if (coolerState == RUNNING) {
    coolerRunningState();
  }
}


// COOLER CONTROL FUNCTIONS //

void coolerOnOff() {
  triggerStateChange = 1;
  if (coolerState == DISABLED) {
    coolerState = IDLE;
  }
  else {
    coolerState = DISABLED;
  }
}

void coolerStateChange(unsigned char newState) {
  triggerStateChange = 0;

  DateTime stateChangeTime = rtc.now();
  printTime(&stateChangeTime);

  // fan off
  lcd.clear();
  allLEDoff();

  if (newState == DISABLED) {
    coolerState = DISABLED;
    lcd.clear();
    yellowOn();
    return;
  }

  if (newState == IDLE) {
    coolerState = IDLE;
    greenOn();
    updateDisplay();
    return;
  }

  if (newState == ERROR) {
    coolerState = ERROR;
    char errorMsg[] = "Water low";
    lcd.print(errorMsg);
    redOn();
    return;
  }
  
  if (newState == RUNNING) {
    coolerState = RUNNING;
    // fan on
    blueOn();
    updateDisplay();
    return;
  }
}

void coolerDisabledState() {}

void coolerIdleState() {
  unsigned int water = adc_read();

  if (water <= WATER_THRESHOLD) {
    coolerStateChange(ERROR);
    return;
  }

  short outsideTemp = (short)dht.readTemperature();

  if (outsideTemp > coolerTemp) {
    coolerStateChange(RUNNING);
    return;
  }

  unsigned long interval = pastUpdateTime - millis();

  if (interval >= 60000) {
    updateDisplay();
  }
}

void coolerErrorState() {
  if (*myPIND & 0x08) {
    coolerStateChange(IDLE);
    return;
  }
}

void coolerRunningState() {
  unsigned int water = adc_read();

  if (water < WATER_THRESHOLD) {
    coolerStateChange(ERROR);
    return;
  }

  short outsideTemp = (short)dht.readTemperature();

  if (outsideTemp <= coolerTemp) {
    coolerStateChange(IDLE);
    return;
  }

  unsigned long interval = pastUpdateTime - millis();

  if (interval >= 60000) {
    updateDisplay();
  }
}

void setCoolerTemp(int newTemp) {
  coolerTemp = newTemp;
}


// RTC FUNCTIONS //

void printTime(DateTime* dt) {
  printNum(dt->year());
  U0putchar('-');
  printNum(dt->month());
  U0putchar('-');
  printNum(dt->day());

  printString(" at ");
  printNum(dt->hour());
  U0putchar(':');
  unsigned char minute = dt->minute();
  if (minute < 10) U0putchar('0');
  printNum(minute);
  U0putchar(':');
  unsigned char second = dt->second();
  if (second < 10) U0putchar('0');
  printNum(second);

  U0putchar('\n');
}


// LCD FUNCTIONS //

void updateDisplay() {
  lcd.clear();

  short temp = (short)dht.readTemperature();
  short humid = (short)dht.readHumidity();

  char tempStr[10];
  char humidStr[10];

  sprintf(tempStr, "%d", temp);
  sprintf(humidStr, "%d", humidStr);

  for (int i=0; i<10; i++) {
    if (tempStr[i] == '\0') {
      tempStr[i] = 'C';
      tempStr[i+1] = '\0';
      i=10;
    }
  }

  for (int i=0; i<10; i++) {
    if (humidStr[i] == '\0') {
      humidStr[i] = '%';
      humidStr[i+1] = '\0';
      i=10;
    }
  }

  lcd.print("Temp   Humid");
  lcd.setCursor(0, 1);
  lcd.print(tempStr);
  lcd.setCursor(8, 1);
  lcd.print(humidStr);

  pastUpdateTime = millis();
}


// DIGITAL IO FUNCTIONS //

void redOn() {*myPORTA |= 0x01;} // pin 22
void redOff() {*myPORTA &= 0xFE;}
void blueOn() {*myPORTA |= 0x02;} // pin 23
void blueOff() {*myPORTA &= 0xFD;}
void greenOn() {*myPORTA |= 0x04;} // pin 24
void greenOff() {*myPORTA &= 0xFB;}
void yellowOn() {*myPORTA |= 0x08;} // pin 25
void yellowOff() {*myPORTA &= 0xF7;}

void allLEDoff() {
  redOff();
  blueOff();
  greenOff();
  yellowOff();
}

// USART FUNCTIONS //

void U0init(int U0baud)
{
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  // Same as (FCPU / (16 * U0baud)) - 1;
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;
}

unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}

unsigned char U0getchar()
{
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}

void printNum(unsigned int value) {
  char digits[10];
  int index = 0;

  do {
    digits[index++] = value % 10 + 48; // ascii numbers 48 ahead of their integer values
    value /= 10;
  } while (value > 0);

  for (; index > 0; index--) {
    U0putchar(digits[index-1]);
  }
}

void printString(char str[]) {
  for (int i=0; str[i] != '\0'; i++) {
    U0putchar(str[i]);
  }
}

// ADC FUNCTIONS //

void adc_init() //write your code after each commented line and follow the instruction 
{
  /* setup the A register */
  // set bit   7 to 1 to enable the ADC
  *my_ADCSRA |= 0b10000000;

  // clear bit 6 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b10111111;

  // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11011111;

  // clear bit 0-2 to 0 to set prescaler selection to slow reading
  *my_ADCSRA &= 0b11111000;

  /* setup the B register */
  // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11110111;

  // clear bit 2-0 to 0 to set free running mode
  *my_ADCSRB &= 0b11111000;

  /* setup the MUX Register */
  // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX &= 0b01111111;

  // set bit 6 to 1 for AVCC analog reference
  *my_ADMUX |= 0b01000000;

  // clear bit 5 to 0 for right adjust result
  *my_ADMUX &= 0b11011111;

  // clear bit 4-0 to 0 to reset the channel and gain bits
  *my_ADMUX &= 0b11100000;
}

unsigned int adc_read() //work with channel 0
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX &= 0xE0;

  // clear the channel selection bits (MUX 5) hint: it's not in the ADMUX register
  *my_ADCSRB &= 0xF7;
 
  // set the channel selection bits for channel 0
  *my_ADMUX &= 0xE0;
  *my_ADCSRB &= 0xF7;

  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;

  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);

  // return the result in the ADC data register and format the data based on right justification (check the lecture slide)
  unsigned int val = (*my_ADC_DATA & 0x03FF);
  return val;
}
