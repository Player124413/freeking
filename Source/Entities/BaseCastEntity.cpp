#include "BaseCastEntity.h"
#include "Shader.h"
#include "Renderer.h"
#include "LineRenderer.h"
#include "Map.h"

namespace Freeking
{
	BaseCastEntity::BaseCastEntity()
	{
	}

	void BaseCastEntity::Initialize()
	{
		PrimitiveEntity::Initialize();

		static const std::array<std::string, 3> bodyParts =
		{
			"head",
			"body",
			"legs"
		};

		int bodyPartIndex = 0;
		for (const auto& bodyPart : bodyParts)
		{
			if (auto mesh = DynamicModel::Library.Get(_modelFolder + "/" + bodyPart + ".mdx"))
			{
				if (auto texture = Texture2D::Library.Get(_skinFolder[bodyPartIndex] + "/" + bodyPart + "_" + _artSkins[bodyPartIndex] + ".tga"))
				{
					_meshTextures.push_back(texture);
					_meshes.push_back(mesh);
				}
			}

			bodyPartIndex++;
		}

		_shader = Shader::Library.DynamicModel;

		SetLocalBounds(Vector3f(-16, -24, -16), Vector3f(16, 50, 16));

		if (_meshes.empty() || _shader == nullptr)
		{
			// Missing model files: hide instead of crashing.
			_hidden = true;

			return;
		}

		for (const auto& frameAnimation : _meshes.front()->GetFrameAnimations())
		{
			_animator.AddAnimation(frameAnimation.name, frameAnimation.firstFrame, frameAnimation.numFrames);
		}
	}

	void BaseCastEntity::Tick(double dt)
	{
		PrimitiveEntity::Tick(dt);

		_animator.Tick(dt);

		Vector3f traceStart = GetTransform().Translation() + Vector3f::Up * 70.0f;
		Vector3f traceEnd = Renderer::ViewMatrix.InverseTranslation();
		if (auto trace = Map::Current->LineTrace(traceStart, traceEnd, BspContentFlags::MaskOpaque);
			trace.hit == false)
		{
			auto traceDirection = traceEnd - traceStart;
			//SetRotation(Quaternion::FromRadianYaw(atan2(traceDirection.x, traceDirection.z) - Math::HalfPi));
		}
	}

	void BaseCastEntity::PostTick()
	{
		PrimitiveEntity::PostTick();

		if (!_hidden && Renderer::DebugDraw)
		{
			const auto& viewMatrix = Renderer::ViewMatrix;
			auto position = GetTransformCenter().Translation();
			float distance = position.LengthBetween(viewMatrix.InverseTranslation());

			if (distance < 512.0f)
			{
				float alpha = 1.0f - (distance / 512.0f);

				for (const auto& mesh : _meshes)
				{
					if (mesh == nullptr)
					{
						continue;
					}

					for (int i = 0; i < mesh->FrameBounds.size(); ++i)
					{
						const auto& bounds = mesh->FrameBounds.at(i);
						if (bounds.empty())
						{
							continue;
						}

						const auto& frameBounds = bounds.at(_animator.GetFrame() % bounds.size());
						const auto& nextFrameBounds = bounds.at(_animator.GetNextFrame() % bounds.size());
						auto boundsMin = Vector3f::Lerp(frameBounds.boundsMin, nextFrameBounds.boundsMin, _animator.GetFrameDelta());
						auto boundsMax = Vector3f::Lerp(frameBounds.boundsMax, nextFrameBounds.boundsMax, _animator.GetFrameDelta());

						LineRenderer::Debug->DrawBox(GetTransform(), boundsMin, boundsMax, LinearColor(0, 1, 1, alpha));
					}
				}

				LineRenderer::Debug->DrawAABBox(GetPosition(), GetLocalMinBounds(), GetLocalMaxBounds(), LinearColor(0, 1, 0, alpha));
			}
		}
	}

	void BaseCastEntity::PreRender(bool translucent)
	{
		if (_shader == nullptr)
		{
			return;
		}

		(void)translucent;

		_shader->Bind();
		_shader->SetParameterValue("modelMatrix", GetTransform());
		_shader->SetParameterValue("normalBuffer", DynamicModel::GetNormalBuffer().get());
	}

	void BaseCastEntity::RenderOpaque()
	{
		if (_shader == nullptr)
		{
			return;
		}

		for (size_t i = 0; i < _meshes.size(); ++i)
		{
			const auto& mesh = _meshes.at(i);
			if (mesh == nullptr || i >= _meshTextures.size() || _meshTextures.at(i) == nullptr)
			{
				continue;
			}

			const auto& meshTexture = _meshTextures.at(i);

			_shader->SetParameterValue("diffuse", meshTexture.get());
			mesh->SetFrameUniforms(_shader.get(), _animator.GetFrame(), _animator.GetNextFrame(), _animator.GetFrameDelta());

			mesh->Draw();
		}
	}

	bool BaseCastEntity::SetProperty(const EntityProperty& property)
	{
		if (property.IsKey("art_skins"))
		{
			std::vector<std::string> artSkins;
			if (property.ValueAsStringArray(artSkins) && artSkins.size() == 3)
			{
				_artSkins[0] = artSkins[0];
				_artSkins[1] = artSkins[1];
				_artSkins[2] = artSkins[2];

				return true;
			}

			return false;
		}

		return PrimitiveEntity::SetProperty(property);
	}

	void BaseCastEntity::Trace(const Vector3f& start, const Vector3f& end, const Vector3f& mins, const Vector3f& maxs, TraceResult& trace, const BspContentFlags& brushMask)
	{
		if ( _hidden)
		{
			return;
		}
	}
}
