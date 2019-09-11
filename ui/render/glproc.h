#pragma once

#include <stdbool.h>

#if defined(_WIN32)

#include <windows.h>
#include <GL/gl.h>

#include "GL/glext30.h"

extern bool GL_PROC_SUCCESS;

#ifdef __cplusplus
extern "C" {
#endif

extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLDETACHSHADERPROC glDetachShader;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLBLENDEQUATIONPROC glBlendEquation;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLDRAWBUFFERSPROC glDrawBuffers;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;
extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;

void gl_proc_load(void);

#ifdef __cplusplus
}
#endif

#else

#define GL_PROC_SUCCESS true

#if defined(__APPLE__)
	#include <OpenGL/gl3.h>

#elif defined(__linux__)
	#define GL_GLEXT_PROTOTYPES
	#include <GL/gl.h>
#endif

#define gl_proc_load()

#endif
