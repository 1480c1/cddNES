#pragma once

#include <stdint.h>
#include <stddef.h>

struct settings;

struct settings *settings_open(void);
void settings_close(struct settings **s_out);

int32_t settings_get_int32(struct settings *s, char *key, int32_t def);
void settings_get_string(struct settings *s, char *key, char *val, size_t val_len, char *def);
#define settings_get_bool(s, key, def) ((bool) settings_get_int32((s), (key), (int32_t) (def)))

void settings_set_int32(struct settings *s, char *key, int32_t val);
void settings_set_string(struct settings *s, char *key, char *val);
#define settings_set_bool(s, key, val) settings_set_int32((s), (key), (int32_t) (val))
