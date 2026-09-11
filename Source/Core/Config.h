#pragma once

#include <string>

namespace Freeking
{
	// Persistent user settings (settings.json in the user directory).
	class Config
	{
	public:

		Config() = delete;
		~Config() = delete;

		static void Load();
		static void Save();

		// Look sensitivity multiplier, 0.1 .. 5.0.
		static float Sensitivity();
		static void SetSensitivity(float value);

		static bool InvertY();
		static void SetInvertY(bool value);

		// 3D render resolution scale, 0.5 .. 1.0 (UI always renders native).
		static float RenderScale();
		static void SetRenderScale(float value);
		static void RefreshAutoRenderScale(int screenWidth, int screenHeight);
		// Resolution the auto mode picked at first launch (for display).
		static float AutoRenderScale();

		// 0 = unlimited, otherwise 30/60/120.
		static int FpsLimit();
		static void SetFpsLimit(int value);

		static float SoundVolume();
		static void SetSoundVolume(float value);

		static bool TouchEnabled();
		static void SetTouchEnabled(bool value);

		static bool ShowFps();
		static void SetShowFps(bool value);

		// 0/2/4x MSAA, takes effect on next launch.
		static int MsaaSamples();
		static void SetMsaaSamples(int value);

		static std::string LastMap();
		static void SetLastMap(const std::string& value);

		static bool IsRussian();
		static void SetRussian(bool value);
		static bool IsRussianExplicit();

	private:

		static float _sensitivity;
		static bool _invertY;
		static float _renderScale;
		static float _autoRenderScale;
		static bool _renderScaleWasAuto;
		static int _fpsLimit;
		static float _soundVolume;
		static bool _touchEnabled;
		static bool _showFps;
		static int _msaaSamples;
		static std::string _lastMap;
		static bool _russian;
		static bool _russianExplicit;
	};
}
