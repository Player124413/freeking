#include "PakFileSystem.h"

namespace Freeking
{
	std::unique_ptr<PakFileSystem> PakFileSystem::Create(const std::filesystem::path& path)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
		{
			return std::make_unique<PakFileSystem>(path);
		}

		return nullptr;
	}

	PakFileSystem::PakFileSystem(const std::filesystem::path& path)
	{
		_stream.open(path, std::ios::binary | std::ios::ate);

		if (!_stream.is_open())
		{
			return;
		}

		PakHeader header = {};
		_stream.seekg(0);
		_stream.read((char*)&header, sizeof(header));

		if (!_stream || header.id != Id || header.size <= 0 ||
			static_cast<size_t>(header.size) % sizeof(PakFileItem) != 0)
		{
			_stream.close();

			return;
		}

		int numFiles = header.size / sizeof(PakFileItem);
		if (numFiles <= 0 || numFiles > 1000000)
		{
			_stream.close();

			return;
		}

		std::vector<PakFileItem> fileItems(numFiles);

		_stream.seekg(header.offset);
		_stream.read((char*)fileItems.data(), header.size);

		if (!_stream)
		{
			_stream.close();

			return;
		}

		_fileItems.reserve(static_cast<size_t>(numFiles));

		for (const PakFileItem& fileItem : fileItems)
		{
			if (fileItem.offset < 0 || fileItem.size < 0)
			{
				continue;
			}

			size_t nameLength = 0;
			while (nameLength < sizeof(fileItem.name) && fileItem.name[nameLength] != '\0')
			{
				++nameLength;
			}

			if (nameLength == 0)
			{
				continue;
			}

			std::string name(fileItem.name, nameLength);

			_fileItems.emplace(
				FileSystem::ToLower(name),
				FileItem{ name, fileItem.offset, fileItem.size });
		}
	}

	PakFileSystem::~PakFileSystem()
	{
		if (_stream.is_open())
		{
			_stream.close();
		}
	}

	bool PakFileSystem::FileExists(const std::string& filename)
	{
		if (!_stream.is_open())
		{
			return false;
		}

		return _fileItems.find(FileSystem::ToLower(filename)) != _fileItems.end();
	}

	std::vector<uint8_t> PakFileSystem::GetFileData(const std::string& filename)
	{
		if (!_stream.is_open())
		{
			return {};
		}

		auto it = _fileItems.find(FileSystem::ToLower(filename));
		if (it == _fileItems.end())
		{
			return {};
		}

		const auto& fileItem = it->second;
		if (fileItem.size <= 0 || fileItem.size > (512 * 1024 * 1024))
		{
			return {};
		}

		std::vector<uint8_t> fileData(static_cast<size_t>(fileItem.size));
		_stream.seekg(fileItem.offset);
		_stream.read((char*)fileData.data(), fileItem.size);

		if (!_stream)
		{
			return {};
		}

		return fileData;
	}

	void PakFileSystem::ListFiles(const std::string& prefix, const std::string& extension, std::vector<std::string>& outFiles)
	{
		if (!_stream.is_open())
		{
			return;
		}

		std::string lowerPrefix = FileSystem::ToLower(prefix);
		std::string lowerExt = FileSystem::ToLower(extension);

		for (const auto& [lowerName, item] : _fileItems)
		{
			if (lowerName.size() < lowerPrefix.size() + lowerExt.size())
			{
				continue;
			}

			if (lowerName.compare(0, lowerPrefix.size(), lowerPrefix) != 0)
			{
				continue;
			}

			if (lowerName.compare(lowerName.size() - lowerExt.size(), lowerExt.size(), lowerExt) != 0)
			{
				continue;
			}

			outFiles.push_back(item.name);
		}
	}
}
