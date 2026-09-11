#pragma once

#include "Texture.h"
#include <memory>

namespace Freeking
{
	class TextureBuffer : public Texture
	{

	public:

#ifndef __ANDROID__
		TextureBuffer(void* buffer, size_t length, GLenum format);
		void SetBuffer(void* buffer, size_t length);
#else
		// OpenGL ES has no buffer textures (GL_TEXTURE_BUFFER), so on Android
		// the same data is uploaded into plain 2D textures and sampled with
		// texelFetch(isampler2D/sampler2D) instead. Shaders switch to the
		// FREEKING_GLES code path automatically, see DynamicModel.shader.
		static std::shared_ptr<TextureBuffer> CreateByte4(int width, int height, const void* data);
		static std::shared_ptr<TextureBuffer> CreateFloat4(int width, int height, const void* data);

		int GetWidth() const { return _width; }
		int GetHeight() const { return _height; }
#endif

		~TextureBuffer();

		virtual const GLuint GetId() const override { return _id; }

	private:

#ifdef __ANDROID__
		TextureBuffer(GLuint id, int width, int height);
#endif

	protected:

		GLuint _id;
		GLuint _bufferId;
		int _width;
		int _height;
	};
}
