#!/usr/bin/env python
#
# 1802 Membership Card Loader
#
# Copyright (c) 2015, Donald T. Meyer
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ------------------------------------------
#
# sudo easy_install -U bincopy
# sudo easy_install -U pyserial
#
#
# https://pypi.python.org/pypi/bincopy
#
#
# ------------------------------------------
# Import S-Records
#    or
# Import binary image
#
# Download it to the Membership Card
#   - Serial transmission to the Arduino
# ------------------------------------------
#


import os, sys, csv, datetime, time
import codecs
import optparse
import re
import StringIO
import string

import serial
import serial.tools
import serial.tools.list_ports

import bincopy

# If the assembler is available, set a flag true, otherwise false.
# This way we can use the assmebler if possible, but not cause an error if it's not present.
try:
	import cosmacasm
	asmAvailable = True
except ImportError, e:
	if e.message != 'No module named cosmacasm':
		raise
	asmAvailable = False



# After we connect to the Arduino serial port, the board resets. This is the delay to let that
# complete and the program to start running before we send commands.
resetDelayTime = 2

# Mainly for debugging...
cmdDelayTime = 0.0

serialAckTimeout = 5

serialPort = None

MAX_HEX_PAIRS = 16



#----------------------------------------------------------------
# File read/write
#----------------------------------------------------------------


def readSRecordFile( file ):
	pass



def readImageFile( file ):
	pass



#----------------------------------------------------------------
# Serial Communication
#----------------------------------------------------------------


def listPorts():
	ports = serial.tools.list_ports.comports()
	for a,b,c in ports:
		print( a, b, c )
		


#
# Pick a serial port to use and retunr the port name.
#
# If no port found, or multiple ports found, this will return None.
# Ignores ports with "Bluetooth" in the name.
#		
def pickPort():
	ports = serial.tools.list_ports.comports()
	portname = None
	for name,b,c in ports:
		# print( name )
		if "Bluetooth" in name:
			# print( "skip", name )
			continue
		if portname == None:
			portname = name
		else:
			# Too many ports
			print( "Too many ports" )
			return None
	
	if portname == None:
		print( "No ports found" )
		
	return portname
	

	
#
# Open the given serial port and configure it.
#
# If the port cannot be opened, this will terminate the program.
#
def _openSerialPort( portName ):
	global serialPort
	serialPort = serial.Serial( port=portName, baudrate=options.baud, timeout=serialAckTimeout )
	if serialPort:
		logVerbose( "Serial port open: '%s'" % portName )
		
		# Wait until the Arduino resets and starts running.
		time.sleep( resetDelayTime )
	else:
		bailout( "Unable to open serial port '%s'" % portName )
	


#
# Select and then open the serial port.
#
# If no port specified and an appropriate port cannot be found, this will terminate the program.
#
def openSerialPort():
	global options, serialPort
	if serialPort:
		# already open
		return
	if options.port != None:
		_openSerialPort( options.port )
	else:
		port = pickPort()
		if port:
			_openSerialPort( port )
		else:
			bailout( "Unable to automatically choose a port" )


	
#----------------------------------------------------------------
#
#----------------------------------------------------------------

def sendCmd( cmd, ack=True ):
	logDebug( "Send command: '%s'" % cmd )
	if options.noaction:
		return			
	serialPort.write( "%s\n" % cmd )
	if ack:
		b = serialPort.read(1)
		if b == '!':
			pass
		elif b == '#':
			bailout( "Error returned for command '%s'" % cmd )
		else:
			bailout( "Timeout waiting for ACK of cmd '%s'" % cmd )
	time.sleep( cmdDelayTime )

	

def rcvByte():
	s = serialPort.read(2)
	# print( "'%s'" % s )
	b = int( s, 16 )
	logDebug( "Received byte: 0x%02X" % b )
	return b
	


def setDownloadMode( addr ):
	logVerbose( "Download mode" )
	sendCmd( "*D%04X" % addr )


def setUploadMode( addr ):
	logVerbose( "Upload mode" )
	sendCmd( "*U%04X" % addr )


#
# Reset the 1802
#
def setResetMode():
	logVerbose( "Reset mode" )
	sendCmd( "*C" )


#
# Start/continue program execution
#
def setRunMode():
	logVerbose( "Run mode" )
	sendCmd( "*R" )




def writeDataByte( b ):
	logDebug( "Write data 0x%02X" % ( b ) )
	sendCmd( "%02x" % b )



def readDataBytes( count ):
	sendCmd( "<%d" % count, ack=False )
	bytes = bytearray()
	for i in range(count):
		b = rcvByte()
		bytes.append(b)
	return bytes
	



#----------------------------------------------------------------
#
#----------------------------------------------------------------


def importHex( filename ):
	global dataImage
	bb = bytearray()
	
	src = open( filename, 'r' )
	lines = src.readlines()
	src.close()

	for line in lines:
		line = string.strip( line )
		if len(line) == 0 or line[0] == '#':
			continue
		m = re.findall( r'([a-fA-F0-9][a-fA-F0-9])', line )
		for num in m:
			b = string.atoi( num, 16 )
			bb.append( b )
	
	dataImage.add_binary( bb )	
	


def dataToHexStrings( data ):
	buf = ""
	bytes = 0
	for b in data:
		buf = buf + "%02X" % b
		bytes += 1
		if bytes >= 16:
			buf += "\n"
			bytes = 0

	return buf

	

def ximportFile( src, filename ):
	global dataImage
	dataImage = bincopy.BinFile()
	bb = bytearray( b'\x7A\x7b\x30\x00' )
	zz = StringIO.StringIO( bb )
	dataImage.add_binary( zz )	
	



def importFile( filename ):
	global dataImage
	dataImage = bincopy.BinFile()
	
	# Step 1, what kind of file?
	type = getFileType( filename )
	if type == "srecord":
		dataImage.add_srec_file( filename )
	elif type == "binary":
		dataImage.add_binary_file( filename )
	elif type == "intelhex":
		dataImage.add_ihex_file( filename )
	elif type == "hex":
		importHex( filename )
	else:
		bailout( "Unknown file type" )
		


#
# Returns "srecord", "hex", "intelhex", "binary", or None
#
def getFileType( filename ):
	m = re.search(".*\\.(.+)$", filename , re.S)
	logDebug( m )
	if m:
		ext = m.group(1).lower()
		logDebug( ext )
		if ext == "s19":
			return "srecord"
		elif ext == "bin":
			return "binary"
		elif ext == "hex":
			return "hex"
		elif ext == "ihex":
			return "intelhex"

	# TODO: implement this!
	# If we get here, need to look at the file contents to try and tell
	src = open( filename, 'r' )
	line = src.readline()
	src.close()
	
	return None



def exportImage( dest ):
	if options.format == "srec":
		dest.write( dataImage.as_srec() )
	elif options.format == "ihex":
		dest.write( dataImage.as_ihex() )
	elif options.format == "hex":
		data = dataImage.as_binary()
		buf = dataToHexStrings( data )
		dest.write( buf )
	else:
		bailout( "Invalid export format '%s'" % options.format )
	


def dumpImage():
	if options.format == "srec":
		print( dataImage.as_srec() )
	elif options.format == "ihex":
		print( dataImage.as_ihex() )
	elif options.format == "hex":
		data = dataImage.as_binary()
		buf = dataToHexStrings( data )
		print( buf )
	else:
		bailout( "Invalid export format '%s'" % options.format )



#----------------------------------------------------------------
#
#----------------------------------------------------------------


def downloadAction( filename ):
	global options
	
	# Open serial port
	if options.noaction == False:
		openSerialPort()
		
	# Import the file
	importFile( filename )
	
	start = time.time()

	# TODO: reconcile address embedded in S-Record file with explicit address if given
	# addr = dataImage.get_minimum_address()
	addr = options.addr
	setDownloadMode( addr )
	
	# Download it to the 1802
	bytes = 0
	buf = ""
	data = dataImage.as_binary()
	for b in data:
		buf = buf + "%02X" % b
		bytes += 1
		if bytes >= MAX_HEX_PAIRS:
			sendCmd( buf )
			bytes = 0
			buf = ""
		# writeDataByte( b )
	if bytes > 0:
		sendCmd( buf )
		
	finish = time.time()
	logVerbose( "Download Done, %d bytes in %.1f seconds." % ( len(data), (finish - start) ) )

	
	
#
# Read bytes from the 1802
#
def uploadAction( filename ):
	global options
	global dataImage
	
	# Open serial port
	if options.noaction:
		return
			
	openSerialPort()

	# TODO: reconcile address emebedded in S-Record file with explicit address if given
	# a = dataImage.get_minimum_address()
	setUploadMode( options.addr )
	
	bb = bytearray()

	remaining = options.size
	while remaining > 0:
		size = remaining
		if size > MAX_HEX_PAIRS:
			size = MAX_HEX_PAIRS
		bytes = readDataBytes( size )
		for b in bytes:
			bb.append( b )
		remaining -= size
					
	# for i in range( options.size ):
	# 	bytes = readDataBytes(1)
	# 	bb.append( bytes )

	dataImage = bincopy.BinFile()

	dataImage.add_binary( bb )	

	dest = open( filename, 'w' )

	exportImage( dest )

	dest.close()



def runAction():
	setResetMode()
	setRunMode()
	
	
		
def terminalAction():
	global serialPort
	openSerialPort()
	print "1802 Membership Card Loader Terminal Mode"
	print "Enter an 'x' or 'q' followed by Enter to quit."
	print "All commands that are understood by the Arduino 1802 Loader can be entered."
	print "In addition, you can download a file to the 1802 by using the '@<filename>' command."
	print "(e.g.  >@test.hex)"

	while True:
		sys.stdout.write( ">" )
		line = sys.stdin.readline().strip()
		logDebug( "Line '%s' length %d" % (line, len(line) ) ) 
		if len(line) == 0:
			pass
		elif line[0] == 'x' or line[0] == 'q':
			# End terminal mode
			return
		elif line[0] == '@':
			filename = line[1:]
			downloadAction( filename )
		else:
			sendCmd( line, ack=False )	# Don't ask for ack, we handle the response manually below.
	
			c = serialPort.read(1)
			# If first character is an "ack", ignore it. (quiet success)
			if c == '!':
				logDebug( "Got bang")
			elif c == '#':
				print( "ERROR" )
			elif c == None or c == "":
				print( "*** No response to command" )
			else:
				if line[0] == '<':
					# Special case for reading bytes back - determine how many characters to expect so we don't have to wait for the timeout
					expect = int( line[1:] ) * 2
				else:
					expect = None
				sys.stdout.write( c )
				while True:
					logDebug( "read 1 char 0x%02X" % ord(c[0]) )
					c = serialPort.read(1)
					if c == None or c == "":
						logDebug( "read none" )
						sys.stdout.write( '\n' )
						break
					else:
						logDebug( "read %d chars '%c'" % ( len(c), c[0] ) )
						sys.stdout.write( c )
					sys.stdout.flush()
					
					if expect:
						expect -= 1
						if expect <= 1:		# Look for one less due to the first char read that is always done
							print
							break

					
				


def assemble( filename ):
	if asmAvailable == False:
		print "Assembler not available. Is 'cosmacasm.py' in the same directory as mcard.py?"
		print "You can also add the path to 'cosmacasm.py' to the PYTHONPATH environment variable."
		sys.exit(1)
	print "Assembling", filename
	# Call the assembler module.
	cosmacasm.process( filename )
	


#----------------------------------------------------------------
#
#----------------------------------------------------------------


def logVerbose( msg ):
	if options.verbose > 0:
		print( msg )



def logDebug( msg ):
	if options.verbose > 1:
		print( msg )


def bailout( msg ):
	print( "*** %s" % msg )
	sys.exit( -1 )
	


def main( argv ):
	global options

	usage = """%prog [options] <file>
	If no port specified, trys to pick an appropriate one."""

	parser = optparse.OptionParser(usage=usage)
	
	parser.add_option( "-a", "--addr",
						action="store", type="int", dest="addr", default=0,
						help="Address to start from when uploading and downloading. Default is 0x0000" )
	
	parser.add_option( "-s", "--size",
						action="store", type="int", dest="size", default=256,
						help="Number of bytes to upload. Default is 256." )
	
	parser.add_option( "-b", "--baud",
						action="store", type="int", dest="baud", default=19200,
						help="Serial port baud rate. Default is 9600." )
	
	parser.add_option( "", "--dump",
						action="store_true", dest="dump",
						help="Dump data downloaded or uploaded to terminal" )
	
	parser.add_option( "-m", "--mode",
						action="store", type="string", dest="mode", default=None,
						help="Mode. Can be: Load, Pause, Reset, Run" )
	
	parser.add_option( "", "--format",
						action="store", type="string", dest="format", default="srec",
						help="Export format. Can be: srec,ihex,hex" )
	
	parser.add_option( "-d", "--download",
						action="store_true", dest="download", default=False,
						help="Download data to the 1802" )
	
	parser.add_option( "-u", "--upload",
						action="store_true", dest="upload", default=False,
						help="Upload data from the 1802." )
	
	parser.add_option( "-r", "--run",
						action="store_true", dest="run", default=False,
						help="Run program" )
	
	parser.add_option( "-t", "--terminal",
						action="store_true", dest="terminal", default=False,
						help="Enter simple terminal mode" )
	
	parser.add_option( "-n", "--noaction",
						action="store_true", dest="noaction", default=False,
						help="Simulate the action, don't communicate with the 1802 or write any files." )
	
	parser.add_option( "-p", "--port",
						action="store", type="string", dest="port", default=None,
						help="Serial port to use. If not given, a best guess is made when possible." )
	
	parser.add_option( "-l", "--listPorts",
						action="store_true", dest="listPorts", default=False,
						help="List the available serial ports" )
	
	parser.add_option( "-q", "--quiet",
						action="store_const", const=0, dest="verbose",
						help="Little to no progress logging to terminal." )
	
	parser.add_option( "-v", "--verbose",
						action="store_const", const=1, dest="verbose", default=1,
						help="Progress logging to terminal. [default]" )
	
	parser.add_option( "--noisy",
						action="store_const", const=2, dest="verbose",
						help="Extensive progress and diagnostic logging to terminal." )
	
	(options, args) = parser.parse_args(argv)
	
	logDebug( options )
	logDebug( args )

	logDebug( "Address: 0x%04X" % options.addr )

	if options.download == False and options.upload == False and options.run == False \
	 and options.listPorts == False and options.terminal == False:
		bailout( "No action chosen! Must be download, upload, run, list ports, or terminal" )


	#
	# Prime Actions
	#

	if options.listPorts == True:
		listPorts()
		sys.exit( 0 )			
			
			
	if options.download == True:
		# Get the filename
		if len(args) > 1:
			filename = args[1]
		else:
			print "No file name given."
			print main.__doc__
			sys.exit(1)

		rootname, ext = os.path.splitext( filename )
		if ext == ".src":
			assemble( filename )
			filename = rootname + ".hex"
			
		# Perform the download
		downloadAction( filename )
	
	
	if options.upload == True:
		# Get the filename
		if len(args) > 1:
			filename = args[1]
		else:
			print "No file name given."
			print main.__doc__
			sys.exit(1)
		
		# Perform the upload
		uploadAction( filename )
	
	
	#
	# Optional add-on actions
	#
	if options.dump == 1:
		dumpImage()
	

	if options.run:
		if options.noaction == False:
			openSerialPort()
			runAction()


	if options.terminal:
		if options.noaction == False:
			terminalAction()


	#
	# Clean up
	#
	if serialPort:
		serialPort.close()



if __name__ == '__main__':
	sys.exit( main(sys.argv) or 0 )

