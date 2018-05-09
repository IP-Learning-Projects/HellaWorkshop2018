/* 
 * File:   clima.c
 * Author: Dragos
 *
 * Created on February 2, 2014, 11:39 PM
 */

#include <stdio.h>
#include <stdlib.h>

#include <p18f8722.h>

#include "clima.h"
#include "lcd.h"
#include "uart.h"

// configuration bits
#pragma config OSC = HS         // Oscillator Selection bits (HS oscillator)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config WDT = OFF        // Watchdog Timer (WDT disabled (control is placed on the SWDTEN bit))


#define DBG_EN  1

#define DBG_MSG 1
#if (DBG_MSG == 1)
#define DBG(x)  UART_puts((char *)x)    // debug messages on UART
#else
#define DBG(x)                  // debug messages are lost
#endif

//                           0123456789012345
#define LCD_STATE_OFF_1     "  Clima is OFF  "
#define LCD_STATE_OFF_2     "----------------"
#define LCD_STATE_ON_COOL_1 "Te:+  C Ti:+  C "
#define LCD_STATE_ON_COOL_2 "Cool       <  C>"
#define LCD_STATE_ON_HEAT_1 "Te:+  C Ti:+  C "
#define LCD_STATE_ON_HEAT_2 "Heat       <  C>"
#define LCD_STATE_ON_VENT_1 "Te:+  C Ti:+  C "
#define LCD_STATE_ON_VENT_2 "Vent          C "



const char LcdLines[STATE_MAX][2][18] =
{
    {LCD_STATE_OFF_1, LCD_STATE_OFF_2},         /* LCD messages for state OFF */
    {LCD_STATE_ON_COOL_1,LCD_STATE_ON_COOL_2},  /* LCD messages for state Cooling */
    {LCD_STATE_ON_HEAT_1,LCD_STATE_ON_HEAT_2},  /* LCD messages for state Heating */
    {LCD_STATE_ON_VENT_1,LCD_STATE_ON_VENT_2}   /* LCD messages for state Ventilation */
};

typedef unsigned char byte;
#define ON          1
#define OFF         0
#define TEMP_STEP   1   /* configured temperature step zise 1*C, 16,17,18... */
#define FAN_STEPS   5   /* how many FAN steps OFF, speed 1, 2, 3, 4, 5 */
#define TEMP_MIN    18  /* minimum desired temperature */

#define INPUT_DEBOUNCE  (3000/100) /* time(ms)/100(ms), how many cycle to wait between 2 temperature measurement */


unsigned int ADCRead(unsigned char ch);
void checkInputs(void);
byte getOnOffButton(void);
void setLcd(void);
void updateLcd(void);
void checkInputs(void);
void stateMachine(void);
void initButtons(void);
void initAdc(void);
void init(void);
void main(void);

/*******************************************************************************
 * Clima state
 */
state_e climaState;         /* global var to store the state of the state machine */
byte fanSpeed;              /* global var to store ventilation FAN speed, it is set by the state machine */
byte heatLevel;             /* global var to store heating element level, it is set by the state machine */
byte heatElement;           /* global var to store heating element state (LED), it is set by the state machine */
byte coolElement;           /* global var to store cooling element state (LED), it is set by the state machine */
byte standbyLed;            /* global var to store standby LED state, it is set by the state machine */
byte lcdBacklightLed;       /* global var to store LCD backlight state (LED), it is set by the state machine */

byte leftButtonEv = 0;
byte rightButtonEv = 0;
byte setTemp = 0;           /* desired temperature, calculated by CheckInputs */
byte inTemp = 0;            /* inside temperature, measured by CheckInputs */
unsigned int outTemp = 0;   /* outside temperature, measured by CheckInputs */

unsigned char tick = 0;     /* counts timer interrupts, each 1ms, free running counter 0,1,2..,254,255,0,1.. */
unsigned char ev = 0;       /* global var to indicate that 100ms passed, calculate by timer interrupt handler */
unsigned char cnt = 0;      /* used to count 4ms periods, handled by timer interrupt, after 25 periods 100ms event is generated */

/* used to format print messages */
char msg[20] = {0};

unsigned char inDeb = 0;    /* counter used to measure the temperature sensors each 3s */



/*******************************************************************************
 * Set Standby LED Function
 */
void setStandbyLed(unsigned state)
{
    /* store new state, it will updated to output later by updateOutputs() */
    standbyLed = state;
} /* void setStandbyLed(unsigned state) */



/*******************************************************************************
 * Set LCD backlight LED Function
 */
void setLcdBacklightLed(unsigned state)
{
    /* store new state, it will updated to output later by updateOutputs() */
    lcdBacklightLed = state;
} /* void setLcdBacklightLed(unsigned state) */



/*******************************************************************************
 * Set Heating Element Function
 */
void setHeatElement(unsigned state)
{
    /* store new state, it will updated to output later by updateOutputs() */
    heatElement = state;
} /* void setHeatElement(unsigned state) */



/*******************************************************************************
 * Set Cooling Element Function
 */
void setCoolElement(unsigned state)
{
    /* store new state, it will updated to output later by updateOutputs() */
    coolElement = state;
} /* void setCoolElement(unsigned state) */



/*******************************************************************************
 * Set FAN Speed Function
 */
void setFanSpeed(unsigned speed)
{
    /* received speed in rage 0..5
     * because low PWM does not have effect on output, we have to remap
     * 0 speed -> 0 FAN PWM
     * 1 speed -> 4 FAN PWM
     * ...
     * 5 speed -> 8 FAN PWM
     * 1,2,3 PWM values are not used because they are not powerfull enough to activate the FAN
     */
    if (speed)
        speed += 3;
    /* store new level in global variable, it will be update later on the output by timer interrupt */
    fanSpeed = speed;
} /* void setFanSpeed(unsigned state) */



/*******************************************************************************
 * Set Heat level Function
 */
void setHeatLevel(unsigned level)
{
    /* received heat level in rage 0..5
     * because low PWM does not have effect on output, we have to remap
     * 0 level -> 0 heat PWM
     * 1 level -> 4 heat PWM
     * ...
     * 5 level -> 8 heat PWM
     * 1,2,3 PWM values are not used because they are not powerfull enough to activate the bulbs
     */
    if (level)
        level += 3;
    /* store new level in global variable, it will be update later on the output by timer interrupt */
    heatLevel = level; 
} /* void setHeatLevel(unsigned level) */



/*******************************************************************************
 * Check Inputs Function
 */
byte getOnOffButton(void)
{
    return leftButtonEv;
} /* unsigned getOnOffButton(void) */


byte getRightButton(void)
{
    return rightButtonEv;
} /* unsigned getOnOffButton(void) */



/*******************************************************************************
 * Check Inputs Function
 */
void setLcd(void)
{
    /* display static message according to the state */
    /* clear LCD */
    LcdClear();
    /* display the first line of LCD corresponding to state: LcdLines[climaState][0] */
    LcdGoTo(0); /* first Line */
    LcdWriteString(LcdLines[climaState][0]);
    /* display the second line of LCD corresponding to state: LcdLines[climaState][1] */
    LcdGoTo(0x40); /* second Line */
    LcdWriteString(LcdLines[climaState][1]);
} /* void setLcd(state_e state) */



/*******************************************************************************
 * Check Inputs Function
 */
void updateLcd(void)
{
    /* display dinamic data: temp int & ext, desired temp, ventilation level */
    state_e climaState_old;
    /* display set temperature */
    if (climaState != STATE_OFF)
    {
        /* out temp */
        sprintf(msg, "%02d", outTemp);

        LcdGoTo(0x00+4);
        LcdWriteString(msg);

        /* in temp */
        sprintf(msg, "%02d", inTemp);
        LcdGoTo(0x00+12);
        LcdWriteString(msg);

    /* fan speed */
        LcdGoTo(0x40+5);
        if (fanSpeed == 4)
            LcdWriteString("|....");
        else if (fanSpeed == 5)
            LcdWriteString("||...");
        else if (fanSpeed == 6)
            LcdWriteString("|||..");
        else if (fanSpeed == 7)
            LcdWriteString("||||.");
        else
            LcdWriteString("|||||");

        /* set temp */
        sprintf(msg, "%d", setTemp);
        LcdGoTo(0x40+12);
        LcdWriteString(msg);

        if (setTemp == TEMP_MIN)
        {
            LcdGoTo(0x40+11);
            LcdWriteString(" ");
            LcdGoTo(0x40+15);
            LcdWriteString(">");
        }
        else if (setTemp == TEMP_MIN+15)
        {
            LcdGoTo(0x40+11); /* second Line */
            LcdWriteString("<");
            LcdGoTo(0x40+15); /* second Line */
            LcdWriteString(" ");
        }
        else
        {
            LcdGoTo(0x40+11); /* second Line */
            LcdWriteString("<");
            LcdGoTo(0x40+15); /* second Line */
            LcdWriteString(">");
        }
    }
} /* void updateLcd(state_e state) */



/*******************************************************************************
 * Check Inputs Function
 */
void checkInputs(void)
{
    unsigned int adcVal = 0;
    byte leftButton = 0;
    byte rightButton = 0;
    static byte leftButton_old = 0;
    static byte rightButton_old = 0;

/* RB0 - check left push button event */
    leftButton = PORTBbits.RB0;
    if (   (leftButton == 0) /* push button pressed */
        && (leftButton != leftButton_old) /* not pressed before */
       )
    {
        leftButtonEv = 1;
    }
    leftButton_old = leftButton;

/* RA5 - check right push button event */
    rightButton = PORTAbits.RA5;
    if (   (rightButton == 0) /* push button pressed */
        && (rightButton != rightButton_old) /* not pressed before */
       )
    {
        rightButtonEv = 1;
    }
    rightButton_old = rightButton;

    /* read ADC AN0 - on board potentiometer */
    adcVal = ADCRead(0);  /* 10 bits value */
    setTemp = adcVal/64 + TEMP_MIN; /* keep the first 4 MSB (0.15) + temp min (18) => 18..35*C */

    if (inDeb == 0)
    {
        /* read ADC AN1 - on board temperature sensor */
       /* MCP9701: 19mV/*C, 400mV@0*C
        * T = (U-400mV)/19
        *
        * U = ADC*5000/1023
        * T = (U-400mV)/19 = (ADC*5000/1023 - 400)/19
        *
        * U = ADC*5000/1023 ~= ADC*5;
        * T ~= (ADC*5-400)/19
        */
        adcVal = ADCRead(1);
        outTemp = (adcVal*5 - 400)/19;
        DBG("-> Temperature out:");
        sprintf(msg, "%d ", outTemp);
        DBG(msg);
        DBG("\n\r");

        /* read ADC AN3 - inside temperature sensor */
       /* LM35: 10mV/*C
        * T = U/10
        *
        * U = ADC*5000/1023
        * T = U/10 = ADC*5000/1023/10
        *
        * U = ADC*5000/1023 ~= ADC*5;
        * T ~= ADC*5/10 ~= ADC/2
        */
        adcVal = ADCRead(3);
        inTemp = adcVal/2;
        DBG("-> Temperature in:");
        sprintf(msg, "%d ", inTemp);
        DBG(msg);
        DBG("\n\r");
        
        inDeb = INPUT_DEBOUNCE;
    }
    else
    {
        inDeb--;
    }

} /* void checkInputs(void) */



/*******************************************************************************
 * State Machine Function
 */
state_e lastState;
void stateMachine(void)
{
    switch(climaState){
    case STATE_OFF:
        if(lastState =! climaState){
            setLcd();
            lastState = climaState;
            setCoolElement(OFF);
            setHeatElement(OFF);
            setFanSpeed(0);
        }
        if(leftButtonEv == 1){
            climaState = STATE_ON_VENT;
            
        }
        break;
        
        case STATE_ON_VENT:
            if(lastState =! climaState){
                updateLcd();

                lastState = climaState;
                setCoolElement(OFF);
                setHeatElement(OFF);
                setFanSpeed(4);
            }
            if(leftButtonEv == 1){
                climaState = STATE_OFF;
            }
            if(inTemp =! setTemp){
                if(inTemp > setTemp){
                    climaState = STATE_ON_COOL;
                }else{
                    climaState = STATE_ON_HEAT;
                }
            }
            break;
        case STATE_ON_COOL:
            if(lastState =! climaState){
                updateLcd();

                lastState = climaState;
                setCoolElement(ON);
                setHeatElement(OFF);
            }
            if(leftButtonEv == 1){
                climaState = STATE_OFF;
            }
            if(inTemp == setTemp){
                climaState = STATE_ON_VENT;
            }else{
                if(inTemp < setTemp){
                    climaState = STATE_ON_HEAT;
                }
            }
            break;
        case STATE_ON_HEAT:
            if(lastState =! climaState){
                updateLcd();

                lastState = climaState;
                setHeatElement(ON);
                setCoolElement(OFF);
            }
            if(leftButtonEv == 1){
                climaState = STATE_OFF;
            }
            if(inTemp == setTemp){
                climaState = STATE_ON_VENT;
            }else{
                if(inTemp < setTemp){
                    climaState = STATE_ON_HEAT;
                }
            }
            break;
    } 
    
    /*place your code here*/
} /* void stateMachine(void) */



/*******************************************************************************
 * Update outputs Function
 */
void updateOutputs(void)
{
    /* put standby LED on the output */
    PORTDbits.RD7 = standbyLed;

    /* put LCD backlight on the output */
    PORTDbits.RD6 = lcdBacklightLed;

    /* put cooling element state on the output */
    PORTDbits.RD1 = coolElement;

    /* put heating element on the output */
    PORTDbits.RD0 = heatElement;
} /* void updateOutputs(void) */



/*******************************************************************************
 * Init Buttons Function
 */
void initButtons(void)
{
    /* RB0 left push button */
    TRISB0 = 1; /* only RB0 as input */
    
    /* RA5 right push button */
    TRISA5 = 1; /* only RA5 as input */

} /* void initButtons(void) */



/*******************************************************************************
 * Init Buttons Function
 */
void initAdc(void)
{
    /* RA0 potentiometer */
    TRISA = TRISA | (1<<0); /* RA0 as input */

    /* RA1 onboard temperature sensor */
    TRISA = TRISA | (1<<1); /* RA1 as input */

    /* RA4 outside temperature sensor */
    TRISA = TRISA | (1<<4); /* RA4 as input */

    TRISA = 0xFF;

// ADCON0
    ADCON0bits.CHS = 1;         // channel AN0
    ADCON0bits.GO_nDONE = 0;    // ADC Idle
    ADCON0bits.ADON = 0;        // turn OFF ADC module
// ADCON1
    ADCON1bits.VCFG = 0b00;     // Voltage ref AVdd AVss
    ADCON1bits.PCFG = 0b0000;   // A/D Port config AN0-AN4 analog, AN5-AN15 digital
// ADCON2
    ADCON2bits.ADFM = 1;        // 0=left, 1=right justified
    ADCON2bits.ACQT = 0b111;    // A/D Acquisition time 20 Tad
    ADCON2bits.ADCS = 0b010;    // A/D Conversion clock Fosc/32
} /* void initButtons(void) */



/*******************************************************************************
 * Init Buttons Function
 */
unsigned int ADCRead(unsigned char ch)
{
   if(ch>13) return 0;  //Invalid Channel

   ADCON0bits.ADON = 1;     // disable AD module
   ADCON0bits.CHS = ch;      // select channel
   ADCON0bits.ADON = 1;     // switch on the adc module
   ADCON0bits.GO_nDONE = 1; //Start conversion
   while(ADCON0bits.GO_nDONE); //wait for the conversion to finish
   //ADCON0bits.ADON = 0;  //switch off adc

   return ADRES;
} /* unsigned int ADCRead(unsigned char ch) */



void initTmr(void)
{
    PORTJbits.RJ6 = 0;
    PORTJbits.RJ7 = 0;
    TRISJbits.TRISJ7 = 0;   // This sets pin RB7 to output
    TRISJbits.TRISJ6 = 0;   // This sets pin RB6 to output

// START - TMR0 setup
    TMR0 = 0;
    T0CON = 0;
    T0CONbits.TMR0ON = 0;   // timer OFF
    T0CONbits.T08BIT = 0;   // 16bit timer
    T0CONbits.T0CS = 0;     // internal source clock
    T0CONbits.T0SE = 0;     // source edge Low-2-High
    T0CONbits.PSA = 0;      // prescaler active
    T0CONbits.T0PS = 0;     // prescaler 3 bits (Fosc/4)/presc 1:2 => timer clock = (10MHz/4) / 2 = 1.25Mhz
    /* 1000Hz */
    /* 1khz => 1ms period
     * 1.25Mhz / 1khz => 1250
     * 65535 - 1250 = 64285 = FB1Dx;
     */
    TMR1H = 0xFB;           //
    TMR1L = 0x1D;           //
    T0IE = 1; //enable TMR0 overflow interrupts
    GIE = 1; //enable Global interrupts
    T0CONbits.TMR0ON = 1;   // timer ON
// END - TMR0 setup
} /* void initTmr(void) */

unsigned char a = 0;

// Interrupt Service Routine (keyword "interrupt" tells the compiler it's an ISR)
void interrupt ISR(void)
{
    if (T0IE && T0IF) // TMR0 interrupt
    {
        /* reconfigure the timer */
        T0IF  = 0;              // clear interrupt flag
        T0CONbits.TMR0ON = 0;   // Turn timer off to reset count register
        TMR0H = 0xFB;           // Reset timer count - 1khz = 1ms
        TMR0L = 0x1D-2;         //

        T0CONbits.TMR0ON = 1;   // Turn timer back on
        
        // each 1ms, count the the tick, tick is a free running counter
        // it is incremented from 0..255 than it will be come 0 again
        tick++;

        if (tick & 0b10) // each 4ms
        {
            cnt++; /* count 4ms periods */
            
            if (cnt == 25) // each 25*4ms = 100ms
            {
                ev = 1;  /* inform the main program 100ms passed */
                cnt = 0; /* reset counter */
            }
        }

        /* - we only need 8 levels of PWM
         * - we can use the tick counter to generate PWM signal
         * - only the first 3 LSB xxxx xBBB can be used since they have a repetitive value
         * 000 = 0 -> start of the period
         * 001 = 1
         * 010 = 2
         * ...
         * 111 = 7 -> end of the period
         * 000 = 0 -> start of a new period
         * 001
         * ...
         * BBB 012345670123456701234567
         * 0   ________________________ 0
         *
         *     _       _       _        1
         * 1   ._______._______._______ 0
         *
         *     __      __      __       1
         * 2   ..______..______..______ 0
         *
         *     ___     ___     ___      1
         * 3   ..._____..._____..._____ 0
         *
         *     ____    ____    ____     1
         * 4   ....____....____....____ 0
         *
         *     _____   _____   _____    1
         * 5   .....___.....___.....___ 0
         *
         *     ______  ______  ______   1
         * 6   ......__......__......__ 0
         *
         *     _______ _______ _______  1
         * 7   ......._......._......._ 0
         *
         *     ________________________ 1
         * 8   ........................ 0
         */

        /* update PWM output for FAN speed */
        if (fanSpeed > (tick & 0x07))
            PORTJbits.RJ6 = 1;
        else
            PORTJbits.RJ6 = 0;

        /* update PWM output for heating element */
        if (heatLevel > (tick & 0x07))
            PORTJbits.RJ7 = 1;
        else
            PORTJbits.RJ7 = 0;
    }

    // process other interrupt sources here, if required
}



/*******************************************************************************
 * Init Function
 */
void init(void)
{
    TRISD=0;
    PORTD=0;
    MEMCONbits.EBDIS=1;

    /* init UART */
    UART_Init();
    UART_puts((char *)"\n\rInitializing...\n\r");

    /* init buttons */
    initButtons();

    /* init ADC */
    initAdc();

    /* init TMR */
    initTmr();

    /* init LCD */
    LcdInit();

/* START - transition from "Power OFF" to "OFF"*/
    DBG("-> T to OFF\n\r");
    climaState = STATE_OFF;
    lastState = climaState;
    setLcd(); /* LCD according to OFF state */
    setStandbyLed(ON); /* Standby LED ON */
    setHeatElement(OFF); /* set Heating element OFF */
    setFanSpeed(0); /* FAN speed 0 = OFF */
    setHeatElement(0); /* Heat level 0 = OFF */
/* END - transition from "Power OFF" to "OFF"*/
} /* void init(void) */



/*******************************************************************************
 * Main Function
 */
void main(void)
{
    init();

    while(1)
    {
        while (ev == 0);
        ev = 0;

        checkInputs();
        stateMachine();
        updateOutputs();

        /* clear events */
        leftButtonEv = 0; /* clear event from left button */
        rightButtonEv = 0; /* clear event from right button */
    }
} /* void main(void) */



