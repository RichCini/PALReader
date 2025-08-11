/*
	Read a 20L10 PAL and come up with a truth table. 

	For initial testing, using a knowd PAL from the S100 Computers board series,
	specifically the ISA-S100 bridge board. Two PALs (ISA_SEL and ABC) use a fixed
	12 inputs and 10 outputs, so none of the I/O pins are configured as inputs. That
	will be handled once this program is known to work.

	Some code adapted from examples found on-line, while other code was developed by
	me.

	2025/07/05	RAC	Started writing it.

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>			// for inp/outp
#include <dos.h>
#include <limits.h> 
#include <time.h>

// Defines
#define	BOARD_1 	0x260           // use this as the "output" card
#define BOARD_2 	0x268           // use this as the "input" card
#define PORTA 		0				// standard offsets from the card base	
#define PORTB		1
#define PORTC 		2
#define CTRL_REG	3
#define CREG1		BOARD_1 + CTRL_REG
#define PORTA1		BOARD_1 + PORTA
#define PORTB1		BOARD_1 + PORTB
#define PORTC1		BOARD_1 + PORTB
#define CREG2		BOARD_2 + CTRL_REG
#define PORTA2		BOARD_2 + PORTA
#define PORTB2		BOARD_2 + PORTB
#define PORTC2		BOARD_2 + PORTB
//#define INPUT_BITS 	20 			// number of input bits for an unknown PAL
#define INPUT_BITS	12				// 20L10 with known 12 inputs and 10 outputs
#define PAL_SIZE (1L<<INPUT_BITS)	// 2**number of inputs

// Typedefs
typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned long  ulong;

// Function to write a control word to the 8255
void set_8255_mode(uchar board, uchar control_word) {

	switch (board){
		case 1:
			*((ushort*)CREG1) = control_word;
			break;
		case 2:
			*((ushort*)CREG2) = control_word;
			break;
		default:
			break;		//eat the call
	}
}

// Function to write data to a port
void write_to_port(ushort port_address, uchar data) {
	*((ushort*)port_address) = data;
}

// Function to read data from a port
uchar read_from_port(ushort port_address) {
	return *((uchar*)port_address);
}

// In case needed for reassembling 
void write_24bit(uchar board, unsigned long data) {

	uchar chunk1, chunk2, chunk3;

	chunk1 = (uchar)((data >> 16) & 0xFF);		// Next most significant 8 bits
	chunk2 = (uchar)((data >> 8) & 0xFF);		// Middle 8 bits
	chunk3 = (uchar)(data & 0xFF);			// Least significant 8 bits

	switch (board){
		case 1:
			write_to_port(PORTA1,chunk3);
			write_to_port(PORTB1,chunk2);
			write_to_port(PORTC1,chunk1);
			break;
		case 2:
			write_to_port(PORTA2,chunk3);
			write_to_port(PORTB2,chunk2);
			write_to_port(PORTC2,chunk1);
			break;
		default:
			break;		//eat the call
	}
	return;
}

// In case needed for reassembling 
unsigned long read_24bit(ushort board) {

	uchar iv1, iv2, iv3 = 0;
	unsigned long int counter_value = 0;

	switch (board){
		case 1:
			iv3 = read_from_port(PORTA1) & 0xFF;	// LSB 8 bits
			iv2 = read_from_port(PORTB1) & 0xFF;	// second 8
			iv1 = read_from_port(PORTC1) & 0xFF;	// upper 8
			break;
		case 2:
			iv3 = read_from_port(PORTA2) & 0xFF;	// LSB 8 bits
			iv2 = read_from_port(PORTB2) & 0xFF;	// second 8
			iv1 = read_from_port(PORTC2) & 0xFF;	// upper 8
			break;
		default:
			return 0xFFFFFFFF;			// the IO board returns FF as default
	}

	counter_value = (iv1 << 16) | (iv2 << 8) | iv3;
	return counter_value;
}


int main(int argc, char *argv[]) {  
// argv[0] is exe name
// argv[1]... is various command options

	int usrchar, gal_word, block;
	int i, j;
	int test;		// enable test data
	clock_t t_start, t_end;
	double cpu_time_used;
	uchar control_word;  
	uchar iv1, iv2;
	uchar chunk1, chunk2, chunk3;
	ulong k;
	FILE  *outfile;

	srand((unsigned)time(NULL));
	printf("\nPAL 20L10 Reader\nFixed I/O Configuration Mode\n");

	// if no filename on the command line, assume it's in non-logging mode
	outfile = 0;
	block = 0;
	test = 0;

	for (i = 0; i < argc; i++){
		if (*argv[i] == '/')
			*argv[i] = '-';						// change switch character
		if (*argv[i] == '-'){ 					// if got an option
			if (toupper(argv[i][1]) == 'T'){
				test = 1;						// test mode flag
				printf("Test mode enabled.\n");
			}
			else {
				fprintf( stderr, "ERROR - option \"%s\" unknown.\n", argv[i]);
				exit(-1);
			}

			for (j = i; j < argc; j++){
				argv[j] = argv[j+1];            // shift things over/delete arg
			} 
			argc--;                        		// we have one less argument
			i--;                           		// back up
		} // if got an option
	} // check each argument

	if (argc >= 2){
		outfile = fopen(argv[1], "w");
		if ( !outfile ) {
			fprintf( stderr, "\nError - could not open %s\n", argv[1]);
			exit(1);
		}
		printf("Output File: %s\n", argv[1]);
	} // if file specified             
	
	// Configure boards
	// Board 1, Mode 0, all ports out and set LOW
	control_word = 0x80;
	set_8255_mode((ushort)1, control_word);
	write_24bit((uchar)1, 0L);

	// Board 2, Mode 0, all ports in
	control_word = 0x9B;
	set_8255_mode((ushort)2, control_word);

	// --- Data Transfer ---
	printf("\nPut device in reader and hit <Return> to start.\n");
	usrchar = getchar();

	// start timer
	t_start = clock();

	// Start sending simulated addresses.
	for (k=0; k<PAL_SIZE; k++){
		// 32-bit long holds the 20-bit data but I/O card is a 24-bit card
		// that's read in 8-bit chunks. May need to adjust if using smaller
		// PALs. This is probably painfully slow due to the repetitive 
		// I/O port accesses, although it's done by dereferencing a pointer 
		// to the port address rather than using inp/outp.

		// Extract the 8-bit chunks for blasting to the write ports
		chunk1 = (uchar)((k >> 16) & 0xFF);		// Next most significant 8 bits
		chunk2 = (uchar)((k >> 8) & 0xFF);		// Middle 8 bits
		chunk3 = (uchar)(k & 0xFF);			// Least significant 8 bits
		write_to_port(PORTA1,chunk3);
		write_to_port(PORTB1,chunk2);
		write_to_port(PORTC1,chunk1);

		// Read 10 bits back
		iv1 = read_from_port(PORTA2) & 0xFF;	// LSB 8 bits
		iv2 = read_from_port(PORTB2) & 0x03;	// second word with only 2 bits

		if (test){
			iv1 = (rand() % 256) & 0xFF;
			iv2 = (rand() % 256) & 0x03;
		}
		gal_word = (iv2 << 8) | iv1;

		if (k % 1024 == 0){
			printf("\nBlock %d read.\n", block);
			block++;
		}

		if ( outfile ) 
			fprintf(outfile,"%04x\n",gal_word);
	}

	if ( outfile )
		fclose(outfile);

	// get ending time
	t_end = clock();
	cpu_time_used = ((double) (t_end - t_start)) / CLOCKS_PER_SEC;
	printf("\nElapsed CPU time: %f seconds.\n", cpu_time_used);
	printf("\nAll done!\n");

    return 0;
}