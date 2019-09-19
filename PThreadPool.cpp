#include "PThreadPool.h"

#define HAVE_STRUCT_TIMESPEC
#define PTW32_USES_SEPARATE_CRT
#include <pthread.h>
#include <stdexcept>
#include <chrono>


#include <iostream>

static std::atomic<unsigned> tasksCount;

struct threadData {
	int index = -1;
	std::atomic<bool> done = false;
	std::unique_ptr<pthread_t> thread = nullptr;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	std::function<void(void*)> routine;
	void* arg = nullptr;
};

void* runThread(threadData* data) {
	if (!data) {
		throw(std::runtime_error("failed to run user task: no data provided"));
		return 0;
	}

	std::cout << "work with " << data->index << "\n";
	data->routine(data->arg);
	if (data->arg) {
		delete data->arg;
		data->arg = nullptr;
	}
	
	data->done = true;
	tasksCount--;
	std::cout << "work with " << data->index << " finished\n";

	pthread_mutex_lock(&data->mutex);
	pthread_cond_signal(&data->cond);
	pthread_mutex_unlock(&data->mutex);

	pthread_exit(&data->thread);

	return 0;
	/*
	if (!data) {
		throw(std::runtime_error("failed to run user task: no data provided"));
		return 0;
	}

	data->childThread = std::make_unique<pthread_t>();
	int err = pthread_create(&(*data->childThread), nullptr, reinterpret_cast<void *(*)(void *)>(&execUserRoutine), data);
	if (err != 0) {
		throw(std::runtime_error("failed to create thread for user task"));
	}

	//pthread_join(*data->childThread, nullptr);
	return 0;
	*/
}

int PThreadPool::getFreeThreadIndex() const
{
	for (unsigned i = 0; i < threadCount_; i++) {
		if (!data_[i].thread) {
			return i;
		}
	}
	return -1;
}

unsigned PThreadPool::waitFreedThread() const
{
	int freeThreadIndex = getFreeThreadIndex();

	if (freeThreadIndex != -1) {
		return freeThreadIndex;
	}
	else {
		static unsigned waitIndex = 0;
		threadData* data = &data_[waitIndex];
		waitIndex = (waitIndex + 1) % threadCount_;

		auto now = std::chrono::high_resolution_clock::now();
		auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
		timespec waitTime;
		waitTime.tv_nsec = 0;
		waitTime.tv_sec = seconds + 5;
		
		//int t = pthread_delay_np(&waitTime);
		//std::cout << " # delay = " << t << "\n";

		pthread_mutex_lock(&data->mutex);
		std::cout << " -------------------------- wait " << data->cond << " from thread " << pthread_self().p << "\n";
		while (!data->done)
		{
			pthread_cond_wait(&data->cond, &data->mutex);
		}
		
		//int err = pthread_cond_timedwait(&data->cond, &data->mutex, &t);
		std::cout << " -------------------------- !wait " << data->cond << "\n";
		pthread_mutex_unlock(&data->mutex);

		data->thread = nullptr;
		return waitFreedThread();

		/*
		if (err == ETIMEDOUT) {
			return waitFreedThread();
		}
		else {
			std::cout << "wait err == " << err << "\n";
			throw(std::runtime_error("unaccepptable wait error code"));
		}
		*/
	}
}

PThreadPool::PThreadPool(unsigned count)
	: threadCount_(count)
	, data_(new threadData[count])
{
	for (int i = 0; i < count; i++) {
		data_[i].index = i;
	}
}

PThreadPool::~PThreadPool()
{
	std::cout << "~PThreadPool\n";

	waitTasksFinished();
	
	delete[] data_;
	pthread_exit(nullptr);
}

bool PThreadPool::addTaskToRun(std::function<void(void*)> func, void* arg /*= nullptr*/, unsigned argSize /*= 0*/)
{
	tasksCount++;

	int index = getFreeThreadIndex();
	if (index == -1) {
		index = waitFreedThread();
	}

	std::cout << "setup " << index << std::endl;
	data_[index].routine = func;
	data_[index].done = false;

	if (arg && argSize > 0) {
		data_[index].arg = malloc(argSize);
		if (!data_[index].arg) {
			return false;
		}

		std::memcpy(data_[index].arg, arg, argSize);
	}

	data_[index].thread = std::make_unique<pthread_t>();
	int err = pthread_create(&(*data_[index].thread), nullptr
		, reinterpret_cast<void*(*)(void*)>(&runThread), &data_[index]);
	if (err != 0) {
		return false;
	}
	
	return true;
}

void PThreadPool::waitTasksFinished() const
{
	while (tasksCount > 0)
	{
		for (int i = 0; i < threadCount_; i++) {
			if (!data_[i].done) {
				pthread_mutex_lock(&data_[i].mutex);
				std::cout << " ! wait " << data_[i].index << std::endl;
				pthread_cond_wait(&data_[i].cond, &data_[i].mutex);
				pthread_mutex_unlock(&data_[i].mutex);

				//std::cout << "join " << data_[i].index << std::endl;
				//pthread_join(*data_[i].thread, nullptr);
			}
		}
	}
}
