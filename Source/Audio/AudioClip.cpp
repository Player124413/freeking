#include "AudioClip.h"
#include "WavLoader.h"

#ifndef __ANDROID__
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#endif

namespace Freeking
{
	void AudioClipLibrary::UpdateLoaders()
	{
		AddLoader<WavLoader>();
	}

	AudioClip::AudioClip(uint32_t numChannels, uint32_t bitsPerSample, uint32_t sampleRate, const std::vector<char>& pcmData) :
		_bufferId(0)
#ifdef __ANDROID__
		, _pcmData(pcmData)
		, _numChannels(numChannels)
		, _bitsPerSample(bitsPerSample)
		, _sampleRate(sampleRate)
#endif
	{
#ifndef __ANDROID__
		ALenum format = AL_INVALID_ENUM;
		if (numChannels == 1)
		{
			if (bitsPerSample == 8) format = AL_FORMAT_MONO8;
			else if (bitsPerSample == 16) format = AL_FORMAT_MONO16;
		}
		else if (numChannels == 2)
		{
			if (bitsPerSample == 8) format = AL_FORMAT_STEREO8;
			else if (bitsPerSample == 16) format = AL_FORMAT_STEREO16;
		}

		if (format != AL_INVALID_ENUM && !pcmData.empty())
		{
			alGenBuffers(1, &_bufferId);
			if (_bufferId != 0 && alGetError() == AL_NO_ERROR)
			{
				alBufferData(_bufferId, format, pcmData.data(), (ALsizei)pcmData.size(), sampleRate);
				if (alGetError() != AL_NO_ERROR)
				{
					alDeleteBuffers(1, &_bufferId);
					_bufferId = 0;
				}
			}
			else
			{
				_bufferId = 0;
			}
		}
#endif
	}

	AudioClip::~AudioClip()
	{
#ifndef __ANDROID__
		if (_bufferId)
		{
			alDeleteBuffers(1, &_bufferId);
		}
#endif
	}

#ifdef __ANDROID__
	const float* AudioClip::GetMixerSamples(size_t& outSampleCount)
	{
		if (!_decoded)
		{
			_decoded = true;

			if (_pcmData.empty() || _sampleRate == 0 ||
				(_numChannels != 1 && _numChannels != 2) ||
				(_bitsPerSample != 8 && _bitsPerSample != 16))
			{
				outSampleCount = 0;

				return nullptr;
			}

			size_t bytesPerSample = (_bitsPerSample / 8) * _numChannels;
			size_t frameCount = _pcmData.size() / bytesPerSample;
			if (frameCount == 0)
			{
				outSampleCount = 0;

				return nullptr;
			}

			// Decode to mono float first.
			std::vector<float> mono;
			mono.reserve(frameCount);

			const uint8_t* bytes = reinterpret_cast<const uint8_t*>(_pcmData.data());
			for (size_t i = 0; i < frameCount; ++i)
			{
				float mixed = 0.0f;
				for (uint32_t ch = 0; ch < _numChannels; ++ch)
				{
					size_t offset = i * bytesPerSample + ch * (_bitsPerSample / 8);
					float sample;
					if (_bitsPerSample == 8)
					{
						sample = (static_cast<float>(bytes[offset]) - 128.0f) / 128.0f;
					}
					else
					{
						int16_t s = static_cast<int16_t>(bytes[offset] | (bytes[offset + 1] << 8));
						sample = static_cast<float>(s) / 32768.0f;
					}
					mixed += sample;
				}
				mono.push_back(mixed / static_cast<float>(_numChannels));
			}

			// Linear resample to the mixer rate (game files are usually
			// 22050 Hz or 11025 Hz, the mixer runs at 44100 Hz).
			const uint32_t mixerRate = 44100;
			if (_sampleRate == mixerRate)
			{
				_mixerSamples = std::move(mono);
			}
			else
			{
				size_t outCount = (frameCount * mixerRate) / _sampleRate;
				if (outCount == 0)
				{
					outSampleCount = 0;

					return nullptr;
				}

				_mixerSamples.resize(outCount);
				for (size_t i = 0; i < outCount; ++i)
				{
					double srcPos = (static_cast<double>(i) * _sampleRate) / mixerRate;
					size_t srcIndex = static_cast<size_t>(srcPos);
					float frac = static_cast<float>(srcPos - srcIndex);
					float a = mono[srcIndex];
					float b = (srcIndex + 1 < mono.size()) ? mono[srcIndex + 1] : a;
					_mixerSamples[i] = a + (b - a) * frac;
				}
			}

			// Source PCM no longer needed.
			_pcmData.clear();
			_pcmData.shrink_to_fit();
		}

		outSampleCount = _mixerSamples.size();

		return _mixerSamples.empty() ? nullptr : _mixerSamples.data();
	}
#endif
}
