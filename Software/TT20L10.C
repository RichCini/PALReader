//*  Reduce a "brute force" PAL16L8 dump file.
//   -----------------------------------------
//
// This will also probably work on several different PALs, such as the 10L8
// as well as certain GALs, provided that the GAL is replacing a PAL design.
//
//    Copyright 2011, C. P. Guzis
//    All rights reserved.
//
//    This program, nor any derivative works may be sold without the
//    express permission of C. P. Guzis.  (i.e. Don't sell this thing
//    and then ask me to support it.)
//
//  NOTE:  This requires 32-bit C and assumes ints to be 32 bits. Compiled
//			with OpenWatcom C compiler with 32-bit extended DOS application
//			profile.
//
//  RAC Notes:
//    A PAL20L10 has the following configuration:
//      * 12 dedicated input pins
//      *  2 dedicated output pins
//      *  8 definable I/O pins
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

//  Constants.

#define INPUT_BITS 20 							// number of input bits
#define DEDICATED_INPUT_BITS 12                 // input bits that are immutable
#define DEDICATED_INPUTS 0x1fff					// which inputs are dedicated as a mask
#define OUTPUT_BITS 10 							// number of output bits
#define DEDICATED_OUTPUTS 0x201                 // dedicated output bits
#define PAL_SIZE (1L<<INPUT_BITS)               // 2**number of inputs
#define PAL_NAME "PA20L10" 						// What is this thing

unsigned char
		Sample[PAL_SIZE]; 						// An array of sample data.

//unsigned int
unsigned long
		InMask,									// mask of effective input bits
		OutMask; 								// mask of effective output bits

// probably OK as a 16-bit int as it's a count of lines and not a mask
int
		InBitCount,								// count of input lines
		OutBitCount;							// count of output lines

char   ProgramName[_MAX_FNAME] = {0};			// the actual name of the program
char   BareFile[_MAX_FNAME] = {0};				// The name of our sample file, sans
												// path or extension.

void FindDontCareBits( void);
//void MaskPrint( FILE *Where, unsigned int What, unsigned int Mask);
void MaskPrint( FILE *Where, unsigned long What, unsigned long Mask);
//int CountBits( unsigned int What);
int CountBits( unsigned long What);

//* TT16L8 - Create a simplified table display from a PALREAD sample file.
//  ======================================================================
//
//  Intended for PAL16L8 devices, but should work with most other 20-pin
//  non-registered PALs.
//
//  Note that this requires 32-bit C because of the array size.
//

void main( int argc, char *argv[])
{

		FILE
				*in,
				*out;                               // input and output files

		unsigned int
				pos;                                // used to index through the sample array

//		unsigned int
		unsigned long
				selMask;                            // mask used to select entries

		int
				espresso,                           // nonzero if produce input for espresso
				optimize,                           // nonzero if optimzation pass
				i, j;                               // usual index stuff

//  Names of pins on the PAL20L10.

		char  *inPins[] =
		{
				"Pin1", "Pin2", "Pin3", "Pin4", "Pin5", "Pin6", "Pin7", "Pin8", "Pin9",
				"Pin10", "Pin11", "Pin13", "Pin22", "Pin21", "Pin20", "Pin19", "Pin18",
				"Pin17", "Pin16", "Pin15"
		};

		char *outPins[] =
		{
				"Pin23", "Pin22", "Pin21", "Pin20", "Pin19", "Pin18", "Pin17", "Pin16",
				"Pin15", "Pin14"
		};

//  Get the program name.

		_splitpath( argv[0], NULL, NULL, ProgramName, NULL);
		argc--;
		argv++;

//  Sign on.

		printf( "\n %s - Read PAL20L10 map file and produce table.\n\n",
				ProgramName);

//  See if there are any options; currently we only recognize -o

		espresso = 0;                         // assume minilog output
		optimize = 0;                         // assume no optimization
		for ( i = 0; i < argc; i++)
		{
				if ( *argv[i] == '/')
						*argv[i] = '-';                   // change switch character
				if ( *argv[i] == '-')
				{ // if got an option
						if ( toupper( argv[i][1]) == 'O')
								optimize = 1;                     // say we optimize
						else if ( toupper( argv[i][1]) == 'E')
								espresso = 1;                     // say we generate espresso input
						else
						{
								fprintf( stderr, "ERROR - option \"%s\" unknown.\n", argv[i]);
								exit(-1);
						}

						for ( j = i; j < argc; j++)
						{
								argv[j] = argv[j+1];            // shift things over
						} // delete the argument just processed
						argc--;                         // we have one less argument
						i--;                            // back up
				} // if got an option
		} // check each argument

//  Open files; if mis-specified, produce a help message.

		if ( argc != 2)
		{ // wrong number of arguments
				fprintf( stderr, "\nERROR - you need an input sample file and an\n"
								"output text file for either Minilog or Espresso.\n");
				fprintf( stderr,
								"\nSyntax is:\n"
								"  %s [-e] [-o] sample-file  text-file\n"
								"\nWhere -e if specified, produces Espresso output instead\n"
								"of Minilog.  If -o is specified, aggressive optimization is\n"
								"is performed (may lead to inaccurate results).", ProgramName);
				exit(1);
		}

//  Get the bare file name for the report.

		_splitpath( argv[0], NULL, NULL, BareFile, NULL);

//  Open the input and output files.

		in = fopen( argv[0],"rb");
		if (!in)
		{
				fprintf( stderr, "\nERROR - cannot open %s\n", argv[1]);
				exit(2);
		}

		out = fopen( argv[1],"w");
		if ( !out)
		{
				fprintf( stderr, "\nERROR - could not create/open %s\n", argv[2]);
				exit(3);
		}

//  Read in the samples.
		printf("\nBuffering sample file, size %u samples.\n",PAL_SIZE);
		memset( Sample, 0, PAL_SIZE);         // clear the sample array
		fread( Sample, sizeof(char), PAL_SIZE, in);  // read them in
		fclose(in);                           // okay we're done

//  Next, figure which of the programmable lines are outputs. Unsigned int gets
//  16 bits.

		OutMask = 0;                       // 2 of the bits are fixed
		for ( pos = 0; pos < PAL_SIZE; pos++)
		{
				OutMask |=
					//	(((unsigned char) (pos >> DEDICATED_INPUT_BITS-1)) ^ Sample[pos]);
						(((unsigned int) (pos >> DEDICATED_INPUT_BITS-1)) ^ Sample[pos]);
		} // search for inputs/outputs.

//  Apologies for the shift constant below.  If we accumulate an 8 bit mask of
//  outputs-as-inputs, it will have the form 0xxxxxx0, so we have to shift it
//  to append to the head of the dedicated input mask.

// 16-bits? Probably need a long here as we need at least 20 bits.
//		InMask = 0xffff ^
		InMask = 0x000fffff ^ 
				((OutMask & ~DEDICATED_OUTPUTS) << (DEDICATED_INPUT_BITS-1));

		InBitCount = CountBits( InMask);
		OutBitCount = CountBits( OutMask);     // get bitcount of both in and out

//  Show the outputs and inputs.

		printf( "\nBefore optimization:\n"
				"  Input mask = %08x (%d bits); Output mask = %04x (%d bits)\n",
				 InMask, InBitCount, OutMask, OutBitCount);

		if ( optimize)
		{ // if we're optimizing
				FindDontCareBits();

				InBitCount = CountBits( InMask);
				OutBitCount = CountBits( OutMask);     // get bitcount of both in and out

//  Show the outputs and inputs again.

				printf( "\nAfter optimization:\n"
						"  Input mask = %08x (%d bits); Output mask = %04x (%d bits)\n",
						 InMask, InBitCount, OutMask, OutBitCount);
		} // if doing optimization

//  Create the Minilog or espresso file.

		if ( espresso)
				fprintf( out, "#   File for %s\n"
					".i %d\n"
					".o %d\n"
					".ilb ", BareFile, InBitCount, OutBitCount);
		else
				fprintf( out, "TABLE %s\n\n  INPUT  ", BareFile);

		for ( i = INPUT_BITS-1; i >= 0; i--)
		{
				if (InMask & (1<<i))
						fprintf( out, "%s ", inPins[i]);
		}

		if ( espresso)
				fprintf( out, "\n.ob ");
		else
				fprintf( out, "\n\n  OUTPUT  ");

		for ( i = OUTPUT_BITS-1; i >= 0; i--)
		{
				if ( OutMask & (1<<i))
						fprintf( out, "%s ", outPins[i]);
		}
		fprintf( out, "\n\n");

		selMask = ~InMask;
		for ( pos = 0; pos < PAL_SIZE; pos++)
		{

				if ( !(pos & selMask))
				{ // print only those values that don't include reassigned bits
						fprintf( out, "  ");
						MaskPrint( out, (unsigned int) pos, InMask); // print inputs
						fprintf( out, "   ");

//  We print the complement of the output bits as the 16L8 outputs are
//  low-true.

						MaskPrint( out, ~Sample[pos], OutMask);
						fprintf( out, " \n");
				}
		} // for
		if ( espresso)
				fprintf(out,".e\n");
		else
				fprintf(out,"END\n");

		fclose( out);
		fclose( in);
		printf( "\nAll Done -- %s file created.\n",
				espresso ? "Espresso" : "Minilog");
		exit(0);
}  // end of main

//* FindDontCareBits - Locate any don't care bits.
//  ----------------------------------------------
//
//    It's obvious that not all bits have the same effect on outputs.
//    Check each of the inputs and determine if they factor in output.
//    If not, remove it from the input mask.  Similarly, if an output
//    bit is always the same, remove it from the output mask.
//

void FindDontCareBits( void)
{

		unsigned int j;							// index variable
		//unsinged int i;						// PAL size on a 20L device exceeds 64K bits.
		unsigned long i;
		//unsigned int sigMask;
		unsigned long sigMask;					// significance mask

		printf( "\nRemoving \"don\'t care\" states...\n");

		sigMask = 0;                          // say nothing signif
		for( i = 0; i < PAL_SIZE; i++)
		{
				for ( j = 0; j < INPUT_BITS; j++)
				{

						unsigned int k = (1<<j);          // select bit

						if ( k & InMask)
						{
								if ( Sample[i] != Sample[i ^ k])
								{
										sigMask |= k;                 // say significant
								} // significant input bit
						} // if this is a bit to consider
				} // for all of the input bits
		} // for the entire set of inputs

		printf( "Old input mask = %04x, New input mask = %04x\n", InMask, sigMask);
		InMask &= sigMask;

//  Now, locate any irrelevant outputs.  That's one that never changes,
//  regardless of input.

		sigMask = 0;                          // say nothing significant
		for( i = 1; i < PAL_SIZE; i++)        // note that we start at 1, not 0
		{

				if ( !(i & ~InMask))
				{
						for ( j = 0; j < OUTPUT_BITS; j++)
						{

								unsigned int k = (1<<j);          // select bit
 
								if ( k & OutMask)
								{
										if ( (Sample[i] & k) != (Sample[0] & k))
										{ // compare against the first
												sigMask |= k;                 // say significant
										} // significant output bit
								} // if this is a bit to consider
						} // for all of the output bits
				} // if input is relevant
		} // for the entire set of inputs

;  printf( "Old output mask = %03x, New output mask = %03x\n", OutMask, sigMask);
		OutMask &= sigMask;
		return;
} // FindDontCareBits


//* CountBits - Return the count of 1 bits.
//  ---------------------------------------
//
//    On entry, a 16-bit unsigned int.
//    On return, the number of 1 bits in the word.
//

//int CountBits( unsigned int What)
int CountBits (unsigned long What)
{
//		unsigned int k = 0x8000;
		unsigned long k = 0x80000000;
		int count = 0;

		do
		{
				if ( What & k)
						count++;
				k = k >> 1;
		} while(k);

		return count;
} // CountBits


//* MaskPrint - Print a binary string, using a mask.
//  ------------------------------------------------
//
//  On entry,
//    Stream descriptor of file to receive output.
//    16-bit unsigned value of item to print.
//    16-bit unsigned value of print mask; contains "1" in significant
//      positions.
//

//void MaskPrint( FILE *Where, unsigned int What, unsigned int Mask)
void MaskPrint( FILE *Where, unsigned long What, unsigned long Mask)
{

//		unsigned int
		unsigned long
				k;

//		k = 0x8000;                           // process bits high-to-low
		k = 0x80000000;
		do
		{
				if ( k & Mask)
				fputc( ((k & What) ? '1' : '0'), Where);
				k = k >> 1;
		} while( k);
} // MaskPrint
