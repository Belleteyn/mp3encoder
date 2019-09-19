#include <iostream>
#include <filesystem>
#include <thread>

#include "LameConverter.h"
#include "PThreadPool.h"

namespace fs = std::filesystem;

struct pathWrapper {
	unsigned size = 0;
	char* path;
};

void convert(void* arg) {

	pathWrapper* wrappedPath = reinterpret_cast<pathWrapper*>(arg);
	fs::path inputFilePath(std::string(wrappedPath->path, wrappedPath->size));
	
	try {
		LameConverter converter(inputFilePath);
		converter();
	}
	catch (std::exception& ex) {
		std::cout << "error on converting " << inputFilePath << ": " << ex.what() << std::endl;
	}
}

int main()
{
	//NOTE: is this allowed?
	unsigned concurentThreadsSupported = std::thread::hardware_concurrency();
	if (concurentThreadsSupported == 0) {
		concurentThreadsSupported = 2;
	}
	//concurentThreadsSupported = 2;

	PThreadPool pool(concurentThreadsSupported);

	std::string folder = "C:/Users/user/source/repos/Converter/Converter/dataSource";
	//TODO: ask about filesystem, possibility to use
	//TODO: recursive directory - ?

	try {
		fs::path path(folder);
		fs::directory_entry dir(path);

		std::cout << "Source directory: " << dir.path() << '\n' << '\n';
		//TODO: are such logs needed?

		if (!dir.is_directory()) {
			std::cout << "no such dir\n";
			return -1;
		}

		//WARNING: corrupts stack on path cout with recursive iterator
		for (auto& p : fs::recursive_directory_iterator(path)) {
			if (p.path().has_extension() && p.path().extension() == ".wav") {
				pathWrapper wrapper;
				std::string pathStr = p.path().string();
				wrapper.size = pathStr.length() * sizeof(char);
				wrapper.path = new char[wrapper.size];
				std::memcpy(wrapper.path, pathStr.c_str(), wrapper.size);

				pool.addTaskToRun(&convert, static_cast<void*>(&wrapper), sizeof(pathWrapper));

			}
		}

		//pool.waitTasksFinished();
	}
	catch (const std::exception& ex) {
		std::cout << ex.what() << std::endl;
		return -1;
	}
	catch (...) {
		return -1;
	}

	return 0;
}
