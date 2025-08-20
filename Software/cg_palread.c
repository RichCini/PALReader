/*  Read a 10L8 PAL and come up with a truth table.
 *
 *		Uses the little board with the 393s
 *
 * Chuck Guzis
 * https://forum.vcfed.org/index.php?threads/cloning-a-pal-hal-part-3.1069763/
 *
 * NOTE that the PAL_DATA is read from BASE+1 (Status Port) BIT 7 which is
 *	BUSY (pin 11 of the DB25)
 *
 * Complied with MSVC 1.52 under Windows 3.1; MS-DOS EXE target.
 * 
 * This version is modified to accommodate the 16L8 which has a larger read
 * array.
 */

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>			// for inp/outp
#include <dos.h>         
#include <ctype.h>
#include <limits.h>

#define PRINTER 0x378			// printer base port
#define PAL_DATA 0x80			// bit representing the PAL data 
#define PAL_RESET 0x01			// reset PAL D0
#define PAL_INCR  0x02			// increment count D7
#define CLOCK_PULSE { outp(PRINTER, 3); outp(PRINTER, 1); }

// I think PAL_BITS is the total of the bits needed to shift the data from the LS151 and
// the addresses through the LS393 shift registers (12 for a 10L8 and 16 for a 16L8 with
// six nudge resistors
//#define PAL_COUNT 1024			// how many total count? 2^10 10L8
//#define INPUT_BITS 10
//#define PAL_BITS 16			// how many bits out for a 10L8

#define INPUT_BITS 16       		// total inputs for 16L8 10 in-only and 6 I/O
#define PAL_SIZE (1L << INPUT_BITS)	// 2**number of inputs. Since it's used as a counter
					// could use size-1 and change the loop to <=
//#define PAL_SIZE 65536
#define PAL_BITS  16			// how many bits to clock for a 16L8

FILE *outfile;

void main( int argc, char *argv[])
{

	int j, usrchar;
	unsigned int tbit;		// 16-bits on Win16
	unsigned long i, pd;		// always 32 bits
	//unsigned int i, pd;

	printf("16L8 device reader using parallel port.\n");
	printf("Sample size %u\n", PAL_SIZE-1);
	printf("Insert device into reader, plug in, and hit <Return>\n");
	usrchar = getchar();

	//  Open the output file
	outfile = 0;
	if (argc >= 2)
	{
		outfile = fopen(argv[1], "w");
		if ( !outfile )
		{
			fprintf( stderr, "\nError - could not open %s\n", argv[1]);
			exit(1);
		}
	} // if file specified


	//  Set the printer port up.  Hold reset active.  Note that increment
	//  happens on low-to-high transition.

	outp( PRINTER, 0);			// clock low, reset active

	outp( PRINTER, 1);			// release reset

	for (i = 0; i < PAL_SIZE; i++)
	{
		pd = 0;
		for ( j = 0; j < PAL_BITS; j++)
		{
			tbit = (inp(PRINTER+1) & 0x80) ? 1 : 0;
			pd |= tbit << j;
			CLOCK_PULSE;
		} // for each bit

		// rather than spamming the screen with each number, we print something
		// every 1k
		if (i % 1024 == 0){
			//printf("%05x\n",i);
			printf(".");
		}

		// note that the bits are inverted, likely because the printer port signal
		// used -- *BUSY -- is negative-true.
		if (outfile)
			fprintf( outfile, "%04x\n", ~pd);
	} // for each byte

	if ( outfile )
		fclose( outfile );
	printf( "\nAll done.\n");

} // end of main

