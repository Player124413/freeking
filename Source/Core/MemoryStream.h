#pragma once

#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace Freeking
{
	enum class SeekMode
	{
		Begin = SEEK_SET,
		Current = SEEK_CUR,
		End = SEEK_END
	};

	class MemoryStream
	{
	public:

		MemoryStream(const std::vector<uint8_t>&);

		void Read(uint8_t*, std::size_t);

		template <typename T>
		T Read()
		{
			// Bounds-checked and alignment-safe (memcpy): truncated data
			// yields zero and parks at EOF instead of crashing.
			T value{};
			if (static_cast<std::size_t>(std::distance(_position, _data.end())) >= sizeof(T))
			{
				std::memcpy(&value, &(*_position), sizeof(T));
				std::advance(_position, sizeof(T));
			}
			else
			{
				_position = _data.end();
			}

			return value;
		}

		void Seek(std::size_t position, SeekMode mode);
		inline std::size_t Position() { return std::distance(_data.begin(), _position); }
		inline std::size_t Size() const { return _data.size(); }
		inline bool End() const { return _position == _data.end(); }

	protected:

		std::vector<uint8_t> _data;
		std::vector<uint8_t>::iterator _position;
	};
}
