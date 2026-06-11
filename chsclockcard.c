/* emulate the CHS clock card 
 * 
 * on ports D0 - D4
 * 
 * Clock driver the real time clock produced by CHS Data services.
 * This is based on the MSM5832 chip from OKI Semiconductor
 *
 */

#include "options.h"
#include "chsclockcard.h"
#include <time.h>
#include <stdio.h>
#include <string.h>

// set to 1 to display clock card processing
// value set in map80nascom.c
//int chsclockcarddebug = CHSCLOCKCARDDEBUG; 
                                  
static int hoursmode=CRTC_HOURS_MODE_24H;             // set to 24 hours mode
static int daysmode=0;                                // set using CRTC_DAYS_MODE_LEAP_YEAR if a leap year     

static int portAdata=0;  // set to the last value written to port A data port
                                         // sets hold read write and address bits
static int portBdata=0;  // set to the last value written to port B data port

static struct tm lasttimestamp;   // will contain the current time when the hold is set or data read without setting hold
/*
 * The structure is 
 * struct tm {
   int tm_sec;         // seconds,  range 0 to 59          
   int tm_min;         // minutes, range 0 to 59           
   int tm_hour;        // hours, range 0 to 23             
   int tm_mday;        // day of the month, range 1 to 31  
   int tm_mon;         // month, range 0 to 11             
   int tm_year;        // The number of years since 1900   
   int tm_wday;        // day of the week, range 0 to 6    
   int tm_yday;        // day in the year, range 0 to 365  
   int tm_isdst;       // daylight saving time             	
    };
 * 
 */

static void crtc_time_update( void );


static void crtc_time_update( void ){
    
    time_t now; // number of seconds since the Epoch (00:00:00 UTC, January 1, 1970)
    time(&now);
    struct tm *t;
    t=localtime(&now);
    memcpy ( &lasttimestamp, t, sizeof( struct tm));
    if (chsclockcarddebug){
        printf("refreshing time stamp: %s", asctime(&lasttimestamp ));
    }

}



// read from Port A - really not sure it should return anything as 
// port A is set to output 
int crtc_PIOportadata_read( int portaddress){
    
    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO A data read value %2.2X\n",portaddress,portAdata);
    }

    return portAdata;
}




// write to PIOdataA which is the address lines Hold and read write lines to the clock chip
// 
void crtc_PIOportadata_write( int portaddress, int port_data){

    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO A data write value %2.2X\n",portaddress,port_data);
    }
    // xor so get changes to bit pattern
    int delta = port_data ^ portAdata;

    portAdata = port_data;

    if ( delta & portAdata & CRTC_CONTROL_HOLD )
    {
        crtc_time_update(  );          /* Update from system clock as late as possible if going into hold mode */
    }

    if ( delta & portAdata & CRTC_CONTROL_WRITE )  /* If rising edge of write strobe */
    {
        // set the special switches when the write pulse is done
        //  check what address has been set on the write 
        // only 2 addresses are relevant 
        switch ( portAdata & CRTC_CONTROL_ADDRESS_MASK )
        {
            case CRTC_REGISTER_TENS_OF_HOURS: 
                  hoursmode = portBdata;
                  break;
            case CRTC_REGISTER_TENS_OF_DAYS:
                  daysmode = portBdata;
                  break;
        }

    }
}



// read the details from the PIO port B 
// technically in the real interface it returns the lower 4 bits 
int crtc_PIOportbdata_read( int portaddress){
   /*
   * If hold is not present then get latest time from operating system
   */
  if ( ! (portAdata & CRTC_CONTROL_HOLD ) )
  {
    crtc_time_update( );
  }

  int port_data = 0xFF;
  if (chsclockcarddebug){
        printf("check portAdata %2.2X read status %2.2X\n",portAdata, portAdata & CRTC_CONTROL_READ);
  }
  if ( portAdata & CRTC_CONTROL_READ ) {
      if (chsclockcarddebug){
        printf("Read active\n");
      }
      switch ( portAdata & CRTC_CONTROL_ADDRESS_MASK )
      {
        case  CRTC_REGISTER_UNITS_OF_SECONDS:
          port_data = lasttimestamp.tm_sec%10;
          break;
        case  CRTC_REGISTER_TENS_OF_SECONDS:
          port_data = lasttimestamp.tm_sec/10;
          break;
        case  CRTC_REGISTER_UNITS_OF_MINUTES:
          port_data = lasttimestamp.tm_min%10;
          break;
        case  CRTC_REGISTER_TENS_OF_MINUTES:
          port_data = lasttimestamp.tm_min/10;
          break;
        case  CRTC_REGISTER_UNITS_OF_HOURS:
          if ( hoursmode & CRTC_HOURS_MODE_24H )
          {
            port_data = lasttimestamp.tm_hour%10;
          }
          else
          {
         int hour = lasttimestamp.tm_hour;
             hour = hour % 12;
             if ( hour == 0 ) hour = 12;
             port_data = hour % 10;
          }
          break;
        case  CRTC_REGISTER_TENS_OF_HOURS:
          if ( hoursmode & CRTC_HOURS_MODE_24H )
          {
            port_data = lasttimestamp.tm_hour/10;
          }
          else
          {
         int hour = lasttimestamp.tm_hour;
             port_data = (hour>=12) ? CRTC_HOURS_MODE_PM : 0;
             hour = hour % 12;
             if ( hour == 0 ) hour = 12;
             port_data |= hour / 10;
          }
          port_data |= ( hoursmode & CRTC_HOURS_MODE_24H );
          break;
        case  CRTC_REGISTER_UNITS_OF_DAYS:
          port_data = lasttimestamp.tm_mday%10;
          break;
        case  CRTC_REGISTER_TENS_OF_DAYS:
          port_data = lasttimestamp.tm_mday/10;
          port_data |= ( daysmode | CRTC_DAYS_MODE_LEAP_YEAR );
          break;
        case  CRTC_REGISTER_DAY_OF_WEEK:
          port_data = lasttimestamp.tm_wday;
          break;
        case  CRTC_REGISTER_UNITS_OF_MONTHS:
          port_data = (lasttimestamp.tm_mon+1)%10;
          break;
        case  CRTC_REGISTER_TENS_OF_MONTHS:
          port_data = (lasttimestamp.tm_mon+1)/10;
          break;
        case  CRTC_REGISTER_UNITS_OF_YEAR:
          port_data = lasttimestamp.tm_year%10;
          break;
        case  CRTC_REGISTER_TENS_OF_YEAR:
          port_data = (lasttimestamp.tm_year/10)%10;
          break;
      }
  }
  if (chsclockcarddebug){
    printf("chs_rtc port %2.2X PIO B data read value %2.2X\n",portaddress,port_data);
  }

  return port_data;
}



// write to PIO port B
void crtc_PIOportbdata_write( int portaddress, int port_data){

    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO B data write  value %2.2X\n",portaddress,port_data);
    }
    portBdata=port_data; 
    
}

int crtc_PIOportacontrol_read( int portaddress ){

    int port_data=0;
    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO A control read value %2.2X\n",portaddress,port_data);
    }
    return port_data;
    
}
    
void crtc_PIOportacontrol_write( int portaddress, int port_data){

    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO A control write  value %2.2X\n",portaddress,port_data);
    }
    
}


int crtc_PIOportbcontrol_read( int portaddress ){
 
    int port_data=0;
    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO B control read value %2.2X\n",portaddress,port_data);
    }
    return port_data;
    
}
    
void crtc_PIOportbcontrol_write( int portaddress, int port_data){

    if (chsclockcarddebug){
        printf("chs_rtc port %2.2X PIO B control write  value %2.2X\n",portaddress,port_data);
    }
    
}


// end of FILE