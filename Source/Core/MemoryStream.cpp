#include "MemoryStream.h"
#include <cstring>
#include <algorithm>

namespace Freeking
{
	MemoryStream::MemoryStream(const std::vector<uint8_t>& data)
	{
		_data = data;
		_position = _data.begin();
	}

	void MemoryStream::Read(uint8_t* dest, std::size_t length)
	{
		// Clamp truncated reads to EOF instead of over-reading the heap.
		std::size_t remaining = static_cast<std::size_t>(std::distance(_position, _data.end()));
		std::size_t clamped = std::min(length, remaining);
		if (clamped > 0)
		{
			std::memcpy(dest, &(*_position), clamped);
			std::advance(_position, clamped);
		}
	}

	void MemoryStream::Seek(std::size_t position, SeekMode mode)
	{
		switch (mode)
		{
		case SeekMode::Begin:
			_position = std::next(_data.begin(), std::min(position, _data.size()));
			break;
		case SeekMode::Current:
			_position = std::next(_position, std::min(position,
				static_cast<std::size_t>(std::distance(_position, _data.end()))));
			break;
		case SeekMode::End:
			_position = std::next(_data.end(),
				-static_cast<std::ptrdiff_t>(std::min(position, _data.size())));
			break;
		}
	}
}
