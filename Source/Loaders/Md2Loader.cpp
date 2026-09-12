#include "Md2Loader.h"
#include "DynamicModel.h"
#include "Md2File.h"
#include <cstring>
#include <climits>

namespace Freeking
{
	bool MD2Loader::CanLoadExtension(const std::string& extension) const
	{
		if (extension == ".md2") return true;

		return false;
	};

	namespace
	{
		// Bounds-checked sequential reader: file offsets/counts come from
		// untrusted data, so every read is validated against the buffer.
		template<typename T>
		const T* ReadArray(const void* base, size_t fileSize, uint32_t& pos, uint32_t count)
		{
			size_t need = sizeof(T) * static_cast<size_t>(count);
			if (pos > fileSize || need > fileSize - pos)
			{
				return nullptr;
			}

			const T* ptr = reinterpret_cast<const T*>(static_cast<const char*>(base) + pos);
			pos += static_cast<uint32_t>(need);

			return ptr;
		}
	}

	MD2Loader::AssetPtr MD2Loader::Load(const std::string& name) const
	{
		if (auto buffer = FileSystem::GetFileData(name); !buffer.empty())
		{
			if (buffer.size() < sizeof(MD2Header))
			{
				return nullptr;
			}

			const auto& file = MD2File::Create(buffer.data());
			if (!file.IsValid())
			{
				return nullptr;
			}

			const auto& header = file.Header;
			if (header.NumFrames < 0 || header.NumVertices < 0 || header.NumCommands < 0 ||
				header.NumSkins < 0)
			{
				return nullptr;
			}

			size_t fileSize = buffer.size();
			auto mesh = std::make_shared<DynamicModel>();

			uint32_t pos = static_cast<uint32_t>(header.OffsetFrames);
			for (int frameIndex = 0; frameIndex < header.NumFrames; ++frameIndex)
			{
				auto frame = ReadArray<MD2Frame>(&file, fileSize, pos, 1);
				auto vertices = (frame != nullptr)
					? ReadArray<MD2Vertex>(&file, fileSize, pos, static_cast<uint32_t>(header.NumVertices))
					: nullptr;
				if (frame == nullptr || vertices == nullptr)
				{
					return nullptr;
				}

				const char* nameChars = reinterpret_cast<const char*>(frame->Name.data());
				std::string frameName(nameChars, strnlen(nameChars, frame->Name.size()));

				mesh->FrameTransforms.push_back(
					{
						frameName,
						Vector3f(frame->Translate[0], frame->Translate[1], frame->Translate[2]),
						Vector3f(frame->Scale[0], frame->Scale[1], frame->Scale[2])
					});

				for (int vertexIndex = 0; vertexIndex < header.NumVertices; ++vertexIndex)
				{
					const auto& vertex = vertices[vertexIndex];

					mesh->FrameVertices.push_back(
						{
							(int8_t)(vertex.X - 128),
							(int8_t)(vertex.Y - 128),
							(int8_t)(vertex.Z - 128),
							(int8_t)(vertex.NormalIndex - 128)
						});
				}
			}

			mesh->SetFrameCount(header.NumFrames);
			mesh->SetFrameVertexCount(header.NumVertices);

			uint32_t vertexOffset = 0;

			pos = static_cast<uint32_t>(header.OffsetCommands);
			for (int commandIndex = 0; commandIndex < header.NumCommands; ++commandIndex)
			{
				auto command = ReadArray<MD2Command>(&file, fileSize, pos, 1);
				if (command == nullptr)
				{
					return nullptr;
				}

				if (command->TrisTypeNum == 0)
				{
					break;
				}

				if (command->TrisTypeNum == INT_MIN)
				{
					return nullptr;
				}

				auto numCommandVertices = abs(command->TrisTypeNum);

				for (int commandVertexIndex = 0; commandVertexIndex < numCommandVertices; ++commandVertexIndex)
				{
					auto commandVertex = ReadArray<MD2CommandVertex>(&file, fileSize, pos, 1);
					if (commandVertex == nullptr)
					{
						return nullptr;
					}

					if (commandVertex->VertexIndex < 0 || commandVertex->VertexIndex >= header.NumVertices)
					{
						return nullptr;
					}

					Vector2f uv(commandVertex->TextureCoordinates[0], commandVertex->TextureCoordinates[1]);

					mesh->Vertices.push_back(
						{
							uv,
							commandVertex->VertexIndex
						});
				}

				for (int vertexIndex = 0; vertexIndex < numCommandVertices - 2; ++vertexIndex)
				{
					if (command->TrisTypeNum < 0)
					{
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 2));
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 1));
						mesh->Indices.emplace_back(vertexOffset);
					}
					else if ((vertexIndex % 2) == 0)
					{
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 2));
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 1));
						mesh->Indices.emplace_back(vertexOffset + vertexIndex);
					}
					else
					{
						mesh->Indices.emplace_back(vertexOffset + vertexIndex);
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 1));
						mesh->Indices.emplace_back(vertexOffset + (vertexIndex + 2));
					}
				}

				vertexOffset += numCommandVertices;
			}

			pos = static_cast<uint32_t>(header.OffsetSkins);
			for (int skinIndex = 0; skinIndex < header.NumSkins; ++skinIndex)
			{
				auto skin = ReadArray<MD2Skin>(&file, fileSize, pos, 1);
				if (skin == nullptr)
				{
					return nullptr;
				}

				const char* pathChars = reinterpret_cast<const char*>(skin->Path.data());
				mesh->Skins.emplace_back(pathChars, strnlen(pathChars, skin->Path.size()));
			}

			mesh->Commit();

			return mesh;
		}

		return nullptr;
	}
}
