// Simple LED handler
//
//  Scott Lawrence  2014-03
//
//  MIT Licensed
//
//  Just something I threw together to have multiple status outputs
//  all funneling through one central interface.


#include "LED.h"

void Led_Setup( void )
{
  pinMode( kRed, OUTPUT );
  pinMode( kGreen, OUTPUT );
  Led_Set( kLedStart );
}


void Led_FatalFlash( void )
{
  Led_Set( kLedError );
  delay( kFast );
  Led_Set( kLedOff );
  delay( kFast );
}

void Led_ProbeFlash( void )
{
  Led_Set( kLedWait );
  delay( kVeryFast );
  Led_Set( kLedOff );
  delay( kVeryFast );
}

void Led_Set( int which )
{
  int r=0;    // red
  int g=0;    // green

  switch( which ) {
    case( kLedFatal ): Led_FatalFlash(); break;
    case( kLedProbe ): Led_ProbeFlash(); break;
    case( kLedWait ):  g=kMedium; break;
    case( kLedOff ):   break;
    case( kLedStart ): r=kBright; g=kBright; break;
    case( kLedError ): r=kBright; break;
    case( kLedRead ):  g=kBright; break;
    case( kLedWrite ): r=kBright; g=kVDim; break;
    case( kLedIdle ):
    default:
      g = kVDim; break;
  }

  analogWrite( kRed, r );
  analogWrite( kGreen, g );
}
