#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CJSON_HIDE_SYMBOLS
#include "cJSON/cJSON.h"

#include "fs.h"

#define SETTINGS_FILE "settings.json"

struct settings {
	cJSON *obj;
};

struct settings *settings_open(void)
{
	struct settings *s = calloc(1, sizeof(struct settings));

	size_t size = 0;
	char *json = (char *) fs_read(SETTINGS_FILE, &size);

	if (json) {
		cJSON *obj = cJSON_Parse(json);

		if (obj && cJSON_IsObject(obj))
			s->obj = obj;
	}

	if (s->obj == NULL)
		s->obj = cJSON_CreateObject();

	free(json);

	return s;
}

void settings_close(struct settings **s_out)
{
	if (s_out == NULL || *s_out == NULL)
		return;

	struct settings *s = *s_out;

	char *json = cJSON_Print(s->obj);
	fs_write(SETTINGS_FILE, (uint8_t *) json, strlen(json));
	free(json);

	free(s);
	*s_out = NULL;
}

int32_t settings_get_int32(struct settings *s, char *key, int32_t def)
{
	cJSON *jval = cJSON_GetObjectItem(s->obj, key);

	if (jval && cJSON_IsNumber(jval))
		return lrint(jval->valuedouble);

	return def;
}

void settings_get_string(struct settings *s, char *key, char *val, size_t val_len, char *def)
{
	cJSON *jval = cJSON_GetObjectItem(s->obj, key);

	if (jval && cJSON_IsString(jval)) {
		snprintf(val, val_len, "%s", jval->valuestring);

	} else {
		snprintf(val, val_len, "%s", def);
	}
}

void settings_set_int32(struct settings *s, char *key, int32_t val)
{
	if (cJSON_HasObjectItem(s->obj, key)) {
		cJSON_ReplaceItemInObject(s->obj, key, cJSON_CreateNumber((double) val));

	} else {
		cJSON_AddItemToObject(s->obj, key, cJSON_CreateNumber((double) val));
	}
}

void settings_set_string(struct settings *s, char *key, char *val)
{
	if (cJSON_HasObjectItem(s->obj, key)) {
		cJSON_ReplaceItemInObject(s->obj, key, cJSON_CreateString(val));

	} else {
		cJSON_AddItemToObject(s->obj, key, cJSON_CreateString(val));
	}
}
