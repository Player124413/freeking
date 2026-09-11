#include "Texture2D.h"
#include <cstring>
#include "TextureLoader.h"
#include "ThirdParty/stb/stb_image.h"
#include <algorithm>
#include <vector>

namespace Freeking
{
	void TextureLibrary::UpdateLoaders()
	{
		AddLoader<TextureLoader>();
	}

	TextureLibrary Texture2D::Library;

	std::shared_ptr<Texture2D> Texture2D::GetFallback()
	{
		static std::shared_ptr<Texture2D> fallback;
		if (!fallback)
		{
			const int size = 64;
			std::vector<uint8_t> pixels(size * size * 4);
			for (int y = 0; y < size; ++y)
			{
				for (int x = 0; x < size; ++x)
				{
					bool odd = ((x / 8) + (y / 8)) % 2 == 0;
					uint8_t* p = &pixels[(y * size + x) * 4];
					p[0] = odd ? 255 : 0;
					p[1] = 0;
					p[2] = odd ? 255 : 0;
					p[3] = 255;
				}
			}
			fallback = std::make_shared<Texture2D>(size, size, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		}
		return fallback;
	}

	Texture2D::Texture2D(GLsizei width, GLsizei height, GLenum internalFormat, GLenum format, GLenum type, const void* data) :
		_width(width),
		_height(height),
		_internalFormat(internalFormat),
		_format(format),
		_type(type),
		_id(0)
	{
		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_2D, _id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, _internalFormat, _width, _height, 0, _format, _type, data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture2D::Texture2D(GLsizei width, GLsizei height, uint8_t r, uint8_t g, uint8_t b) :
		_width(width),
		_height(height),
		_internalFormat(GL_RGB8),
		_format(GL_RGB),
		_type(GL_UNSIGNED_BYTE),
		_id(0)
	{
		std::vector<uint8_t> buffer((width * height) * 3, 0);
		uint8_t pixel[] { r, g, b };
		for (auto i = 0; i < width * height; ++i)
		{
			std::memcpy(&buffer[i * 3], pixel, 3);
		}

		glGenTextures(1, &_id);
		glBindTexture(GL_TEXTURE_2D, _id);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, _internalFormat, _width, _height, 0, _format, _type, (void*)buffer.data());
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	Texture2D::~Texture2D()
	{
		if (_id != 0)
		{
			glDeleteTextures(1, &_id);
		}
	}
}
