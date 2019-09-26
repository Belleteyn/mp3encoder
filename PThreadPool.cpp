#include "PThreadPool.h"

#define HAVE_STRUCT_TIMESPEC
#define PTW32_USES_SEPARATE_CRT
#include <pthread.h>
#include <stdexcept>
#include <chrono>

static std::atomic<unsigned> tasksCount;

struct threadData {
	std::atomic<ErrorCode> errorCode = ErrorCode::IN_PROGRESS;

	std::unique_ptr<pthread_t> thread = nullptr;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	Func routine;
	void* arg = nullptr;
};

void* runThread(threadData* data) {
	if (!data) {
		return 0;
	}

	data->errorCode = data->routine(data->arg);
	if (data->arg) {
		delete data->arg;
		data->arg = nullptr;
	}
	
	tasksCount--;

	pthread_mutex_lock(&data->mutex);
	pthread_cond_signal(&data->cond);
	pthread_mutex_unlock(&data->mutex);

	pthread_exit(&data->thread);

	return 0;
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

		timespec waitTime;
		if (timespec_get(&waitTime, TIME_UTC) == 0) {
			waitTime.tv_sec += 1;
		}
		else {
			throw std::runtime_error("timespec_get failed");
		}
				
		pthread_mutex_lock(&data->mutex);
		while (data->errorCode == ErrorCode::IN_PROGRESS)
		{
			int err = pthread_cond_timedwait(&data->cond, &data->mutex, &waitTime);
			if (err == ETIMEDOUT) {
				return waitFreedThread();
			}
		}
		pthread_mutex_unlock(&data->mutex);

		data->thread = nullptr;
		return waitFreedThread();
	}
}

PThreadPool::PThreadPool(unsigned count)
	: threadCount_(count)
	, data_(new threadData[count])
{}

PThreadPool::~PThreadPool()
{
	waitTasksFinished();
	
	delete[] data_;
	pthread_exit(NULL);
}

bool PThreadPool::addTaskToRun(const Func& func, void* arg /*= nullptr*/, unsigned argSize /*= 0*/)
{
	tasksCount++;

	int index = getFreeThreadIndex();
	if (index == -1) {
		index = waitFreedThread();
	}

	data_[index].routine = func;
	data_[index].errorCode = ErrorCode::IN_PROGRESS;

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
		for (unsigned i = 0; i < threadCount_; i++) {
			if (data_[i].errorCode == ErrorCode::IN_PROGRESS) {
				pthread_mutex_lock(&data_[i].mutex);
				pthread_cond_wait(&data_[i].cond, &data_[i].mutex);
				pthread_mutex_unlock(&data_[i].mutex);
			}
		}
	}
}

void PThreadPool::terminate() const
{
	pthread_exit(NULL);
}
