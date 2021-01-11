#include <string>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#define check(VALUE, MSG, BADVAL) if (VALUE == BADVAL) { perror(MSG); exit(1); }

uint8_t checksum(uint8_t block[8]) {
	uint16_t res = 0;
	for (int i = 0; i < 8; ++i) {
		res += block[i];
	}
	res = (res ^ (res >> 8)) & ((1u << 8) - 1);
	return static_cast<uint8_t>(res);
}

void makecs(std::string filename) {
	FILE *file = fopen(filename.c_str(), "rb");
	check(file, "fopen error", NULL);
	FILE *cs_file = fopen((filename + ".chksm").c_str(), "wb");
	check(cs_file, "fopen error", NULL);
	uint8_t block[8];
	memset(block, 0, 8);
	while (fread(block, 1, 8, file) > 0) {
		uint8_t cs = checksum(block);
		fwrite(&cs, 1, 1, cs_file);
		memset(block, 0, 8);
	}
	fclose(file);
	fclose(cs_file);
}

int checkcs(std::string filename) {
	FILE *file = fopen(filename.c_str(), "rb");
	if (file == NULL) {
		return -1;
	}
	struct stat file_info;
	check(fstat(fileno(file), &file_info), "fstat error", -1);
	FILE *cs_file = fopen((filename + ".chksm").c_str(), "rb");
	if (cs_file == NULL) {
		return -1;
	}
	struct stat cs_info;
	check(fstat(fileno(cs_file), &cs_info), "fstat error", -1);
	if ((file_info.st_size / 8 + (file_info.st_size % 8 > 0 ? 1 : 0)) != cs_info.st_size) {
		fclose(file);
		fclose(cs_file);
		return 0;
	}
	uint8_t block[8];
	memset(block, 0, 8);
	while (fread(block, 1, 8, file) > 0) {
		uint8_t cs = checksum(block);
		uint8_t cs2;
		if (fread(&cs2, 1, 1, cs_file) < 1) {
			fclose(file);
			fclose(cs_file);
			return 0;
		}
		if (cs != cs2) {
			fclose(file);
			fclose(cs_file);
			return 0;
		}
		memset(block, 0, 8);
	}
	fclose(file);
	fclose(cs_file);
	return 1;
}