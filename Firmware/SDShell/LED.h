// Simple LED handler
//
//  Scott Lawrence  2014-03
//
//  Serial multiplexer, kinda.
//  MIT Licensed
//
//  Just something I threw together to have multiple status outputs
//  all funneling through one central interface.


// LED pins (use PWM pins!)
#define kGreen    5
#define kRed      6

// LED Brightnesses
#define kBright 255
#define kVDim   20

// LED Blink Speeds
#define kFast   100
#define kSlow   500

// setup the LED stuff
void Led_Setup( void );

// and to set the LED display, here are our modes
#define kLedOff    (0)  // both off
#define kLedStart  (1)  // startup time
#define kLedIdle   (2)  // idle. nothing happening
#define kLedError  (3)  // recoverable error
#define kLedRead   (4)  // reading from SD
#define kLedWrite  (5)  // writing to SD
#define kLedFatal  (6)  // fatal error, blink and loop forever

// and the function to enable a mode
void Led_Set( int which );

