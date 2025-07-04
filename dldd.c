/* See LICENSE file for license details */
/* dldd - less-like utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "config.h"

#define version "0.2"

const int so = STDOUT_FILENO;
const int si = STDIN_FILENO;

struct state{
	int ld;
	int row_of;
	int rows, cols;
	int num_lines;
	char **lines;
	struct termios o;
} State;

void d(const char *s){
	write(so, "\x1b[2J\x1b[H\x1b[?25h", 11);
	perror(s);
	exit(1);
}

void disable_row_m(){
	write(so, "\x1b[0m\x1b[2J\x1b[H\x1b[?25h", 15);
	tcsetattr(si, TCSAFLUSH, &State.o);
}

void enable_row_m(){
	if(tcgetattr(si, &State.o) == -1) d("tcgetattr");
	atexit(disable_row_m);
	struct termios raw = State.o;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;
	if(tcsetattr(si, TCSAFLUSH, &raw) == -1) d("tcsetattr");
}

void of(const char *file){
	FILE *f = fopen(file, "r");
	if(!f) d("fopen");
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while((linelen = getline(&line, &linecap, f)) != -1){
		if(linelen > 0 && line[linelen-1] == '\n') line[linelen-1] = '\0';
		State.lines = realloc(State.lines, sizeof(char*) * (State.num_lines + 1));
		State.lines[State.num_lines++] = strdup(line);
	}

	free(line);
	fclose(f);
}

void draw_scrn(){
	write(so, "\x1b[2J\x1b[H", 7);
	for(int l = 0; l < State.rows - 1; l++){
		int frow = l + State.row_of;
		if(frow < State.num_lines){
			dprintf(so, "%.*s\r\n", State.cols, State.lines[frow]);
		} else {
			dprintf(so, "~\r\n");
		}
	}

	dprintf(so, "\x1b[2K\x1b[0m%d-%d, press 'q' to quit", State.row_of + State.ld + 1, State.num_lines);
	dprintf(so, "\x1b[%d;1H", State.ld + 1);
}

int readk(){
	char c;
	while(1){
		int rn = read(si, &c, 1);
		if(rn == 1) return c;
		if(rn == -1 && errno != EAGAIN) d("read");
	}
}

void move_cur(int key){
	switch(key){
		case key_up:
			if(State.ld > 0) State.ld--;
			else if(State.row_of > 0) State.row_of--;
			break;
		case key_down:
			if(State.row_of + State.ld < State.num_lines - 1){
				if(State.ld < State.rows - 2) State.ld++;
				else State.row_of++;
			}

			break;
		case key_top:
			State.ld = 0;
			State.row_of = 0;
			break;
		case key_bottom:
			if(State.num_lines <= State.rows - 1){
				State.ld = State.num_lines - 1;
			} else {
				State.ld = State.rows - 2;
				State.row_of = State.num_lines - (State.rows - 1);
			}

			break;
	}
}

void help(const char *dldd){
	printf("usage: %s [options]..\n", dldd);
	printf("options:\n");
	printf("  -v	show version information\n");
	printf("  -h	display this\n");
	exit(1);
}

void show_version(){
	printf("dldd-%s\n", version);
	exit(1);
}

int main(int argc, char **argv){
	if(argc == 2){
		if(strcmp(argv[1], "-h") == 0){
			help(argv[0]);
		}

		if(strcmp(argv[1], "-v") == 0){
			show_version();
		}
	}

	if(argc != 2){
		fprintf(stderr, "usage: %s [file]\n", argv[0]);
		return 1;
	}

	struct winsize ws;
	if(ioctl(so, TIOCGWINSZ, &ws) == -1) d("ioctl");
	State.rows = ws.ws_row;
	State.cols = ws.ws_col;
	State.ld = 0;
	State.row_of = 0;
	State.lines = NULL;
	State.num_lines = 0;
	enable_row_m();
	of(argv[1]);
	while(1){
		draw_scrn();
		int key = readk();
		if(key == key_quit) break;
		move_cur(key);
	}

	disable_row_m();

	return 0;
}
