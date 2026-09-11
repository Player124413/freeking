#include "AudioDevice.h"
#include "AudioClip.h"
#include <stdexcept>
#include <iostream>
#include <cmath>

namespace Freeking
{
	AudioDevice* AudioDevice::Current = nullptr;

	AudioDevice::AudioDevice()
	{
		Current = this;

#ifdef __ANDROID__
		InitializeSdlAudio();
#else
		InitializeOpenAl();
#endif
	}

	AudioDevice::~AudioDevice()
	{
#ifdef __ANDROID__
		ShutdownSdlAudio();
#else
		ShutdownOpenAL();
#endif

		if (Current == this)
		{
			Current = nullptr;
		}
	}

	void AudioDevice::SetMasterVolume(float volume)
	{
		if (volume < 0.0f) volume = 0.0f;
		if (volume > 1.0f) volume = 1.0f;
		_masterVolume = volume;

#ifndef __ANDROID__
		if (_initialized)
		{
			alListenerf(AL_GAIN, volume);
		}
#endif
	}

#ifdef __ANDROID__

	void SDLCALL AudioDevice::AudioCallback(void* userdata, uint8_t* stream, int length)
	{
		auto* device = static_cast<AudioDevice*>(userdata);
		device->Mix(reinterpret_cast<int16_t*>(stream), length / (2 * sizeof(int16_t)));
	}

	void AudioDevice::Mix(int16_t* output, int frames)
	{
		for (int i = 0; i < frames * 2; ++i)
		{
			output[i] = 0;
		}

		if (_masterVolume <= 0.0f)
		{
			return;
		}

		for (auto& voice : _voices)
		{
			if (!voice.active || voice.clip == nullptr)
			{
				continue;
			}

			size_t sampleCount = 0;
			const float* samples = voice.clip->GetMixerSamples(sampleCount);
			if (samples == nullptr || sampleCount == 0)
			{
				voice.active = false;

				continue;
			}

			float gainL = voice.gainL * _masterVolume;
			float gainR = voice.gainR * _masterVolume;

			for (int i = 0; i < frames; ++i)
			{
				if (voice.position >= sampleCount)
				{
					if (voice.loop)
					{
						voice.position = 0;
					}
					else
					{
						voice.active = false;

						break;
					}
				}

				float s = samples[voice.position++];

				int mixedL = static_cast<int>(output[i * 2]) + static_cast<int>(s * gainL * 32767.0f);
				int mixedR = static_cast<int>(output[i * 2 + 1]) + static_cast<int>(s * gainR * 32767.0f);

				if (mixedL > 32767) mixedL = 32767;
				if (mixedL < -32768) mixedL = -32768;
				if (mixedR > 32767) mixedR = 32767;
				if (mixedR < -32768) mixedR = -32768;

				output[i * 2] = static_cast<int16_t>(mixedL);
				output[i * 2 + 1] = static_cast<int16_t>(mixedR);
			}
		}
	}

	void AudioDevice::StartVoice(std::shared_ptr<AudioClip> clip, const Vector3f& position, bool loop, bool relative)
	{
		size_t sampleCount = 0;
		// Decode on the game thread (not in the audio callback).
		if (clip->GetMixerSamples(sampleCount) == nullptr || sampleCount == 0)
		{
			return;
		}

		float gainL = 1.0f;
		float gainR = 1.0f;

		if (!relative)
		{
			Vector3f toSound = position - _listenerPosition;
			float distance = toSound.Length();

			// Matches the desktop linear clamped model (max 400 units).
			float gain = 1.0f - (distance / 400.0f);
			if (gain <= 0.0f)
			{
				return;
			}

			if (distance > 0.001f)
			{
				float pan = Vector3f::Dot(toSound * (1.0f / distance), _listenerRight);
				if (pan < -1.0f) pan = -1.0f;
				if (pan > 1.0f) pan = 1.0f;
				gainL = gain * (1.0f - pan);
				gainR = gain * (1.0f + pan);
				if (gainL > 1.0f) gainL = 1.0f;
				if (gainR > 1.0f) gainR = 1.0f;
			}
			else
			{
				gainL = gainR = gain;
			}
		}

		SDL_LockAudioDevice(_deviceId);

		Voice* target = nullptr;
		for (auto& voice : _voices)
		{
			if (!voice.active)
			{
				target = &voice;

				break;
			}
		}

		// Voice stealing: reuse the quietest voice.
		if (target == nullptr)
		{
			target = &_voices[0];
			for (auto& voice : _voices)
			{
				if ((voice.gainL + voice.gainR) < (target->gainL + target->gainR))
				{
					target = &voice;
				}
			}
		}

		target->clip = clip;
		target->position = 0;
		target->gainL = gainL;
		target->gainR = gainR;
		target->loop = loop;
		target->active = true;

		SDL_UnlockAudioDevice(_deviceId);
	}

	void AudioDevice::Play(std::shared_ptr<AudioClip> audioClip, const Vector3f& position, bool loop, bool relative, bool queued)
	{
		if (audioClip == nullptr || !_initialized)
		{
			return;
		}

		if (queued)
		{
			_audioQueue.push_back({ audioClip, position, loop, relative });

			return;
		}

		StartVoice(audioClip, position, loop, relative);
	}

	void AudioDevice::FlushQueue()
	{
		if (!_initialized)
		{
			_audioQueue.clear();

			return;
		}

		for (const auto& queuedAudio : _audioQueue)
		{
			StartVoice(queuedAudio.audioClip, queuedAudio.position, queuedAudio.loop, queuedAudio.relative);
		}

		_audioQueue.clear();
	}

	void AudioDevice::SetListenerTransform(const Vector3f& position, const Quaternion& rotation)
	{
		_listenerPosition = position;
		_listenerForward = (rotation * Vector3f::Forward).Normalise();
		_listenerRight = (rotation * Vector3f::Right).Normalise();
	}

	void AudioDevice::SetPaused(bool paused)
	{
		if (_initialized && _deviceId != 0)
		{
			SDL_PauseAudioDevice(_deviceId, paused ? 1 : 0);
		}
	}

	void AudioDevice::InitializeSdlAudio()
	{
		SDL_AudioSpec want = {};
		want.freq = MixerRate;
		want.format = AUDIO_S16SYS;
		want.channels = 2;
		want.samples = 1024;
		want.callback = AudioCallback;
		want.userdata = this;

		SDL_AudioSpec have = {};
		_deviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
		if (_deviceId == 0)
		{
			std::clog << "Warning: could not open SDL audio device: " << SDL_GetError() << std::endl;

			return;
		}

		if (have.format != AUDIO_S16SYS || have.channels != 2)
		{
			std::clog << "Warning: unexpected SDL audio format, disabling audio." << std::endl;
			SDL_CloseAudioDevice(_deviceId);
			_deviceId = 0;

			return;
		}

		// Mixer rate mismatch is tolerated (pitch shift) rather than fatal.
		if (have.freq != MixerRate)
		{
			std::clog << "Warning: SDL audio rate " << have.freq << " != " << MixerRate << std::endl;
		}

		for (auto& voice : _voices)
		{
			voice = Voice();
		}

		SDL_PauseAudioDevice(_deviceId, 0);
		_initialized = true;
	}

	void AudioDevice::ShutdownSdlAudio()
	{
		if (_deviceId != 0)
		{
			SDL_CloseAudioDevice(_deviceId);
			_deviceId = 0;
		}

		_initialized = false;
	}

#else

	void AudioDevice::FlushQueue()
	{
		if (!_initialized)
		{
			_audioQueue.clear();

			return;
		}

		if (_audioQueue.empty())
		{
			return;
		}

		std::vector<uint32_t> sourceIds;

		for (size_t i = 0, queueIndex = 0; i < _sourceIds.size(); ++i)
		{
			ALenum state;
			auto sourceId = _sourceIds[i];
			alGetSourcei(sourceId, AL_SOURCE_STATE, &state);

			if (state == AL_PLAYING)
			{
				continue;
			}

			const auto& queuedAudio = _audioQueue.at(queueIndex);
			if (queuedAudio.audioClip == nullptr || queuedAudio.audioClip->GetBufferId() == 0)
			{
				queueIndex++;

				if (queueIndex >= _audioQueue.size())
				{
					break;
				}

				continue;
			}

			sourceIds.push_back(sourceId);

			alSourceStop(sourceId);
			alSourcei(sourceId, AL_BUFFER, static_cast<ALint>(queuedAudio.audioClip->GetBufferId()));
			alSourcei(sourceId, AL_LOOPING, queuedAudio.loop ? AL_TRUE : AL_FALSE);
			alSourcei(sourceId, AL_SOURCE_RELATIVE, queuedAudio.relative ? AL_TRUE : AL_FALSE);

			if (queuedAudio.relative)
			{
				alSource3f(sourceId, AL_POSITION, 0, 0, 0);
			}
			else
			{
				alSource3f(sourceId, AL_POSITION, queuedAudio.position.x, queuedAudio.position.y, queuedAudio.position.z);
			}

			queueIndex++;

			if (queueIndex >= _audioQueue.size())
			{
				break;
			}
		}

		if (!sourceIds.empty())
		{
			alSourcePlayv((ALsizei)sourceIds.size(), sourceIds.data());
		}

		_audioQueue.clear();
	}

	void AudioDevice::Play(std::shared_ptr<AudioClip> audioClip, const Vector3f& position, bool loop, bool relative, bool queued)
	{
		if (audioClip == nullptr || !_initialized)
		{
			return;
		}

		if (queued)
		{
			_audioQueue.push_back(
				{
					audioClip,
					position,
					loop,
					relative
				});

			return;
		}

		if (audioClip->GetBufferId() == 0)
		{
			return;
		}

		for (size_t i = 0; i < _sourceIds.size(); ++i)
		{
			ALenum state;
			auto sourceId = _sourceIds[i];
			alGetSourcei(sourceId, AL_SOURCE_STATE, &state);

			if (state == AL_PLAYING)
			{
				continue;
			}

			alSourceStop(sourceId);
			alSourcei(sourceId, AL_BUFFER, static_cast<ALint>(audioClip->GetBufferId()));
			alSourcei(sourceId, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
			alSourcei(sourceId, AL_SOURCE_RELATIVE, relative ? AL_TRUE : AL_FALSE);

			if (relative)
			{
				alSource3f(sourceId, AL_POSITION, 0, 0, 0);
			}
			else
			{
				alSource3f(sourceId, AL_POSITION, position.x, position.y, position.z);
			}

			alSourcePlay(sourceId);

			break;
		}
	}

	void AudioDevice::SetListenerTransform(const Vector3f& position, const Quaternion& rotation)
	{
		if (!_initialized)
		{
			return;
		}

		alListener3f(AL_POSITION, position.x, position.y, position.z);

		Vector3f forward = rotation * Vector3f::Forward;
		Vector3f up = rotation * Vector3f::Up;
		float orientation[6] = { forward[0], forward[1], forward[2], up[0], up[1], up[2] };

		alListenerfv(AL_ORIENTATION, orientation);
	}

	void AudioDevice::SetPaused(bool paused)
	{
		if (!_initialized)
		{
			return;
		}

		if (paused)
		{
			alcSuspendContext(_alContext);
		}
		else
		{
			alcProcessContext(_alContext);
		}
	}

	void AudioDevice::InitializeOpenAl()
	{
		try
		{
			if (_alDevice = alcOpenDevice(nullptr); !_alDevice)
			{
				throw std::runtime_error("Failed to open OpenAL device.");
			}

			if (_alContext = alcCreateContext(_alDevice, nullptr); !_alContext)
			{
				throw std::runtime_error("Failed to create OpenAL context.");
			}

			alcMakeContextCurrent(_alContext);

			alcGetIntegerv(_alDevice, ALC_MONO_SOURCES, 1, &_numMonoSources);
			alcGetIntegerv(_alDevice, ALC_STEREO_SOURCES, 1, &_numStereoSources);

			alGenSources((int)_sourceIds.size(), _sourceIds.data());
			if (alGetError() != AL_NO_ERROR)
			{
				throw std::runtime_error("Failed to generate OpenAL sources.");
			}

			for (const auto& sourceId : _sourceIds)
			{
				alSourcei(sourceId, AL_LOOPING, AL_FALSE);
				alSourcef(sourceId, AL_PITCH, 1);
				alSourcef(sourceId, AL_GAIN, 1);
				alSourcei(sourceId, AL_SOURCE_RELATIVE, AL_FALSE);
				alSourcef(sourceId, AL_REFERENCE_DISTANCE, 0.0f);
				alSourcef(sourceId, AL_MAX_DISTANCE, 400.0f);
			}

			alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
			alListenerf(AL_GAIN, _masterVolume);

			_initialized = true;
		}
		catch (const std::exception& e)
		{
			std::clog << "Warning: audio unavailable (" << e.what() << "). Continuing without sound." << std::endl;
			ShutdownOpenAL();
		}
	}

	void AudioDevice::ShutdownOpenAL()
	{
		if (_initialized)
		{
			alDeleteSources((int)_sourceIds.size(), _sourceIds.data());
		}

		if (_alContext)
		{
			alcMakeContextCurrent(nullptr);
			alcDestroyContext(_alContext);

			_alContext = nullptr;
		}

		if (_alDevice)
		{
			alcCloseDevice(_alDevice);

			_alDevice = nullptr;
		}

		_initialized = false;
	}

#endif
}
