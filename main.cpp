#include <iostream>
#include <thread>

#ifndef _MSC_VER
#include <cstring>
#endif

#include "LameConverter.h"
#include "PThreadPool.h"

namespace fs = std::filesystem;

struct PathWrapper {
	unsigned size = 0;
	char* path;
};

ErrorCode convert(void* arg) 
{
	PathWrapper* wrappedPath = reinterpret_cast<PathWrapper*>(arg);
	fs::path inputFilePath(std::string(wrappedPath->path, wrappedPath->size));
	
	try {
		LameConverter converter(inputFilePath);
		converter();
		return ErrorCode::FINISHED_SUCCESSFULLY;
	}
	catch (std::exception& ex) {
		std::cout << "Conversion failed for " << inputFilePath << ": " << ex.what() << std::endl;
		return ErrorCode::ERROR;
	}
}

int main(int argc, char** argv)
{
	std::string folder = "";

	if (argc == 2) {	
		folder = argv[1];
	}
	else {
		std::cout << "Directory path for conversion is missing\n";
		return -1;
	}
	
	try {
		fs::path path(folder);
		fs::directory_entry dir(path);

		if (dir.status().type() != fs::file_type::directory) {
			std::cout << "directory " << folder << " does not exist\n";
			return -1;
		}

		unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
		if (concurentThreadsSupported == 0) {
			concurentThreadsSupported = 2;
		}

		PThreadPool pool(concurentThreadsSupported);

		for (auto& p : fs::directory_iterator(path)) {
			if (p.path().has_extension() && p.path().extension() == ".wav") {
				const std::string& pathStr = p.path().string();
				
				PathWrapper wrapper;
				wrapper.size = pathStr.length() * sizeof(char);
				wrapper.path = new char[wrapper.size];
				std::memcpy(wrapper.path, pathStr.c_str(), wrapper.size);

				try {
					std::cout << " run conversion of " << p.path().filename() << std::endl;
					pool.addTaskToRun(&convert, static_cast<void*>(&wrapper), sizeof(PathWrapper));
				}
				catch (const std::exception& ex) {
					std::cout << "run task failed: " << ex.what() << std::endl;
					pool.terminate();
					return -1;
				}
			}
		}
	}
	catch (const std::exception& ex) {
		std::cout << "exception " << ex.what() << std::endl;
		return -1;
	}
	catch (...) {
		std::cout << "unknown error\n";
		return -1;
	}

	return 0;
}
