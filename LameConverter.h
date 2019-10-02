#ifndef LAME_CONVERTER_H
#define LAME_CONVERTER_H

#ifdef __GNUC__
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif

class LameConverter 
{
public:
	LameConverter(const fs::path& inputFilePath);
	~LameConverter();

	void operator() ();
	
private:
	class LameConverterPrivate* pimpl;
};

#endif //LAME_CONVERTER_H