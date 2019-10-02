#include "LameConverter.h"

#include <iostream>
#include <fstream>
#include <filesystem>

#include <lame/lame.h>
#include "WavHeader.h"

class LameConverterPrivate
{
	std::unique_ptr<std::ifstream> input_;
	std::unique_ptr<std::ofstream> output_;

	lame_t lame_;

	WavHeader header_;
	int pcmBufSize_;
	int mp3BufSize_;

	short* pcmBuffer_;
	unsigned char* mp3Buffer_;

	void initLame()
	{
		lame_ = lame_init();
		if (!lame_) {
			throw std::runtime_error("lame init failed");
		}

		//NOTE: do i need to set it? quality?
		if (lame_set_in_samplerate(lame_, header_.sampleFreq) < 0) {
			throw std::runtime_error("in samplerate configure failed");
		}
		if (lame_set_out_samplerate(lame_, header_.sampleFreq) < 0) {
			throw std::runtime_error("out samplerate configure failed");
		}
		if (lame_set_num_channels(lame_, header_.channelFlag == 0x01 ? 1 : 2) < 0) {
			throw std::runtime_error("channels configure failed");
		}

		int initParamsRes = lame_init_params(lame_);
		if (initParamsRes == -1) {
			throw std::runtime_error("lame params init failed");
		}

		//lame_print_config(lame_);
		//lame_print_internals(lame_);
	}

public:
	LameConverterPrivate(const fs::path& inputFilePath)
		: input_(nullptr)
		, output_(nullptr)
		, lame_(nullptr)
		, pcmBufSize_(0)
		, mp3BufSize_(0)
		, pcmBuffer_(nullptr)
		, mp3Buffer_(nullptr) 
	{
		auto outputFilePath = inputFilePath;
		outputFilePath.replace_extension(".mp3");

		input_ = std::make_unique<std::ifstream>(std::ifstream(inputFilePath, std::ifstream::binary));
		output_ = std::make_unique<std::ofstream>(std::ofstream(outputFilePath, std::ofstream::binary | std::ofstream::out));
		
		if (!input_->is_open()) {
			throw std::runtime_error("input open error");
		}

		input_->read(reinterpret_cast<char*>(&header_), sizeof(WavHeader));
		if (input_->gcount() < sizeof(WavHeader)) {
			throw std::runtime_error("wav header not found");
		}

		pcmBufSize_ = static_cast<int>(1.25 * header_.bitsPerSample) + 7200;
		mp3BufSize_ = static_cast<int>(1.25 * header_.bitsPerSample) + 7200;

		pcmBuffer_ = new(std::nothrow) short[pcmBufSize_ * 2];
		if (!pcmBuffer_) {
			throw std::runtime_error("buffer memory allocation failed");
		}

		mp3Buffer_ = new(std::nothrow) unsigned char[mp3BufSize_];
		if (!mp3Buffer_) {

			//clean up previously allocated memory
			delete[] pcmBuffer_;
			pcmBuffer_ = nullptr;

			throw std::runtime_error("buffer memory allocation failed");
		}
	}

	~LameConverterPrivate()
	{
		if (lame_) {
			lame_close(lame_);
			lame_ = nullptr;
		}

		if (pcmBuffer_) {
			delete[] pcmBuffer_;
			pcmBuffer_ = nullptr;
		}

		if (mp3Buffer_) {
			delete[] mp3Buffer_;
			mp3Buffer_ = nullptr;
		}

		if (input_) {
			input_->close();
			input_ = nullptr;
		}

		if (output_) {
			output_->close();
			output_ = nullptr;
		}
	}

	void convert() 
	{
		initLame();

		std::streamsize readBytes = 0, writeBytes = 0;

		while (!input_->eof()) {
			input_->read(reinterpret_cast<char*>(pcmBuffer_), static_cast<std::streamsize>(pcmBufSize_) * 2 * sizeof(short));
			readBytes = input_->gcount();

			writeBytes = lame_encode_buffer_interleaved(lame_, pcmBuffer_, readBytes / (2 * sizeof(short)), mp3Buffer_, mp3BufSize_);
			if (writeBytes < 0) { // error code
				switch (writeBytes)
				{
				case -1:
					throw std::exception("encoding error: mp3buf was too small");
				case -2:
					throw std::exception("encoding error: malloc() problem");
				case -3:
					throw std::exception("encoding error: lame_init_params() not called");
				case -4:
					throw std::exception("encoding error: psycho acoustic problems");
				default:
					throw std::exception("encoding error: unknown lame error code");
				}
			}

			output_->write(reinterpret_cast<const char*>(mp3Buffer_), writeBytes);
		}

		writeBytes = lame_encode_flush(lame_, mp3Buffer_, mp3BufSize_);
		output_->write(reinterpret_cast<const char*>(mp3Buffer_), writeBytes);
	}
};

LameConverter::LameConverter(const fs::path& inputFilePath) 
	: pimpl(new LameConverterPrivate(inputFilePath))
{

}

LameConverter::~LameConverter() 
{
	delete pimpl;
}

void LameConverter::operator()()
{
	pimpl->convert();
}


