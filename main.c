#include <msp430g2452.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "flash.h"
#include "main.h"
#include "UART_settings.h"
#include "UART_functions.h"



void I2C_Init(unsigned char slave_address,  unsigned char prescale);
void I2CWrite_Register(char reg_address, char new_value);
void I2CRead_Register(unsigned char reg_address, unsigned char number_of_bytes, unsigned char *readBuffer);
void M95_ONOFF (void);
void Clear_flags();
unsigned char check_TELIT(void);


void parser(void);
void write_2_flash(unsigned short data, unsigned long address);

unsigned char I2C_buffer[30], i;
unsigned long bufferSize=0, count=0, ciklu_count=0;
unsigned char ADDRESS=0;
unsigned char first_flag=0;
unsigned short timing[60];
unsigned char j=0, flag_high=0, program_flag=0, jau_flag=0;
unsigned short command = 0, skait=0, program_count=0;
int main(void) {
WDTCTL = WDTPW + WDTHOLD;

 DCOCTL= 0;
 BCSCTL1= CALBC1_1MHZ;
 DCOCTL= CALDCO_1MHZ;

 P1DIR &=~BIT7;
 P1DIR = BIT6 | BIT0 | BIT4;
 P1OUT &= ~(BIT6 | BIT0 | BIT4);

 P1DIR &= ~BIT3;

 P1IE |= BIT3; // P1.3 interrupt enable
 P1IFG &= ~BIT3; // P1.3 IFG cleared
 P1REN |= BIT3; // P1.3 Resistor enable (Pull up or Pull down)

 BCSCTL1= CALBC1_8MHZ;
 //BCSCTL2 |= DIVS_0;  // set sub-system clock divider to 8
 TACCR0 = 400;  // set TACCRO register -> count to
 
 P1SEL |= BIT6; // set ouput to Port P1.6 
 TACCTL1 = OUTMOD_7 | CCIE;  // select timer compare mode // Interrupt called when counter reaches TACCR1  
 TACCR1 = 401; // set up counter_limit for interrupt TIMER0_A1_VECTOR
 TACTL = ID_1 | TASSEL_2 | MC_1; // select TimerA source SMCLK, set mode to up-counting 
 
 FCTL2 = FWKEY + FSSEL0 + FN1;             // MCLK/3 for Flash Timing Generator
// TBCTL = TASSEL_2 +ID_0+ MC_1+ TAIE +TACLR;
// TBCCTL0 |= CCIE;
// TBCCR0 = 1;
 
 
 
 
 
  unsigned char *PAR3 = (unsigned char*)(0xD000);
 
  ADDRESS=*PAR3;
 
 __enable_interrupt();// enable all interrupts

 // endless loop 
 while(1) {
   
   if(P1IN&BIT7){
      if(first_flag==0){
        count=0;
        first_flag=1;
      }
      //flag_low=0;
      flag_high=1;
      
   }
   else {
     if(flag_high==1){
        timing[j++]=count;
        if(j>59)
          j=0;
        //oldcount=count;
        
        first_flag=0;
        flag_high=0;
     }
     if(count>800)
        parser();
     
     
//     if(ciklu_count++<50000)
//        TACCR1 = 0;
//       else if(ciklu_count>=50000 && ciklu_count<100000)
//       {
//        TACCR1 = 100; 
//        
//       }
//       else
//         ciklu_count=0;
     
     
     if(program_flag==1){
       if(skait++<15000)
        TACCR1 = 0;
       else if(skait>=15000 && skait<30000)
       {
        TACCR1 = 30; 
        
       }
       else
         skait=0;
       jau_flag=1;
     }
     else if(jau_flag==1){
       TACCR1 = 30; 
       jau_flag=0;
     }
       
     
   }
   count++;
 }

}

void parser(void){
  command=0;
  if(j<24){
    j=0;
    return;
  }
  for(int i=0;i<24;i++){
    if(i%2){
      //if(timing[i]+timing[i-1]>700)
      if(timing[i]+timing[i-1]>800)
        command<<=1;
      //else if(timing[i]+timing[i-1]<300){
      else if(timing[i]+timing[i-1]<400){  
        command<<=1;
        command+=1;
      }
    }
  }
  if(ADDRESS!=0xFF) {
    if(ADDRESS&command>>4){
            switch (command&0x0F){
            
            case 0x0E: //D
              program_count=0;
              P1OUT ^= BIT0;
              TACCR1 = 0;
              break;
            case 0x0D: //C
              program_count=0;
              P1OUT |= BIT0;
              TACCR1 = 401;
              break;
            case 0x0B: //B
              program_count=0;
              if(TACCR1>0){
               if(TACCR1<=50)
                  TACCR1 = (TACCR1-1);
                if(TACCR1>50 && TACCR1<=250)
                  TACCR1 = (TACCR1-2);
                if(TACCR1>250)
                  TACCR1 = (TACCR1-4);
              }
              break;
            case 0x07: //A
              program_count=0;
              if(TACCR1<400){
                if(TACCR1<=50)
                  TACCR1 = (TACCR1+1);
                if(TACCR1>50 && TACCR1<=250)
                  TACCR1 = (TACCR1+2);
                if(TACCR1>250){
                  TACCR1 = (TACCR1+4);
                  if(TACCR1>400)
                    TACCR1=401;
                }
              }
              break;
            case 0x06: //A+D ieina i programavimo rezima
              if(program_count++>10)
                program_flag=1;
              break;
            case 0x09: // C+D baigia programuoti
              program_flag=0;
              ADDRESS=command>>4;
              break;
        }
      }
    j=0;
  }
  else {
     switch (command&0x0F){
        case 0x06: //A+D ieina i programavimo rezima
              if(program_count++>10)
                program_flag=1;
              break;
        case 0x09: // C+D baigia programuoti
              program_flag=0;
              ADDRESS=command>>4;
              write_2_flash(ADDRESS,0xD000);
              break;
     }
  }
  }




void write_2_flash(unsigned short data, unsigned long address) {

    
  char *Flash_ptr;                          // Flash pointer
  unsigned int i;

  Flash_ptr = (char *) address;              // Initialize Flash pointer
  FCTL1 = FWKEY + ERASE;                    // Set Erase bit
  FCTL3 = FWKEY;                            // Clear Lock bit
  *Flash_ptr = 0;                           // Dummy write to erase Flash segment

  FCTL1 = FWKEY + WRT;                      // Set WRT bit for write operation

//  for (i=0; i<64; i++)
//  {
    *Flash_ptr = data;                   // Write value to flash
//  }

  FCTL1 = FWKEY;                            // Clear WRT bit
  FCTL3 = FWKEY + LOCK;                     // Set LOCK bit

 
}


void write_flash (char value)
{
  char *Flash_ptr;                          // Flash pointer
  unsigned int i;

  Flash_ptr = (char *) 0x1040;              // Initialize Flash pointer
  FCTL1 = FWKEY + ERASE;                    // Set Erase bit
  FCTL3 = FWKEY;                            // Clear Lock bit
  *Flash_ptr = 0;                           // Dummy write to erase Flash segment

  FCTL1 = FWKEY + WRT;                      // Set WRT bit for write operation

  for (i=0; i<64; i++)
  {
    *Flash_ptr++ = value;                   // Write value to flash
  }

  FCTL1 = FWKEY;                            // Clear WRT bit
  FCTL3 = FWKEY + LOCK;                     // Set LOCK bit
}


// This will be called when timer counts to TACCR1.
#pragma vector=TIMER0_A1_VECTOR
__interrupt void TIMER0_A1_ISR(void){
 // clear interrupt flag
 TACCTL1 &= ~CCIFG;
// count++;
 //P1OUT^=BIT0;

}

#pragma vector=PORT1_VECTOR
__interrupt void PORT1_VECTOR_ISR(void){

 if (P1IN & BIT3) {
 TACCR1 = (TACCR1 +1000)%100;
 }
 P1OUT ^= BIT0;
 P1IFG &= ~BIT3; // P1.3 IFG cleared
 P1IES ^= BIT3;
}


// Timer A0 interrupt service routine
#pragma vector=TIMER0_A0_VECTOR
__interrupt void TIMER0_A0_ISR(void)
{
 // P1OUT^=BIT0;
  //delay_cnt++;
}



