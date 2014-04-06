/*
 SD Card Serial Shell

 Scott Lawrence
 yorgle@gmail.com
 2014-04
 */

// Quick pin/port configuration:

// D0  Serial RX
// D1  Serial TX
// D2  -
// D3  -
// D4  - (SD Card Select for Seeed Studio, Ethernet shield)
// D5  Green LED (sourced) (configure in LED.h)
// D6  Red LED (sourced) (configure in LED.h)
// D7  Break button (press to ground) (configure below)

// D8  - (SD Card Select for Sparkfun)
// D9  -
// D10 - SPI Card Select (Adafruit Card Select)
// D11 - SPI MOSI (SD Card)
// D12 - SPI MISO (SD Card)
// D13 - SPI Clock (SD Card)

// A0 -
// A1 -
// A2 -
// A3 -
// A4 - 
// A5 -


// Need to add: 
//    delayed writes for small buffered targets systems
//    auto speed detect

#define VERSION "0.5"
// v0.05 2014-04-05 - Path support for files added
//                    capt/ecapt/onto/eonto added/put into one function
//                    SD bugfix
//                    'type' skips '\0' now.  (esave will write out nulls)
//                    Experimental autobaud startup added.  Just hit 'return' a bunch
//                    MuxSerial removed
// v0.04 2014-03-15 - MuxSerial added, to do both hard and soft serial simultaneously
//                    delay after Nchars for slow systems
//                    card info on startup
// v0.03 2014-03-14 - EEProm functions, added
// v0.02 2014-03-13 - progmem, capt, type
// v0.01 2014-03-12 - Initial testing, cd, ls


#include <SPI.h>
#include <SD.h>            // SD Card file IO
#include <avr/pgmspace.h>  // PROGMEM stuff (strings to DATA)
#include <EEPROM.h>
#include "LED.h"           // LED interface
// other definitions and variables

// Button pin
#define kButton 7

// change this to match your SD shield or module;
// Arduino Ethernet shield: pin 4
// Adafruit SD shields and modules: pin 10
// Sparkfun SD shield: pin 8
// Seeed Studio SD shield: pin 4
#define kSDPin 10

// size of our string buffers
#define kMaxBuf 100

// user-entered command line
char line[ kMaxBuf ];
int linelen;

// current operating path
char path[ kMaxBuf ];

// secondary buffer (for various functions)
char buf[32];

// argvector for user input, split up
char * argv[10];
int argc = 0;

// for status flags
unsigned char flags;
#define kFlagSDOK    (0x01)

//////////////////////////////////////////
// progmem helpers

// we try to put as much here as possible.  For every byte saved here
// in program space, that's another byte of RAM we get back.
static const unsigned char msg_Ready[]   PROGMEM = "Ready.";
static const unsigned char msg_SD[]      PROGMEM = "SD: ";
static const unsigned char msg_Failed[]  PROGMEM = "Failed.";
static const unsigned char msg_Break[]   PROGMEM = "Break.";
static const unsigned char msg_Ok[]      PROGMEM = "Ok.";
static const unsigned char msg_Working[] PROGMEM = "Working.";
static const unsigned char msg_Done[]    PROGMEM = "Done.";
static const unsigned char msg_No[]      PROGMEM = " Nope.";
static const unsigned char msg_Cant[]    PROGMEM = " Can't.";
static const unsigned char msg_What[]    PROGMEM = ": What?";
static const unsigned char msg_Path[]    PROGMEM = " Bad Path.";
static const unsigned char msg_SDCmd[]   PROGMEM = "No SD.";
static const unsigned char msg_Cmds[]    PROGMEM = "Commands:";
static const unsigned char msg_Bytes[]   PROGMEM = " bytes.";
static const unsigned char msg_Files[]   PROGMEM = " Files.";

static const unsigned char msg_StartCapture[]  PROGMEM = "Enter your text.  End with '.'";
static const unsigned char msg_DoneCapture[]   PROGMEM = "Saved to file.";

static const unsigned char msg_Version[]   PROGMEM = "SDSH The SD Shell v" VERSION;
static const unsigned char msg_ByLine[]   PROGMEM = "by yorgle@gmail.com (Scott Lawrence)";

static const unsigned char msg_Line[] PROGMEM = "---8<---";
static const unsigned char msg_HexHeader[] PROGMEM =  "       0  1  2  3  4  5  6  7   8  9  A  B  C  D  E  F";
static const unsigned char msg_EEOver[] PROGMEM = "EEProm space overflow.";


void printmsgNoNL(const unsigned char *msg)
{
  while( pgm_read_byte( msg ) != 0 ) {
    Serial.write( pgm_read_byte( msg++ ) );
  };
}


#define CR	'\r'
#define NL	'\n'

void line_terminator( void )
{
  Serial.write( NL );
  Serial.write( CR );
}

void printmsg(const unsigned char *msg)
{
  printmsgNoNL(msg);
  line_terminator();
}


#define kErrOk     (0)
#define kErrNo     (1)
#define kErrCant   (2)
#define kErrWhat   (3)
#define kErrPath   (4)
#define kErrFailed (5)
#define kErrSDCmd  (10)

void cmd_error( char type )
{
  switch( type )
  {
    case( kErrOk ):   printmsg( msg_Ok ); break;
    case( kErrNo ):   printmsg( msg_No ); break;
    case( kErrCant ): printmsg( msg_Cant ); break;
    case( kErrWhat ): printmsg( msg_What ); break;
    case( kErrPath ): printmsg( msg_Path ); break;
    case( kErrFailed ): printmsg( msg_Failed ); break;
    case( kErrSDCmd ): printmsg( msg_SDCmd ); break;
  }
}


//////////////////////////////////////////
// util functions

void(* resetFunc) (void) = 0; //declare reset function @ address 0


void idle( void )
{
  if( digitalRead( kButton ) == LOW ) {
    printmsg( msg_Break );
    while( digitalRead( kButton ) == LOW ) delay( 10 );
  } else {
    delay( 10 );
  }
}


long bauds[] = { /*115200,*/ 9600, 19200, 4800, 57600, // common
                        2400, 1200, 38400, 28800, 14400, 1200, 0 }; // less common
// doesn't seem to work for 300, 2400, 115200 baud
// well, it detects 115200, but it doesn't work

long baud = 0;

void autobaud_begin( void )
{
  baud = 0;
  do {
    Serial.begin( bauds[baud] );
    while( !Serial ); // leonardo fix
    
    // wait for a character
    while( !Serial.available() ) { delay( 5 ); }
    
    int ch = Serial.read();
    Serial.println( ch, DEC );
    if( ch == 0x0d || ch == 0x0a) {
      baud = bauds[baud];
      Serial.flush();
      Serial.print( "CONNECT " );
      Serial.println( baud, DEC );
      return;
    }
    Serial.end();
    
    baud++;
  } while( bauds[baud] != 0 );
  baud = 0;
}


//////////////////////////////////////////

void setup()
{
  // reset flags
  flags = 0;
  
  // led output
  Led_Setup();
  
  // set up the serial
  //Serial.begin( 9600 );
  autobaud_begin();
  
  // button input
  pinMode( kButton, INPUT_PULLUP );

  cmd_version();
///  Serial.println("Init...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
  pinMode(10, OUTPUT);

  //testButton();
  SD_Start();
  
  cwd( "/" );
 
  printmsg( msg_Ready );
}

void testButton( void )
{
  // button test
  while( digitalRead( kButton ) == LOW) 
  {
    Led_Set( kLedRead );
  }  
  Led_Set( kLedIdle );
}

void SD_Start( void )
{
  printmsgNoNL( msg_SD );
    
  if (!SD.begin(kSDPin)) {
    printmsg( msg_Failed );
    // use this chunk if you want it to sit forever with a bad sd.
    // we want to be able to use the EE, so we don't want this now.
#ifdef LOCK_ON_BAD_SD    
    while( digitalRead( kButton ) == HIGH ) {
      Led_Set( kLedFatal );
    }
    resetFunc();
    return;
#else
    Led_Set( kLedFatal );
#endif
  } else {
   flags |= kFlagSDOK;
  }
  
  Led_Set( kLedIdle );
}


//////////////////////////////////////////
// sd and fs helpers

// change the current working directory
void cwd( char * pth )
{
  bool itWorked = true;
  int stlen = strlen( path );
  if( !pth ) return;
  if( pth[0] == '/' ) {
    if( pth[1] == '\0' ) {
      strcpy( path, pth );
    }
    else if( !SD.exists( pth )) {
      itWorked = false;
    } else {
      strcpy( path, pth );
    }
  }
  
  else if( !strcmp( pth, "." )) {
    // do nothing
  }
  
  else if( !strcmp( pth, ".." )) {
    // back a dir (should always work
    for( int i=stlen-2 ; i>= 0 ; i-- ) {
      if( path[i] == '/' ) {
        path[i+1] = '\0';
        i=-1;
      }
    }
    
  }
  else {
    // just throw it on there.
    strcat( path, pth );
    // check for '/' on the end
    if( path[ strlen(path)-1] != '/' ) {
      strcat( path, "/" );
    }
    // make sure it exists
    if( !SD.exists( path )) {
      itWorked = false;
      path[ stlen ] = '\0'; // restore it
    }
  }

  // if fail, print a message
  if( !itWorked ) {
    cmd_error( kErrPath );
  }
  
  if( path[0] == '\0' ) {
    strcpy( path, "/" );
  }

}



void printDirectory(File dir, int numTabs) 
{
  int nFiles = 0;

  Led_Set( kLedRead );
  dir.seek( 0 );
  
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       entry.close(); // Added
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       //printDirectory(entry, numTabs+1); // no recursion
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       unsigned long sz = entry.size();
       Serial.println( sz, DEC);
       nFiles++;
     }
     entry.close(); // Added
   }
   Serial.print( "\t" );
   Serial.print( nFiles, DEC );
   printmsg( msg_Files );
  Led_Set( kLedIdle );
}




//////////////////////////////////////////
// command line interface

void getLine( void )
{
  int ch = 'g';
  linelen = 0;
  
  do {
    while( !Serial.available() ) idle();
    ch = Serial.read();
    
    // check for backspace
    if( ch == 0x08 /* backspace character, CTRL-H */ ) {
      if( linelen > 0 ) {
        linelen--;
      }
    } else {
      line[linelen] = ch;
      linelen++;
    }
  } while( ch != '\n' && ch != '\r' && linelen < kMaxBuf);
  line[linelen] = '\0'; // terminate the string
}

void cleanLine( void )
{
  // remove extra whitespaces?
  for( int i=0 ; i<linelen ; i++ )
  {
    // adjust end of string to newline, removing the newline
    if( line[i] == '\n' ||
        line[i] == '\r' ) { line[i] = '\0'; linelen = i; }
        
    // adjust whitespace and non-printable characters to spaces
    if( line[i] > 0 && line[i] <= 31 ) { line[i] = ' '; }
  }
}

void splitLine( void )
{
  char * lp = line;
  argv[0] = NULL;
  argc = 0;
  
  // advance to first non-space
  while( *lp == ' ' && *lp != '\0' ) lp++;
  
  // store the first element
  argv[0] = lp;
  argc++;
  
  lp++;
  
  // split up the remaining line into an argvector
  for( ; *lp != '\0' ; lp++ )
  {
    // return on end of string
    if( *lp == '\0' ) return;
    
    // if whitespace, SPLIT
    if( *lp == ' ' ) {
      *lp = '\0'; // terminate that arg
      lp++; // start on the next item
      // advance to end of whitespace
      while( *lp == ' ' && *lp !='\0' ) lp++;
      if( *lp == '\0' ) return;
      argv[argc] = lp;
      argc++;
    }
  }
  
  // cleanup
  if( argc == 1 && argv[0][0] == '\0' ) {
    argc = 0;
  }
}


//////////////////////////////////////////
// command spigot and functions

struct fcns {
  char * name;
  void (*fcn)(void);
  unsigned char flags;
};
#define kFcnFlagSD  (0x01)    /* command needs SD card */
#define kFcnFlagHDR (0x80)    /* fakeo for header */

struct fcns fcnlist[] =
{
  // these require SD card
  { "SD Card", NULL, kFcnFlagHDR },
  { "cd", &cmd_cd, kFcnFlagSD },
  { "pwd", &cmd_pwd, kFcnFlagSD },
  { "mkdir", &cmd_mkdir, kFcnFlagSD },
  { "rmdir", &cmd_rmdir, kFcnFlagSD },
  { "ls", &cmd_ls, kFcnFlagSD },
  { "dir", &cmd_ls, kFcnFlagSD },
  { "rm", &cmd_rm, kFcnFlagSD },
  { "del", &cmd_rm, kFcnFlagSD },
  { "capt", &cmd_capture, kFcnFlagSD },
  { "onto", &cmd_onto, kFcnFlagSD },
  { "hex", &cmd_hex, kFcnFlagSD },
  { "type", &cmd_type, kFcnFlagSD },
  
  // these are for EEProm IO
  { "EEProm", NULL, kFcnFlagHDR },
  { "ecapt", &cmd_ecapture },
  { "eonto", &cmd_eonto },
  { "ehex", &cmd_ehex },
  { "etype", &cmd_etype },
  { "erm", &cmd_erm },
  { "eload", &cmd_eload },
  { "esave", &cmd_esave },

  // Utility
  { "Utility", NULL, kFcnFlagHDR },
  { "?", &cmd_help },
  { "help", &cmd_help },
  { "vers", &cmd_version },
  { "reset", &cmd_reset },
  { NULL, NULL }
};


// build a path for use;
// tack the passed in filename on the end of the path, noting where it started
// we do this in-place so that we don't need another buffer.
void buildPath( char * filenameInCWD )
{
  strcat( path, filenameInCWD );
}

// remove from the end of the string back to the last slash
void unbuildPath( void )
{
  char * p = path + strlen( path ) -1;
  while( p != path && *p != '/' ) p--;
  p++;
  *p='\0';
}


void cmd_cd( void )
{
  if( argc > 1 ) {
    cwd( argv[1] );
    cmd_error( kErrOk );
  } else {
    cmd_error( kErrNo );
  }
}

void cmd_pwd( void )
{
  Serial.println( path );
}

void cmd_mkdir( void )
{
  if( argc > 1 ) {
    buildPath( argv[1] );
    if( !SD.mkdir( path ) ) { 
      cmd_error( kErrFailed );
    } else {
      cmd_error( kErrOk );
    }
    unbuildPath();
  } else {
    cmd_error( kErrNo );
  }

}

void cmd_rmdir( void )
{
  if( argc > 1 ) {
    buildPath( argv[1] );
    if( !SD.rmdir( path ) ) { 
      cmd_error( kErrFailed );
    } else {
      cmd_error( kErrOk );
    }
    unbuildPath();
  } else {
    cmd_error( kErrNo );
  }
}

void cmd_rm( void )
{
  if( argc > 1 ) {
    buildPath( argv[1] );
    if( !SD.remove( path ) ) { 
      cmd_error( kErrFailed );
    } else {
      cmd_error( kErrOk );
    }
    unbuildPath();
  } else {
    cmd_error( kErrNo );
  }
}


void cmd_ls( void )
{
  File ppp;
  ppp = SD.open( path );
  printDirectory( ppp, 0 );
  ppp.close();
}



////////////////////////////////
// capture to eeprom, file, new or append

#define kCaptureEEPROM  1
#define kCaptureFILE    2

#define kCaptureNEW     4
#define kCaptureAPPEND  5

void cmd_capture( void )
{
  Serial.println( "New capt" );
  do_capture( kCaptureFILE, kCaptureNEW );
}

void cmd_onto( void )
{
  Serial.println( "New onto" );
  do_capture( kCaptureFILE, kCaptureAPPEND );
}

void cmd_ecapture( void )
{
  Serial.println( "New ecapt" );
  do_capture( kCaptureEEPROM, kCaptureNEW );
}

void cmd_eonto( void )
{
  Serial.println( "New eonto" );
  do_capture( kCaptureEEPROM, kCaptureAPPEND );
}

void do_capture( int FileOrEEPROM, int NewOrAppend )
{
  // this could probebly be combined with cmd_capture()
  bool echo = true;
  long nbytes = 0;
  File myFile;
  
  if( FileOrEEPROM == kCaptureFILE ) {
    if( argc < 2 ) { cmd_error( kErrNo ); return; }
  
    buildPath( argv[1] );
    // need to explicitly do this for a new file, since SD library is wonky
    if( NewOrAppend == kCaptureNEW ) {
      SD.remove( path );
    }
    myFile = SD.open( argv[1], FILE_WRITE );
    unbuildPath();
    if( !myFile ) { cmd_error( kErrCant ); return; }
  } else {
    if( NewOrAppend == kCaptureNEW ) {
      nbytes = 0;
    } else {
      // scan to the end of EEPROM
      while( EEPROM.read( nbytes ) != '\0' && nbytes < E2END ) nbytes++;
    }
  }
  
  
  Led_Set( kLedWrite );
  
  printmsg( msg_StartCapture );
  
  int linepos = 0;
  bool done = false;
  while( !done && (digitalRead( kButton ) == HIGH && nbytes < E2END ) ) {
    if( Serial.available() ) {
      // get the character
      int ch = Serial.read();
      
      // check for end of input
      if( ch == '.' && linepos == 0 ) {
        done = true;
      } else {
        // or dump it to the file
        if( echo ) Serial.write( ch );
        
        if( FileOrEEPROM == kCaptureFILE ) {
          myFile.write( ch );
        } else {
          EEPROM.write( nbytes++, ch );
        }
        linepos++;
      }
      
      // inc or reset the linepos
      if( ch == '\n' || ch == '\r' ) {
        linepos = 0;
      } else {
        linepos++;
      }
    } else {
      idle();
    }
  }
  
  if( FileOrEEPROM == kCaptureFILE ) {
    myFile.close();
    printmsg( msg_DoneCapture );
  } else {
    if( nbytes >= E2END ) {
       printmsg( msg_EEOver );
     } else {
       EEPROM.write( nbytes, '\0' ); // end the string, just in case
       printmsg( msg_DoneCapture );
     }
  }

  Led_Set( kLedIdle );
}

////////

void cmd_type( void )
{
  if( argc < 2 ) { cmd_error( kErrNo ); return; }
  
  buildPath( argv[1] );
  File myFile = SD.open( path );
  unbuildPath();
  if( !myFile ) { cmd_error( kErrCant ); return; }
  
  Led_Set( kLedRead );
  printmsg( msg_Line );
  while( myFile.available() && (digitalRead( kButton ) == HIGH )) {
    int ch = myFile.read();
    if( ch != '\0' ) {
      Serial.write( ch );
    }
  }
  if( digitalRead( kButton ) == LOW ) {
    printmsg( msg_Break );
  }

  myFile.close();
  printmsg( msg_Line );
  Led_Set( kLedIdle );
}

//   Do a hex dump of the file
void cmd_hex( void )
{
  long offset = 0;
  
  if( argc < 2 ) { cmd_error( kErrNo ); return; }
  buildPath( argv[1] );
  File myFile = SD.open( path );
  unbuildPath();
  if( !myFile ) { cmd_error( kErrCant ); return; }
  
  Led_Set( kLedRead );
  printmsg( msg_HexHeader );
  
  
  for( offset = 0 ; myFile.available() && (digitalRead( kButton ) == HIGH); offset += 16 )
  {
    sprintf( (char*)buf, "%04x   ", offset );
    Serial.print( (char *)buf );
    
    int lastGood = 0;
    for( int b=0 ; b<16 ; b++ )
    {
      if( myFile.available() ) {
        buf[b] = myFile.read();
        lastGood = b;
      } else {
        buf[b] = 0x00;
      }
    }
    
    hexdump( (unsigned char*)buf, lastGood );
    Serial.println( "" );
  }
  
  if( digitalRead( kButton ) == LOW ) {
    printmsg( msg_Break );
  }
  Led_Set( kLedIdle );
  
  myFile.close();
}

//////////////////////////////////////////



// hexdump
//   hexdump to serial the first 16 bytes passed in
//   helper function for dumpEE below
void hexdump( unsigned char *bufh, int lastgood ) /* implied 16 byte buffer */
{
  int i;
  char s[8];
  
  // A.  hex bytes
  for( i=0 ; i<16 ; i++ ) {
     sprintf( s, "%02x ", bufh[i] & 0x0ff );
     if( i > lastgood ) {
       Serial.print( "   " );
     } else {
       Serial.print( s );
     }
     if( i==7 ) Serial.write( ' ' );
   }
   
   Serial.write( ' ' );
   
   s[1] = '\0';
   
   for( i=0 ; i<16 ; i++ ) {
     if( i > lastgood ) {
       Serial.write( ' ' );
     } else {
       if( bufh[i] >=0x20 && bufh[i] <= 0x7e ) {
         s[0] = bufh[i];
         Serial.print( s );
       } else {
         Serial.write( '.' );
       }
     }
     if( i==7 ) Serial.write( ' ' );
   }
}


//   Do a hex dump of the entire EEProm
void cmd_ehex( void )
{
  long offset = 0;  
  
  Led_Set( kLedRead );
  printmsg( msg_HexHeader );
  
  for( offset = 0 ; (offset < E2END) && (digitalRead( kButton ) == HIGH) ; offset += 16 )
  {
    sprintf( (char*)buf, "%04x   ", offset );
    Serial.print( (char*)buf );
    
    
    for( int b=0 ; b<16 ; b++ )
    {
      buf[b] = EEPROM.read( offset + b );
    }
    
    hexdump( (unsigned char *)buf, 16 );
    Serial.println( "" );
  }
  
  if( digitalRead( kButton ) == LOW ) {
    printmsg( msg_Break );
  }
  Led_Set( kLedIdle );
  
  Serial.print( E2END + 1 );
  printmsg( msg_Bytes );
}



// cmd_etype
//   print out the contents, as-is, printable or not
//   ends when it hits a null or the end of data
void cmd_etype( void )
{
  buf[0] = 'X';
  buf[1] = '\0';
  
  Led_Set( kLedRead );
  printmsg( msg_Line );
  for( int i=0 ; (i<=E2END) && (buf[0] != '\0') && (digitalRead( kButton ) == HIGH) ; i++ )
  {
    buf[0] = EEPROM.read( i );
    
    if( buf[0] == '\0' ) continue;
    
    if( buf[0] ) Serial.print( buf );
  }
  printmsg( msg_Line );
  if( digitalRead( kButton ) == LOW ) {
    printmsg( msg_Break );
  }
  Led_Set( kLedIdle );
}


// load eprom contents to file, fill with zeros at end
void cmd_eload( void )
{
  if( argc < 2 ) { cmd_error( kErrNo ); return; }

  buildPath( argv[1] );
  File myFile = SD.open( path );
  unbuildPath();
  if( !myFile ) { cmd_error( kErrCant ); return; } 
  
  Led_Set( kLedWrite );
  printmsg( msg_Working );

  for( int i=0 ; i<=E2END ; i++ )
  {
    if( i%64 == 0 ) Serial.write( '.' );
    if( myFile.available() ) {
      EEPROM.write( i,  myFile.read() );
    } else {
      EEPROM.write( i, 0 );
    }
  }
  
  if( myFile.available() )
  {
    printmsg( msg_EEOver );
  }
  
  myFile.close();
  printmsg( msg_Done );
  Led_Set( kLedIdle );
}

// save eprom contents to file 
void cmd_esave( void )
{
  if( argc < 2 ) { cmd_error( kErrNo ); return; }

  buildPath( argv[1] );
  File myFile = SD.open( path, FILE_WRITE );
  unbuildPath();
  
  if( !myFile ) { cmd_error( kErrCant ); return; }

  Led_Set( kLedWrite);
  printmsg( msg_Working );
  for( int i=0 ; i<=E2END ; i++ )
  {
    if( i%64 == 0 ) Serial.write( '.' );
    myFile.write( EEPROM.read( i ) );
  }

  myFile.close();
  printmsg( msg_Done );
  Led_Set( kLedIdle );
}


void cmd_erm( void )
{
  Led_Set( kLedWrite);
  printmsg( msg_Working );
  for( int i=0 ; i<=E2END ; i++ )
  {
    if( i%64 == 0 ) Serial.write( '.' );
    EEPROM.write( i, 0 );
  }
  printmsg( msg_Done );
  Led_Set( kLedIdle );
}


void cmd_reset( void )
{
  resetFunc();
}

void cmd_help( void )
{
  printmsg( msg_Cmds );
  int i=0;
  int items = 0;
  while( fcnlist[i].name ) {
    if( fcnlist[i].flags & kFcnFlagHDR ) {
      items = 0;
      Serial.println( "" );
      Serial.println( fcnlist[i].name );
    } else {
      if( 0 == (items % 0)) { Serial.print( "   " ); }
      Serial.print( fcnlist[i].name );
      Serial.print( " \t" );
      items++;
    }
    i++;
  }
  Serial.println();
}

void cmd_version( void )
{
  printmsg( msg_Version );
  printmsg( msg_ByLine );
}



void processLine( void )
{
  if( argc <= 0 ) return;
  
  for( int i=0 ; fcnlist[i].name != NULL ; i++ )
  {
    if( !strcmp( argv[0], fcnlist[i].name ) && fcnlist[i].fcn ) {
      // item compares ok and there's a function to call
      
      if(   ((flags & kFlagSDOK) == kFlagSDOK)
         || ((fcnlist[i].flags & kFcnFlagSD) == 0 )
         ) {
        fcnlist[i].fcn();
      } else {
        cmd_error( kErrSDCmd );
      }
      Serial.println( "" );
      return;
    }
  }
  
  Serial.print( argv[0] );
  cmd_error( kErrWhat );
}


//////////////////////////////////////////
// main loop entry point

bool showPrompt = true;

void loop()
{
  // nothing happens after setup finishes.
  if( showPrompt ) {
    Serial.print( "> " );
    Serial.flush();
  }
  
  getLine();
  Serial.println( "" );
  cleanLine();
  splitLine();
  processLine();
}
