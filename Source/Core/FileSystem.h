#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

namespace Freeking
{
	class IFileSystem
	{
	public:

		IFileSystem() {}
		virtual ~IFileSystem() {}

		virtual bool FileExists(const std::string& filename) = 0;
		virtual std::vector<uint8_t> GetFileData(const std::string& filename) = 0;

		// Lists files under "prefix" (e.g. "maps") whose names end with
		// "extension" (e.g. ".bsp", case-insensitive). Returned names use
		// forward slashes, e.g. "maps/sr1.bsp".
		virtual void ListFiles(const std::string& prefix, const std::string& extension, std::vector<std::string>& outFiles)
		{
			(void)prefix;
			(void)extension;
			(void)outFiles;
		}
	};

	class FileSystem
	{
	public:

		FileSystem() = delete;
		~FileSystem() = delete;

		static void AddFileSystem(std::unique_ptr<IFileSystem> fileSystem);
		static bool FileExists(const std::string& filename);
		static std::vector<uint8_t> GetFileData(const std::string& filename);
		static std::vector<std::string> ListFiles(const std::string& prefix, const std::string& extension);

		static std::string ToLower(std::string s);

	private:

		static std::vector<std::unique_ptr<IFileSystem>> _fileSystems;
	};
}
