
// Modifica di versione funzionante testata 01.07.2012
// tutto ok salvo che e` su detezione cambio



#include <htc.h>
#include <stdio.h>





//__CONFIG(INTIO & WDTDIS & PWRTDIS & BORDIS & LVPEN & DEBUGEN & UNPROTECT);

__CONFIG(1, PLLDIV_1 & FOSC_INTOSCIO_EC);
__CONFIG(2, WDT_OFF);
//__CONFIG(3, MCLRE_OFF);
__CONFIG(5, CP0_OFF & CP1_OFF & CP2_OFF & CPB_OFF & CPD_OFF);
__CONFIG(6, WRT0_OFF & WRT1_OFF & WRT2_OFF & WRTC_OFF & WRTB_OFF & WRTD_OFF);
__CONFIG(7, EBTR0_OFF & EBTR1_OFF & EBTR2_OFF & EBTRB_OFF);




#define _XTAL_FREQ 8000000
#define  DELAY_100ms(time)  { for(TimeCounter=0;TimeCounter<(time*5);TimeCounter++) __delay_ms(20); }

#define SETUP   ( RB2)
#define NEUTRAL (!RC6)
#define CLUTCH  (!RC7)

#define REF_SPEED_CLOCK 100
#define REF_SPEED_CYCLE 4


#define SEVENSEG  (LATA) //(PORTA)
#define SEG_E     (RB5)

#define EEPROM_TH2_ADD  0
#define EEPROM_TH3_ADD  (EEPROM_TH2_ADD + 1)
#define EEPROM_TH4_ADD  (EEPROM_TH3_ADD + 1)
#define EEPROM_TH5_ADD  (EEPROM_TH4_ADD + 1)
#define EEPROM_TH6_ADD  (EEPROM_TH5_ADD + 1)

 __EEPROM_DATA(0x24,0x1E,0x1A,0x18,0x15,0xFF,0xFF,0xFF);

char SpeedFlag;
int CountSpeedPulse;
int TimeCounter;



void init_IO(void)
{
  // port directions: 1=input, 0=output
  // ANSEL  = 0x00; // all digital I/O
  // ANSELH = 0x00;
  ADCON1 = 0x0F;
  ADCON0 = 0; //AD MODULE OFF 
  CMCON = 7; //COMPARATORS OFF 

  USBEN = 0;
  UTRDIS = 0; 


  TRISA = 0b00010000; // display, all output except RA4 (Timer 0 clock for speed)
  TRISB = 0b11011111; // speed, rmp and setup, all pins input except RB5 (SEG_E)
  TRISC = 0b11111111; // RPM, speed, clutch and neutral all pins input


  INTEDG0 = 1; // ext interrupt on rising edge  // 0 falling 1 rising
  INT0E = 1;
// IOCB0  = 1; // enable interrupt on change of speed signal
// RBIE   = 1;

}

void init_Timer1_counter(void)
{
  TMR1ON = 0; // swith off the timer
  
  T1OSCEN = 0; // external clock
  TMR1CS  = 1;
  T1SYNC  = 1; // Not sync input

  T1CKPS0 = 0; // prescaler 1
  T1CKPS1 = 0;  
}

void init_Timer0_counter(void)
{
  TMR0ON = 0; // swith off the timer
  
  T0SE    = 0; // increment on rising edge
  T08BIT  = 0; // 16bit counter
  T0CS    = 1; // external pin clock
  
  PSA     = 1; // No prescaler
  

  T0PS0 = 0; // prescaler 1:2
  T0PS1 = 0;
  T0PS2 = 0;

}




void update7seg(char Value)
{

  switch(Value) 
  { 
                         //-GFEDCBA   
    case '0': SEVENSEG = 0b00111111; SEG_E = 1; break;
    case '1': SEVENSEG = 0b00000110; SEG_E = 0; break;
    case '2': SEVENSEG = 0b01011011; SEG_E = 1; break;
    case '3': SEVENSEG = 0b01001111; SEG_E = 0; break;
    case '4': SEVENSEG = 0b01100110; SEG_E = 0; break;
    case '5': SEVENSEG = 0b01101101; SEG_E = 0; break;
    case '6': SEVENSEG = 0b01111101; SEG_E = 1; break;
    case '7': SEVENSEG = 0b00000111; SEG_E = 0; break;
    case '8': SEVENSEG = 0b01111111; SEG_E = 1; break;
    case '9': SEVENSEG = 0b01100111; SEG_E = 1; break;
    case 'A': SEVENSEG = 0b01110111; SEG_E = 1; break;
    case 'B': SEVENSEG = 0b01111100; SEG_E = 1; break;
    case 'C': SEVENSEG = 0b00111001; SEG_E = 1; break;
    case 'D': SEVENSEG = 0b01011110; SEG_E = 1; break;
    case 'E': SEVENSEG = 0b01111001; SEG_E = 1; break;
    case 'F': SEVENSEG = 0b01110001; SEG_E = 1; break;
    case 'o': SEVENSEG = 0b01011100; SEG_E = 1; break;

    case 'N': SEVENSEG = 0b01010100; SEG_E = 1; break;
    case 'L': SEVENSEG = 0b00111000; SEG_E = 1; break;
    case '-': SEVENSEG = 0b01000000; SEG_E = 0; break;
    case ' ':
    default : SEVENSEG = 0b00000000; SEG_E = 0; break;

  } 

}

unsigned char CalcGearLevel(void)
{
   unsigned int speed, rpm;
   unsigned char result, count;

   TMR1ON = 0;
   TMR1IF = 0;
   TMR1 = 0;
   
   
   TMR0ON = 0;
   TMR0IF = 0;
   TMR0 = 0;
   
 
   TMR0ON = 1; // start rpm counter
   TMR1ON = 1; // start speed counter
   
   DELAY_100ms(5)

   
   TMR0ON = 0; // stop rpm counter
   TMR1ON = 0; // stop speed counter
  
   if(TMR0IF == 1)
   {

     for(count=0;count<5;count++) //10s delay sleep
     {
       update7seg('o');
       DELAY_100ms(5);
       update7seg('0');
       DELAY_100ms(5);
     }

     return 0;  // owerflow
   }
 
   if(TMR1IF == 1)
   {

     for(count=0;count<5;count++) //10s delay sleep
     {
       update7seg('o');
       DELAY_100ms(5);
       update7seg('1');
       DELAY_100ms(5);
     }

     return 0;  // owerflow
   }

   rpm    = TMR1L;
   rpm   |= (TMR1H<<8);
   rpm   <<= 4; // moltiplica *16
   
   speed  = TMR0L;
   speed |= (TMR0H<<8);

   if((speed == 0) || (rpm == 0))
   {
     return 0;  // fermo, nessuna velocità
   }
   result = (unsigned char)(rpm/speed);

   return (result);
   
}


void main(void)
{
  unsigned char THRESH7;
  unsigned char THRESH6;
  unsigned char THRESH5;
  unsigned char THRESH4;
  unsigned char THRESH3;
  unsigned char THRESH2;


  unsigned char GearLevel;
  unsigned char SetupCounter;
  unsigned char count;

  unsigned char string[4];



  IRCF0 = 1;
  IRCF1 = 1;
  IRCF2 = 1; // 8Mhz
  SCS1   = 1; // internal oscillator 
 
  init_IO();
  init_Timer1_counter();
  init_Timer0_counter();
  
  //ei(); // interrupt enable


// read from EEPROM the gear thresholds
  THRESH2 = eeprom_read(EEPROM_TH2_ADD);
  THRESH3 = eeprom_read(EEPROM_TH3_ADD);
  THRESH4 = eeprom_read(EEPROM_TH4_ADD);
  THRESH5 = eeprom_read(EEPROM_TH5_ADD);
  THRESH6 = eeprom_read(EEPROM_TH6_ADD);


  GearLevel = 0;
  SpeedFlag = 1;
  SetupCounter = '8'; // per permettere prima setup

 // Display test
  for(count='1';count<'7';count++) 
  {
    update7seg(count);
    DELAY_100ms(1);
  }

  for(count='6';count>='1';count--) 
  {
    update7seg(count);
    DELAY_100ms(1);
  }
  update7seg(' ');

  while(1)
  {
    
    if(NEUTRAL)
    {
       
       if(SETUP)  // tra prima e seconda non riprende
       {
          if(SetupCounter > '2')
            SetupCounter = '1';
          update7seg('L');
       }
       else
       {
          update7seg('N');
       }
    }
    else if(CLUTCH)
    {
       update7seg('-');     
    }
    else
    {  // read gear and display


       if(SETUP)
       {
          if(SetupCounter < '7')
          {   
            for(count=0;count<5;count++) //10s delay sleep
            {
              update7seg(SetupCounter);
              DELAY_100ms(5);
              update7seg('L');
              DELAY_100ms(5);
            }
          
            GearLevel = CalcGearLevel();
          }

          switch(SetupCounter)
          {
            case '1': THRESH2 = GearLevel; SetupCounter++;  break;
            case '2': THRESH3 = GearLevel; SetupCounter++;  break;
            case '3': THRESH4 = GearLevel; SetupCounter++;  break;
            case '4': THRESH5 = GearLevel; SetupCounter++;  break;
            case '5': THRESH6 = GearLevel; SetupCounter++;  break;
            case '6': THRESH7 = GearLevel; SetupCounter++;  break;
            
            case '7': THRESH2 = THRESH3 + ((THRESH2 - THRESH3)>>1);
                      THRESH3 = THRESH4 + ((THRESH3 - THRESH4)>>1);
                      THRESH4 = THRESH5 + ((THRESH4 - THRESH5)>>1);
                      THRESH5 = THRESH6 + ((THRESH5 - THRESH6)>>1);
                      THRESH6 = THRESH7 + ((THRESH6 - THRESH7)>>1);

                      // store value in flash
                      eeprom_write(EEPROM_TH2_ADD  ,  THRESH2    );
                      eeprom_write(EEPROM_TH3_ADD  ,  THRESH3    );
                      eeprom_write(EEPROM_TH4_ADD  ,  THRESH4    );
                      eeprom_write(EEPROM_TH5_ADD  ,  THRESH5    );
                      eeprom_write(EEPROM_TH6_ADD  ,  THRESH6    );    
                      SetupCounter = 'F';   
                      
                      update7seg(SetupCounter);
                      DELAY_100ms(5);
                      update7seg('L');
                      DELAY_100ms(5);              
                      break;
            case 'F':
            default: break;
          }


          if(SetupCounter == 'F')
          {
            GearLevel = CalcGearLevel();
    
  	        if(GearLevel == 0)
  	           update7seg('0');
  	        else if(GearLevel < THRESH6)
  	           update7seg('6');      
  	        else if(GearLevel < THRESH5)
  	           update7seg('5');  
  	        else if(GearLevel < THRESH4)
  	           update7seg('4');  
  	        else if(GearLevel < THRESH3)
  	           update7seg('3');  
  	        else if(GearLevel < THRESH2)
  	           update7seg('2');  
  	        else
  	           update7seg('1'); 
          }
          else
          {
            update7seg(SetupCounter);
          }

       }
       else
       {

         GearLevel = CalcGearLevel();

/*
         sprintf(string,"%02X",GearLevel);

         for(count=0;count<5;count++) //10s delay sleep
         {
            update7seg('-');
            DELAY_100ms(3);
            update7seg(string[0]);
            DELAY_100ms(3);
            update7seg(string[1]);
            DELAY_100ms(3);
         }

*/

         if(GearLevel == 0)
            update7seg('0');
         else if(GearLevel < THRESH6)
            update7seg('6');  
         else if(GearLevel < THRESH5)
            update7seg('5');  
         else if(GearLevel < THRESH4)
            update7seg('4');  
         else if(GearLevel < THRESH3)
            update7seg('3');  
         else if(GearLevel < THRESH2)
            update7seg('2');  
         else
            update7seg('1'); 

        
    
       }
    } 

  }

}

