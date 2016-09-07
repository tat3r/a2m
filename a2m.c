#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_FG		7
#define DEFAULT_BG		0
#define DEFAULT_BOLD	false
#define DEFAULT_ICE		false

/* technically theres no limit in the format */
#define MAX_PARAMS	8

/* 4 bytes + '\0' */
#define MAX_UTFSTR	5

typedef struct cell_s {
	char *utfchar;
	int fg;
	int bg;
	bool bold;
	bool ice;
} cell_t;

int get_fgcolor(cell_t *cell);
int get_bgcolor(cell_t *cell);
void print_color(cell_t *cell, int col);
void inc_row(void);
void draw_cell(char *inchar, int fg, int bg, bool bold, bool ice);
void usage(void);

/* lol globals */

/* ANSI COLORS               BLK RED GRN YEL BLU MAG CYN WHT */
/*                            0   1   2   3   4   5   6   7  */
/* MIRC COLORS               ------------------------------- */
const char color[8] = {       1,  5,  3,  7,  2,  6, 10, 15 };
const char color_bold[8] = { 14,  4,  9,  8, 12, 13, 11,  0 };

/* canvas state */
cell_t **canvas;
char fg = DEFAULT_FG;
char bg = DEFAULT_BG;	
bool bold = false;
bool ice = false;

/* cursor state */
int row = -1;
int col = 0;
int bottomrow = -1;
int oldrow = 0;
int oldcol = 0;

/* options */
int max_cols = 80;
bool use_color = true;
bool show_unp = false;
int tab_size = 8;
int lcrop = 0;
int rcrop = 0;
int default_fg = 7;
int default_bg = 0;

int main(int argc, char *argv[]) {
	int opt;
	FILE *fh;

	while((opt = getopt(argc, argv, "npl:r:w:t:")) != -1) {
		switch(opt) {
			case 'n':
				use_color = false;
				break;
			case 'p':
				show_unp = true;
				break;
			case 'l':
				lcrop = atoi(optarg);
				break;
			case 'r':
				rcrop = atoi(optarg);
				break;
			case 'w':
				max_cols = atoi(optarg);
				break;
			case 't':
				tab_size = atoi(optarg);
				break;
			case '?':
				/* fallthrough */
			case 'h':
				/* fallthrough */
			default:
				usage();
				return (1);
		}
	}

	argc -= optind;
	argv += optind;

	/* init the first line */
	inc_row();	
	
	if (argc == 1) {
		fh = fopen(argv[0], "r");

	} else if (argc == 0) {
		fh = stdin;

	} else {
		usage();
		exit(EXIT_FAILURE);
	}

	if (!fh) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	char *charp = malloc(1);
	if (!charp) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	char *space = " ";

	int rc = 0;
	int pcnt = 0;
	int param[MAX_PARAMS];
	uint32_t ch = 0;

	while ((ch = fgetc(fh)) != EOF) {

		/* ignore sauce record */
		if (ch == 0x1A) {
			break;
		}

		/* drawable character */
		if (ch != 0x1B) {
			*charp = (char)ch;

			/* convert tabs to spaces */
			if (ch == '\t') {
				for (int i = 0; i < tab_size; i++) {
					draw_cell(space, fg, bg, bold, ice);
				}

			} else if (ch == '\0') {
				draw_cell(space, DEFAULT_FG, DEFAULT_BG, bold, ice);

			} else if (ch != '\r' && ch != '\n') {
				draw_cell(charp, fg, bg, bold, ice);
			}

			if (ch == '\n') {
				while (col != 0) {
					draw_cell(space, fg, bg, bold, ice);
				} 
			}

		/* escape code */
		} else {
			rc = 0;
			pcnt = 0;
			
			/* no rational way to recover tbqh imho */
			if ((ch = fgetc(fh)) != '[') {
				fprintf(stderr, "Invalid escape code, aborting at line %d\n",
					row + 1);
				exit(EXIT_FAILURE);
			}

			while ((ch = fgetc(fh)) != EOF) {
				
				/* parameters */
				if (isdigit(ch)) {
					*charp = (char)ch;
					rc = (rc * 10) + atoi(charp);

				/* oddball parameter prefixes */
				} else if (ch == '?' || ch == '=' || ch == '>') {
					/* just eat them */

				/* end of parameter, havent encountered ':' but its legit */
				} else if (ch == ';' || ch == ':') {
					param[pcnt] = rc;
					rc = 0;
					pcnt++;

				/* spacing */
				} else if (ch == 'C') {

					/* default to one space */
					if (rc == 0) {
						rc = 1;
					}
					for (int i = 0; i < rc; i++) {
						draw_cell(space, DEFAULT_FG, DEFAULT_BG,
							DEFAULT_BOLD, DEFAULT_ICE);

					}
					rc = 0;
					pcnt = 0;
					break;

				/* SGR sequence */
				} else if (ch == 'm') {
					param[pcnt] = rc;
					pcnt++;
					for (int i = 0; i < pcnt; i++) {

						/* reset */
						if (param[i] == 0) {
							bold = false;
							ice = false;
							fg = DEFAULT_FG;
							bg = DEFAULT_BG;

						} else if (param[i] == 1) {
							bold = true;
						} else if (param[i] == 5) {
							ice = true;
						} else if (param[i] == 21 || param[i] == 22) {
							bold = false;
						} else if (param[i] == 25) {
							ice = false;
						} else if (param[i] >= 30 && param[i] <= 37) {
							fg = param[i] - 30;
						} else if (param[i] >= 40 && param[i] <= 47) {
							bg = param[i] - 40;
						} else if (param[i] == 39) {
							fg = DEFAULT_FG;
						} else if (param[i] == 49) {
							bg = DEFAULT_BG;
						} else {
							fprintf(stderr,
								"Ignored SGR parameter %d at line %d\n",
								param[i], row + 1);
						}						
					}
					rc = 0;
					pcnt = 0;
					break;

				/* clear screen */
				/* todo flesh this out */
				} else if (ch == 'J') { 
					row = 0;
					col = 0;

					for (int i = 0; i <= bottomrow; i++) {
						for (int j = 0; j < max_cols; j++) {
							canvas[i][j].fg = DEFAULT_FG;
							canvas[i][j].bg = DEFAULT_BG;
							canvas[i][j].bold = DEFAULT_BOLD;
							canvas[i][j].ice = DEFAULT_ICE;

							if (canvas[i][j].utfchar) {
								free(canvas[i][j].utfchar);
							}
						}
					}
					rc = 0;
					pcnt = 0;
					break;

				/* move up */
				} else if (ch == 'A') {

					/* default is 1 */
					if (rc == 0) {
						rc = 1;
					}

					while (rc > 0) {

						/* found an ansi that tried this */
						/* probably from a bbs prelogin that assumed */
						/* frontdoor was eating the first few lines */
						if (row == 0) {
							break;
						}

						row--;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move down */
				} else if (ch == 'B') {

					if (rc == 0) {
						rc = 1;
					}

					while (rc > 0) {

						if (row == bottomrow) {
							break;
						}

						row++;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move forward */
				} else if (ch == 'C') {

					if (rc == 0) {
						rc = 1;
					}

					while (rc > 0) {

						if (col == max_cols - 1) {
							break;
						}

						col++;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move back */
				} else if (ch == 'D') {

					if (rc == 0) {
						rc = 1;
					}

					while (rc > 0) {

						if (col == 0) {
							break;
						}

						col--;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* goto */
				} else if (ch == 'f' || ch == 'H') {

					if (pcnt != 2) {
						break;
					}

					row = param[0];
					col = param[1];

					rc = 0;
					pcnt = 0;
					break;

				/* set mode */
				} else if (ch == 'h' || ch == 'n') {

					rc = 0;
					pcnt = 0;
					break;

				/* save position */
				} else if (ch == 's') {

					oldrow = row;
					oldcol = col;

					rc = 0;
					pcnt = 0;
					break;

				/* restore position */
				} else if (ch == 'u') {

					row = oldrow;
					col = oldcol;

					rc = 0;
					pcnt = 0;
					break;

				/* sequences we dont care and/or know about */
				} else {
					fprintf(stderr, "Ignored escape code [");
					for (int i = 0; i < pcnt; ++i) {
						fprintf(stderr, "%d%s", param[pcnt],
							i + 1 < pcnt ? ";" : "");
					}
					fprintf(stderr, "0x%02x at line %d col %d\n", ch,
						row + 1, col + 1);

					rc = 0;
					pcnt = 0;
					break;
				}
			}
		}
	}

	for (int i = 0; i <= row; i++) {
		for (int j = 0 + lcrop; j < max_cols - rcrop; j++) {
			cell_t *cell = &canvas[i][j];

			if (!cell->utfchar) {
				printf("\n");
				return 0;
			}

			if (use_color) {
				print_color(cell, j);
			}

			printf("%s", cell->utfchar);
		}
		printf("\n");
	}

	if (argc == 1) {
		fclose(fh);
	}

	return 0;
}

void usage(void) {
	fprintf(stderr, "Usage: a2m [options] input.ans\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:");
	fprintf(stderr, "\n");		
	fprintf(stderr, "    -l n      Crop n lines from the left side.\n");
	fprintf(stderr, "    -r n      Crop n lines from the right side.\n");
	fprintf(stderr, "    -n        Disable color output.\n");
	fprintf(stderr, "    -p        Print unprintable characters.\n");
	fprintf(stderr, "    -t size   Specify tab size, default is 8.\n");	
	fprintf(stderr, "    -w width  Specify width, default is 80.\n");	
	return;
}

int get_fgcolor(cell_t *cell) {
	return cell->bold ? color_bold[cell->fg] : color[cell->fg];
}

int get_bgcolor(cell_t *cell) {
	return cell->ice ? color_bold[cell->bg] : color[cell->bg];
}

void print_color(cell_t *cell, int col) {
	static cell_t *prev = NULL;

	int oldfg = (!prev || col == 0 + lcrop) ? DEFAULT_FG : prev->fg;
	int oldbg = (!prev || col == 0 + lcrop) ? DEFAULT_BG : prev->bg;
	int oldbold = (!prev || col == 0 + lcrop) ? DEFAULT_BOLD : prev->bold;
	int oldice = (!prev || col == 0 + lcrop) ? DEFAULT_ICE : prev->ice;

	if (cell->fg != oldfg ||
		cell->bg != oldbg ||
		cell->bold != oldbold ||
		cell->ice != oldice) {

		printf("\x03");

		if (cell->fg != oldfg || cell->bold != oldbold) {
			printf("%d", get_fgcolor(cell));
		}

		if (cell->bg != oldbg || (cell->ice != oldice)) {
			printf(",%d", get_bgcolor(cell));
		}
	}

	prev = cell;
	return;
}

void inc_row(void) {
	row++;

	if (row <= bottomrow) {
		return;
	}

	bottomrow = row;

	cell_t **newrows = (cell_t **)calloc(row + 1, sizeof(cell_t *));

	if (!newrows) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < row; i++) {
		newrows[i] = canvas[i];
	}

	newrows[row] = (cell_t *)calloc(max_cols, sizeof(cell_t));

	free(canvas);
	canvas = newrows;

	for (int j = 0; j < max_cols; j++) {
		canvas[row][j].fg = DEFAULT_FG;
		canvas[row][j].bg = DEFAULT_BG;
		canvas[row][j].bold = DEFAULT_BOLD;
		canvas[row][j].ice = DEFAULT_ICE;
	}
}

void draw_cell(char *inchar, int fg, int bg, bool bold, bool ice) {
	static iconv_t conv = (iconv_t)0;

	char *outcharp = calloc(1, MAX_UTFSTR);
	if (!outcharp) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}
	char *oldoutcharp = outcharp;

	if ((unsigned char)inchar[0] < 32 && !show_unp) {
				inchar[0] = ' ';
	}

	size_t incharsize = 1;
	size_t outcharsize = MAX_UTFSTR;

	if (!conv) {
		conv = iconv_open("UTF-8", "CP437");
	}

	iconv(conv, &inchar, &incharsize, &outcharp, &outcharsize);

	canvas[row][col].utfchar = calloc(1, MAX_UTFSTR);
	if (!canvas[row][col].utfchar) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	strcpy(canvas[row][col].utfchar, oldoutcharp);

	free(oldoutcharp);

	canvas[row][col].fg = fg;
	canvas[row][col].bg = bg;
	canvas[row][col].bold = bold;
	canvas[row][col].ice = ice;

	col++;

	if (col == max_cols) {
		inc_row();
		col = 0;
	}
	return;
}
