/*
 * Copyright (C) 2019 by John A. Krallmann
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOFTWARE.
 */

/* This program is for printing crossword puzzles (writes to stdout).
 * It was written for my own use, so is not fancy.
 * If given just a file name, it prints the grid with numbers.
 * If there is second argument of "answer" it prints the answer key.
 * If there is second argument of "clues" it prints a template for clues,
 *  having lines starting with the numbers that will needed for across and down.
 *
 * If there is second argument of "blank" it prints a completely empty grid
 * that is the size implied by the input file. The original idea was that
 * that could be used to try words with pencil and eraser while creating
 * a puzzle, but then I wrote cwm program to do that on screen,
 * so it's probably not very useful, and it would be better to have an option
 * to specify rows and columns rather than deduce from some file whose content
 * is ignored other then the length and number of lines.
 *
 * The input file is expected to have the same number of characters on each line.
 * Any non-alpha character will be considered to be a black square.
 * To be compatible with the cwm program,
 * it is recommended to use upper case letters, and dots for black squares.
 * There can optionally be a title line, starting with a tab.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define PAPERHEIGHT 11.0
#define PAPERWIDTH 8.5
#define TOPMARGIN 1.5
#define LEFTMARGIN 0.5
#define PPI 72.0
#define TITLESIZE 16
#define TITLELOC 0.7
#define SIZE 11
#define HPAD 2
#define VPAD 1


/* Convert row and column into an index into a one-dimensional array */

int
arr_index(int line, int col, int width)
{
	return(line*width + col);
}


/* Output PostScript to print lines to make a crossword grid. */

void
print_grid(int width, int height, double cellsize)
{
	int row, col;

	/* Horizontal lines */
	for (row = 0; row <= height; row++) {
		printf("newpath %f %f moveto %f %f lineto stroke\n",
				PPI * LEFTMARGIN,
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row),
				PPI * (LEFTMARGIN + cellsize * width),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row));
	}
	/* Vertical lines */
	for (col = 0; col <= width; col++) {
		printf("newpath %f %f moveto %f %f lineto stroke\n",
				PPI * (LEFTMARGIN + cellsize * col),
				PPI * (PAPERHEIGHT - TOPMARGIN),
				PPI * (LEFTMARGIN + cellsize * col),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * height));
	}
}


/* This always fills in the black squares.  If the "answer" parameter is true,
 * it prints the letters in their squares. Otherwise, it prints the numbers
 * in the upper left corner of squares that should have numbers.
 */

void
print_data(int width, int height, double cellsize, char *letters, short *numbers, int answer)
{
	int row, col;
	int letter_index;
	int number_index;

	for (row = 0; row < height; row++) {
		for (col = 0; col < width; col++) {

			letter_index = arr_index(row+1, col+1, width + 2);
			if (letters[letter_index] == '\0') {
				/* Fill in black square */
				printf("newpath %f %f moveto %f %f lineto %f %f lineto %f %f lineto closepath fill stroke\n",
				PPI * (LEFTMARGIN + cellsize * col),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row),
				PPI * (LEFTMARGIN + cellsize * (col+1)),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row),
				PPI * (LEFTMARGIN + cellsize * (col+1)),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * (row+1)),
				PPI * (LEFTMARGIN + cellsize * col),
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * (row+1)) );
			}
			else if (answer) {
				/* Print letter in square */
				int xoff, yoff;
				if (cellsize < 0.45) {
					xoff = 10;
					yoff = 19;
				}
				else {
					xoff = 12;
					yoff = 22;
				}
				printf("%f %f moveto (%c) show\n",
					PPI * (LEFTMARGIN + cellsize * col) + xoff,
					PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row) - yoff,
					letters[letter_index]);
			}

			number_index = arr_index(row, col, width);
			if ( ! answer && numbers[number_index] != 0) {
				/* Print number */
				printf("%f %f moveto (%d) show\n",
				PPI * (LEFTMARGIN + cellsize * col) + HPAD,
				PPI * (PAPERHEIGHT - TOPMARGIN - cellsize * row) - VPAD - SIZE,
				numbers[number_index]);
			}
		}
	}
}


/* This prints a template for clues.  It begins with a short nroff/troff/groff
 * preface, followed by "ACROSS" and "DOWN" lists. Each list consists of the
 * the numbers in that direction that need clues, one per line.
 * The puzzle maker can then add the actual clues.
 * The preface uses the "memorandum macro" .S (for size) and .2C (for
 * two columns). The user can manually add blanks lines to make the break
 * between ACROSS and DOWN coincide with the column break if they want.
 */

void
clues_template(int width, int height, char * letters, short * numbers)
{
	int row, col;
	int before, after;
	int letter_index;
	int number_index;

	printf(".nf\n.na\n.S 12\n.2C\nACROSS\n\n");
	for (row = 0; row < height; row++) {
		for (col = 0; col < width; col++) {
			letter_index = arr_index(row+1, col+1, width + 2);
			if (letters[letter_index] != '\0') {
				before = arr_index(row+1, col, width + 2);
				after = arr_index(row+1, col+2, width + 2);
				if (letters[before] == '\0' && letters[after] != '\0') {
					printf("%d. \n", numbers[arr_index(row, col, width)]);
				}
			}
		}
	}
	printf("\n\nDOWN\n\n");
	for (row = 0; row < height; row++) {
		for (col = 0; col < width; col++) {
			letter_index = arr_index(row+1, col+1, width + 2);
			if (letters[letter_index] != '\0') {
				before = arr_index(row, col+1, width + 2);
				after = arr_index(row+2, col+1, width + 2);
				if (letters[before] == '\0' && letters[after] != '\0') {
					printf("%d. \n", numbers[arr_index(row, col, width)]);
				}
			}
		}
	}
}


/* Print usage message and exit 1 */

void
usage(char * pname)
{
	(void) fprintf(stderr, "usage: %s infile [answer|blank|clues]\n", pname);
	exit(1);
}


int
main(int argc, char **argv)
{
	FILE * file;
	char buff[BUFSIZ];
	int lwidth;
	int width = -100;
	int height = 0;
	int row;
	int col;
	char *title = 0;
	char *letters;
	short *numbers;
	int num;
	double cellsize;
	int answer = 0;
	int blank = 0;
	int clues = 0;


	if (argc < 2) {
		usage(argv[0]);
	}

	if ((file = fopen(argv[1], "r")) == 0) {
		fprintf(stderr, "can't open %s\n", argv[1]);
		exit(1);
	}
	if (argc > 2) {
		if (strcmp(argv[2], "answer") == 0) {
			answer = 1;
		}
		else if (strcmp(argv[2], "blank") == 0) {
			blank=1;
		}
		else if (strcmp(argv[2], "clues") == 0) {
			clues=1;
		}
		else {
			usage(argv[0]);
		}
	}

	/* Read the lines of the input file, to determine
	 * how many rows and columns there are. */
	while (fgets(buff, sizeof(buff), file)) {
		if (buff[0] == '\t') {
			buff[strlen(buff)-1] = '\0';
			title = strdup(buff+1);
			continue;
		}
		lwidth = strlen(buff) - 1;
		height++;
		if (width < -2) {
			width = lwidth;
		}
		else if (width != lwidth) {
			fprintf(stderr, "line %d: expecting equal width lines, not %d and %d\n", height, lwidth, width);
			exit(2);
		}
	}

	fprintf(stderr, "%d wide, %d high\n", width, height);

	/* Allocate the right amount of space for the puzzle data */
	letters = calloc((width+2) * (height+2), sizeof(char));
	numbers = calloc((width*height), sizeof(short));

	/* Now that we know the size, read everything again,
	 * this time saving the data into internal array */
	rewind(file);
	for (row = 1; fgets(buff, sizeof(buff), file) != 0; row++) {
		if (buff[0] == '\t') {
			row--;
			continue;
		}
		strncpy(letters + arr_index(row, 1, width+2), buff, width);
	}

	/* change the non-alphas to \0 */
	for (row = 1; row <= height; row++) {
		for (col = 1; col <= width; col++) {
			if ( ! isalpha(letters[arr_index(row, col, width+2)])) {
				letters[arr_index(row, col, width+2)] = '\0';
			}
		}
	}

	/* Determine which squares should have numbers. */
	num = 1;
	for (row = 1; row <= height; row++) {
		for (col = 1; col <= width; col++) {
			char above, below, left, right;

			if (letters[arr_index(row, col, width+2)] == '\0') {
				continue;
			}
			above = letters[arr_index(row-1, col, width+2)];
			below = letters[arr_index(row+1, col, width+2)];
			left = letters[arr_index(row, col-1, width+2)];
			right = letters[arr_index(row, col+1, width+2)];

			if ( (above == '\0' && below != '\0') ||
					(left == '\0' && right != '\0') ) {
				numbers[arr_index(row-1, col-1, width)] = num;
				num++;
			}
		}
	}

	if (clues) {
		clues_template(width, height, letters, numbers);
		exit(0);
	}

	/* Start with max cell size of 1/2 inch. Then if that would make
	 * the puzzle too big to fit on the page, shrink to be no more than
	 * 7.5 inches wide (to leave 1/2 margins on the sides) and no more
	 * than 9 inches high (to leave 1 inch margins on top and bottom)
	 * assuming American 8.5x11 inch paper. */
	cellsize = 0.5;
	if (width > 15) {
		cellsize = 7.5 / width;
	}
	if (height > 20) {
		cellsize = 9.0 / height;
	}

	/* Output a PostScript header. */
	printf("%%!PS-Adobe-3.0\n");
	printf("%%%%Creator: cwp\n");
	printf("%%%%Title: crossword puzzle\n");
	printf("%%%%Pages: 1\n");
	printf("%%%%DocumentFonts: TimesRoman\n");
	printf("%%%%BoundingBox: 0 0 612 792\n");
	printf("%%%%DocumentMedia: Default 612 792 0 () ()\n");
	printf("%%%%Orientation: Portrait\n");
	printf("%%%%EndComments\n");
	printf("1 setlinewidth\n");
	printf("0.25 setgray\n");

	if (answer) {
		int asize;

		if (cellsize < 0.45) {
			asize = 14;
		}
		else {
			asize = 16;
		}
		printf("/CourierBold findfont %d scalefont setfont\n", asize);
		print_grid(width, height, cellsize);
		print_data(width, height, cellsize, letters, numbers, answer);
	}
	else if (blank) {
		print_grid(width, height, cellsize);
	}
	else {
		if (title != 0) {
			printf("/TimesBold findfont %d scalefont setfont\n", TITLESIZE);
			printf("%f 2.0 div %f mul %f %f mul moveto\n", PAPERWIDTH, PPI, PAPERHEIGHT - TITLELOC, PPI);
			printf("(%s) dup stringwidth exch -2.0 div exch rmoveto show\n", title);
	
		}
		printf("/TimesRoman findfont %d scalefont setfont\n", SIZE);
		print_grid(width, height, cellsize);
		print_data(width, height, cellsize, letters, numbers, answer);
	}

	/* Finish up the PostScript. */
	printf("showpage\n");
	printf("%%%%Trailer\n");
	
	exit(0);
}
