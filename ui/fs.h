#pragma once

#include <stdint.h>

#include "../src/nes.h"

#define MAX_FILE_NAME 1024

struct finfo {
	bool dir;
	char name[MAX_FILE_NAME];
};

#ifdef __cplusplus
extern "C" {
#endif

uint8_t *fs_read(char *file_name, size_t *size);
void fs_write(char *file_name, uint8_t *bytes, size_t size);

uint8_t *fs_load_sram(char *crc32str, size_t *sram_size);
void fs_save_sram(struct nes *nes, char *crc32);
void fs_load_rom(struct nes *nes, char *rom_name, char *crc32);
void fs_cwd(char *cwd, int32_t len);
void fs_path(char *buf, char *path, char *name);
uint32_t fs_list(char *path, struct finfo **fi);

#ifdef __cplusplus
}
#endif
