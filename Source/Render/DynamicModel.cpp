#include "DynamicModel.h"
#include "NormalTable.h"
#include "Md2Loader.h"
#include "MdxLoader.h"

namespace Freeking
{
	void DynamicModelLibrary::UpdateLoaders()
	{
		AddLoader<MD2Loader>();
		AddLoader<MDXLoader>();
	}

	DynamicModelLibrary DynamicModel::Library;

	const std::shared_ptr<TextureBuffer>& DynamicModel::GetNormalBuffer()
	{
#ifdef __ANDROID__
		// 256x1 RGBA32F 2D texture; indices 162..255 are padding (the MD2/MDX
		// normal table has 162 entries, loaders store index-128 as int8 so
		// shader-side "n + 128" always lands in [0, 161]).
		static auto normalBuffer = []
		{
			float data[256 * 4] = {};
			for (int i = 0; i < 162; ++i)
			{
				data[i * 4 + 0] = NormalTable[i][0];
				data[i * 4 + 1] = NormalTable[i][1];
				data[i * 4 + 2] = NormalTable[i][2];
				data[i * 4 + 3] = 0.0f;
			}
			for (int i = 162; i < 256; ++i)
			{
				data[i * 4 + 1] = 1.0f;
			}
			return TextureBuffer::CreateFloat4(256, 1, data);
		}();
#else
		static auto normalBuffer = std::make_shared<TextureBuffer>((void*)&NormalTable[0][0], (162 * 3) * sizeof(float), GL_RGB32F);
#endif
		return normalBuffer;
	}

	void DynamicModel::Draw()
	{
		_vertexBinding->Bind();

		glDrawElements(GL_TRIANGLES, _vertexBinding->GetNumElements(), GL_UNSIGNED_INT, (void*)0);

		_vertexBinding->Unbind();
	}

	void DynamicModel::DrawSubObject(int index)
	{
		if (index < 0 || index >= SubObjects.size())
		{
			return;
		}

		_vertexBinding->Bind();

		const auto& subObject = SubObjects.at(index);
		glDrawElements(GL_TRIANGLES, subObject.numIndices, GL_UNSIGNED_INT, (void*)(subObject.firstIndex * sizeof(uint32_t)));

		_vertexBinding->Unbind();
	}

	void DynamicModel::Commit()
	{
		static const int vertexSize = sizeof(Vertex);

		_indexBuffer = std::make_unique<IndexBuffer>(Indices.data(), Indices.size(), GL_UNSIGNED_INT);
		_vertexBuffer = std::make_unique<VertexBuffer>(Vertices.data(), Vertices.size(), vertexSize, GL_STATIC_DRAW);
#ifdef __ANDROID__
		_frameVertexBuffer = TextureBuffer::CreateByte4(
			static_cast<int>(_frameVertexCount),
			static_cast<int>(_frameCount),
			FrameVertices.data());
#else
		_frameVertexBuffer = std::make_unique<TextureBuffer>(FrameVertices.data(), FrameVertices.size() * sizeof(FrameVertex), GL_RGBA8I);
#endif

		ArrayElement vertexLayout[] =
		{
			ArrayElement(_vertexBuffer.get(), 0, 2, ElementType::Float, vertexSize, 0),
			ArrayElement(_vertexBuffer.get(), 1, 1, ElementType::Int, vertexSize, 2 * sizeof(float)),
		};

		_vertexBinding = std::make_unique<VertexBinding>();
		_vertexBinding->Create(vertexLayout, 2, *_indexBuffer, ElementType::UInt);
	}

	std::vector<FrameAnimation> DynamicModel::GetFrameAnimations() const
	{
		std::vector<FrameAnimation> animations;

		std::string currentFrameName = "";
		size_t currentFrameIndex = 0;

		for (auto frameTransform : FrameTransforms)
		{
			auto indexStart = frameTransform.name.find_last_of('_');
			auto frameName = frameTransform.name.substr(0, indexStart);

			if (currentFrameName != frameName)
			{
				currentFrameName = frameName;
				animations.push_back({ frameName, currentFrameIndex, 0 });
			}

			auto& animFrameIndex = animations.back();
			animFrameIndex.numFrames += 1;

			currentFrameIndex++;
		}

		return animations;
	}

	FrameAnimator::FrameAnimator() :
		_playTime(0),
		_frame(0),
		_nextFrame(0),
		_frameDelta(0),
		_currentAnimation(0)
	{
	}

	void FrameAnimator::Tick(double dt)
	{
		if (_animations.empty() || _currentAnimation >= _animations.size())
		{
			return;
		}

		const auto& animation = _animations.at(_currentAnimation);
		auto frameCount = animation.numFrames;

		if (frameCount == 0)
		{
			return;
		}

		_playTime += (10.0 * dt);
		_playTime = fmod(_playTime, (float)frameCount);

		_frame = (size_t)floor(_playTime);
		_frame %= frameCount;
		_nextFrame = (_frame + 1) % frameCount;
		_frameDelta = (float)_playTime - (float)_frame;

		_frame += animation.firstFrame;
		_nextFrame += animation.firstFrame;
		_frameDelta = Math::Clamp(_frameDelta, 0.0f, 1.0f);
	}

	void FrameAnimator::AddAnimation(const std::string& name, size_t firstFrame, size_t numFrames)
	{
		_animationNameIds.insert({ name, (int)_animations.size() });
		_animations.push_back({ name, firstFrame, numFrames });
	}

	void FrameAnimator::SetAnimation(const std::string& name)
	{
		if (const auto& it = _animationNameIds.find(name); it != _animationNameIds.end())
		{
			SetAnimation(it->second);
		}
	}

	void FrameAnimator::SetAnimation(size_t index)
	{
		_currentAnimation = index;
		_playTime = 0;
		_frame = 0;
		_nextFrame = 0;
		_frameDelta = 0;
	}

	void DynamicModel::SetFrameUniforms(Shader* shader, size_t frame, size_t nextFrame, float delta) const
	{
		if (shader == nullptr || FrameTransforms.empty())
		{
			return;
		}

		frame %= FrameTransforms.size();
		nextFrame %= FrameTransforms.size();

		shader->SetParameterValue("delta", delta);
		shader->SetParameterValue("frameVertexBuffer", GetFrameVertexBuffer().get());
		shader->SetParameterValue("frames[0].index", static_cast<int>(frame * GetFrameVertexCount()));
		shader->SetParameterValue("frames[0].translate", FrameTransforms[frame].translate);
		shader->SetParameterValue("frames[0].scale", FrameTransforms[frame].scale);
		shader->SetParameterValue("frames[1].index", static_cast<int>(nextFrame * GetFrameVertexCount()));
		shader->SetParameterValue("frames[1].translate", FrameTransforms[nextFrame].translate);
		shader->SetParameterValue("frames[1].scale", FrameTransforms[nextFrame].scale);
	}
}
