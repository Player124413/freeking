#pragma once

#include "AssetLibrary.h"
#include <vector>

namespace Freeking
{
	class AudioClip;

	class AudioClipLibrary : public AssetLibrary<AudioClip>
	{
	protected:

		virtual void UpdateLoaders() override;
	};

	class AudioClip
	{
	public:

		static AudioClipLibrary Library;

		AudioClip(uint32_t numChannels, uint32_t bitsPerSample, uint32_t sampleRate, const std::vector<char>& pcmData);
		~AudioClip();

		uint32_t GetBufferId() const { return _bufferId; }

#ifdef __ANDROID__
		// Mono float samples at the mixer rate, decoded lazily on first use.
		// The returned pointer stays valid for the lifetime of the clip.
		const float* GetMixerSamples(size_t& outSampleCount);
#endif

	private:

		uint32_t _bufferId;

#ifdef __ANDROID__
		std::vector<char> _pcmData;
		uint32_t _numChannels;
		uint32_t _bitsPerSample;
		uint32_t _sampleRate;
		std::vector<float> _mixerSamples;
		bool _decoded = false;
#endif
	};
}
