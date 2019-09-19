#ifndef WAV_HEADER_H
#define WAV_HEADER_H

#include <cstdint>

struct WavHeader
{	
	char riff[4]; //RIFF
	uint32_t fileSize;
	char desc[4]; //WAVE
	char fmt[4];

	uint32_t wavSectionSize; //usually 16
	uint16_t wavTypeFmt; //PCM header or 0x01
	uint16_t channelFlag; //mono (0x01) or stereo (0x02)
	uint32_t sampleFreq;
	uint32_t bytePerSec; //bytes per second
	uint16_t blockAlignment;
	uint16_t bitsPerSample;
	char dataDesc[4]; //"data"
	uint32_t dataSize;
};

#endif // !WAV_HEADER_H

