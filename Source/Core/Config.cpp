#include "Config.h"
#include "Paths.h"
#include "Maths.h"
#include "json.hpp"
#include <SDL.h>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace Freeking
{
	using json = nlohmann::json;

	float Config::_sensitivity = 1.0f;
	bool Config::_invertY = false;
	float Config::_renderScale = 1.0f;
	float Config::_autoRenderScale = 1.0f;
	bool Config::_renderScaleWasAuto = true;
#ifdef __ANDROID__
	int Config::_fpsLimit = 60;
	bool Config::_touchEnabled = true;
	bool Config::_showFps = false;
	int Config::_msaaSamples = 0;
#else
	int Config::_fpsLimit = 0;
	bool Config::_touchEnabled = false;
	bool Config::_showFps = true;
	int Config::_msaaSamples = 4;
#endif
	float Config::_soundVolume = 1.0f;
	std::string Config::_lastMap;
	bool Config::_russian = false;
	bool Config::_russianExplicit = false;

	static float ClampFloat(float v, float minV, float maxV)
	{
		return Math::Clamp(v, minV, maxV);
	}

	float Config::Sensitivity() { return _sensitivity; }
	void Config::SetSensitivity(float value) { _sensitivity = ClampFloat(value, 0.1f, 5.0f); }

	bool Config::InvertY() { return _invertY; }
	void Config::SetInvertY(bool value) { _invertY = value; }

	float Config::RenderScale() { return _renderScale; }
	void Config::SetRenderScale(float value)
	{
		_renderScale = ClampFloat(value, 0.5f, 1.0f);
		_renderScaleWasAuto = false;
	}
	float Config::AutoRenderScale() { return _autoRenderScale; }

	static float ComputeAutoScale(int width, int height)
	{
		float scale = 1.0f;
		if (width > 0 && height > 0)
		{
			double pixels = static_cast<double>(width) * static_cast<double>(height);
			// Target ~1.5M shaded pixels (roughly 720p-class load).
			double s = std::sqrt(1500000.0 / pixels);
			scale = ClampFloat(static_cast<float>(s), 0.5f, 1.0f);
		}

		// Low-RAM devices get capped further.
		if (SDL_GetSystemRAM() > 0 && SDL_GetSystemRAM() < 3072)
		{
			scale = Math::Min(scale, 0.6f);
		}

		return scale;
	}

	// Re-evaluates the automatic render scale once the real drawable size
	// is known (SDL video must be initialised first, so this cannot run
	// inside Config::Load). Explicitly saved user values are kept.
	void Config::RefreshAutoRenderScale(int screenWidth, int screenHeight)
	{
		_autoRenderScale = ComputeAutoScale(screenWidth, screenHeight);
		if (_renderScaleWasAuto)
		{
			_renderScale = _autoRenderScale;
		}
	}

	int Config::FpsLimit() { return _fpsLimit; }
	void Config::SetFpsLimit(int value)
	{
		_fpsLimit = (value == 30 || value == 60 || value == 120) ? value : 0;
	}

	float Config::SoundVolume() { return _soundVolume; }
	void Config::SetSoundVolume(float value) { _soundVolume = ClampFloat(value, 0.0f, 1.0f); }

	bool Config::TouchEnabled() { return _touchEnabled; }
	void Config::SetTouchEnabled(bool value) { _touchEnabled = value; }

	bool Config::ShowFps() { return _showFps; }
	void Config::SetShowFps(bool value) { _showFps = value; }

	int Config::MsaaSamples() { return _msaaSamples; }
	void Config::SetMsaaSamples(int value)
	{
		_msaaSamples = (value == 2 || value == 4) ? value : 0;
	}

	std::string Config::LastMap() { return _lastMap; }
	void Config::SetLastMap(const std::string& value) { _lastMap = value; }

	bool Config::IsRussian() { return _russian; }
	void Config::SetRussian(bool value)
	{
		_russian = value;
		_russianExplicit = true;
	}
	bool Config::IsRussianExplicit() { return _russianExplicit; }

	static std::filesystem::path SettingsPath()
	{
		return Paths::UserDir() / "settings.json";
	}

	void Config::Load()
	{
		// Pick an automatic render scale from the display resolution: big
		// phone screens get downscaled a bit so weak GPUs keep up. Video
		// is usually not initialised yet here, so the real value is fixed
		// up later by RefreshAutoRenderScale().
		_autoRenderScale = 1.0f;
		_renderScale = 1.0f;
		_renderScaleWasAuto = true;

#ifdef __ANDROID__
		SDL_DisplayMode mode = {};
		if (SDL_GetCurrentDisplayMode(0, &mode) == 0)
		{
			_autoRenderScale = ComputeAutoScale(mode.w, mode.h);
			_renderScale = _autoRenderScale;
		}
#endif

		std::error_code ec;
		auto path = SettingsPath();
		if (!std::filesystem::exists(path, ec))
		{
			return;
		}

		try
		{
			std::ifstream stream(path);
			json j;
			stream >> j;

			if (j.contains("sensitivity") && j["sensitivity"].is_number())
			{
				SetSensitivity(j["sensitivity"].get<float>());
			}
			if (j.contains("invertY") && j["invertY"].is_boolean())
			{
				SetInvertY(j["invertY"].get<bool>());
			}
			if (j.contains("renderScale") && j["renderScale"].is_number())
			{
				SetRenderScale(j["renderScale"].get<float>());
			}
			if (j.contains("fpsLimit") && j["fpsLimit"].is_number_integer())
			{
				SetFpsLimit(j["fpsLimit"].get<int>());
			}
			if (j.contains("soundVolume") && j["soundVolume"].is_number())
			{
				SetSoundVolume(j["soundVolume"].get<float>());
			}
			if (j.contains("touchEnabled") && j["touchEnabled"].is_boolean())
			{
				SetTouchEnabled(j["touchEnabled"].get<bool>());
			}
			if (j.contains("showFps") && j["showFps"].is_boolean())
			{
				SetShowFps(j["showFps"].get<bool>());
			}
			if (j.contains("msaaSamples") && j["msaaSamples"].is_number_integer())
			{
				SetMsaaSamples(j["msaaSamples"].get<int>());
			}
			if (j.contains("lastMap") && j["lastMap"].is_string())
			{
				SetLastMap(j["lastMap"].get<std::string>());
			}
			if (j.contains("russian") && j["russian"].is_boolean())
			{
				_russian = j["russian"].get<bool>();
				_russianExplicit = true;
				if (j.contains("russianExplicit") && j["russianExplicit"].is_boolean())
				{
					_russianExplicit = j["russianExplicit"].get<bool>();
				}
			}
		}
		catch (...)
		{
			// Corrupt settings file: keep defaults.
		}
	}

	void Config::Save()
	{
		try
		{
			json j;
			j["sensitivity"] = _sensitivity;
			j["invertY"] = _invertY;
			// If the user never touched the slider we persist the auto flag
			// so the next launch picks a fresh automatic value again.
			j["renderScaleAuto"] = _renderScaleWasAuto;
			j["renderScale"] = _renderScaleWasAuto ? _autoRenderScale : _renderScale;
			j["fpsLimit"] = _fpsLimit;
			j["soundVolume"] = _soundVolume;
			j["touchEnabled"] = _touchEnabled;
			j["showFps"] = _showFps;
			j["msaaSamples"] = _msaaSamples;
			j["lastMap"] = _lastMap;
			j["russian"] = _russian;
			j["russianExplicit"] = _russianExplicit;

			std::error_code ec;
			auto path = SettingsPath();
			std::filesystem::create_directories(path.parent_path(), ec);

			std::ofstream stream(path, std::ios::trunc);
			stream << j.dump(2);
		}
		catch (...)
		{
		}
	}
}
