/* 
 * File:   StateMachine.c
 * Author: Anca
 *
 * Created on May 19, 2014, 7:15 PM
 */

#include <stdio.h>
#include <stdlib.h>

#include <p18f8722.h>
#include <spi.h>
#include <delays.h>

#include "StateMachine.h"
#include "LCD.h"

// configuration bits
#pragma config OSC = HS       // Oscillator Selection bits (HS oscillator)
#pragma config WDT = OFF      // Watchdog Timer (WDT disabled (control is placed on the SWDTEN bit))

//                           0123456789012345
#define LCD_STATE_ONE_1     "     STATE 1    "
#define LCD_STATE_ONE_2     "PRESS RB0 BUTTON"
#define LCD_STATE_TWO_1     "     STATE 2    "
#define LCD_STATE_TWO_2     "     WAIT 10s   "
#define LCD_STATE_THREE_1   "     STATE 3    "
#define LCD_STATE_THREE_2   "PRESS RB0 BUTTON"

const char LcdLines[STATE_MAX][2][18] =
{
    {LCD_STATE_ONE_1, LCD_STATE_ONE_2},
    {LCD_STATE_TWO_1,LCD_STATE_TWO_2},
    {LCD_STATE_THREE_1,LCD_STATE_THREE_2}
};

typedef unsigned char byte;

#define ON          1
#define OFF         0
/*******************************************************************************
 * State machine example
 */
state_e state;
state_e last_state;
byte RD5Led;
byte RD8Led;
byte leftButtonEv = 0;

/*******************************************************************************
 * Set RD5 LED Function
 */
void setRD5Led(unsigned state)
{
    RD5Led = state; /* store new state */

} /* void setRD5Led(unsigned state) */

/*******************************************************************************
 * Set RD8 LED Function
 */
void setRD8Led(unsigned state)
{
    RD8Led = state; /* store new state */

} /* void setRD8Led(unsigned state) */

/*******************************************************************************
 * Check On/Off Left Button Function
 */
byte getOnOffLeftButton(void)
{
    return leftButtonEv;
} /* unsigned getOnOffLeftButton(void) */

/*******************************************************************************
 * Set LCD Function
 */
void setLcd(void)
{
    LcdClear();
    LcdGoTo(0); /* first Line */
    LcdWriteString(LcdLines[state][0]);
    LcdGoTo(0x40); /* second Line */
    LcdWriteString(LcdLines[state][1]);
} /* void setLcd(void) */

/*******************************************************************************
 * Check Inputs Function
 */
void checkInput(void)
{
    byte leftButton = 0;
    static byte leftButton_old = 0;

/* RB0 - check left push button event */
    leftButton = PORTBbits.RB0;
    if (   (leftButton == 0) /* push button pressed */
        && (leftButton != leftButton_old) /* not pressed before */
       )
    {
        leftButtonEv = 1;
    }
    leftButton_old = leftButton;

} /* void checkInput(void) */

/*******************************************************************************
 * Delay of 1s Function
 */
void Delay1s (int x)
{
    int i;
    for (i=0; i<x; i++)
    {
        /* Fosc = 10MHz
         * 10000 x 250 x 4 x (1/10e6) = 1s
         */
        Delay10KTCYx(250);
    }
}/* void Delay1s(int x) */

/*******************************************************************************
 * State Machine Function
 */
static int counter = 0;
void stateMachine(void)
{   
    switch(state){
        case STATE_ONE:
            if(last_state != state){
                setLcd();
                setRD5Led(OFF);
                setRD8Led(OFF);
                last_state=state;
            }
            if(leftButtonEv == 1){
                state = STATE_TWO;
            }
            break;
        case  STATE_TWO:
            if(last_state != state){
            setLcd();
            setRD5Led(ON);
            last_state = state;
            }
            counter++;
            if(counter == 20000){
                state = STATE_THREE;
                counter = 0;
            }
            //Delay1s (10);
            
            break;
        case STATE_THREE: 
             if(last_state != state){
                setLcd();
                setRD5Led(ON);
                setRD8Led(ON);
                last_state=state;
            }           
            if(leftButtonEv == 1){
                state = STATE_ONE;
            }
            break;
    }
   
} /* void stateMachine(void) */

/*******************************************************************************
 * Update outputs Function
 */
void updateOutputs(void)
{
    /* put RD5 led on the output */
    PORTDbits.RD4 = RD5Led;

    /* put RD8 LED on the output */
    PORTDbits.RD7= RD8Led;
   
} /* void updateOutputs(void) */


/*******************************************************************************
 * Init Buttons Function
 */
void initButtons(void)
{
    /* RB0 left push button */
    TRISB = TRISB | (1<<0); /* only RB0 as input */
    
} /* void initButtons(void) */


/*******************************************************************************
 * Init Function
 */
void init(void)
{
    /* init buttons */
    initButtons();
 
    TRISD=0;
    MEMCONbits.EBDIS=1;
    PORTD=0;
    
  /* init LCD */
    LcdInit();

  /* transition from "Power OFF" to "STATE1" */

    state = STATE_ONE;
    last_state = state;
    setLcd(); /* set LCD according to STATE1 */

 } /* void init(void) */

/*******************************************************************************
 * Main Function
 */
void main(void)
{
    init();

    while(1)
    {
        checkInput();
        stateMachine();
        updateOutputs();

        /* clear event from left button */
        leftButtonEv = 0;
    }
} /* void main(void) */
