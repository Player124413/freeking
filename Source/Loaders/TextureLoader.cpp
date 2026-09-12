#include "TextureLoader.h"
#include "Texture2D.h"
#include "stb_image.h"

namespace Freeking
{
	bool TextureLoader::CanLoadExtension(const std::string& extension) const
	{
		if (extension == ".png") return true;
		if (extension == ".jpg") return true;
		if (extension == ".jpeg") return true;
		if (extension == ".tga") return true;
		if (extension == ".bmp") return true;
		if (extension == ".psd") return true;

		return false;
	};

	TextureLoader::AssetPtr TextureLoader::Load(const std::string& name) const
	{
		if (auto buffer = FileSystem::GetFileData(name); !buffer.empty())
		{
			int imageWidth, imageHeight, imageChannels;
			uint8_t* image = stbi_load_from_memory(
				(uint8_t*)buffer.data(), (std::int32_t)buffer.size(),
				&imageWidth, &imageHeight, &imageChannels, 0);
			if (image != nullptr && imageChannels != 3 && imageChannels != 4)
			{
				// Grayscale images would mismatch the RGB/RGBA upload below;
				// reload converted to RGBA instead of over-reading in the driver.
				stbi_image_free(image);
				image = stbi_load_from_memory(
					(uint8_t*)buffer.data(), (std::int32_t)buffer.size(),
					&imageWidth, &imageHeight, &imageChannels, 4);
			}
			if (image != nullptr)
			{
				auto texture = std::make_shared<Texture2D>(
					imageWidth, imageHeight,
					imageChannels == 3 ? GL_RGB8 : GL_RGBA8,
					imageChannels == 3 ? GL_RGB : GL_RGBA,
					GL_UNSIGNED_BYTE,
					image);

				stbi_image_free(image);

				return texture;
			}
		}

		return nullptr;
	}
}
