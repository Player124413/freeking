#include "NavFile.h"
#include <cstring>

namespace Freeking
{
	std::vector<NavNode> NavFile::ReadNodes(const uint8_t* data, size_t size)
	{
		if (data == nullptr || size < 6)
		{
			return {};
		}

		uint16_t magic = 0;
		std::memcpy(&magic, &data[0], sizeof(magic));
		if (magic != 4)
		{
			return {};
		}

		size_t pos = 4;
		uint16_t numNodes = 0;
		std::memcpy(&numNodes, &data[pos], sizeof(numNodes));
		pos += 2;

		// Sanity cap: a corrupt/truncated nav file must not allocate gigabytes.
		if (numNodes == 0 || numNodes > 65535)
		{
			return {};
		}

		size_t flagBytes = (static_cast<size_t>(numNodes) + 1) / 2;
		size_t perNode = 16 + 52 + flagBytes;
		if (perNode > size || (size - pos) / perNode < numNodes)
		{
			return {};
		}

		std::vector<NavNode> nodes;
		nodes.reserve(numNodes);

		for (int i = 0; i < numNodes; ++i)
		{
			NavNode node;
			std::memcpy(&node.Position, &data[pos], sizeof(Vector4f));
			pos += 16;
			pos += 52; // what's this data

			node.NodeFlags.reserve(numNodes);
			for (int j = 0; j < numNodes; ++j)
			{
				uint8_t packed = data[pos + (j / 2)];
				node.NodeFlags.emplace_back((NavFlag)((j % 2 == 0) ? packed >> 4 : packed & 0x0F));
			}

			pos += flagBytes;
			nodes.push_back(std::move(node));
		}

		return nodes;
	}
}
