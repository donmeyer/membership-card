//
// MembershipCard_Loader
//

/*
 * Copyright (c) 2015, Donald T. Meyer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, this list of conditions
 * and the following disclaimer.
 * 
 * - Redistributions in binary form must reproduce the above copyright notice, this list of
 *  conditions and the following disclaimer in the documentation and/or other materials provided
 *  with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


// This disables the LCD display functionality, which slows things down by a factor of perhaps 40.
// This does not remove every shred of the LCD code, just the parts that take noticeable time or space. If you are really tight on memory for some reason, there is still some code that could be trimmed.
#define ENABLE_LCD		0


#include <Wire.h>
#include <PString.h>

// This is my slightly modified library. For some reason, the Port A polarity register in the MCP23017 was being set to invert the states.
// My hacked library explicitly sets that register to non-inverting. No idea whether this is a chip issue or ???
#include "DTM_Adafruit_MCP23017.h"

#if ENABLE_LCD
#include <LiquidCrystal.h>
#endif


//
//  ----------------------
//  | <banner / command> |
//  |Cy: <nnn> Cmds: nnn |
//  |<mode>              |
//  |0ABC : 55   I WC we |
//  ----------------------
//


// The amount of time in milliseconds that we assert the "Input" signal.
#define LOAD_DELAY		1


// Maximum hex digit pairs we can process on a single line.
#define MAX_HEX_PAIRS	16




int cycle_count = 0;

int cmd_count = 0;

	
#if ENABLE_LCD
// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidCrystal lcd(0);
#endif

// The 16-pin I2C port expander.
Adafruit_MCP23017 mcp;


//
// Modes
//
#define MODE_RESET		0
#define MODE_PAUSE		1
#define MODE_RUN		2
#define MODE_UPLOAD		3
#define MODE_DOWNLOAD	4

int mode;


//
// States
//
#define STATE_RESET		0
#define STATE_PAUSE		1
#define STATE_LOAD		2
#define STATE_RUN		3

int state;


//
// /Pins on the port expander
//
#define PIN_IN		4
#define PIN_BWAIT	5
#define PIN_BCLR	6
#define PIN_MWR		7

// Track the pin states. Yes, this wastes some memory with values we never use, but it keeps it simple.
uint8_t pinState[8];


uint16_t address;	// Current address for upload and download, also R0 when entering run mode.
uint8_t dataByte;	// The last byte read or written from the Membership Card.



//---------------------------------------------------------------------------
// Initial Setup
//---------------------------------------------------------------------------

void setup()
{
	Serial.begin( 19200 );

#if ENABLE_LCD
	// set up the LCD's number of rows and columns: 
	lcd.begin( 20, 4 );

	lcd.setBacklight( HIGH );

	// Display our banner on the LCD.
	lcd.print( "1802 Loader v1.0" );	
#endif
	
	//
	// IO Chip setup
	//
    mcp.begin(1);      // I2C address 1, the LCD is on address 0

	// Port A, low nybble - inputs for reading data from the 1802
    mcp.pinMode(0, INPUT);
    mcp.pinMode(1, INPUT);
    mcp.pinMode(2, INPUT);
    mcp.pinMode(3, INPUT);
	
	mcp.pullUp(0, HIGH);  // turn on a 100K pullup internally
	mcp.pullUp(1, HIGH);  // turn on a 100K pullup internally
	mcp.pullUp(2, HIGH);  // turn on a 100K pullup internally
	mcp.pullUp(3, HIGH);  // turn on a 100K pullup internally

	// Port A, high nybble - Outputs for the control signals
    mcp.pinMode(4, OUTPUT);		// Pin In
    mcp.pinMode(5, OUTPUT);
    mcp.pinMode(6, OUTPUT);
    mcp.pinMode(7, OUTPUT);

	// Port B all outputs, for the data write lines
    mcp.pinMode(8, OUTPUT);
    mcp.pinMode(9, OUTPUT);
    mcp.pinMode(10, OUTPUT);
    mcp.pinMode(11, OUTPUT);
    mcp.pinMode(12, OUTPUT);
    mcp.pinMode(13, OUTPUT);
    mcp.pinMode(14, OUTPUT);
    mcp.pinMode(15, OUTPUT);
	
	
	// Pause mode
	setPauseMode();
	
	disableWrite();

	setPin(PIN_IN, HIGH);
	
	setResetMode();
}



//---------------------------------------------------------------------------
// Serial Input
//---------------------------------------------------------------------------


/**
 * Read a line of serial data.
 *
 * Returns true if a line is available.
 */ 
const char *readCmdLine()
{
	static char cmdBuf[(MAX_HEX_PAIRS*2)+1];
	static int cmdIndex = 0;

	while( Serial.available() > 0 )
	{
		char c = Serial.read();
		if( c == '\r' || c == '\n' )
		{
			// Line terminator
			// Serial.print( "Zot\n" );
			if( cmdIndex > 0 )
			{
				// And the line contains some characters.
				cmdBuf[cmdIndex] = '\0';
				cmdIndex = 0;
				// Serial.print( "Zap\n" );
				return cmdBuf;
			}
		}
		else
		{
			if( cmdIndex < (sizeof(cmdBuf)-1) )
			{
				// Only store a character if there is room in the command buffer!
				// Overflow characters are dropped.
				cmdBuf[cmdIndex] = c;
				cmdIndex++;
			}
		}
	}
	
	return NULL;
}



//---------------------------------------------------------------------------
// Modes
//---------------------------------------------------------------------------


void setDownloadMode()
{	
	setState( STATE_RESET );

	setPin(PIN_IN, HIGH);
	
	setState( STATE_LOAD );

	enableWrite();

	mode = MODE_DOWNLOAD;
}


void setUploadMode()
{
	setState( STATE_RESET );

	disableWrite();
	
	setPin(PIN_IN, LOW);

	setState( STATE_LOAD );

	mode = MODE_UPLOAD;
}


void setPauseMode()
{
	setState( STATE_PAUSE );
	mode = MODE_PAUSE;
}


void setResetMode()
{
	setState( STATE_RESET );
	mode = MODE_RESET;	
}


void setRunMode()
{
	setState( STATE_PAUSE );

	setPin(PIN_IN, HIGH);

	enableWrite();

	setState( STATE_RUN );
	
	mode = MODE_RUN;
}



//---------------------------------------------------------------------------
// Control Signal States
//---------------------------------------------------------------------------


void setPin( uint8_t pin, uint8_t d )
{
	mcp.digitalWrite( pin, d );
	pinState[pin] = d;
	
	displayPins();
}


//
// BWAIT	BCLR	State
//	0		0		Load
//	0		1		Pause
//	1		0		Reset
//	1		1		Run
//
void setState( int st )
{
	switch( st )
	{
		case STATE_LOAD:
			setPin(PIN_BWAIT, LOW);
			setPin(PIN_BCLR, LOW);	
			break;
		
		case STATE_RUN:
			setPin(PIN_BCLR, HIGH);
			setPin(PIN_BWAIT, HIGH);
			break;
		
		case STATE_PAUSE:
			setPin(PIN_BCLR, HIGH);
			setPin(PIN_BWAIT, LOW);		
			break;
		
		case STATE_RESET:
			setPin(PIN_BWAIT, HIGH);
			setPin(PIN_BCLR, LOW);	
			address = 0;
			break;
	}
	
	state = st;

	displayAddressAndData();
}



void enableWrite()
{
	setPin(PIN_MWR, LOW);	
}


void disableWrite()
{
	setPin(PIN_MWR, HIGH);
}



//---------------------------------------------------------------------------
// Data in and out to 1802
//---------------------------------------------------------------------------


void outputByte( uint8_t b )
{
#if 1
	// Data is on Port B
	uint16_t w = b<<8;
	
	// Control pins on Port A
	w |= ( pinState[PIN_IN] ? 1 : 0 ) << PIN_IN;
	w |= ( pinState[PIN_BWAIT] ? 1 : 0 ) << PIN_BWAIT;
	w |= ( pinState[PIN_BCLR] ? 1 : 0 ) << PIN_BCLR;
	w |= ( pinState[PIN_MWR] ? 1 : 0 ) << PIN_MWR;
	
	// Write all of them.
	mcp.writeGPIOAB( w );
#else
	// Just write the data pins one at a time. This is slightly slower that doing a full AB write.
	for( int i=0; i<8; i++ )
	{
		uint8_t state = b & (1<<i) ? HIGH : LOW;
		mcp.digitalWrite( i+8, state );
	}
#endif
		
	dataByte = b;
	
	// We don't increment the address here because in some cases the user might place a byte on the
	// 1802 input port when not in Load mode.
}


/**
 * Read a byte from the Membership Card data port.
 *
 * This _will_ cycle the Input pin to HIGH and then back to LOW.
 * (neccessary to read both nybbles)
 */
uint8_t readByte()
{
	// Cause a load operation. For upload, the IN pin is normally low.
	setPin(PIN_IN, HIGH);
	delay( LOAD_DELAY );
	uint16_t high = mcp.readGPIOAB() & 0x000F;

	setPin(PIN_IN, LOW);
	delay( 10 );
	uint16_t low = mcp.readGPIOAB() & 0x000F;
	
	uint8_t b = low | (high<<4);

	dataByte = b;

	address++;
	
	return b;
}



//---------------------------------------------------------------------------
// Command Processing
//---------------------------------------------------------------------------

int processMode( const char *p )
{
	int rc = 0;
	switch( p[0] )
	{
		case 'D':
		case 'd':
			displayCommand( "Download" );
			setDownloadMode();
			setAddress( &p[1] );
			break;
			
		case 'U':
		case 'u':
			displayCommand( "Upload" );
			setUploadMode();
			setAddress( &p[1] );
			break;
			
		case 'C':
		case 'c':
			// Clear / Reset Mode
			displayCommand( "Reset" );
			setResetMode();
			break;
			
		case 'R':
		case 'r':
			// Run Mode
			displayCommand( "Run" );
			setRunMode();
			break;
			
		case 'P':
		case 'p':
			// Wait / Pause Mode
			displayCommand( "Pause" );
			setPauseMode();
			break;
			
		default:
			displayCommand( "Invalid Mode" );
			rc = -1;
			break;
	}
	
	return rc;
}


/**
 * Process a string of hex digits, sending them to the 1802.
 */
void processHex( const char *p )
{
	for(;;)
	{
		char c = *p++;
		if( c )
		{
			int high = convertHexDigit( c );

			c = *p++;
			if( c )
			{
				int low = convertHexDigit( c );
				
				if( high >=0 && low >= 0 )
				{
					uint8_t b = high<<4 | low;
										
#if ENABLE_LCD
					char buffer[16];
					PString mystring( buffer, sizeof(buffer) );
					mystring.print( "Write 0x" );	
					mystring.print( b, HEX );
					displayCommand( mystring );
#endif
					
					outputByte( b );

					// Cause a load operation. For download, the IN pin is normally high.
					setPin(PIN_IN, LOW);
					delay( LOAD_DELAY );
					setPin(PIN_IN, HIGH);	
					address++;
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}
	
	displayAddressAndData();
}


/**
 * Parse the hex value from p and place it on the data out pins of the port expander.
 *
 * This does _not_ cycle the Input pin.
 */
void processWrite( const char *p )
{	
	char *dummy;
	uint8_t b = (uint8_t)strtoul( p, &dummy, HEX );

	outputByte( b );

	displayAddressAndData();

#if ENABLE_LCD
	char buffer[16];
	PString mystring( buffer, sizeof(buffer) );
 
	mystring.print( "Out Byte 0x" );
	
	mystring.print( b, HEX );
		
	displayCommand( mystring );
#endif
}


/**
 * Read a byte from the Membership Card data out port.
 *
 * The Input pin will be cycled.
 */
void processRead( const char *p )
{
	char hex_buffer[(MAX_HEX_PAIRS*2)+1];
	PString hexstring( hex_buffer, sizeof(hex_buffer) );
	
	int numBytes = 1;
	
	if( *p )
	{		
		numBytes = atoi(p);
		// TODO: clip range!
	}
	
	for( int i=0; i<numBytes; i++ )
	{		
		uint8_t b = readByte();
	
		hexstring.format( "%02X", b );
	}	

	Serial.print( hex_buffer );

#if ENABLE_LCD
	char buffer[16];
	PString mystring( buffer, sizeof(buffer) );

	mystring.print( "Read 0x" );
	mystring.print( hex_buffer );
	
	displayCommand( mystring );
#endif
	
	displayAddressAndData();
}


/**
 * Set Address command.
 */
void processSetAddress( const char *p )
{
	char *dummy;
	uint16_t a = (uint16_t)strtoul( p, &dummy, HEX );

	setAddress( a );

#if ENABLE_LCD
	char buffer[16];
	PString mystring( buffer, sizeof(buffer) );
 
 
	mystring.format( "Set Addr 0x%04X", a );	
		
	displayCommand( mystring );	
#endif		
}


/**
 * Parse the hex number at p and set the address.
 */
void setAddress( const char *p )
{
	char *dummy;
	uint16_t a = (uint16_t)strtoul( p, &dummy, HEX );
	setAddress( a );
}


/**
 * Set the address.
 */
void setAddress( uint16_t a )
{
	disableWrite();
	
#if 0
	setState( STATE_RESET );
#else
	if( a > address )
	{
		a -= address;
	}
	else if( a < address )
	{
		// Reset
		setState( STATE_RESET );
	}
#endif
	
	setState( STATE_LOAD );
	
	for( uint16_t i=0; i<a; i++ )
	{
		// Cycle the input pin. Since we are in the load state, this will advance the program counter R0.
		if( pinState[PIN_IN] == HIGH )
		{
			setPin(PIN_IN, LOW);
			delay( LOAD_DELAY );
			setPin(PIN_IN, HIGH);
		}
		else
		{
			setPin(PIN_IN, HIGH);
			delay( LOAD_DELAY );
			setPin(PIN_IN, LOW);			
		}
		
		address++;
	}
	
	if( mode == MODE_DOWNLOAD )
	{
		enableWrite();
	}

	displayAddressAndData();
}


/**
 * Implement a raw control pin set.
 */
int processPinSet( uint8_t d, const char *p )
{
	int rc = 0;
	const char *cmd = "Set Pin";
	switch( *p )
	{
		case 'M':
		case 'm':
		 	setPin( PIN_MWR, d );
			break;
			
		case 'C':
		case 'c':
		 	setPin( PIN_BCLR, d );
			break;
			
		case 'W':
		case 'w':
		 	setPin( PIN_BWAIT, d );
			break;
			
		case 'I':
		case 'i':
		 	setPin( PIN_IN, d );
			break;

		default:
			cmd = "Invalid pin";
			rc = -1;
			break;
	}
	
	displayCommand( cmd );

	return rc;
}



//---------------------------------------------------------------------------
// LCD Support
//---------------------------------------------------------------------------

void displayCommand( const char *cmd )
{
#if ENABLE_LCD
	lcd.setCursor( 0, 0 );
	lcd.print( "                   " );
	lcd.setCursor( 0, 0 );
	
	int numbytes = strlen(cmd);
	for( int i=0; i<numbytes; i++ )
	{
		lcd.write( cmd[i] );
	}
#endif
}


void displayMode()
{
#if ENABLE_LCD
	lcd.setCursor( 0, 2 );
	lcd.print( "                   " );
	lcd.setCursor( 0, 2 );
	
	lcd.print( "Mode " );
	
	switch( mode )
	{
		case MODE_DOWNLOAD: lcd.print( "Download" );	break;
		case MODE_RUN: 		lcd.print( "Run" );			break;
		case MODE_RESET: 	lcd.print( "Reset" );		break;
		case MODE_PAUSE: 	lcd.print( "Pause" );		break;
		case MODE_UPLOAD: 	lcd.print( "Upload" );		break;
		default: 			lcd.print( "???" );			break;
	}
#endif
}


//
// BWAIT	BCLR	State
//	0		0		Load
//	0		1		Pause
//	1		0		Reset
//	1		1		Run
//
void displayPins()
{
#if ENABLE_LCD
	lcd.setCursor( 10, 3 );

#if 1
	// Textual state.
	if( pinState[PIN_BWAIT] == HIGH )
	{
		if( pinState[PIN_BCLR] == HIGH )
		{
			lcd.print( "Run  " );
		}
		else
		{
			lcd.print( "Reset" );			
		}
	}
	else
	{
		if( pinState[PIN_BCLR] == HIGH )
		{
			lcd.print( "Pause" );
		}
		else
		{
			lcd.print( "Load " );
		}		
	}
#else
	// Raw pin states.
	lcd.print( pinState[PIN_BWAIT] == HIGH ? "W" : "w" );
	lcd.print( pinState[PIN_BCLR] == HIGH ? "C" : "c" );
#endif	
		
	lcd.print( pinState[PIN_IN] == HIGH ? " I" : " i" );
	lcd.print( pinState[PIN_MWR] == HIGH ? " Rd" : " Wr" );
#endif
}


void displayAddressAndData()
{
#if ENABLE_LCD
	char hex_buffer[10];
	PString hexstring( hex_buffer, sizeof(hex_buffer) );
	
	hexstring.format( "%04X: %02X", address, dataByte );

	lcd.setCursor( 0, 3 );
	lcd.print( hexstring );
#endif
}



//---------------------------------------------------------------------------
// Utilities 
//---------------------------------------------------------------------------

// Returns 0-15 or -1 to indicate not a hex digit
int convertHexDigit( char c )
{
	if( c >= '0' && c <= '9' )
	{
		return c - '0';
	}
	
	if( c >= 'A' && c <= 'F' )
	{
		return ( c - 'A' ) + 10;
	}

	if( c >= 'a' && c <= 'f' )
	{
		return ( c - 'a' ) + 10;
	}
	
	return -1;
}



//---------------------------------------------------------------------------
// Main Loop and command parsing
//---------------------------------------------------------------------------

void parseCommand( const char *cmd )
{
	// Usually we send the ack character, but not in all cases.
	bool sendAck = true;
	
	int rc = 0;
	
	switch( cmd[0] )
	{
		case '*':
			// Set the high-level Mode
			rc = processMode( &cmd[1] );
			break;
			
		case '>':
			// Load a byte to the output port. Does not cycle Input pin.
			processWrite( &cmd[1] );
			break;
			
		case '<':
			// Read a byte or sequence of bytes from the input port.
			processRead( &cmd[1] );
			sendAck = false;
			break;
			
		case '^':
			// Set address.
			processSetAddress( &cmd[1] );
			break;
			
		case '+':
			// Set a control pin high.
			rc = processPinSet( HIGH, &cmd[1] );
			break;

		case '-':
			// Set a control pin low.
			rc = processPinSet( LOW, &cmd[1] );
			break;
			
		default:
			// If we are in download mode, assume these are hex digits.
			if( mode == MODE_DOWNLOAD )
			{
				// Hex digits?
				processHex( &cmd[0] );				
			}
			else
			{
				displayCommand( "Invalid Cmd" );
				rc = -1;
			}
			break;
	}	
	
	if( sendAck )
	{
		Serial.print( rc == 0 ? '!' : '#' );	// Tell host we are ready for a new command.
	}
}


/**
 * The main loop!
 */
void loop()
{
	static unsigned long previousMillis = 0;
	
	const char *cmd = readCmdLine();
	if( cmd )
	{
		parseCommand( cmd );
		cmd_count++;
	}
    
	unsigned long currentMillis = millis();
	if( currentMillis - previousMillis > 1000 )
	{
		previousMillis = currentMillis;   

#if ENABLE_LCD
		lcd.setCursor( 0, 1 );
		lcd.print( "Cy: " );
		lcd.print( cycle_count );

		// lcd.setCursor( 0, 2 );
		lcd.print( " Cmds: " );
		lcd.print( cmd_count );
#endif

		cycle_count++;
	}
}
