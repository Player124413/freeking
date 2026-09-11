#include "Paths.h"
#include "ThirdParty/ValveFileVDF/vdf_parser.hpp"
#include <cstdlib>

#ifdef _WIN32
#include <Windows.h>
#include <winreg.h>
#endif

#ifdef __ANDROID__
#include <SDL_system.h>
#endif

namespace Freeking
{
#ifdef __ANDROID__
	static std::filesystem::path AndroidInternalStorage()
	{
		if (const char* path = SDL_AndroidGetInternalStoragePath())
		{
			return std::filesystem::path(path);
		}

		return {};
	}

	static std::filesystem::path AndroidExternalStorage()
	{
		if (const char* path = SDL_AndroidGetExternalStoragePath())
		{
			return std::filesystem::path(path);
		}

		return {};
	}
#endif

	std::filesystem::path Paths::SteamDir()
	{
		static std::filesystem::path dir;
		static bool probed = false;

		if (!probed)
		{
			probed = true;
#if defined(__linux__) || defined(__APPLE__)
			if (const char* home = std::getenv("HOME"))
			{
				std::error_code ec;
				std::filesystem::path candidates[] =
				{
#ifdef __APPLE__
					std::filesystem::path(home) / "Library/Application Support/Steam",
#else
					std::filesystem::path(home) / ".steam/steam",
					std::filesystem::path(home) / ".local/share/Steam",
#endif
				};

				for (const auto& candidate : candidates)
				{
					if (std::filesystem::exists(candidate / "steamapps/libraryfolders.vdf", ec))
					{
						dir = candidate;
						break;
					}
				}
			}
#endif
#ifdef _WIN32
			HKEY key;
			TCHAR value[1024];
			DWORD length = sizeof(value);
			LONG result = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &key);
			if (result == 0)
			{
				RegQueryValueEx(key, "SteamPath", NULL, NULL, reinterpret_cast<LPBYTE>(&value), &length);
				RegCloseKey(key);

				return value;
			}
#endif
		}

		return dir;
	}

	static bool LooksLikeKingpinDir(const std::filesystem::path& dir)
	{
		if (dir.empty())
		{
			return false;
		}

		std::error_code ec;
		if (std::filesystem::is_directory(dir / "main", ec))
		{
			return true;
		}

		for (int i = 0; i <= 9; ++i)
		{
			if (std::filesystem::exists(dir / ("Pak" + std::to_string(i) + ".pak"), ec) ||
				std::filesystem::exists(dir / ("pak" + std::to_string(i) + ".pak"), ec) ||
				std::filesystem::exists(dir / ("main/Pak" + std::to_string(i) + ".pak"), ec) ||
				std::filesystem::exists(dir / ("main/pak" + std::to_string(i) + ".pak"), ec))
			{
				return true;
			}
		}

		return false;
	}

	std::filesystem::path Paths::KingpinDir()
	{
		static std::filesystem::path dir;
		static bool probed = false;

		if (probed)
		{
			return dir;
		}

		probed = true;

#ifdef __ANDROID__
		// The launcher copies the user's game files here.
		auto external = AndroidExternalStorage();
		if (!external.empty() && LooksLikeKingpinDir(external / "kingpin"))
		{
			dir = external / "kingpin";
		}

		return dir;
#else
		// Explicit override wins everywhere.
		if (const char* env = std::getenv("FREEKING_KINGPIN_DIR"))
		{
			if (LooksLikeKingpinDir(env))
			{
				dir = env;

				return dir;
			}
		}

		// Next to the executable / working directory.
		{
			std::error_code ec;
			auto cwd = std::filesystem::current_path(ec);
			if (LooksLikeKingpinDir(cwd / "kingpin"))
			{
				dir = cwd / "kingpin";

				return dir;
			}

			if (LooksLikeKingpinDir(cwd))
			{
				dir = cwd;

				return dir;
			}
		}
#ifdef _WIN32
			HKEY key;
			TCHAR value[1024];
			DWORD length = sizeof(value);
			LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\kingpin.exe", 0, KEY_READ, &key);
			if (result == 0)
			{
				RegQueryValueEx(key, "Path", NULL, NULL, reinterpret_cast<LPBYTE>(&value), &length);
				RegCloseKey(key);

				dir = value;
			}
			else
			{
				dir = SteamGameDir(38430);
			}
#else
			dir = SteamGameDir(38430);
#endif
#endif

		return dir;
	}

	std::filesystem::path Paths::SteamGameDir(uint32_t appid)
	{
		const auto& steamPath = SteamDir();
		if (steamPath.empty())
		{
			return {};
		}

		auto vdfPath = steamPath / "steamapps/libraryfolders.vdf";
		if (!std::filesystem::exists(vdfPath))
		{
			return {};
		}

		std::ifstream vdfFile(vdfPath);
		auto vdfRoot = tyti::vdf::read(vdfFile);

		std::vector<std::filesystem::path> libraryFolders;
		libraryFolders.push_back(steamPath);

		for (const auto& attrib : vdfRoot.attribs)
		{	
			const auto& key = attrib.first;
			if (key.size() != 1 || !std::isdigit(key[0]))
			{
				continue;
			}

			libraryFolders.push_back(attrib.second);
		}

		auto appidFilename = "appmanifest_" + std::to_string(appid) + ".acf";

		for (auto libraryFolder : libraryFolders)
		{
			for (const auto& file : std::filesystem::directory_iterator(libraryFolder / "steamapps"))
			{
				if (file.is_directory() ||
					file.path().extension() != ".acf")
				{
					continue;
				}

				if (file.path().filename() == appidFilename)
				{
					std::ifstream acfFile(file.path());
					auto acfRoot = tyti::vdf::read(acfFile);
					auto installDir = acfRoot.attribs["installdir"];
					auto gamePath = file.path().parent_path() /
						std::filesystem::path("common") /
						std::filesystem::path(installDir);

					return gamePath;
				}
			}
		}

		return {};
	}

	std::filesystem::path Paths::AssetsDir()
	{
#ifdef __ANDROID__
		// Engine assets are copied out of the APK by the launcher on first run.
		// The APK assets mirror the repo Assets/ tree at internal-storage root.
		return AndroidInternalStorage();
#else
		std::error_code ec;
		return std::filesystem::current_path(ec) / "Assets";
#endif
	}

	std::filesystem::path Paths::UserDir()
	{
#ifdef __ANDROID__
		auto internal = AndroidInternalStorage();
		if (!internal.empty())
		{
			return internal;
		}

		return {};
#else
		std::error_code ec;
		auto dir = std::filesystem::current_path(ec) / "user";
		std::filesystem::create_directories(dir, ec);

		return dir;
#endif
	}
}
