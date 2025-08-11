//	Read a 10L8 PAL and come up with a truth table.
//
//		Uses the little board with the 393s
//
// Chuck Guzis
//https://forum.vcfed.org/index.php?threads/cloning-a-pal-hal-part-3.1069763/


#include <stdlib.h>
#include <stdio.h>
#include <dos.h>

#define PRINTER 0x378			// printer base port
#define PAL_DATA 0x80			// bit representing the PAL data
#define PAL_RESET 0x01			// reset PAL
#define PAL_INCR  0x02			// increment count
#define PAL_COUNT 1024			// how many total count?
#define PAL_BITS  16			// how many bits out
#define CLOCK_PULSE { outp( PRINTER,3); outp(PRINTER,1); }

FILE *outfile;

void main( int argc, char *argv[])
{

  int i,j;
  unsigned int pd,tbit;

//  Open the output file

  outfile = 0;
  if (argc >= 2)
  {
    outfile = fopen( argv[1], "w");
    if ( !outfile)
    {
      fprintf( stderr, "\nError - could not open %s\n", argv[1]);
      exit(1);
    }
  } // if file specified


//  Set the printer port up.  Hold reset active.  Note that increment
//  happens on low-to-high transition.

  outp( PRINTER, 0);			// clock low, reset active

  outp( PRINTER, 1);			// release reset

  for (i = 0; i < PAL_COUNT; i++)
  {
    pd = 0;
    for ( j = 0; j < PAL_BITS; j++)
    {
      tbit = (inp(PRINTER+1) & 0x80) ? 1 : 0;
      pd |= tbit << j;
      CLOCK_PULSE;
    } // for each bit
    printf( "%04x\n", ~pd);
    if (outfile)
      fprintf( outfile, "%04x\n", ~pd);
  } // for each byte
  if ( outfile)
    fclose( outfile);
  printf( "\nAll done.\n");
} // end of main

