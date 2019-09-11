#include "args.h"

#include <string.h>
#include <stdio.h>

#if defined(_WIN32)
	#include <windows.h>
	#define strtok_r strtok_s
#endif

#define MAX_ARG_LEN 256

static void args_spawn_console(void)
{
	#if defined(_WIN32)
		AllocConsole();
		AttachConsole(GetCurrentProcessId());
		freopen("CONIN$", "r", stdin);
		freopen("CONOUT$", "w", stdout);
	#endif
}

static void args_split(char *arg, char split[2][MAX_ARG_LEN])
{
	split[0][0] = split[1][0] = '\0';

	char tmp[MAX_ARG_LEN];
	snprintf(tmp, MAX_ARG_LEN, "%s", arg);

	char *ptr = NULL;
	char *str = strtok_r(tmp, "=", &ptr);

	if (str == NULL)
		return;

	snprintf(split[0], MAX_ARG_LEN, "%s", str);
	str = strtok_r(NULL, "=", &ptr);

	if (str != NULL)
		snprintf(split[1], MAX_ARG_LEN, "%s", str);
}

static void args_assign(char split[2][MAX_ARG_LEN], struct args *args)
{
	if (!strcmp(split[0], "-session")) {
		if (split[1][0] != '\0')
			snprintf(args->session, SESSION_ID_LEN, "%s", split[1]);

	} else if (!strcmp(split[0], "-console")) {
		args->console = true;

	} else if (!strcmp(split[0], "-headless")) {
		args->headless = true;
	}
}

void args_parse(int32_t argc, char **argv, struct args *args)
{
	memset(args, 0, sizeof(struct args));

	for (int32_t x = 1; x < argc; x++) {
		// first argument may be the .nes rom to be opened
		if (x == 1 && strstr(argv[x], ".nes")) {
			snprintf(args->rom, MAX_ROM_LEN, "%s", argv[x]);

		// check for other arguments
		} else {
			char split[2][MAX_ARG_LEN];
			args_split(argv[x], split);
			args_assign(split, args);
		}
	}

	if (args->console || args->headless)
		args_spawn_console();
}
