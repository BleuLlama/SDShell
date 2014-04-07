// Simple LED handler
//
//  Scott Lawrence  2014-03
//
//  MIT Licensed
//
//  Just something I threw together to have multiple status outputs
//  all funneling through one central interface.


// LED pins (use PWM pins!)
#define kGreen    5
#define kRed      6

// LED Brightnesses
#define kBright 255
#define kMedium 100
#define kVDim   50

// LED Blink Speeds
#define kVeryFast  40
#define kFast      100
#define kSlow      500

// setup the LED stuff
void Led_Setup( void );

// and to set the LED display, here are our modes
#define kLedOff    (0)  // both off
#define kLedStart  (1)  // startup time
#define kLedIdle   (2)  // idle. nothing happening
#define kLedError  (3)  // recoverable error
#define kLedRead   (4)  // reading from SD
#define kLedWrite  (5)  // writing to SD
#define kLedWait   (6)
#define kLedFatal  (7)  // fatal error, blink and loop forever
#define kLedProbe  (8)  // green flash

// and the function to enable a mode
void Led_Set( int which );

