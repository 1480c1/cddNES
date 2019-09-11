#include "gl.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "SDL2/SDL.h"

#include "glproc.h"

typedef GLuint   GLShader;
typedef GLShader GLVertexShader;
typedef GLShader GLFragmentShader;
typedef GLuint   GLProgram;
typedef GLint    GLLocation;
typedef GLuint   GLVertexBuffer;
typedef GLuint   GLElementBuffer;
typedef GLuint   GLTexture2D;
typedef GLuint   GLFramebuffer;
typedef GLuint   GLVertexArray;
typedef GLenum   GLShaderType;
typedef GLenum   GLTextureUnit;
typedef GLenum   GLFormat;

#if defined(GLES)
	#define VSTR \
	"#version 100                                       \n" \
	"precision mediump float;                           \n"
#else
	#define VSTR \
	"#version 110                                       \n"
#endif

static const GLchar *VERT =
	VSTR
	"                                                   \n"
	"attribute vec4 position;                           \n"
	"attribute vec2 texcoord;                           \n"
	"                                                   \n"
	"varying vec2 v_texcoord;                           \n"
	"                                                   \n"
	"void main(void) {                                  \n"
	"    v_texcoord = texcoord;                         \n"
	"                                                   \n"
	"    gl_Position = position;                        \n"
	"}                                                  \n";

static const GLchar *FRAG =
	VSTR
	"                                                   \n"
	"varying vec2 v_texcoord;                           \n"
	"                                                   \n"
	"uniform sampler2D sample0;                         \n"
	"                                                   \n"
	"void main(void) {                                  \n"
	"    gl_FragColor = texture2D(sample0, v_texcoord); \n"
	"}                                                  \n";

struct gl {
	SDL_Window *window;
	SDL_GLContext ctx;

	GLProgram prog;
	GLVertexShader vs;
	GLFragmentShader fs;

	GLTexture2D tex;
	GLVertexBuffer vb;
	GLElementBuffer eb;

	GLTexture2D staging;
	GLFramebuffer staging_fb;

	uint32_t w;
	uint32_t h;
	bool dummy_window;

	GLfloat sampler;
};

void gl_set_sampler(struct render_mod *mod, enum sampler sampler)
{
	struct gl *gl = (struct gl *) mod;
	gl->sampler = (GLfloat) ((sampler == SAMPLE_LINEAR) ? GL_LINEAR : GL_NEAREST);

	if (gl->tex != 0) {
		glBindTexture(GL_TEXTURE_2D, gl->tex);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl->sampler);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl->sampler);
	}
}

static int32_t gl_log_shader_errors(GLShader shader)
{
	GLint e = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &e);

	if (e != GL_TRUE) {
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &e);

		if (e > 0) {
			char *log = calloc(e, 1);

			glGetShaderInfoLog(shader, e, NULL, log);
			printf("%s\n", log);
			free(log);
		}

		return -1;
	}

	return 0;
}

int32_t gl_init(struct render_mod **mod_out, void *window, bool vsync,
	uint32_t width, uint32_t height, enum sampler sampler)
{
	struct gl **gl_out = (struct gl **) mod_out;
	struct gl *gl = *gl_out = calloc(1, sizeof(struct gl));

	int32_t e = PARSEC_OK;

	// GL needs a cross platform window to create a context, create a dummy for off screen drawing
	// if there isn't a real, visible window
	// XXX as far as I know, macOS does NOT need an NSWindow under the hood, Windows DOES need
	// an HWND, linux does NOT need an X window
	if (window) {
		gl->window = (SDL_Window *) window;

	} else {
		e = SDL_Init(SDL_INIT_VIDEO);
		if (e != 0) {printf("SDL_Init=%d\n", e); goto except;}
		gl->dummy_window = true;

		gl->window = SDL_CreateWindow("cddNES", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			500, 500, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
		if (gl->window == NULL) {printf("SDL_CreateWindow=0\n"); goto except;}
	}

	gl->ctx = SDL_GL_CreateContext(gl->window);
	if (!gl->ctx) {e = -1; printf("SDL_GL_CreateContext=%d\n", 0); printf("%s\n", SDL_GetError()); goto except;}

	e = SDL_GL_SetSwapInterval(vsync ? 1 : 0);
	if (e != 0) {printf("SDL_GL_SetSwapInterval=%d\n", e); printf("%s\n", SDL_GetError()); goto except;}

	gl_proc_load();
	if (!GL_PROC_SUCCESS) {e = -1; goto except;}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	gl->vs = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(gl->vs, 1, &VERT, NULL);
	glCompileShader(gl->vs);
	e = gl_log_shader_errors(gl->vs);
	if (e != 0) goto except;

	gl->fs = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(gl->fs, 1, &FRAG, NULL);
	glCompileShader(gl->fs);
	e = gl_log_shader_errors(gl->fs);
	if (e != 0) goto except;

	gl->prog = glCreateProgram();
	glAttachShader(gl->prog, gl->vs);
	glAttachShader(gl->prog, gl->fs);

	glBindAttribLocation(gl->prog, 0, "position");
	glBindAttribLocation(gl->prog, 1, "texcoord");

	glLinkProgram(gl->prog);
	glUseProgram(gl->prog);

	GLLocation sample0 = glGetUniformLocation(gl->prog, "sample0");

	glGenTextures(1, &gl->tex);
	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glUniform1i(sample0, 0);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	gl_set_sampler((struct render_mod *) gl, sampler);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glGenBuffers(1, &gl->vb);
	GLfloat vertices[] = {
		-1.0f,  1.0f,	// Position 0
		 0.0f,  0.0f,	// TexCoord 0
		-1.0f, -1.0f,	// Position 1
		 0.0f,  1.0f,	// TexCoord 1
		 1.0f, -1.0f,	// Position 2
		 1.0f,  1.0f,	// TexCoord 2
		 1.0f,  1.0f,	// Position 3
		 1.0f,  0.0f	// TexCoord 3
	};

	glBindBuffer(GL_ARRAY_BUFFER, gl->vb);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glGenBuffers(1, &gl->eb);
	GLshort elements[] = {
		0, 1, 2,
		2, 3, 0
	};

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->eb);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glGenFramebuffers(1, &gl->staging_fb);

	e = glGetError();
	if (e != 0) e = -1;

	except:

	if (e != 0)
		gl_destroy(mod_out);

	return e;
}

void gl_destroy(struct render_mod **mod_out)
{
	struct gl **gl_out = (struct gl **) mod_out;

	if (gl_out == NULL || *gl_out == NULL)
		return;

	struct gl *gl = *gl_out;

	if (gl->ctx) {
		if (GL_PROC_SUCCESS) {
			SDL_GL_MakeCurrent(gl->window, gl->ctx);

			if (gl->staging != 0)
				glDeleteTextures(1, &gl->staging);

			if (gl->staging_fb != 0)
				glDeleteFramebuffers(1, &gl->staging_fb);

			if (gl->vb != 0)
				glDeleteBuffers(1, &gl->vb);

			if (gl->eb != 0)
				glDeleteBuffers(1, &gl->eb);

			if (gl->tex != 0)
				glDeleteTextures(1, &gl->tex);

			if (gl->fs != 0)
				glDeleteShader(gl->fs);

			if (gl->vs != 0)
				glDeleteShader(gl->vs);

			if (gl->prog != 0)
				glDeleteProgram(gl->prog);
		}

		SDL_GL_MakeCurrent(gl->window, NULL);
		SDL_GL_DeleteContext(gl->ctx);
	}

	if (gl->dummy_window) {
		if (gl->window)
			SDL_DestroyWindow(gl->window);

		SDL_Quit();
	}

	free(gl);
	*gl_out = NULL;
}

void gl_get_device(struct render_mod *mod, struct render_device **device, struct render_context **context)
{
	mod;

	*device = NULL;
	*context = NULL;
}

static void gl_refresh_staging_texture(struct gl *gl, uint32_t window_w, uint32_t window_h)
{
	if ((gl->staging == 0) || (gl->w != window_w) || (gl->h != window_h)) {
		if (gl->staging != 0) {
			glDeleteTextures(1, &gl->staging);
			gl->staging = 0;
		}

		glGenTextures(1, &gl->staging);
		glBindTexture(GL_TEXTURE_2D, gl->staging);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window_w, window_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glBindFramebuffer(GL_FRAMEBUFFER, gl->staging_fb);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gl->staging, 0);

		gl->w = window_w;
		gl->h = window_h;
	}
}

static void gl_set_viewport(uint32_t window_w, uint32_t window_h, float ratio)
{
	float swidth = (float) window_w;
	float sheight = swidth / ratio;

	if ((float) window_h / 240.0f < ((float) window_w / ratio) / 256.0f) {
		sheight = (float) window_h;
		swidth = sheight * ratio;
	}

	int32_t x = ((int32_t) window_w - (int32_t) swidth) / 2;
	int32_t y = ((int32_t) window_h - (int32_t) sheight) / 2;

	glViewport(x, y, lrint(swidth), lrint(sheight));
}

void gl_draw(struct render_mod *mod, uint32_t window_w, uint32_t window_h, uint32_t *pixels, uint32_t aspect)
{
	struct gl *gl = (struct gl *) mod;

	glUseProgram(gl->prog);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vb);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl->eb);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void *) (2 * sizeof(GLfloat)));

	gl_refresh_staging_texture(gl, window_w, window_h);
	gl_set_viewport(window_w, window_h, ASPECT_RATIO(aspect));

	glBindTexture(GL_TEXTURE_2D, gl->tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 240, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	glBindFramebuffer(GL_FRAMEBUFFER, gl->staging_fb);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}

enum ParsecStatus gl_submit_parsec(struct render_mod *mod, ParsecDSO *parsec)
{
	struct gl *gl = (struct gl *) mod;

	return ParsecHostGLSubmitFrame(parsec, gl->staging);
}

void gl_present(struct render_mod *mod)
{
	struct gl *gl = (struct gl *) mod;

	if (!gl->dummy_window) {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gl->staging_fb);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBlitFramebuffer(0, 0, gl->w, gl->h, 0, 0, gl->w, gl->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		SDL_GL_SwapWindow(gl->window);
	}
}
