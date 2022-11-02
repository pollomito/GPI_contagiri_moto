
// Modifica di versione funzionante testata 01.07.2012
// migliorie per problemi lentezza su detezione cambio

// Modificato CalcGearLevel, ridotto tempo di pausa da 500ms a 200ms
// Durante pasusa uscita rapida in caso di chlutch o neutral
// Modificato soglia marcia tra 5 e 6 perche su rilascio in 6 a volte segna 5
// TH6 portato da 0x15 a 0x16

// 17.4.2020
// modificato per Mplab X IDE
// nuovi livelli, ottimizzati per diminuire errore, moltiplicatore rpm a x32 (di più fa overflow su int a 2 byte)

// SEG_E da RB5 a RB4 perché 5 rotto pin
// SEG_G ko RA6 -> spostato su RC4


#define _XTAL_FREQ 8000000

#include "configurationbits.h"

#include <stdio.h>



unsigned char Read_eeprom(unsigned int address)
{
    EEADR=address; // load address of EEPROM to read
    EECON1bits.EEPGD=0; // access EEPROM data memory
    EECON1bits.CFGS=0; // do not access configuration registers
    EECON1bits.RD=1; // initiate read
    return EEDATA; // return EEPROM byte
}



void Write_eeprom(unsigned int address, unsigned char data)
{
    
    EECON1bits.WREN=1; // allow EEPROM writes
    EEADR=address; // load address of write to EEPROM
    EEDATA=data; // load data to write to EEPROM
    EECON1bits.EEPGD=0;// access EEPROM data memory
    EECON1bits.CFGS=0; // do not access configuration registers
    INTCONbits.GIE=0; // disable interrupts for critical EEPROM write sequence
    //===============//
    EECON2=0x55;
    EECON2=0xAA;
    EECON1bits.WR=1;
    //==============//
    INTCONbits.GIE=1; // enable interrupts, critical sequence complete
    while (EECON1bits.WR==1); // wait for write to complete
    EECON1bits.WREN=0; // do not allow EEPROM writes

}

// setup sposto su RC5 pin 16 solo input (per liberare R6 e mettere seg_g)



#define SETUP   ( PORTCbits.RC5)
#define NEUTRAL (!PORTCbits.RC6)
#define CLUTCH  (!PORTCbits.RC7)

#define REF_SPEED_CLOCK 100
#define REF_SPEED_CYCLE 4


#define SEVENSEG  (LATA) //(PORTA)
#define SEG_E     (PORTBbits.RB4)
#define SEG_G     (PORTBbits.RB2)

#define EEPROM_TH2_ADD  0
#define EEPROM_TH3_ADD  (EEPROM_TH2_ADD + 1)
#define EEPROM_TH4_ADD  (EEPROM_TH3_ADD + 1)
#define EEPROM_TH5_ADD  (EEPROM_TH4_ADD + 1)
#define EEPROM_TH6_ADD  (EEPROM_TH5_ADD + 1)


#define ShiftFactor    70
// valori calcolati

// EEPROM_TH2 .. EEPROM_TH6
//     if < Th6 ->   6
//else if < th5 ->   5
// con #define RpmMultFactor 16
//__EEPROM_DATA(0x3E,0x21,0x1C,0x18,0x16,0xFF,0xFF,0xFF); // valori calcolati Excel
//__EEPROM_DATA(0x24,0x1E,0x1A,0x18,0x15,0xFF,0xFF,0xFF); // prima era cos?i

// con #define RpmShiftFactor 17  (con dalay di 500ms fa overflow il shift)
//__EEPROM_DATA(0x2A,0x23,0x1E,0x1A,0x17,0xFF,0xFF,0xFF); // valori calcolati Excel
// __EEPROM_DATA(0x31,0x29,0x23,0x1F,0x1B,0xFF,0xFF,0xFF); // valori calcolati Excel

__EEPROM_DATA(0xCE,0xAB,0x92,0x80,0x72,0xFF,0xFF,0xFF); // valori calcolati Excel




char SpeedFlag;
int CountSpeedPulse;

#define DELAY_100ms(x) __delay_ms(x*100) // pausa max 50 (5 secondi)

unsigned char DELAY_100msExit(unsigned char time) // pausa max 50 (5 secondi)
{
  unsigned char TimeCounter;
  time = time * 5;
  for(TimeCounter=0;TimeCounter<time;TimeCounter++)
  {
     __delay_ms(20);
   

     if(CLUTCH)
     {
        __delay_ms(1);
        if(CLUTCH)  // doublecheck
            return 0xFF;
     }
     
     if(NEUTRAL)
       return 0xFF;
       
  }
  return 0;
}


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


  UCONbits.USBEN = 0;
  UCFGbits.UTRDIS = 1; // disable use to use input pin
  
//  TRISCbits.RC5 = 1; // SETUP Input (sempre input)
  TRISCbits.RC6 = 1; // NEUTRAL Input
  TRISCbits.RC7 = 1; // CLUTCH Input
  TRISA = 0b00010000; // all segment output
  TRISAbits.RA4 = 1; // Timer 0 clock for speed input
  
  TRISCbits.RC0 = 1; // Timer 1 clock for rpm input
  
  TRISBbits.RB2 = 0; // SEG_G Output
  TRISBbits.RB4 = 0; // SEG_E Output


  
  // TRISA = 0b00010000; // display, all output except RA4 (Timer 0 clock for speed)
  // TRISB = 0b11001011; // speed, rmp and setup, all pins input except RB5 RB4 (SEG_E) RB2 (SEG_G))
  // TRISC = 0b11111111; // RPM, speed, clutch and neutral SETUP all pins input


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
    case '0': SEVENSEG = 0b00111111; SEG_E = 1; SEG_G = 0; break;
    case '1': SEVENSEG = 0b00000110; SEG_E = 0; SEG_G = 0; break;
    case '2': SEVENSEG = 0b01011011; SEG_E = 1; SEG_G = 1; break;
    case '3': SEVENSEG = 0b01001111; SEG_E = 0; SEG_G = 1; break;
    case '4': SEVENSEG = 0b01100110; SEG_E = 0; SEG_G = 1; break;
    case '5': SEVENSEG = 0b01101101; SEG_E = 0; SEG_G = 1; break;
    case '6': SEVENSEG = 0b01111101; SEG_E = 1; SEG_G = 1; break;
    case '7': SEVENSEG = 0b00000111; SEG_E = 0; SEG_G = 0; break;
    case '8': SEVENSEG = 0b01111111; SEG_E = 1; SEG_G = 1; break;
    case '9': SEVENSEG = 0b01100111; SEG_E = 1; SEG_G = 1; break;
    case 'A': SEVENSEG = 0b01110111; SEG_E = 1; SEG_G = 1; break;
    case 'B': SEVENSEG = 0b01111100; SEG_E = 1; SEG_G = 1; break;
    case 'C': SEVENSEG = 0b00111001; SEG_E = 1; SEG_G = 0; break;
    case 'D': SEVENSEG = 0b01011110; SEG_E = 1; SEG_G = 1; break;
    case 'E': SEVENSEG = 0b01111001; SEG_E = 1; SEG_G = 1; break;
    case 'F': SEVENSEG = 0b01110001; SEG_E = 1; SEG_G = 1; break;
    case 'o': SEVENSEG = 0b01011100; SEG_E = 1; SEG_G = 1; break;


    case 'N': SEVENSEG = 0b01010100; SEG_E = 1; SEG_G = 1; break;
    case 'L': SEVENSEG = 0b00111000; SEG_E = 1; SEG_G = 0; break;
    case '-': SEVENSEG = 0b01000000; SEG_E = 0; SEG_G = 1; break;
    case ' ':
    default : SEVENSEG = 0b00000000; SEG_E = 0; SEG_G = 0; break;

  } 

}

int round(float num) 
{ 
    return num < 0 ? num - 0.5 : num + 0.5; 
} 
    
unsigned char CalcGearLevel(void)
{
   float speed, rpm;
   
   unsigned char result, count;

   TMR1ON = 0;
   TMR1IF = 0;
   TMR1H = 0;
   TMR1L = 0;
   
   
   TMR0ON = 0;
   TMR0IF = 0;
   TMR0H = 0;
   TMR0L = 0;
   
 
   TMR0ON = 1; // start rpm counter
   TMR1ON = 1; // start speed counter
   
   if( DELAY_100msExit(5 ))
   {  //on clutch or neutral return 0xFF
       
      TMR0ON = 0; // stop rpm counter
      TMR1ON = 0; // stop speed counter
      return 0xFF;
   }
   
   
   TMR0ON = 0; // stop rpm counter
   TMR1ON = 0; // stop speed counter
  
   if(TMR0IF == 1)
   {

     for(count=0;count<5;count++) //10s delay sleep
     {
       update7seg('o'); // overflow timer 0
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
       update7seg('o'); // overflow timer §
       DELAY_100ms(5);
       update7seg('1');
       DELAY_100ms(5);
     }

     return 0;  // owerflow
   }


   rpm = TMR1;
   
   speed = TMR0;
    
   if((TMR1 == 0) || (TMR0 == 0))
   {
     return 0;  // fermo, nessuna velocità
   }

    
   result = (unsigned char)( round(rpm/speed*ShiftFactor) );
   
   if (result == 0xFF)
        result = 0xFE; // plafona a FE per distinguere da detezione frizione

   return (result);
   
}


void main(void)
{
  
  unsigned char THRESH6;
  unsigned char THRESH5;
  unsigned char THRESH4;
  unsigned char THRESH3;
  unsigned char THRESH2;
  unsigned char LVL1;


  unsigned int  GearLevelCalc;
  
  unsigned char GearLevel;
  unsigned char SetupCounter;
  unsigned char count;

  char string[4];



  IRCF0 = 1;
  IRCF1 = 1;
  IRCF2 = 1; // 8Mhz
  SCS1   = 1; // internal oscillator 
 
  init_IO();
  init_Timer1_counter();
  init_Timer0_counter();
  
  INTCONbits.GIE = 0;
  
  //ei(); // interrupt enable


// read from EEPROM the gear thresholds
  THRESH2 = Read_eeprom(EEPROM_TH2_ADD);
  THRESH3 = Read_eeprom(EEPROM_TH3_ADD);
  THRESH4 = Read_eeprom(EEPROM_TH4_ADD);
  THRESH5 = Read_eeprom(EEPROM_TH5_ADD);
  THRESH6 = Read_eeprom(EEPROM_TH6_ADD);


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
  
  /*
  char temp = '8';
  while(1)
  {
      DELAY_100ms(10);
      update7seg(temp);
      DELAY_100ms(10);
      update7seg(' ');
  }
  */
  
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
       
       while(CLUTCH);       
       __delay_ms(200); // su rilascio aspetta prima di prossimo conteggio
       
    }
    else
    {  // read gear and display

       
       if(SETUP)
       {
          if(SetupCounter < '7')
          {   
            for(count=0;count<5;count++) //5s delay sleep
            {
              update7seg(SetupCounter);
              DELAY_100ms(5);
              update7seg('L');
              DELAY_100ms(5);
            }
          
            GearLevelCalc = CalcGearLevel();
            GearLevelCalc += CalcGearLevel();
            GearLevelCalc += CalcGearLevel();
            GearLevelCalc += CalcGearLevel();
            GearLevel = GearLevelCalc>>2;
          }

          switch(SetupCounter)
          {
            case '1': LVL1 = GearLevel; SetupCounter++;  break;
            case '2': THRESH2 = GearLevel; SetupCounter++;  break;
            case '3': THRESH3 = GearLevel; SetupCounter++;  break;
            case '4': THRESH4 = GearLevel; SetupCounter++;  break;
            case '5': THRESH5 = GearLevel; SetupCounter++;  break;
            case '6': THRESH6 = GearLevel; SetupCounter++;  break;
            
            case '7':
                      Write_eeprom(EEPROM_TH2_ADD + 0x20,  LVL1    ); // salva valori rilevati
                      Write_eeprom(EEPROM_TH3_ADD + 0x20,  THRESH2    );
                      Write_eeprom(EEPROM_TH4_ADD + 0x20,  THRESH3    );
                      Write_eeprom(EEPROM_TH5_ADD + 0x20,  THRESH4    );
                      Write_eeprom(EEPROM_TH6_ADD + 0x20,  THRESH5    ); 
                      Write_eeprom(EEPROM_TH6_ADD + 0x21,  THRESH6    ); 
                      
                      THRESH2 = THRESH2 + ((LVL1    - THRESH2)>>1);
                      THRESH3 = THRESH3 + ((THRESH2 - THRESH3)>>1);
                      THRESH4 = THRESH4 + ((THRESH3 - THRESH4)>>1);
                      THRESH5 = THRESH5 + ((THRESH4 - THRESH5)>>1);
                      THRESH6 = THRESH6 + ((THRESH5 - THRESH6)>>1);

                      // store value in flash
                      Write_eeprom(EEPROM_TH2_ADD + 0x10,  THRESH2    ); // salva soglie calcolate
                      Write_eeprom(EEPROM_TH3_ADD + 0x10,  THRESH3    );
                      Write_eeprom(EEPROM_TH4_ADD + 0x10,  THRESH4    );
                      Write_eeprom(EEPROM_TH5_ADD + 0x10,  THRESH5    );
                      Write_eeprom(EEPROM_TH6_ADD + 0x10,  THRESH6    );    
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
       
            if(GearLevel != 0xFF)
            {
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
          else
          {
            update7seg(SetupCounter);
          }

       }
       else
       {
         // normal operation
           
         GearLevel = CalcGearLevel();

/*
         sprintf(string,"%02X",GearLevel);

         
         for(count=0;count<5;count++) //10s delay sleep
         {
            update7seg('-');
            DELAY_100ms(3);
            update7seg(string[1]);
            DELAY_100ms(3);
            update7seg(string[0]);
            DELAY_100ms(3);
         }

*/

	     if(GearLevel != 0xFF)
         {
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

}
