#include "Renderer.h"
#include "VertexBinding.h"
#include "Shader.h"

namespace Freeking
{
	Matrix4x4 Renderer::ProjectionMatrix;
	Matrix4x4 Renderer::ViewMatrix;
	float Renderer::ViewportWidth;
	float Renderer::ViewportHeight;
	bool Renderer::DebugDraw = true;

	static inline GLenum GLDrawPrimitive(DrawPrimitive e)
	{
		switch (e)
		{
		case DrawPrimitive::Triangles: return GL_TRIANGLES;
		case DrawPrimitive::Lines: return GL_LINES;
		}

		return GL_INVALID_ENUM;
	}

	void Renderer::Draw(VertexBinding* binding, Shader* shader, DrawPrimitive primitive, int offset, int count, int instances)
	{
		auto mode = GLDrawPrimitive(primitive);
		if (mode == GL_INVALID_ENUM)
		{
			return;
		}

		shader->Bind();
		binding->Bind();

		if (binding->HasIndices())
		{
			GLenum indexType = static_cast<GLenum>(binding->GetIndexType());

			if (offset > 0)
			{
#ifdef __ANDROID__
				// GLES has no BaseVertex variants; approximate with an index
				// buffer byte offset (this helper is not used on the hot path).
				size_t indexSize = (indexType == GL_UNSIGNED_SHORT) ? 2 : ((indexType == GL_UNSIGNED_BYTE) ? 1 : 4);
				const void* byteOffset = reinterpret_cast<const void*>(static_cast<intptr_t>(offset) * static_cast<intptr_t>(indexSize));
				if (instances > 1)
				{
					glDrawElementsInstanced(mode, count, indexType, byteOffset, instances);
				}
				else
				{
					glDrawElements(mode, count, indexType, byteOffset);
				}
#else
				if (instances > 1)
				{
					glDrawElementsInstancedBaseVertex(mode, count, indexType, nullptr, instances, offset);
				}
				else
				{
					glDrawElementsBaseVertex(mode, count, indexType, nullptr, offset);
				}
#endif
			}
			else
			{
				if (instances > 1)
				{
					glDrawElementsInstanced(mode, count, indexType, nullptr, instances);
				}
				else
				{
					glDrawElements(mode, count, indexType, nullptr);
				}
			}
		}
		else
		{
			if (instances > 1)
			{
				glDrawArraysInstanced(mode, offset, count, instances);
			}
			else
			{
				glDrawArrays(mode, offset, count);
			}
		}

		binding->Unbind();
		shader->Unbind();
	}
}
