#pragma once

#include <cstdint>
#include "FileSystem.h"
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <string>

namespace Freeking
{
	class PhysicalFileSystem : public IFileSystem
	{
	public:

		static std::unique_ptr<PhysicalFileSystem> Create(const std::filesystem::path& path);

		PhysicalFileSystem(const std::filesystem::path& path);

		virtual bool FileExists(const std::string& filename) override;
		virtual std::vector<uint8_t> GetFileData(const std::string& filename) override;
		virtual void ListFiles(const std::string& prefix, const std::string& extension, std::vector<std::string>& outFiles) override;

	private:

		// Resolves "filename" against _path. Exact matches win; otherwise a
		// case-insensitive component-by-component search is performed (game
		// installs often have mixed-case file names while lookups are
		// lower-case, and Android/Linux file systems are case-sensitive).
		std::filesystem::path Resolve(const std::string& filename);

		std::filesystem::path _path;
		std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> _dirCache;
	};
}
