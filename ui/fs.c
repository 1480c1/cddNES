#include "fs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "assets/default-rom.h"

#if defined(_WIN32)
	#include <direct.h>
	#include <windows.h>
	#define SEP "\\"

	#define mkdir(dir, mode) _mkdir(dir)
	#define chdir(dir) _chdir(dir)
	#define getcwd _getcwd
	#define strcasecmp _stricmp
	#define FILENAME(ent) (ent)->cFileName
	#define ISDIR(ent) ((ent)->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	#define CLOSEDIR(dir) FindClose(dir)
	#define READDIR(dir, ent) (FindNextFileA(dir, ent) ? 1 : 0)
	typedef HANDLE OS_DIR;
	typedef WIN32_FIND_DATAA OS_DIRENT;
#else
	#include <unistd.h>
	#include <dirent.h>
	#include <sys/stat.h>
	#include <libgen.h>
	#define SEP "/"

	#define FILENAME(ent) (*(ent))->d_name
	#define ISDIR(ent) ((*(ent))->d_type == DT_DIR)
	#define CLOSEDIR(dir) closedir(dir)
	#define READDIR(dir, ent) (*(ent) = readdir(dir), *(ent) ? 1 : 0)
	typedef DIR * OS_DIR;
	typedef struct dirent * OS_DIRENT;
#endif

static uint32_t fs_crc32(uint8_t *data, size_t n_bytes)
{
	uint32_t crc = 0;
	uint32_t table[0x100];

	for (uint32_t x = 0; x < 0x100; x++) {
		uint32_t r = x;

		for (uint8_t y = 0; y < 8; y++)
			r = (r & 1 ? 0 : 0xEDB88320) ^ r >> 1;

		table[x] = r ^ 0xFF000000;
	}

	for (size_t x = 0; x < n_bytes; x++)
		crc = table[(uint8_t) crc ^ data[x]] ^ crc >> 8;

	return crc;
}

uint8_t *fs_read(char *file_name, size_t *size)
{
	FILE *f = fopen(file_name, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	fseek(f, 0, SEEK_SET);

	// leave a '\0' so the buffer can be treated as a string
	uint8_t *bytes = calloc(*size + 1, 1);
	fread(bytes, *size, 1, f);
	fclose(f);

	return bytes;
}

void fs_write(char *file_name, uint8_t *bytes, size_t size)
{
	FILE *f = fopen(file_name, "wb");
	fwrite(bytes, size, 1, f);
	fclose(f);
}

uint8_t *fs_load_sram(char *crc32str, size_t *sram_size)
{
	char sram_file[30];
	snprintf(sram_file, 30, "save%s%s.sav", SEP, crc32str);

	return fs_read(sram_file, sram_size);
}

void fs_save_sram(struct nes *nes, char *crc32)
{
	size_t sram_len = nes_cart_sram_dirty(nes);

	if (sram_len > 0) {
		mkdir("save", 0755);

		char sav_name[30];
		snprintf(sav_name, 30, "save%s%s.sav", SEP, crc32);

		uint8_t *sram = calloc(1, sram_len);
		nes_cart_sram_get(nes, sram, sram_len);

		fs_write(sav_name, sram, sram_len);
		free(sram);
	}
}

void fs_load_rom(struct nes *nes, char *rom_name, char *crc32)
{
	bool should_free = false;
	uint8_t *rom = DEFAULT_ROM;
	size_t rom_size = sizeof(DEFAULT_ROM);

	if (rom_name[0] != '\0') {
		should_free = true;
		rom_size = 0;
		rom = fs_read(rom_name, &rom_size);

		if (!rom)
			assert(!"Failed to read ROM file");
	}

	snprintf(crc32, 10, "%08X", fs_crc32(rom + 16, rom_size - 16));

	size_t sram_size = 0;
	uint8_t *sram = fs_load_sram(crc32, &sram_size);
	nes_cart_load(nes, rom, rom_size, sram, sram_size, NULL);

	free(sram);

	if (should_free)
		free(rom);
}

void fs_cwd(char *cwd, int32_t len)
{
	getcwd(cwd, len);
}

void fs_path(char *buf, char *path, char *name)
{
	char im[MAX_FILE_NAME];
	snprintf(im, MAX_FILE_NAME, "%s" SEP "%s", path, name);
	snprintf(buf, MAX_FILE_NAME, "%s", im);
}

static int32_t fs_file_compare(const void *p1, const void *p2)
{
	struct finfo *fi1 = (struct finfo *) p1;
	struct finfo *fi2 = (struct finfo *) p2;

	if (fi1->dir && !fi2->dir) {
		return -1;
	} else if (!fi1->dir && fi2->dir) {
		return 1;
	} else {
		int32_t r = strcasecmp(fi1->name, fi2->name);

		if (r != 0)
			return r;

		return -strcmp(fi1->name, fi2->name);
	}
}

uint32_t fs_list(char *path, struct finfo **fi)
{
	*fi = NULL;
	uint32_t n = 0;
	OS_DIRENT ent;

	#if defined(_WIN32)
		size_t path_wildcard_len = strlen(path) + 3;
		char *path_wildcard = calloc(path_wildcard_len, 1);
		snprintf(path_wildcard, path_wildcard_len, "%s\\*", path);

		OS_DIR dir = FindFirstFileA(path_wildcard, &ent);
		bool ok = (dir != INVALID_HANDLE_VALUE);

		free(path_wildcard);
	#else
		bool ok = false;
		OS_DIR dir = opendir(path);

		if (dir) {
			ent = readdir(dir);
			ok = ent;
		}
	#endif

	while (ok) {
		char *name = FILENAME(&ent);
		bool is_dir = ISDIR(&ent);

		if (is_dir || strstr(name, ".nes")) {
			*fi = realloc(*fi, (n + 1) * sizeof(struct finfo));
			snprintf((*fi)[n].name, MAX_FILE_NAME, "%s", name);
			(*fi)[n].dir = is_dir;
			n++;
		}

		ok = READDIR(dir, &ent);
		if (!ok) CLOSEDIR(dir);
	}

	if (n > 0)
		qsort(*fi, n, sizeof(struct finfo), fs_file_compare);

	return n;
}
