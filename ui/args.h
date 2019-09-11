#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "api.h"

#define MAX_ROM_LEN 1024

struct args {
	char rom[MAX_ROM_LEN];
	char session[SESSION_ID_LEN];
	bool console;
	bool headless;
};

void args_parse(int32_t argc, char **argv, struct args *args);
