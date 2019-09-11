#pragma once

#include <stdint.h>

#define SESSION_ID_LEN 65
#define USER_ID_LEN 65
#define USER_NAME_LEN 255
#define CODE_LEN 11
#define HASH_LEN 65

struct api;

#ifdef __cplusplus
extern "C" {
#endif

void api_init(struct api **ctx_out);
void api_destroy(struct api **ctx_out);

int32_t api_invite(struct api *ctx, char *session_id, uint32_t expires_in, uint32_t max_grants, char *code);
int32_t api_code(struct api *ctx, char *game_id, char *code, char *hash);
int32_t api_poll_code(struct api *ctx, char *hash, char *session_id);

#ifdef __cplusplus
}
#endif
