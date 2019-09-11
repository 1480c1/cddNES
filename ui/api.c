#include "api.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CJSON_HIDE_SYMBOLS
#include "cJSON/cJSON.h"
#include "uncurl/uncurl.h"

#include "assets/cacert.h"

#define HOST "kessel-api.parsecgaming.com"

struct api {
	struct uncurl_tls_ctx *uc;
};

void api_init(struct api **ctx_out)
{
	struct api *ctx = *ctx_out = calloc(1, sizeof(struct api));

	uncurl_new_tls_ctx(&ctx->uc);
	uncurl_set_cacert(ctx->uc, (char *) CACERT, sizeof(CACERT));
}

void api_destroy(struct api **ctx_out)
{
	if (ctx_out == NULL || *ctx_out == NULL)
		return;

	struct api *ctx = *ctx_out;

	uncurl_free_tls_ctx(ctx->uc);

	free(ctx);
	*ctx_out = NULL;
}

static int32_t https_post(struct uncurl_tls_ctx *uc, char *path, char *session_id,
	char *body, int32_t timeout_ms, char **response, uint32_t *response_len)
{
	*response_len = 0;
	*response = NULL;

	int32_t r = -1;

	//make the socket/TLS connection
	struct uncurl_conn *ucc = uncurl_new_conn();

	//make the TCP connection
	int32_t e = uncurl_connect(uc, ucc, UNCURL_HTTPS, HOST, 443, true, timeout_ms);
	if (e != UNCURL_OK) goto except;

	//set request headers
	int32_t body_len = (int32_t) strlen(body);
	uncurl_set_header_str(ucc, "User-Agent", "uncurl/0.0");
	uncurl_set_header_str(ucc, "Connection", "close");
	uncurl_set_header_str(ucc, "Content-Type", "application/json");
	uncurl_set_header_int(ucc, "Content-Length", body_len);

	if (session_id) {
		char bearer[128];
		snprintf(bearer, 128, "Bearer %s", session_id);
		uncurl_set_header_str(ucc, "Authorization", bearer);
	}

	//send the request header
	e = uncurl_write_header(ucc, "POST", path, UNCURL_REQUEST);
	if (e != UNCURL_OK) goto except;

	//send the request body
	e = uncurl_write_body(ucc, body, body_len);
	if (e != UNCURL_OK) goto except;

	//read the response header
	e = uncurl_read_header(ucc, timeout_ms);
	if (e != UNCURL_OK) goto except;

	//get the status code
	int32_t status_code = 0;
	e = uncurl_get_status_code(ucc, &status_code);
	if (e != UNCURL_OK) goto except;
	r = status_code;

	//read the response body if not HEAD request -- uncompress if necessary
	e = uncurl_read_body_all(ucc, response, response_len, timeout_ms, 128 * 1024 * 1024);
	if (e != UNCURL_OK) {r = -1; goto except;}

	except:

	if (r == -1)
		free(*response);

	uncurl_close(ucc);

	return r;
}

int32_t api_invite(struct api *ctx, char *session_id, uint32_t expires_in, uint32_t max_grants, char *code)
{
	cJSON *req = cJSON_CreateObject();

	cJSON_AddNumberToObject(req, "expires_in", expires_in);
	cJSON_AddNumberToObject(req, "max_grants", max_grants);

	char *req_str = cJSON_PrintUnformatted(req);
	cJSON_Delete(req);

	char *res_str = NULL;
	uint32_t res_len = 0;
	int32_t e = https_post(ctx->uc, "/host-invites/", session_id, req_str, 5000, &res_str, &res_len);
	free(req_str);

	if (e != -1) {
		cJSON *res = cJSON_Parse(res_str);
		if (res == NULL) {e = -1; goto except;}

		cJSON *data = cJSON_GetObjectItem(res, "data");
		if (data == NULL || !cJSON_IsObject(data)) {e = -1; goto except;}

		cJSON *val = cJSON_GetObjectItem(data, "code");
		if (val == NULL || !cJSON_IsString(val)) {e = -1; goto except;}
		snprintf(code, CODE_LEN, "%s", val->valuestring);

		except:

		if (res)
			cJSON_Delete(res);

		free(res_str);
	}

	return e;
}

int32_t api_code(struct api *ctx, char *game_id, char *code, char *hash)
{
	cJSON *req = cJSON_CreateObject();

	cJSON_AddStringToObject(req, "game_id", game_id);

	char *req_str = cJSON_PrintUnformatted(req);
	cJSON_Delete(req);

	char *res_str = NULL;
	uint32_t res_len = 0;
	int32_t e = https_post(ctx->uc, "/auth/codes/", NULL, req_str, 5000, &res_str, &res_len);
	free(req_str);

	if (e == 201) {
		cJSON *res = cJSON_Parse(res_str);
		if (res == NULL) {e = -1; goto except;}

		cJSON *data = cJSON_GetObjectItem(res, "data");
		if (data == NULL || !cJSON_IsObject(data)) {e = -1; goto except;}

		cJSON *val = cJSON_GetObjectItem(data, "user_code");
		if (val == NULL || !cJSON_IsString(val)) {e = -1; goto except;}
		snprintf(code, CODE_LEN, "%s", val->valuestring);

		val = cJSON_GetObjectItem(data, "hash");
		if (val == NULL || !cJSON_IsString(val)) {e = -1; goto except;}
		snprintf(hash, HASH_LEN, "%s", val->valuestring);

		except:

		if (res)
			cJSON_Delete(res);

		free(res_str);
	}

	return e;
}

int32_t api_poll_code(struct api *ctx, char *hash, char *session_id)
{
	cJSON *req = cJSON_CreateObject();

	cJSON_AddStringToObject(req, "grant_type", "auth_code");
	cJSON_AddStringToObject(req, "auth_code_hash", hash);

	char *req_str = cJSON_PrintUnformatted(req);
	cJSON_Delete(req);

	char *res_str = NULL;
	uint32_t res_len = 0;
	int32_t e = https_post(ctx->uc, "/auth/sessions/", NULL, req_str, 5000, &res_str, &res_len);
	free(req_str);

	if (e == 201) {
		cJSON *res = cJSON_Parse(res_str);
		if (res == NULL) {e = -1; goto except;}

		cJSON *data = cJSON_GetObjectItem(res, "data");
		if (data == NULL || !cJSON_IsObject(data)) {e = -1; goto except;}

		cJSON *val = cJSON_GetObjectItem(data, "id");
		if (val == NULL || !cJSON_IsString(val)) {e = -1; goto except;}
		snprintf(session_id, SESSION_ID_LEN, "%s", val->valuestring);

		except:

		if (res)
			cJSON_Delete(res);

		free(res_str);
	}

	return e;
}
