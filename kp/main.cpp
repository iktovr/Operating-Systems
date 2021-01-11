#include <utility>
#include <list>
#include <string>
#include <iostream>
#include <ncurses.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "checksum.hpp"

#define ctrl(k) ((k) & 0x1f)
#define check(VALUE, MSG, BADVAL) if (VALUE == BADVAL) { perror(MSG); exit(1); }

using string_t = std::list<char>;
using text_t = std::list<string_t>;
using cursor_t = std::pair<text_t::iterator, string_t::iterator>;
const int ENTER = 10;
const int TAB_SIZE = 4;

void str2text(char *str, int size, text_t &text) {
	text.clear();
	text.push_back(string_t());
	for (int i = 0; i < size && str[i] != '\0'; ++i) {
		if (str[i] == '\n') {
			text.push_back(string_t());
		} else if (str[i] == '\t') {
			for (int i = 0; i < TAB_SIZE; ++i) {
				text.back().push_back(' ');
			}
		} else {
			text.back().push_back(str[i]);
		}
	}
}

void load(text_t &text, int mmap_size, int fd, int offset) {
	char *chunk = (char *)mmap(NULL, mmap_size, PROT_READ, MAP_PRIVATE, fd, offset);
	check(chunk, "mmap error", MAP_FAILED);
	str2text(chunk, mmap_size, text);
	check(munmap(chunk, mmap_size), "munmap error", -1);
}

void save(text_t &text, int mmap_size, int fd, int offset, struct stat &file_info) {
	FILE* tmp = tmpfile();
	check(tmp, "tmpfile error", NULL);
	char linebreak = '\n';
	int new_size = text.size()-1;
	for (auto line = text.begin(); line != text.end(); ++line) {
		new_size += line->size();
		for (char ch: *line) {
			fwrite(&ch, 1, 1, tmp);
		}
		if (line != --text.end()) {
			fwrite(&linebreak, 1, 1, tmp);
		}
	}
	if (offset + mmap_size < file_info.st_size) {
		check(lseek(fd, offset+mmap_size, SEEK_SET), "lseek error", -1);
		char c;
		while (read(fd, &c, 1) > 0) {
			fwrite(&c, 1, 1, tmp);
		}
	}
	check(ftruncate(fd, file_info.st_size + new_size - (file_info.st_size < mmap_size ? file_info.st_size : mmap_size)), "ftruncate error", -1);
	check(lseek(fd, offset, SEEK_SET), "lseek error", -1);
	check(fseek(tmp, 0, SEEK_SET), "fseek error", -1);
	char c;
	while (fread(&c, 1, 1, tmp) > 0) {
		write(fd, &c, 1);
	}
	fclose(tmp);
	check(lseek(fd, 0, SEEK_SET), "lseek error", -1);
	check(fstat(fd, &file_info), "fstat error", -1);
}

int main(int argc, char *argv[])
{
	std::string filename = "";
	int pagesize = sysconf(_SC_PAGESIZE);
	int mmap_size = pagesize;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-m") == 0) {
			++i;
			if (i == argc) {
				std::cerr << "Invalid argument\n";
				exit(1);
			}
			mmap_size = strtol(argv[i], NULL, 10);
			if (mmap_size <= 0) {
				std::cerr << "Invalid argument\n";
				exit(1);
			}
			if (mmap_size % pagesize != 0) {
				mmap_size = pagesize * (mmap_size / pagesize + 1);
			}
		}

		else {
			filename = std::string(argv[i]);
		}
	}

	if (filename == "") {
		std::cerr << "File name expected\n";
		exit(1);
	}

	if (checkcs(filename) == 0) {
		std::cerr << "Checksum does not match\n";
		exit(1);
	}

	text_t text;
	int offset = 0;
	int fd = open(filename.c_str(), O_RDWR | O_CREAT, S_IRWXU);
	check(fd, "open error", -1);
	struct stat file_info;
	check(fstat(fd, &file_info), "fstat error", -1);

	if (file_info.st_size == 0) {
		check(ftruncate(fd, mmap_size), "ftruncate error", -1);
	}
	load(text, mmap_size, fd, offset);

	cursor_t cursor(text.begin(), text.begin()->begin());
	int cursor_x = 0, cursor_y = 0;
	text_t::iterator start_line = text.begin();
	
	initscr();
	raw();
	keypad(stdscr, TRUE);
	noecho();
	int width, height;
	getmaxyx(stdscr, height, width);

	int key;
	while (true) {
		clear();
		move(0, 0);
		int i = 0;
		for (auto line = start_line; line != text.end() && i < height; ++line, ++i) {
			int j = 0;
			for (auto ch = line->begin(); ch != line->end() && j < width-1; ++ch, ++j) {
				addch(*ch);
			}
			printw("\n\r");
		}
		move(cursor_y, cursor_x);
		refresh();

		key = getch();

		if (key == ctrl('q')) {
			break;
		}

		if (key == ctrl('s')) {
			save(text, mmap_size, fd, offset, file_info);
			makecs(filename);
			load(text, mmap_size, fd, offset);
			cursor_x = 0;
			cursor_y = 0;
			start_line = text.begin();
			cursor.first = text.begin();
			cursor.second = cursor.first->begin();
		}

		else if (key == KEY_UP) {
			if (cursor.first != text.begin()) {
				--cursor.first;
				cursor.second = cursor.first->begin();
				int i;
				for (i = 0; cursor.second != cursor.first->end() && i < cursor_x; ++i, ++cursor.second);
				if (i != cursor_x) {
					cursor_x = i;
				}
				if (cursor_y > 0) {
					--cursor_y;
				} else if (start_line != text.begin()) {
					--start_line;
				}
			} else if (offset > 0) {
				save(text, mmap_size, fd, offset, file_info);
				offset -= mmap_size;
				load(text, mmap_size, fd, offset);
				cursor.first = --text.end();
				cursor.second = cursor.first->begin();
				int i;
				for (i = 0; cursor.second != cursor.first->end() && i < cursor_x; ++i, ++cursor.second);
				if (i != cursor_x) {
					cursor_x = i;
				}
				start_line = --text.end();
				for (cursor_y = 0; start_line != text.begin() && cursor_y < height-1; ++cursor_y, --start_line);
			}
		}

		else if (key == KEY_DOWN) {
			if (cursor.first != --text.end()) {
				++cursor.first;
				cursor.second = cursor.first->begin();
				int i;
				for (i = 0; cursor.second != cursor.first->end() && i < cursor_x; ++i, ++cursor.second);
				if (i != cursor_x) {
					cursor_x = i;
				}
				if (cursor_y < height-1) {
					++cursor_y;
				} else if (start_line != --text.end()) {
					++start_line;
				}
			} else if (offset + mmap_size < file_info.st_size) {
				save(text, mmap_size, fd, offset, file_info);
				offset += mmap_size;
				load(text, mmap_size, fd, offset);
				cursor.first = text.begin();
				cursor.second = cursor.first->begin();
				int i;
				for (i = 0; cursor.second != cursor.first->end() && i < cursor_x; ++i, ++cursor.second);
				if (i != cursor_x) {
					cursor_x = i;
				}
				cursor_y = 0;
				start_line = text.begin();
			}
		}

		else if (key == KEY_LEFT) {
			if (cursor.second == cursor.first->begin() && cursor.first != text.begin()) {
				--cursor.first;
				cursor.second = cursor.first->end();
			} else if (cursor.second != cursor.first->begin()) {
				--cursor.second;
			} else if (offset > 0) {
				save(text, mmap_size, fd, offset, file_info);
				offset -= mmap_size;
				load(text, mmap_size, fd, offset);
				cursor.first = --text.end();
				cursor.second = cursor.first->end();
				cursor_x = cursor.first->size();
				start_line = --text.end();
				for (cursor_y = 0; start_line != text.begin() && cursor_y < height-1; ++cursor_y, --start_line);
				continue;
			} else {
				continue;
			}

			if (cursor_x == 0 && cursor_y > 0) {
				--cursor_y;
				cursor_x = cursor.first->size();
			} else if (cursor_x > 0) {
				--cursor_x;
			} else if (start_line != text.begin()) {
				--start_line;
			}
		}

		else if (key == KEY_RIGHT) {
			if (cursor.second == cursor.first->end() && cursor.first != --text.end()) {
				++cursor.first;
				cursor.second = cursor.first->begin();
			} else if (cursor.second != cursor.first->end()) {
				++cursor.second;
			} else if (offset + mmap_size < file_info.st_size) {
				save(text, mmap_size, fd, offset, file_info);
				offset += mmap_size;
				load(text, mmap_size, fd, offset);
				cursor.first = text.begin();
				cursor.second = cursor.first->begin();
				start_line = text.begin();
				cursor_y = 0;
				cursor_x = 0;
				continue;
			} else {
				continue;
			}

			if ((cursor_x == width-1 || (cursor.second == cursor.first->begin() && cursor.first != text.begin())) && cursor_y < height-1) {
				++cursor_y;
				cursor_x = 0;
			} else if (cursor_x < width-1) {
				++cursor_x;
			} else if (start_line != --text.end()) {
				++start_line;
			}
		}

		else if (key == KEY_BACKSPACE) {
			if (cursor.second == cursor.first->begin() && cursor.first != text.begin()) {
				auto prev_line = cursor.first;
				int size = cursor.first->size();
				--prev_line;
				int prev_size = prev_line->size();
				prev_line->insert(prev_line->end(), cursor.first->begin(), cursor.first->end());
				text.erase(cursor.first);
				cursor.first = prev_line;
				cursor.second = cursor.first->end();

				if (cursor_x == 0 && cursor_y > 0) {
					--cursor_y;
					cursor_x = prev_size;
				} else if (start_line != text.begin()) {
					--start_line;
				}

				while (size--) {
					--cursor.second;
				}
			} else if (cursor.second != cursor.first->begin()) {
				auto col = cursor.second;
				--cursor.second;
				cursor.first->erase(cursor.second);
				cursor.second = col;
				--cursor_x;
			} else {
				continue;
			}
		}

		else if (key == ENTER) {
			cursor_x = 0;
			if (cursor_y < height-1) {
				++cursor_y;
			} else if (start_line != --text.end()) {
				++start_line;
			}
			auto end = cursor.first->end();
			++cursor.first;
			text.insert(cursor.first, string_t(cursor.second, end));
			--cursor.first;
			--cursor.first;
			cursor.first->erase(cursor.second, cursor.first->end());
			++cursor.first;
			cursor.second = cursor.first->begin();
		}

		else if (key == KEY_RESIZE) {
			getmaxyx(stdscr, height, width);
			cursor_x = 0;
			cursor_y = 0;
			start_line = text.begin();
			cursor.first = text.begin();
			cursor.second = cursor.first->begin();
		}

		else if (key == '\t') {
			for (int i = 0; i < TAB_SIZE; ++i) {
				cursor.first->insert(cursor.second, ' ');
				if (cursor_x == width-1 && cursor_y < height-1) {
					// ++cursor_y;
					// cursor_x = 0;
				} else {
					++cursor_x;
				}
			}
		}

		else if (isprint(key)) {
			cursor.first->insert(cursor.second, key);
			if (cursor_x == width-1 && cursor_y < height-1) {
				// ++cursor_y;
				// cursor_x = 0;
			} else {
				++cursor_x;
			}
		}
	}
	endwin();

	save(text, mmap_size, fd, offset, file_info);
	close(fd);
	makecs(filename);

	return 0;
}
