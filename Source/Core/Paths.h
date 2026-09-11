#pragma once

#include <filesystem>

namespace Freeking
{
	class Paths
	{
	public:

		static std::filesystem::path SteamDir();
		static std::filesystem::path SteamGameDir(uint32_t appid);
		static std::filesystem::path KingpinDir();
		// Bundled engine assets (shaders, fonts, UI textures).
		static std::filesystem::path AssetsDir();
		// Writable per-user directory (settings, saves, touch layout).
		static std::filesystem::path UserDir();
	};
}
