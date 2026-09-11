#pragma once

// Single point of inclusion for OpenGL headers.
// Desktop builds use the vendored glad loader (OpenGL 4.6 core).
// Android builds use OpenGL ES 3.0 directly from the NDK (no loader needed,
// all core ES 3.0 entry points are linked from libGLESv2).

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#define FREEKING_GLES 1
#else
#include <glad/gl.h>
#endif

namespace Freeking
{
#ifdef __ANDROID__
	inline constexpr bool IsGLESBuild = true;
#else
	inline constexpr bool IsGLESBuild = false;
#endif
}
