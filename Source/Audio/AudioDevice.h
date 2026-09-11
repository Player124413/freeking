#pragma once

#include "Vector.h"
#include "Quaternion.h"
#include <vector>
#include <array>
#include <memory>
#include <stdint.h>

#ifdef __ANDROID__
#include <SDL.h>
#else
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#endif

namespace Freeking
{
	class AudioClip;

	class AudioDevice
	{
	public:

		static AudioDevice* Current;

		AudioDevice();
		~AudioDevice();

		void Play(std::shared_ptr<AudioClip> audioClip, const Vector3f& position, bool loop = false, bool relative = false, bool queued = false);
		void FlushQueue();

		void SetListenerTransform(const Vector3f& position, const Quaternion& rotation);
		void SetMasterVolume(float volume);
		void SetPaused(bool paused);

		bool IsInitialized() const { return _initialized; }

	private:

		struct QueuedAudio
		{
			std::shared_ptr<AudioClip> audioClip;
			Vector3f position;
			bool loop;
			bool relative;
		};

		bool _initialized = false;
		float _masterVolume = 1.0f;

#ifdef __ANDROID__
		static const int MaxVoices = 24;
		static const int MixerRate = 44100;

		struct Voice
		{
			std::shared_ptr<AudioClip> clip;
			size_t position = 0;
			float gainL = 0.0f;
			float gainR = 0.0f;
			bool loop = false;
			bool active = false;
		};

		static void SDLCALL AudioCallback(void* userdata, uint8_t* stream, int length);
		void Mix(int16_t* output, int frames);
		void StartVoice(std::shared_ptr<AudioClip> clip, const Vector3f& position, bool loop, bool relative);

		void InitializeSdlAudio();
		void ShutdownSdlAudio();

		SDL_AudioDeviceID _deviceId = 0;
		std::array<Voice, MaxVoices> _voices;
		Vector3f _listenerPosition = Vector3f(0, 0, 0);
		Vector3f _listenerForward = Vector3f(0, 0, 1);
		Vector3f _listenerRight = Vector3f(1, 0, 0);
		std::vector<QueuedAudio> _audioQueue;
#else
		void InitializeOpenAl();
		void ShutdownOpenAL();

		ALCcontext* _alContext = nullptr;
		ALCdevice* _alDevice = nullptr;

		std::array<uint32_t, 255> _sourceIds;

		ALCint _numMonoSources = 0;
		ALCint _numStereoSources = 0;

		std::vector<QueuedAudio> _audioQueue;
#endif
	};
}
