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

/* This program is for help with creating crossword puzzles.
 * It can probably be run on almost any system that supports X-windows.
 *
 * The size can be specified either via -h and -w arguments
 * for height and width, or -s size argument that applies to both dimensions.
 * Default is 15x15 (which is a common "daily newspaper" size).
 *
 * Final command line argument is a file name.
 * If the file exists, it will be read in to populate the puzzle.
 * In any case, it will be used to write out the puzzle at the 'Q' command.
 * The format of the file is one line per row, with the letters,
 * or a dot for a black square.
 * There can optionally be a title, on a line starting with a tab.
 * 
 * Mirroring of black squares is enforced, but other rules (e.g. 3-letter
 * minimum words, and all letters being used both down and across)
 * are not enforced. Only rectangular grids are directly supported,
 * although other shapes could be effectively obtained by blackening squares,
 * as long as the shape is symmetrical, such that mirroring works.
 *
 * User interface:
 * - any lower case letter will have its corresponding upper case letter
 *   placed in the square of the cursor.
 * - a dot will blacken the square of the cursor and its mirrored square.
 * - a space will clear the square of the cursor. If the mirrored square was
 *   currently blackened, it will also be cleared.
 *  [The three commands above also move to the next square
 *  in the current direction, unless already at the edge.]
 * - Backspace will move back one square in the current direction,
 *   unless already at the edge.
 * - Tab will flip the direction between vertical and horizontal.
 *   The color of the cursor will change to show the current direction.
 * - Arrow keys move in the appropriate direction.
 * - Q will write out the puzzle to the file and exit the program.
 * - X will exit the program without writing the file.
 *
 * Typical command for compiling:
 *	cc -o cwm cwm.c -lX11
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>


/* Most rows or columns allowed */
#define MAXSQ 75
/* When to switch to smaller cells */
#define BIG_PUZZLE 25
#define HUGE_PUZZLE 42
#define ENORMOUS_PUZZLE 54
/* Direction: horizontal or vertical */
#define HOR 0
#define VERT 1

/* Define all the X-window things. Define globally so don't have to
 * keep passing to all the functions. */
static Display *Display_p;
static int Xscreen;
static XImage *Image_p;
static XFontStruct *Font_info_p;
static GC gc;                   /* X graphics context */
static Window Win;
static unsigned int Width, Height;
static XSizeHints Size_hints;   /* tell window manager what size we want */
static unsigned long Foreground; /* color */
static unsigned long Background; /* color */
static int Rows, Cols;
/* The size of one square, in pixels */
static int Boxsize;
/* This is the puzzle contents */
static char Puzzle[MAXSQ][MAXSQ];
/* Optional title for the puzzle */
static char *Title;
/* Current direction and cursor position */
static int Dir = HOR;
static int Cur_row = 0;
static int Cur_col = 0;

/* Function declarations */
void color_square(int row, int col, unsigned long color);
void add_letter(int row, int col, char letter);
void show(void);
void read_puzzle(char *filename);
void write_puzzle(char *filename);
int user_interface(void);
void set_cursor(void);


/* Print the given message, and exit non-zero (specifically, 2) */

void
error(char *format, ...)
{
	va_list args;

	va_start(args, format);
	(void) vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(2);
}


/* Print usage message, and exit */

void
usage(char *pname)
{
	error("usage: %s [-h N] [-s N] [-w N] file\n"
		"  -h N   height of N rows\n"
		"  -w N   width of N columns\n"
		"  -s N   square with N rows of N columns\n"
		"  file   contains the puzzle, with dots for black squares\n",
		pname);
}


int
main(int argc, char **argv)
{
	int x, y;
	int border_width;
	char *display_name = NULL;
	char *window_name = argv[0];
	unsigned long valuemask = 0;
	XGCValues values;
	Pixmap icon_pixmap = 0;
	/* For keyboard/mouse input */
	XEvent event;
	/* Command line argument index */
	int a;
	char *font;


	/* Set default size */
	Rows = Cols = 15;

	/* Process command line arguments */
	while ((a = getopt(argc, argv, "h:s:w:")) != -1) {
		switch (a) {
		case 'h':
			Rows = atoi(optarg);
			break;
		case 's':
			Rows = Cols = atoi(optarg);
			break;
		case 'w':
			Cols = atoi(optarg);
			break;
		case '?':
			usage(argv[0]);
			break;
		}
	}

	/* Validate the arugments */
	if (Rows < 3 || Cols < 3 || Rows > MAXSQ || Cols > MAXSQ) {
		error("invalid size (%d x %d)\n", Rows, Cols);
	}

	if (optind != argc - 1) {
		usage(argv[0]);
	}

	/* Decide how big to make the boxes.
	 * If a large puzzle, make the boxes smaller.
	 * Depending on screen resolution, these may not be good.
	 * Could improve this some day, to scale based on screen size
	 * and/or allow user to specify.
	 */
	if (Rows > ENORMOUS_PUZZLE || Cols > ENORMOUS_PUZZLE) {
		Boxsize = 14;
	}
	else if (Rows > HUGE_PUZZLE || Cols > HUGE_PUZZLE) {
		Boxsize = 19;
	}
	else if (Rows > BIG_PUZZLE || Cols > BIG_PUZZLE) {
		Boxsize = 24;
	}
	else {
		Boxsize = 36;
	}

	/* Read in the existing puzzle, if any, or initialize to all blanks */
	read_puzzle(argv[optind]);

	/* Do all the X-window setup */
	if ( (Display_p = XOpenDisplay(display_name)) == NULL) {
		error("can't open display");
	}
	Xscreen = DefaultScreen(Display_p);

	Background = WhitePixel(Display_p, Xscreen);
	Foreground = BlackPixel(Display_p, Xscreen);

	x = 50;
	y = 50;
	border_width = 5;
	Width = Cols * Boxsize + 1;
	Height = Rows * Boxsize + 1;
	Win = XCreateSimpleWindow(Display_p, RootWindow(Display_p, Xscreen),
			x, y, Width, Height, border_width,
			Foreground, Background);
	Size_hints.flags = PSize | PMinSize | PMaxSize;
	Size_hints.width = Width;
	Size_hints.height = Height;
	Size_hints.min_width = Width;
	Size_hints.max_width = Width;
	Size_hints.min_height = Height;
	Size_hints.max_height = Height;

	XSetStandardProperties(Display_p, Win, window_name, window_name,
			icon_pixmap, argv, argc, &Size_hints);

	XSelectInput(Display_p, Win, ExposureMask | KeyPressMask |
                        KeyReleaseMask | ButtonPressMask | StructureNotifyMask);

	/* Select a font, bigger if the squares are larger */
	if (Rows > ENORMOUS_PUZZLE || Cols > ENORMOUS_PUZZLE) {
		font = "7x13";
	}
	else if (Rows > BIG_PUZZLE || Cols > BIG_PUZZLE) {
		font = "9x15";
	}
	else {
		font = "12x24";
	}
	if ((Font_info_p = XLoadQueryFont(Display_p, font)) == NULL) {
		error("can't load font");
	}
	gc = XCreateGC(Display_p, Win, valuemask, &values);
	XSetFont(Display_p, gc, Font_info_p->fid);
	XSetForeground(Display_p, gc, Foreground);
	XSetBackground(Display_p, gc, Background);
	XSetLineAttributes(Display_p, gc, 1, LineSolid, CapButt,
			JoinRound);

	XMapWindow(Display_p, Win);
	XWindowEvent(Display_p, Win, ExposureMask, &event);

	show();
	XFlush(Display_p);

	/* Main user interface */
	if (user_interface()) {
		write_puzzle(argv[optind]);
	}
	exit(0);
}


/* Fill in the square at the given row and column with the given color. */

void
color_square(int row, int col, unsigned long color)
{
	XSetForeground(Display_p, gc, color);
	XFillRectangle(Display_p, Win,gc, col * Boxsize+2, row * Boxsize+2,
					Boxsize-1, Boxsize-1);
	XSetForeground(Display_p, gc, Foreground);
}


/* Fill in the square at the given row and column with the given letter.
 * If the "letter" is a dot, blacken both the current square and its mirror.
 * If the square was previously black, but now given a letter or space,
 * clear its mirror.
 */

void
add_letter(int row, int col, char letter)
{
	char str[4];
	int mirror_row, mirror_col;
	int xadj, yadj;

	mirror_row = Rows - row - 1;
	mirror_col = Cols - col - 1;
	Puzzle[row][col] = letter;
	if (letter == '.') {
		color_square(row, col, BlackPixel(Display_p, Xscreen));
		Puzzle[mirror_row][mirror_col] = letter;
		color_square(mirror_row, mirror_col, BlackPixel(Display_p, Xscreen));
	}
	else {
		str[0] = letter;
		str[1] = '\0';
		if (Rows > ENORMOUS_PUZZLE || Cols > ENORMOUS_PUZZLE) {
			xadj = 2;
			yadj = 4;
		}
		else if (Rows > HUGE_PUZZLE || Cols > HUGE_PUZZLE) {
			xadj = 3;
			yadj = 5;
		}
		else if (Rows > BIG_PUZZLE || Cols > BIG_PUZZLE) {
			xadj = 4;
			yadj = 7;
		}
		else {
			xadj = 6;
			yadj = 10;
		}
		XDrawString(Display_p, Win, gc, col*Boxsize + Boxsize/2 - xadj + 1,
				row*Boxsize + Boxsize / 2 + yadj + 1, str, 1);
		if (Puzzle[mirror_row][mirror_col] == '.') {
			Puzzle[mirror_row][mirror_col] = ' ';
			color_square(mirror_row, mirror_col, WhitePixel(Display_p, Xscreen));
		}
	}
}


/* Display the puzzle */

void
show(void)
{
	int line;
	int offset;
	int row;
	int col;

	/* Draw the horizontal lines */
	for (line = 0; line <= Rows; line++) {
		offset = line * Boxsize + 1;
		XDrawLine(Display_p, Win, gc, 0, offset, Width, offset);
	}
	/* Draw the vertical lines */
	for (line = 0; line <= Cols; line++) {
		offset = line * Boxsize + 1;
		XDrawLine(Display_p, Win, gc, offset, 0, offset, Height);
	}
	/* Fill in the squares */
	for (row = 0; row < Rows; row++) {
		for (col = 0; col < Cols; col++) {
			if (isupper(Puzzle[row][col]) || Puzzle[row][col] == '.') {
				add_letter(row, col, Puzzle[row][col]);
			}
			else if (Puzzle[row][col] != ' ') {
				error("unexpected character '%c'", Puzzle[row][col]);
			}
		}
	}
	set_cursor();
	XFlush(Display_p);
}


/* Read puzzle from file if it exists; otherwise initialize to all empty. */

void
read_puzzle(char *filename)
{
	FILE *f;
	char buff[MAXSQ];
	int leng;
	char *end;	/* location of carriage return or newline */
	int row;
	int col;

	if ((f = fopen(filename, "r")) == NULL) {
		/* No existing file. Initialize to all spaces. */
		for (row = 0; row < Rows; row++) {
			for (col = 0; col < Cols; col++) {
				Puzzle[row][col] = ' ';
			}
		}
		return;
	}

	row = 0;
	while (fgets(buff, sizeof(buff), f)) {
		if (buff[0] == '\t') {
			/* Optional title */
			Title = strdup(buff+1);
			continue;
		}
		if (row >= Rows) {
			error("more than %d rows", Rows);
		}

		/* Remove carriage returns / newlines */
		if ((end = strpbrk(buff, "\r\n")) != 0) {
			*end = '\0';
		}

		leng = strlen(buff);
		if (leng != Cols) {
			error("line length (%d) not equal to number of columns (%d)", leng, Cols);
		}
		for (col = 0; col < leng; col++) {
			Puzzle[row][col] = buff[col];
		}
		row++;
	}
	fclose(f);
	if (row != Rows) {
		error("Too few rows; expecting %d, got %d", Rows, row);
	}
}


/* Write out the contents of puzzle to given file. */

void
write_puzzle(char *filename)
{
	FILE *f;
	int row;
	int col;

	if ((f = fopen(filename, "w")) == NULL) {
		error("cannot open file for writing");
	}
	if (Title != 0){
		fprintf(f, "\t%s", Title);
	}
	for (row = 0; row < Rows; row++) {
		for (col = 0; col < Cols; col++) {
			putc(Puzzle[row][col], f);
		}
		putc('\n', f);
	}
	fclose(f);
}


/* Get input from user, and do what they say. */

int
user_interface(void)
{
	XEvent report;          /* what X event happened */
	char inpbuff[BUFSIZ];
	KeySym keysym;
	XComposeStatus compose;

	while(1) {
		XNextEvent(Display_p, &report);
	
		switch(report.type) {

		case Expose:
			while(XCheckTypedEvent(Display_p, Expose, &report)) {
				;
			}
			show();
			break;

		case ConfigureNotify:
			if (report.xconfigure.width != Width) {
				error("wrong width");
			}
			if (report.xconfigure.height != Height) {
				error("wrong height");
			}
			break;

		case ButtonPress:
			/* Mouse click. Set cursor to where mouse is */
			Cur_col = report.xbutton.x / Boxsize;
			Cur_row = report.xbutton.y / Boxsize;
			break;

		case KeyPress:
			XLookupString(&(report.xkey), inpbuff, BUFSIZ, &keysym,
						&compose);
			switch (keysym) {

			case 'Q':
				/* Write out and quit */
				return(1);

			case 'X':
				/* Quit without writing */
				return(0);

			/* Arrow keys */
			case XK_Up:
				if (Cur_row > 0) {
					Cur_row--;
				}
				break;
			case XK_Down:
				if (Cur_row < Rows - 1) {
					Cur_row++;
				}
				break;
			case XK_Left:
				if (Cur_col > 0) {
					Cur_col--;
				}
				break;
			case XK_Right:
				if (Cur_col < Cols - 1) {
					Cur_col++;
				}
				break;

			case XK_Tab:
				/* Flip directions */
				Dir = (Dir == HOR ? VERT : HOR);
				break;

			case XK_BackSpace:
				if (Dir == HOR && Cur_col > 0) {
					Puzzle[Cur_row][Cur_col] = ' ';
					Cur_col--;
				}
				if (Dir == VERT && Cur_row > 0) {
					Puzzle[Cur_row][Cur_col] = ' ';
					Cur_row--;
				}
				break;

			default:
				if ((keysym >= 'a' && keysym <= 'z')
							|| (keysym == ' ')
							|| (keysym == '.')) {
					add_letter(Cur_row, Cur_col, toupper(keysym));
					/* Move curser if not at edge */
					if (Dir == HOR && Cur_col < Cols - 1) {
						Cur_col++;
					}
					if (Dir == VERT && Cur_row < Rows - 1) {
						Cur_row++;
					}
				}
				break;
			}
			break;
		}
		set_cursor();
	}
}


/* Draw the cursor */

void
set_cursor(void)
{
	/* The old_* keep track of previous location, so where to erase */
	static int old_row = -1;
	static int old_col = -1;
	unsigned long color;

	if (old_row >= 0) {
		color = (Puzzle[old_row][old_col] == '.'
				? BlackPixel(Display_p, Xscreen)
				: WhitePixel(Display_p, Xscreen));
		color_square(old_row, old_col, color);
		if (isupper(Puzzle[old_row][old_col])) {
			add_letter(old_row, old_col, Puzzle[old_row][old_col]);
		}
	}

	/* Use different cursor colors for vertical and horizontal,
	 * and if on a blackened or normal square. */
	if (Puzzle[Cur_row][Cur_col] == '.') {
		color = (Dir == HOR ? 0x20ff20 : 0x2020ff);
	}
	else {
		color = (Dir == HOR ? 0xc0ffc0 : 0xc0c0ff);
	}
	/* Fill in the square having cursor with the correct color */
	color_square(Cur_row, Cur_col, color);
	/* If has a letter in it, put that back */
	if (isupper(Puzzle[Cur_row][Cur_col])) {
		add_letter(Cur_row, Cur_col, Puzzle[Cur_row][Cur_col]);
	}

	/* Keep track of where we are now, for next cursor move. */
	old_row = Cur_row;
	old_col = Cur_col;
}
