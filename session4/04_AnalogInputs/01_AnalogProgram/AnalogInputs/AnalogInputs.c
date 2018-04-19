/* ========================================================================== */
/*                                                                            */
/*   Filename.c                                                               */
/*   (c) 2001 Author                                                          */
/*                                                                            */
/*   Description                                                              */
/*                                                                            */
/* ========================================================================== */


#include <stdio.h>
#include <stdlib.h>

#include <p18f8722.h>
//#include <spi.h>
//#include <delays.h>
#include "LCD.h"
#include "AnalogInputs.h"


// configuration bits
#pragma config OSC = HS         // Oscillator Selection bits (HS oscillator)
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Enable bit (Fail-Safe Clock Monitor disabled)
#pragma config WDT = OFF        // Watchdog Timer (WDT disabled (control is placed on the SWDTEN bit))

void initButtons(void);
void checkInputs(void);
void sequence1(void); /*display on the LCD the state of the button - pressed or not pressed */

volatile int state = 4;
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
 * Init LEDs Function
 */
void initLEDs(void)
{
	/* set all PortD bits as Output */
	TRISD = 0x0;
	/* set state of the pins as STD_OFF */
	LATD = 0x0;
} /* void initLEDs(void) */

/*******************************************************************************
 * Init Buttons Function
 */
void initAdc(void)
{
    /* RA0 potentiometer */
    TRISA = TRISA | (1<<0); /* only RA0 as input */

    ADCON0 = 0b00000001; // (00) unimplemented (0000) channel AN0 (0)don't start yet (1) turn on analog to digital module, see page 270
    ADCON1 = 0b00000000; // (00) unimplemented (00) using AVDD and AVSS (0000) A/D port bits, see page 271
    ADCON2 = 0b00000010; // (0) left justified (0) unimplemented (000) 0 TAD (010) FOSC/32, see page 272
} /* void initButtons(void) */





/*******************************************************************************
   
 * sequence1 -  Function for displaing the value of the poti to the LATD
 */
void sequence1(void)
{
    byte potiValue = 0;
    char reg[100];
    /* start conversion*/
    /* -- your code here -- */
    ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE == 1){
        ;//cat timp e unu asteapta , nu face nimic
        //_delay(5000);
    }
    /*  wait to finish conversion*/
    /* -- your code here -- */
    potiValue = ADRESH;  //Adc value of the potentiometer
    /* read value of the input*/
    /* -- your code here -- */
    LATD = potiValue;
    Delay10KTCYx(10); //codul ruleaza ciclic, de asta se mai pune un delay ca sa nu fie prea rapid

} /* void sequence1(void) */
/*******************************************************************************

 * sequence2 -  Function for displaing the value of the poti to LCD
 */
void sequence2(void)
{
    byte potiValue = 0;
    /* -- your code here -- */
    ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE == 1){
        ;
    }
    potiValue = ADRESH; 
    /*
    char reg[32];
    sprintf(reg,"Poti value %d",potiValue);

    LcdGoTo(0);
    LcdWriteString(reg);
//merge!
    */
    //sau
    char message[16] = "Poti value     ";
    message[15] = '0'+ potiValue%10;
    message[14] = '0'+(potiValue/10)%10;
    message[13] = '0'+ potiValue/100;
    LcdGoTo(0);
    LcdWriteString(message);
    
    //merge si asa , deci ori message + partea a 2 a 
    //ori reg + partea 1 
} /* void sequence2(void) */


/*******************************************************************************

 * sequence3 -  Function for displaing the value of the poti to the LATD
 */
void sequence3(void)
{ // facem ca in functie de tensiune sa se aprinda un led 
    //avem 8 praguri de tensiune pe 8 leduri si 255 valori , deci fiecare led se aprinde la diferenta de 32 
    //adica 0-31 led 1 , 32-63 led 2 etc
    byte potiValue = 0;
    byte i =0;
    /* -- your code here -- */
     ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE == 1){
        ;
    }
    potiValue = ADRESH;
    /*
    if(potiValue<32){
        LATD = 0b00000001;
    }else if(potiValue<2*32){
        LATD = 0b00000010;
    }else if(potiValue<3*32){
        LATD = 0b00000100;
    }else if(potiValue<4*32){
        LATD = 0b00001000;
    }else if(potiValue<5*32){
        LATD = 0b00010000;
    }else if(potiValue<6*32){
        LATD = 0b00100000;
    }else if(potiValue<7*32){
        LATD = 0b01000000;
    }else if(potiValue<8*32){
        LATD = 0b10000000;
    }
    */
    //
    
    //de la ei 
    potiValue = potiValue/32;
    LATD = 1 << potiValue;
    
} /* void sequence3(void) */

/*******************************************************************************

 * sequence4 -  Function for displaing the value of the poti to the LATD
 */
void sequence4(void)
{// ca la 3 dar acum le aprindem pe toate din dreapta celui aprins
    byte potiValue = 0;
    byte ledValue =0;
    int i =0;
    /* -- your code here -- */
    ADCON0bits.GO_DONE = 1;
    while(ADCON0bits.GO_DONE == 1){
        ;
    }
    potiValue = ADRESH;
    potiValue = potiValue/32;
    
    for (i = 0; i <= potiValue ; i++){
        ledValue = ledValue + (1 << i);
    }
    LATD = ledValue;
    
    // sau ca gabi 
    //for (i = 0; i <= potiValue ; i++){
    //    LATD = (1 << i);
    //}
} /* void sequence4(void) */

void main()
{
  
  initButtons();
  initAdc();
  LcdInit();
  initLEDs();
  while(1)
  {
      switch(state)
      {
          case 1:
          {
              sequence1();
              break;
          }
          case 2:
          {
              sequence2();
              break;
          }
          case 3:
          {
              sequence3();
              break;
          }
          case 4:
          {
              sequence4();
              break;
          }
          default:
          {
              break;
          }
      }
   }
}
