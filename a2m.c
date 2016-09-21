#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <getopt.h>

#define DPRINTF(fmt, ...) if (DEBUG) do {				       \
	fprintf(stderr, fmt, __VA_ARGS__);				       \
} while (0)

#define DEFAULT_FG	7
#define DEFAULT_BG	0
#define DEFAULT_BOLD	false
#define DEFAULT_ICE	false
#define MAX_PARAMS	16

/* 4 bytes + '\0' */
#define MAX_UTFSTR	5

typedef struct cell_s {
	char	*utfchar;
	int	fg;
	int	bg;
	bool	bold;
	bool	ice;
} cell_t;

typedef struct canvas_s {
	cell_t	**cell;		/* canvas state */
	char	fg;
	char	bg;
	bool	bold;
	bool	ice;
	int	row;		/* cursor state */
	int	col;
	int	bottomrow;
	int	oldrow;
	int	oldcol;
	int	max_cols;	/* options */
	bool	use_color;
	bool	show_unp;
	int	tab_size;
	int	lcrop;
	int	rcrop;
	int	default_fg;
	int	default_bg;
} canvas_t;

int	get_fgcolor(cell_t *cell);
int	get_bgcolor(cell_t *cell);
void	print_color(canvas_t *c, cell_t *cell, int col);
void	inc_row(canvas_t *c);
void	draw_cell(canvas_t *c, char *inch, int fg, int bg, bool bold, bool ice);
void	usage(void);

/* lol globals */

/*
 * ANSI COLORS               BLK RED GRN YEL BLU MAG CYN WHT
 *                            0   1   2   3   4   5   6   7
 */
const char color[8] = {       1,  5,  3,  7,  2,  6, 10, 15 };
const char color_bold[8] = { 14,  4,  9,  8, 12, 13, 11,  0 };

int main(int argc, char *argv[]) {
	int opt;
	FILE *fd;

	canvas_t *c;

	c = (canvas_t *)calloc(1, sizeof(canvas_t));

	c->fg = DEFAULT_FG;
	c->bg = DEFAULT_BG;
	c->bold = false;
	c->ice = false;

	c->row = -1;
	c->col = 0;
	c->bottomrow = -1;
	c->oldrow = 0;
	c->oldcol = 0;

	c->max_cols = 80;
	c->use_color = true;
	c->show_unp = false;
	c->tab_size = 8;
	c->lcrop = 0;
	c->rcrop = 0;
	c->default_fg = 7;
	c->default_bg = 0;

	while((opt = getopt(argc, argv, "npl:r:w:t:")) != -1) {
		switch(opt) {
		case 'n':
			c->use_color = false;
			break;
		case 'p':
			c->show_unp = true;
			break;
		case 'l':
			c->lcrop = atoi(optarg);
			break;
		case 'r':
			c->rcrop = atoi(optarg);
			break;
		case 'w':
			c->max_cols = atoi(optarg);
			break;
		case 't':
			c->tab_size = atoi(optarg);
			break;
		case '?':
			/* fallthrough */
		case 'h':
			/* fallthrough */
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* init the first line */
	inc_row(c);

	fd = NULL;

	if (argc == 1)
		fd = fopen(argv[0], "r");
	else if (argc == 0)
		fd = stdin;
	else {
		usage();
	}

	if (!fd) {
		perror(NULL);
		exit(EX_NOINPUT);
	}

	char *charp = malloc(1);

	if (!charp) {
		perror(NULL);
		exit(EX_OSERR);
	}

	char *space = " ";

	int rc = 0;
	int pcnt = 0;
	int param[MAX_PARAMS];
	uint32_t ch = 0;

	while ((ch = fgetc(fd)) != EOF) {

		/* ignore sauce record */
		if (ch == 0x1a)
			break;

		/* drawable character */
		if (ch != 0x1b) {
			*charp = (char)ch;

			/* convert tabs to spaces */
			if (ch == '\t')
				for (int i = 0; i < c->tab_size; i++)
					draw_cell(c, space, c->fg, c->bg, c->bold, c->ice);
			else if (ch == '\0')
				draw_cell(c, space, DEFAULT_FG, DEFAULT_BG, c->bold, c->ice);
			else if (ch != '\r' && ch != '\n')
				draw_cell(c, charp, c->fg, c->bg, c->bold, c->ice);

			if (ch == '\n')
				while (c->col != 0)
					draw_cell(c, space, c->fg, c->bg, c->bold, c->ice);

		/* escape code */
		} else {
			rc = 0;
			pcnt = 0;

			/* no rational way to recover tbqh imho */
			if ((ch = fgetc(fd)) != '[') {
				fprintf(stderr, "Invalid escape code, aborting at line %d\n",
					c->row + 1);
				exit(EX_DATAERR);
			}

			while ((ch = fgetc(fd)) != EOF) {

				/* parameters */
				if (isdigit(ch)) {
					*charp = (char)ch;
					rc = (rc * 10) + atoi(charp);

				/* oddball parameter prefixes */
				} else if (ch == '?' || ch == '=' || ch == '>') {
					/* just eat them for now */

				/* end of parameter, havent encountered ':' but its legit */
				} else if (ch == ';' || ch == ':') {
					param[pcnt] = rc;
					rc = 0;
					pcnt++;

				/* spacing */
				} else if (ch == 'C') {
					/* default to one space */
					if (rc == 0)
						rc = 1;
					for (int i = 0; i < rc; i++) {
						draw_cell(c, space, DEFAULT_FG, DEFAULT_BG,
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
							c->bold = false;
							c->ice = false;
							c->fg = DEFAULT_FG;
							c->bg = DEFAULT_BG;
						} else if (param[i] == 1)
							c->bold = true;
						else if (param[i] == 5)
							c->ice = true;
						else if (param[i] == 21 || param[i] == 22)
							c->bold = false;
						else if (param[i] == 25)
							c->ice = false;
						else if (param[i] >= 30 && param[i] <= 37)
							c->fg = param[i] - 30;
						else if (param[i] >= 40 && param[i] <= 47)
							c->bg = param[i] - 40;
						else if (param[i] == 39)
							c->fg = DEFAULT_FG;
						else if (param[i] == 49)
							c->bg = DEFAULT_BG;
						else
							fprintf(stderr,
								"Ignored SGR parameter %d at line %d\n",
								param[i], c->row + 1);
					}
					rc = 0;
					pcnt = 0;
					break;

				/* clear screen */
				/* todo flesh this out */
				} else if (ch == 'J') {
					c->row = 0;
					c->col = 0;

					for (int i = 0; i <= c->bottomrow; i++) {
						for (int j = 0; j < c->max_cols; j++) {
							c->cell[i][j].fg = DEFAULT_FG;
							c->cell[i][j].bg = DEFAULT_BG;
							c->cell[i][j].bold = DEFAULT_BOLD;
							c->cell[i][j].ice = DEFAULT_ICE;

							if (c->cell[i][j].utfchar)
								free(c->cell[i][j].utfchar);
						}
					}
					rc = 0;
					pcnt = 0;
					break;

				/* move up */
				} else if (ch == 'A') {

					/* default is 1 */
					if (rc == 0)
						rc = 1;

					while (rc > 0) {

						/* found an ansi that tried this */
						/* probably from a bbs prelogin that assumed */
						/* frontdoor was eating the first few lines */
						if (c->row == 0)
							break;

						c->row--;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move down */
				} else if (ch == 'B') {

					if (rc == 0)
						rc = 1;

					while (rc > 0) {
						inc_row(c);
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move forward */
				} else if (ch == 'C') {

					if (rc == 0)
						rc = 1;

					while (rc > 0) {

						if (c->col == c->max_cols - 1)
							break;

						c->col++;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* move back */
				} else if (ch == 'D') {

					if (rc == 0)
						rc = 1;

					while (rc > 0) {
						if (c->col == 0)
							break;

						c->col--;
						rc--;
					}

					rc = 0;
					pcnt = 0;
					break;

				/* goto */
				/* todo add check for inc_row */
				} else if (ch == 'f' || ch == 'H') {

					if (pcnt != 2) {
						break;
					}

					c->row = param[0];
					c->col = param[1];

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

					c->oldrow = c->row;
					c->oldcol = c->col;

					rc = 0;
					pcnt = 0;
					break;

				/* restore position */
				} else if (ch == 'u') {

					c->row = c->oldrow;
					c->col = c->oldcol;

					rc = 0;
					pcnt = 0;
					break;

				/* sequences we dont care and/or know about */
				} else {
					fprintf(stderr, "Ignored escape code [");

					for (int i = 0; i < pcnt; i++)
						fprintf(stderr, "%d%s", param[pcnt],
							i + 1 < pcnt ? ";" : "");

					fprintf(stderr, "0x%02x at line %d col %d\n", ch,
						c->row + 1, c->col + 1);

					rc = 0;
					pcnt = 0;
					break;
				}
			}
		}
	}

	for (int i = 0; i <= c->row; i++) {
		for (int j = c->lcrop; j < c->max_cols - c->rcrop; j++) {
			cell_t *cell = &c->cell[i][j];

			if (!cell->utfchar) {
				printf("\n");
				return (0);
			}

			if (c->use_color)
				print_color(c, cell, j);

			printf("%s", cell->utfchar);
		}
		printf("\n");
	}

	if (argc == 1)
		fclose(fd);

	return (0);
}

void
usage(void)
{
	fprintf(stderr, "usage: a2m [options] [input.ans]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "options:");
	fprintf(stderr, "\n");
	fprintf(stderr, "    -l n      Crop n lines from the left side.\n");
	fprintf(stderr, "    -r n      Crop n lines from the right side.\n");
	fprintf(stderr, "    -n        Disable color output.\n");
	fprintf(stderr, "    -p        Print unprintable characters.\n");
	fprintf(stderr, "    -t size   Specify tab size, default is 8.\n");
	fprintf(stderr, "    -w width  Specify width, default is 80.\n");
	exit(EX_USAGE);
}

int
get_fgcolor(cell_t *cell)
{
	return (cell->bold ? color_bold[cell->fg] : color[cell->fg]);
}

int
get_bgcolor(cell_t *cell)
{
	return (cell->ice ? color_bold[cell->bg] : color[cell->bg]);
}

void
print_color(canvas_t *c, cell_t *cell, int col)
{
	static cell_t *prev = NULL;

	int oldfg = (!prev || col == 0 + c->lcrop) ? DEFAULT_FG : prev->fg;
	int oldbg = (!prev || col == 0 + c->lcrop) ? DEFAULT_BG : prev->bg;
	int oldbold = (!prev || col == 0 + c->lcrop) ? DEFAULT_BOLD : prev->bold;
	int oldice = (!prev || col == 0 + c->lcrop) ? DEFAULT_ICE : prev->ice;

	if (cell->fg != oldfg || cell->bg != oldbg || cell->bold != oldbold || cell->ice != oldice) {

		printf("\x03");

		if (cell->fg != oldfg || cell->bold != oldbold)
			printf("%d", get_fgcolor(cell));

		if (cell->bg != oldbg || (cell->ice != oldice))
			printf(",%d", get_bgcolor(cell));
	}

	prev = cell;
	return;
}

void
inc_row(canvas_t *c)
{
	cell_t **newrows;

	c->row++;

	if (c->row <= c->bottomrow)
		return;

	c->bottomrow = c->row;

	newrows = (cell_t **)calloc(c->row + 1, sizeof(cell_t *));

	if (!newrows) {
		perror(NULL);
		exit(EX_OSERR);
	}

	for (int i = 0; i < c->row; i++)
		newrows[i] = c->cell[i];

	newrows[c->row] = (cell_t *)calloc(c->max_cols, sizeof(cell_t));

	free(c->cell);
	c->cell = newrows;

	for (int j = 0; j < c->max_cols; j++) {
		c->cell[c->row][j].fg = DEFAULT_FG;
		c->cell[c->row][j].bg = DEFAULT_BG;
		c->cell[c->row][j].bold = DEFAULT_BOLD;
		c->cell[c->row][j].ice = DEFAULT_ICE;
	}
	return;
}

void
draw_cell(canvas_t *c, char *inch, int fg, int bg, bool bold, bool ice)
{
	static iconv_t conv = (iconv_t)0;
	char *outchp;
	char *oldoutchp;

	outchp = calloc(1, MAX_UTFSTR);

	if (!outchp) {
		perror(NULL);
		exit(EX_OSERR);
	}

 	oldoutchp = outchp;

	if ((unsigned char)inch[0] < 32 && !c->show_unp)
		inch[0] = ' ';

	size_t inchsize = 1;
	size_t outchsize = MAX_UTFSTR;

	if (!conv)
		conv = iconv_open("UTF-8", "CP437");

	iconv(conv, &inch, &inchsize, &outchp, &outchsize);

	c->cell[c->row][c->col].utfchar = calloc(1, MAX_UTFSTR);
	if (!c->cell[c->row][c->col].utfchar) {
		perror(NULL);
		exit(EX_OSERR);
	}

	strcpy(c->cell[c->row][c->col].utfchar, oldoutchp);

	free(oldoutchp);

	c->cell[c->row][c->col].fg = fg;
	c->cell[c->row][c->col].bg = bg;
	c->cell[c->row][c->col].bold = bold;
	c->cell[c->row][c->col].ice = ice;

	c->col++;

	if (c->col == c->max_cols) {
		inc_row(c);
		c->col = 0;
	}
	return;
}
