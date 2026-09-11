#include "PhysicalFileSystem.h"
#include <fstream>
#include <algorithm>

namespace Freeking
{
	std::unique_ptr<PhysicalFileSystem> PhysicalFileSystem::Create(const std::filesystem::path& path)
	{
		return std::make_unique<PhysicalFileSystem>(path);
	}

	PhysicalFileSystem::PhysicalFileSystem(const std::filesystem::path& path) :
		_path(path)
	{
	}

	std::filesystem::path PhysicalFileSystem::Resolve(const std::string& filename)
	{
		std::error_code ec;

		auto direct = _path / filename;
		if (std::filesystem::exists(direct, ec) && !std::filesystem::is_directory(direct, ec))
		{
			return direct;
		}

		// Case-insensitive fallback, component by component.
		std::filesystem::path current = _path;
		std::filesystem::path request(filename);

		for (const auto& component : request)
		{
			std::string wanted = FileSystem::ToLower(component.string());
			if (wanted.empty() || wanted == "." || wanted == "/" || wanted == "\\")
			{
				continue;
			}

			std::string cacheKey = FileSystem::ToLower(current.string());
			auto cacheIt = _dirCache.find(cacheKey);
			if (cacheIt == _dirCache.end())
			{
				std::vector<std::pair<std::string, std::string>> entries;
				std::error_code iterEc;
				auto it = std::filesystem::directory_iterator(current, iterEc);
				if (!iterEc)
				{
					for (const auto& entry : it)
					{
						std::string name = entry.path().filename().string();
						entries.emplace_back(FileSystem::ToLower(name), name);
					}
				}
				cacheIt = _dirCache.emplace(cacheKey, std::move(entries)).first;
			}

			bool found = false;
			for (const auto& [lowerName, actualName] : cacheIt->second)
			{
				if (lowerName == wanted)
				{
					current /= actualName;
					found = true;
					break;
				}
			}

			if (!found)
			{
				return {};
			}
		}

		if (std::filesystem::exists(current, ec) && !std::filesystem::is_directory(current, ec))
		{
			return current;
		}

		return {};
	}

	bool PhysicalFileSystem::FileExists(const std::string& filename)
	{
		return !Resolve(filename).empty();
	}

	std::vector<uint8_t> PhysicalFileSystem::GetFileData(const std::string& filename)
	{
		auto filepath = Resolve(filename);
		if (filepath.empty())
		{
			return {};
		}

		std::error_code ec;
		auto fileSize = std::filesystem::file_size(filepath, ec);
		if (ec || fileSize == 0 || fileSize > (512 * 1024 * 1024))
		{
			return {};
		}

		std::ifstream stream(filepath, std::ios::binary | std::ios::ate);
		if (!stream.is_open())
		{
			return {};
		}

		std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
		stream.seekg(0);
		stream.read((char*)buffer.data(), buffer.size());

		if (!stream)
		{
			return {};
		}

		return buffer;
	}

	void PhysicalFileSystem::ListFiles(const std::string& prefix, const std::string& extension, std::vector<std::string>& outFiles)
	{
		std::error_code ec;
		std::filesystem::path base = _path / prefix;
		if (!std::filesystem::exists(base, ec) || !std::filesystem::is_directory(base, ec))
		{
			// Try case-insensitive resolution of the prefix directory.
			base = _path;
			std::filesystem::path request(prefix);
			for (const auto& component : request)
			{
				std::string probe = (base / component).string();
				(void)probe;
				base /= component;
			}
			if (!std::filesystem::exists(base, ec))
			{
				return;
			}
		}

		std::string lowerExt = FileSystem::ToLower(extension);
		std::error_code iterEc;
		for (auto it = std::filesystem::recursive_directory_iterator(base, iterEc);
			it != std::filesystem::recursive_directory_iterator(); it.increment(iterEc))
		{
			if (iterEc)
			{
				break;
			}

			if (!it->is_regular_file(iterEc) || iterEc)
			{
				continue;
			}

			std::string ext = FileSystem::ToLower(it->path().extension().string());
			if (ext != lowerExt)
			{
				continue;
			}

			auto relative = std::filesystem::relative(it->path(), _path, ec);
			if (ec)
			{
				continue;
			}

			std::string name = relative.string();
			std::replace(name.begin(), name.end(), '\\', '/');
			outFiles.push_back(name);
		}
	}
}
