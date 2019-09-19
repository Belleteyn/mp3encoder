#ifndef LAME_CONVERTER_H
#define LAME_CONVERTER_H

#include <filesystem>

class LameConverter //RAII principle
{
public:
	LameConverter(const std::filesystem::path& inputFilePath);
	~LameConverter();

	void operator() ();
	
private:
	class LameConverterPrivate* pimpl;
};

#endif //LAME_CONVERTER_H