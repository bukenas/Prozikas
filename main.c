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
unsigned long old_address=0;

void parser(void);
void write_2_flash(unsigned long data, unsigned long address);

unsigned char I2C_buffer[30], i;
unsigned long bufferSize=0, count=0, ciklu_count=0;
unsigned char ADDRESS=0;
unsigned char first_flag=0, uzlaikymas=0, program_count=0;
unsigned short timing[60];
unsigned char j=0, flag_high=0, program_flag=0, jau_flag=0;
unsigned short skait=0, off_count=0;
unsigned short command = 0;
int main(void) {
WDTCTL = WDTPW + WDTHOLD;

 DCOCTL= 0;

 DCOCTL= CALDCO_16MHZ;
 BCSCTL1= CALBC1_16MHZ_;//CALBC1_1MHZ;
  
 P1DIR &=~BIT7;
 P1DIR = BIT6 | BIT0 | BIT4;
 P1OUT &= ~(BIT6 | BIT0 | BIT4);

 P1DIR &= ~BIT3;

 P1IE |= BIT3; // P1.3 interrupt enable
 P1IFG &= ~BIT3; // P1.3 IFG cleared
 P1REN |= BIT3; // P1.3 Resistor enable (Pull up or Pull down)

 TACCR0 = PWM_HIGH;  // set TACCRO register -> count to
 
 P1SEL |= BIT6; // set ouput to Port P1.6 
 TACCTL1 = OUTMOD_7 | CCIE;  // select timer compare mode // Interrupt called when counter reaches TACCR1  
 TACCR1 = PWM_HIGH+1; // set up counter_limit for interrupt TIMER0_A1_VECTOR
 TACTL = ID_0 | TASSEL_2 | MC_1; // select TimerA source SMCLK, set mode to up-counting 
 
 FCTL2 = FWKEY + FSSEL0 + FN1;             // MCLK/3 for Flash Timing Generator

 unsigned char *PAR3 = (unsigned char*)(0xD000);
 
 ADDRESS=*PAR3;
  
  if(ADDRESS==0 || ADDRESS==0xFF)
    program_flag=1;
 
 __enable_interrupt();// enable all interrupts

 while(1) {
   
   if(P1IN&BIT7){
      if(first_flag==0){
        count=0;
        //delay_cnt=0;
        first_flag=1;
      }
      flag_high=1;
      
   }
   else {
     if(flag_high==1){
        timing[j++]=count;
        if(j>59)
          j=0;
        first_flag=0;
        flag_high=0;
     }
     if(count>300)//200)
        parser();
     
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
      //if(timing[i]+timing[i-1]>HIGH_TIMING) //timing dvieju laiku suma
      if(timing[i]+timing[i-1]>HIGH_TIMING) 
        command<<=1;
      //else if(timing[i]+timing[i-1]<300){
     // else if(timing[i]+timing[i-1]<LOW_TIMING){ 
      else if(timing[i]+timing[i-1]<LOW_TIMING){ 
        command<<=1;
        command+=1;
      }
    }
  }
  //memset(timing,0,60);
  //printf("%x\n",command);
  if(ADDRESS!=0xFF) {
    if(ADDRESS==(command>>4) && program_flag!=1){
            switch (command&0x0F){
            
            case 0x0E: //D
                program_count=0;
                P1OUT ^= BIT0;
                TACCR1 = 0;
                off_count=0;
              break;
            case 0x0D: //C
              program_count=0;
              if(off_count++>5){
                P1OUT |= BIT0;
                TACCR1 = PWM_HIGH+1;
                off_count=0;
              }
              break;
            case 0x0B: //B
              program_count=0;
              off_count=0;
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
              off_count=0;
              if(TACCR1<PWM_HIGH){
                if(TACCR1<=50)
                  TACCR1 = (TACCR1+1);
                if(TACCR1>50 && TACCR1<=250)
                  TACCR1 = (TACCR1+2);
                if(TACCR1>250){
                  TACCR1 = (TACCR1+4);
                  if(TACCR1>PWM_HIGH)
                    TACCR1=PWM_HIGH+1;
                }
              }
              break;
  
        }
      }
      switch (command&0x0F){
            case 0x06: //A+D ieina i programavimo rezima
              if(program_count++>30){
                program_flag=1;
                program_count=0;               
              }
              break;
            case 0x09: // C+D baigia programuoti
              if(program_flag==1){
                ADDRESS=command>>4;
                write_2_flash(ADDRESS,0xD000);
                program_flag=0;
                program_count=0;
              }
              break;
      }
  }
   else {
     switch (command&0x0F){
        case 0x06: //A+D ieina i programavimo rezima
              if(program_count++>30)
                program_flag=1;
              break;
        case 0x09: // C+B baigia programuoti
              if(program_flag==1){
                ADDRESS=command>>4;
                write_2_flash(ADDRESS,0xD000);
                program_flag=0;
              }
              break;
     }
  }
  j=0;
}

void write_2_flash(unsigned long data, unsigned long address) {
  unsigned long *Flash_ptr;                          // Flash pointer
  Flash_ptr = (unsigned long *) address;              // Initialize Flash pointer
  FCTL1 = FWKEY + ERASE;                    // Set Erase bit
  FCTL3 = FWKEY;                            // Clear Lock bit
  *Flash_ptr = 0;                           // Dummy write to erase Flash segment
  FCTL1 = FWKEY + WRT;                      // Set WRT bit for write operation
  *Flash_ptr = data;                   // Write value to flash
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
    *Flash_ptr++ = value;                   // Write value to flash
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

// Timer A0 interrupt service routine
#pragma vector=TIMER1_A1_VECTOR
__interrupt void TIMER1_A1_ISR(void)
{
 // P1OUT^=BIT0;
  delay_cnt++;
}



