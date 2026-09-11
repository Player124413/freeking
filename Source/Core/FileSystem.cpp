#include "FileSystem.h"
#include <filesystem>
#include <fstream>
#include <cctype>
#include <algorithm>

namespace Freeking
{
	std::vector<std::unique_ptr<IFileSystem>> FileSystem::_fileSystems = {};

	void FileSystem::AddFileSystem(std::unique_ptr<IFileSystem> fileSystem)
	{
		if (fileSystem)
		{
			_fileSystems.push_back(std::move(fileSystem));
		}
	}

	bool FileSystem::FileExists(const std::string& filename)
	{
		for (const auto& fileSystem : _fileSystems)
		{
			if (fileSystem->FileExists(filename))
			{
				return true;
			}
		}

		return false;
	}

	std::vector<uint8_t> FileSystem::GetFileData(const std::string& filename)
	{
		for (const auto& fileSystem : _fileSystems)
		{
			if (fileSystem->FileExists(filename))
			{
				return fileSystem->GetFileData(filename);
			}
		}

		return {};
	}

	std::vector<std::string> FileSystem::ListFiles(const std::string& prefix, const std::string& extension)
	{
		std::vector<std::string> files;

		for (const auto& fileSystem : _fileSystems)
		{
			fileSystem->ListFiles(prefix, extension, files);
		}

		std::sort(files.begin(), files.end());
		files.erase(std::unique(files.begin(), files.end()), files.end());

		return files;
	}

	std::string FileSystem::ToLower(std::string s)
	{
		for (auto& c : s)
		{
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}

		return s;
	}
}
