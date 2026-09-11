#include "TextureSampler.h"
#include <cstring>

namespace Freeking
{
	static void ApplyAnisotropy(GLuint sampler)
	{
#ifdef __ANDROID__
		// Anisotropic filtering is an extension on GLES; enable a modest
		// level only when the driver exposes it.
		GLint numExtensions = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
		for (GLint i = 0; i < numExtensions; ++i)
		{
			const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
			if (ext != nullptr && std::strstr(ext, "GL_EXT_texture_filter_anisotropic") != nullptr)
			{
				GLfloat maxAnisotropy = 0.0f;
				glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
				if (maxAnisotropy > 1.0f)
				{
					glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT,
						maxAnisotropy < 4.0f ? maxAnisotropy : 4.0f);
				}
				break;
			}
		}
#else
		glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY, 16);
#endif
	}

	static int WrapModes[2] =
	{
		GL_REPEAT,
		GL_CLAMP_TO_EDGE,
	};

	static int FilterModesMin[4] =
	{
		GL_LINEAR_MIPMAP_LINEAR,
		GL_NEAREST_MIPMAP_NEAREST,
		GL_LINEAR,
		GL_NEAREST,
	};

	static int FilterModesMag[4] =
	{
		GL_LINEAR,
		GL_NEAREST,
		GL_LINEAR,
		GL_NEAREST,
	};

	TextureSamplerLibrary TextureSampler::Library = TextureSamplerLibrary();

	std::shared_ptr<TextureSampler> TextureSampler::GetDefault()
	{
		static std::shared_ptr<TextureSampler> sampler = Library.Get({ TextureWrapMode::Repeat, TextureFilterMode::Linear });
		return sampler;
	}

	const TextureSamplerLibrary::TextureSamplerPtr& TextureSamplerLibrary::Get(const TextureSamplerInitializer& initializer)
	{
		if (auto it = _samplers.find(initializer); it != _samplers.end())
		{
			return it->second;
		}
		else
		{
			return _samplers.emplace(initializer, std::make_shared<TextureSampler>(initializer)).first->second;
		}
	}

	TextureSampler::TextureSampler(const TextureSamplerInitializer& initializer) :
		_id(0)
	{
		glGenSamplers(1, &_id);
		glSamplerParameteri(_id, GL_TEXTURE_MIN_FILTER, FilterModesMin[static_cast<int>(initializer.filter)]);
		glSamplerParameteri(_id, GL_TEXTURE_MAG_FILTER, FilterModesMag[static_cast<int>(initializer.filter)]);
		glSamplerParameteri(_id, GL_TEXTURE_WRAP_S, WrapModes[static_cast<int>(initializer.wrap)]);
		glSamplerParameteri(_id, GL_TEXTURE_WRAP_T, WrapModes[static_cast<int>(initializer.wrap)]);
		ApplyAnisotropy(_id);
	}

	TextureSampler::~TextureSampler()
	{
		if (_id != 0)
		{
			glDeleteSamplers(1, &_id);
		}
	}
}
