#include "TextureBuffer.h"
#include <memory>

namespace Freeking
{
#ifndef __ANDROID__
	TextureBuffer::TextureBuffer(void* buffer, size_t length, GLenum format) :
		_id(0),
		_bufferId(0),
		_width(0),
		_height(0)
	{
		glGenTextures(1, &_id);
		glGenBuffers(1, &_bufferId);

		glBindBuffer(GL_TEXTURE_BUFFER, _bufferId);
		glBufferData(GL_TEXTURE_BUFFER, length, buffer, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);

		glBindTexture(GL_TEXTURE_BUFFER, _id);
		glTexBuffer(GL_TEXTURE_BUFFER, format, _bufferId);
		glBindTexture(GL_TEXTURE_BUFFER, 0);
	}

	void TextureBuffer::SetBuffer(void* buffer, size_t length)
	{
		glBindBuffer(GL_TEXTURE_BUFFER, _bufferId);
		glBufferSubData(GL_TEXTURE_BUFFER, 0, length, buffer);
		glBindBuffer(GL_TEXTURE_BUFFER, 0);
	}
#else
	TextureBuffer::TextureBuffer(GLuint id, int width, int height) :
		_id(id),
		_bufferId(0),
		_width(width),
		_height(height)
	{
	}

	std::shared_ptr<TextureBuffer> TextureBuffer::CreateByte4(int width, int height, const void* data)
	{
		GLuint id = 0;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8I, width, height, 0, GL_RGBA_INTEGER, GL_BYTE, data);
		// Integer textures are never filtered; texelFetch() ignores these,
		// but keep them valid anyway since no sampler object is bound.
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		return std::shared_ptr<TextureBuffer>(new TextureBuffer(id, width, height));
	}

	std::shared_ptr<TextureBuffer> TextureBuffer::CreateFloat4(int width, int height, const void* data)
	{
		GLuint id = 0;
		glGenTextures(1, &id);
		glBindTexture(GL_TEXTURE_2D, id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);

		return std::shared_ptr<TextureBuffer>(new TextureBuffer(id, width, height));
	}
#endif

	TextureBuffer::~TextureBuffer()
	{
		if (_bufferId != 0)
		{
			glDeleteBuffers(1, &_bufferId);
			_bufferId = 0;
		}

		if (_id != 0)
		{
			glDeleteTextures(1, &_id);
			_id = 0;
		}
	}
}
