/*
 * LPC4088 Embedded Firmware System (mbed)
 * Peripherals over I2C: DS1337 RTC (alarm), DS1631 temp sensor, PCF8574T + 4x20 LCD
 * UI: 4x4 keypad (mode switch: normal display <-> calculator)
 * Notes:
 * - LCD is driven via I2C expander (4-bit mode)
 * - Alarm uses DS1337 Alarm1 flag polling + user acknowledge
 */

#include "mbed.h"
#include "i2c.h"

// ===========================================================
//                      I2C ADDRESSES
// ===========================================================
#define DS1337_ADDR 0xD0
#define DS1631_ADDR 0x90
#define LCD_ADDR    0x27

// ===========================================================
//                      LCD COMMANDS
// ===========================================================
#define LCD_CLEARDISPLAY 0x01
#define LCD_SETDDRAMADDR 0x80

// ===========================================================
//                      HARDWARE
// ===========================================================
I2C i2c(p9, p10);            // SDA, SCL
DigitalOut alarmLED(p25);    // external alarm LED

// ===========================================================
//                      LCD ROUTINES
// ===========================================================
static unsigned char wr_lcd_mode(char c, char mode) {
    char seq[5];
    static char backlight = 8;
    if (mode == 8) { backlight = (c != 0) ? 8 : 0; return 0; }
    mode |= backlight;
    seq[0] = mode;
    seq[1] = (c & 0xF0) | mode | 4;
    seq[2] = seq[1] & ~4;
    seq[3] = (c << 4) | mode | 4;
    seq[4] = seq[3] & ~4;
    i2c.start();
    i2c.write(LCD_ADDR << 1);
    for (int i = 0; i < 5; i++) { i2c.write(seq[i]); wait_us(2000); }
    i2c.stop();
    if (!(mode & 1) && c <= 2) wait_ms(2);
    return 0;
}
void lcd_command(char c){ wr_lcd_mode(c,0); }
void lcd_data(char c){ wr_lcd_mode(c,1); }
void lcd_backlight(char on){ wr_lcd_mode(on,8); }
void lcd_clear(void){ lcd_command(LCD_CLEARDISPLAY); wait_ms(2); }
void lcd_set_cursor(int col,int row){
    static const int offs[]={0x00,0x40,0x14,0x54};
    lcd_command(LCD_SETDDRAMADDR | (col+offs[row]));
}
void lcd_print(const char*s){ while(*s) lcd_data(*s++); }
void lcd_init(void){
    char seq[]={0x33,0x32,0x28,0x0C,0x06,0x01};
    wait(1);
    for(int i=0;i<6;i++) lcd_command(seq[i]);
}

// ===========================================================
//                      KEYPAD SETUP
// ===========================================================
DigitalOut row[4]={p17,p18,p19,p20};
DigitalIn  col[4]={p13,p14,p15,p16};
const char KEYS[4][4]={
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'0','F','E','D'}
};

// -----------------------------------------------------------
// Read a single key (blocking)
// -----------------------------------------------------------
char read_keypad(){
    while(1){
        for(int r=0;r<4;r++){
            for(int i=0;i<4;i++) row[i]=1;
            row[r]=0;
            for(int c=0;c<4;c++){
                if(col[c].read()==0){
                    char key=KEYS[r][c];
                    while(col[c].read()==0);
                    wait_ms(150);
                    return key;
                }
            }
        }
    }
}

// ===========================================================
//                      RTC UTILITIES
// ===========================================================
int dec_to_bcd(int val){ return ((val/10)<<4)|(val%10); }
int bcd_to_dec(char val){ return ((val>>4)*10)+(val&0x0F); }

void rtc_write_reg(char reg, char val){
    char cmd[2]={reg,val};
    i2c.write(DS1337_ADDR,cmd,2);
}

bool check_alarm1_flag() {
    char addr = 0x0F, data;
    i2c.write(DS1337_ADDR,&addr,1);
    i2c.read(DS1337_ADDR,&data,1);
    return (data & 0x01);
}
void clear_alarm1_flag() {
    char cmd[2] = {0x0F, 0x00};
    i2c.write(DS1337_ADDR, cmd, 2);
}

// ===========================================================
//                      GLOBALS
// ===========================================================
int alarm_hour=0, alarm_minute=0;
char alarm_ampm='A';
bool calculatorMode=false;

// ===========================================================
//                READ MULTI-DIGIT NUMBER FROM KEYPAD
// ===========================================================
int get_number(const char* prompt, int minv, int maxv) {
    lcd_clear();
    lcd_print(prompt);
    lcd_set_cursor(0, 1);

    char buf[6]={0};
    int idx=0;
    while(1){
        for(int r=0;r<4;r++){
            for(int i=0;i<4;i++) row[i]=1; row[r]=0;
            for(int c=0;c<4;c++){
                if(col[c].read()==0){
                    char key=KEYS[r][c];
                    if(key>='0'&&key<='9'){
                        if(idx<5){ buf[idx++]=key; lcd_data(key); }
                    }
                    else if(key=='E'){
                        buf[idx]='\0';
                        int val=atoi(buf);
                        if(val>=minv&&val<=maxv){
                            while(col[c].read()==0); wait_ms(150);
                            return val;
                        } else {
                            lcd_clear(); lcd_print("Invalid! Try again");
                            wait(1);
                            lcd_clear(); lcd_print(prompt);
                            lcd_set_cursor(0,1); idx=0;
                        }
                    }
                    while(col[c].read()==0); wait_ms(150);
                }
            }
        }
    }
}

// ===========================================================
//                      SET CLOCK TIME
// ===========================================================
void set_clock_time(){
    lcd_clear(); lcd_print("Setting clock time");
    wait(1);
    int hour=get_number("Clock Hour? (1-12)",1,12);
    int minute=get_number("Minutes? (0-59)",0,59);

    lcd_clear(); lcd_print("AM=A PM=B then E");
    char ampm='A'; bool selected=false, confirmed=false;
    while(!confirmed){
        for(int r=0;r<4;r++){
            for(int i=0;i<4;i++) row[i]=1; row[r]=0;
            for(int c=0;c<4;c++){
                if(col[c].read()==0){
                    char key=KEYS[r][c];
                    if(key=='A'||key=='B'){ ampm=key; selected=true; lcd_set_cursor(0,1); lcd_print(key=='A'?"Selected: AM ":"Selected: PM "); }
                    else if(key=='E'&&selected){ confirmed=true; }
                    while(col[c].read()==0); wait_ms(150);
                }
            }
        }
    }

    int month=get_number("Month? (1-12)",1,12);
    int date=get_number("Date? (1-31)",1,31);
    int day=get_number("Day? (1-7)",1,7);
    int year=get_number("Year? (2020-2099)",2020,2099);

    if(ampm=='B' && hour<12) hour+=12;
    if(ampm=='A' && hour==12) hour=0;

    rtc_write_reg(0x00, dec_to_bcd(0));
    rtc_write_reg(0x01, dec_to_bcd(minute));
    rtc_write_reg(0x02, dec_to_bcd(hour));
    rtc_write_reg(0x03, dec_to_bcd(day));
    rtc_write_reg(0x04, dec_to_bcd(date));
    rtc_write_reg(0x05, dec_to_bcd(month));
    rtc_write_reg(0x06, dec_to_bcd(year%100));

    lcd_clear(); lcd_print("Clock time set!");
    wait(1.5);
}

// ===========================================================
//                      SET ALARM1
// ===========================================================
void set_alarm1(){
    lcd_clear(); lcd_print("Setting Alarm1 time");
    wait(1);
    alarm_hour=get_number("Alarm Hour? (1-12)",1,12);
    alarm_minute=get_number("Minutes? (0-59)",0,59);

    lcd_clear(); lcd_print("AM=A PM=B then E");
    bool selected=false, confirmed=false;
    while(!confirmed){
        for(int r=0;r<4;r++){
            for(int i=0;i<4;i++) row[i]=1; row[r]=0;
            for(int c=0;c<4;c++){
                if(col[c].read()==0){
                    char key=KEYS[r][c];
                    if(key=='A'||key=='B'){ alarm_ampm=key; selected=true; lcd_set_cursor(0,1); lcd_print(key=='A'?"Selected: AM ":"Selected: PM "); }
                    else if(key=='E'&&selected){ confirmed=true; }
                    while(col[c].read()==0); wait_ms(150);
                }
            }
        }
    }

    int date=get_number("Date? (1-31)",1,31);
    int hour24=alarm_hour;
    if(alarm_ampm=='B'&&hour24<12)hour24+=12;
    if(alarm_ampm=='A'&&hour24==12)hour24=0;

    rtc_write_reg(0x07, dec_to_bcd(0));
    rtc_write_reg(0x08, dec_to_bcd(alarm_minute));
    rtc_write_reg(0x09, dec_to_bcd(hour24));
    rtc_write_reg(0x0A, dec_to_bcd(date));
    rtc_write_reg(0x0E, 0x05);
    rtc_write_reg(0x0F, 0x00);

    lcd_clear(); lcd_print("Alarm1 set!");
    wait(1.5);
}

// ===========================================================
//                   READ TIME AND TEMP
// ===========================================================
void read_time(int *hr,int *mn,int *sc,int *dy,int *dt,int *mo,int *yr){
    char addr=0x00, data[7];
    i2c.write(DS1337_ADDR,&addr,1);
    i2c.read(DS1337_ADDR,data,7);
    *sc=bcd_to_dec(data[0]&0x7F);
    *mn=bcd_to_dec(data[1]);
    *hr=bcd_to_dec(data[2]&0x3F);
    *dy=bcd_to_dec(data[3]);
    *dt=bcd_to_dec(data[4]);
    *mo=bcd_to_dec(data[5]&0x1F);
    *yr=bcd_to_dec(data[6])+2000;
}
float read_temperature(){
    char cmd=0x51; i2c.write(DS1631_ADDR,&cmd,1);
    wait(0.75);
    cmd=0xAA; i2c.write(DS1631_ADDR,&cmd,1);
    char data[2]; i2c.read(DS1631_ADDR,data,2);
    int raw=(data[0]<<8)|data[1];
    return raw/256.0;
}

// ===========================================================
//                     CALCULATOR MODE
// ===========================================================
void calculator_mode(){
    lcd_clear();
    lcd_print("Calculator Mode");
    wait(1);
    lcd_clear();

    int num1=0,num2=0,result=0;
    char op=0;
    bool firstDone=false;
    while(1){
        char key=read_keypad();

        if(key>='0'&&key<='9'){
            lcd_data(key);
            if(!firstDone) num1=num1*10+(key-'0');
            else num2=num2*10+(key-'0');
        }
        else if(key=='A'||key=='B'||key=='C'||key=='D'){
            op=key; firstDone=true;
            lcd_data((key=='A')?'+':(key=='B')?'-':(key=='C')?'*':'/');
        }
        else if(key=='E'){
            switch(op){
                case 'A': result=num1+num2; break;
                case 'B': result=num1-num2; break;
                case 'C': result=num1*num2; break;
                case 'D': result=(num2!=0)?num1/num2:0; break;
                default: result=0; break;
            }
            lcd_set_cursor(0,1);
            char buf[16];
            sprintf(buf,"= %d",result);
            lcd_print(buf);
        }
        else if(key=='F'){ // Exit calculator
            lcd_clear();
            lcd_print("Returning...");
            wait(1);
            calculatorMode=false;
            return;
        }
    }
}

// ===========================================================
//                NORMAL DISPLAY WITH ALARM CHECK
// ===========================================================
void display_normal() {
    int hr, mn, sc, dy, dt, mo, yr;
    float tempC, tempF;
    char line1[21], line2[21];
    int toggle = 0;

    while (!calculatorMode) {
        // --- Check for alarm ---
        if (check_alarm1_flag()) {
            clear_alarm1_flag();
            read_time(&hr, &mn, &sc, &dy, &dt, &mo, &yr);

            lcd_clear();
            lcd_print("Alarm 1 expired!");
            lcd_set_cursor(0, 1);
            sprintf(line1, "%02d:%02d %02d/%02d", hr, mn, mo, dt);
            lcd_print(line1);

            for (int i = 0; i < 10; i++) {
                alarmLED = 1; wait(0.25);
                alarmLED = 0; wait(0.25);
            }

            lcd_clear();
            lcd_print("Press F to clear...");
            bool cleared = false;
            while (!cleared) {
                char key = read_keypad();
                if (key == 'F') {
                    cleared = true;
                    lcd_clear();
                    lcd_print("Alarm cleared");
                    wait(1.0);
                }
            }
        }

        // --- Regular display ---
        read_time(&hr, &mn, &sc, &dy, &dt, &mo, &yr);
        tempC = read_temperature();
        tempF = tempC * 9.0 / 5.0 + 32.0;

        char ampm[3] = "AM";
        int hr12 = hr;
        if (hr >= 12) { strcpy(ampm, "PM"); if (hr > 12) hr12 = hr - 12; }
        if (hr12 == 0) hr12 = 12;

        lcd_clear();
        sprintf(line1, "%02d:%02d %s %02d/%02d/%04d", hr12, mn, ampm, mo, dt, yr);
        lcd_set_cursor(0, 0);
        lcd_print(line1);

        if (toggle == 0) {
            sprintf(line2, "Temp: %.1fC %.1fF", tempC, tempF);
            toggle = 1;
        } else {
            sprintf(line2, "Alarm: %02d:%02d %s",
                    alarm_hour, alarm_minute,
                    (alarm_ampm == 'A') ? "AM" : "PM");
            toggle = 0;
        }

        lcd_set_cursor(0, 1);
        lcd_print(line2);

        // --- Wait ~5s but allow keypresses ---
        Timer t;
        t.start();
        while (t.read() < 5.0) {
            for (int r = 0; r < 4; r++) {
                for (int i = 0; i < 4; i++) row[i] = 1;
                row[r] = 0;
                for (int c = 0; c < 4; c++) {
                    if (col[c].read() == 0) {
                        char key = KEYS[r][c];
                        while (col[c].read() == 0);
                        wait_ms(150);

                        if (key == 'F') {
                            calculatorMode = true;
                            lcd_clear();
                            lcd_print("Entering Calc...");
                            wait(1);
                            return;
                        }
                    }
                }
            }
            wait_ms(100);
        }
    }
}


// ===========================================================
//                        MAIN
// ===========================================================
int main(){
    lcd_init(); lcd_backlight(1);
    for(int c=0;c<4;c++) col[c].mode(PullUp);
    alarmLED=0;

    lcd_clear(); lcd_print("RTC + Alarm + Temp");
    wait(1.5);

    set_clock_time();
    set_alarm1();

    lcd_clear(); lcd_print("Setup Complete!");
    wait(1.5);

    while(1){
        if(!calculatorMode) display_normal();
        else calculator_mode();
    }
}
