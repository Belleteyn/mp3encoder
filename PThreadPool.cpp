#include "PThreadPool.h"

#if defined (_MSC_VER)
#define HAVE_STRUCT_TIMESPEC
#define PTW32_USES_SEPARATE_CRT

#include <time.h>
#include <stdexcept>
#else
#include <memory>
#include <cstring>
#endif

#include <pthread.h>

static std::atomic<unsigned> tasksCount;

struct ThreadTaskData {
	std::atomic<ErrorCode> errorCode = ErrorCode::IN_PROGRESS;

	std::unique_ptr<pthread_t> thread = nullptr;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

	Func routine;
	void* arg = nullptr;
};

void* runThread(ThreadTaskData* data) {
	if (!data) {
		return 0;
	}

	data->errorCode = data->routine(data->arg);
	if (data->arg) {
		std::free(data->arg);
		data->arg = nullptr;
	}
	
	tasksCount--;

	pthread_mutex_lock(&data->mutex);
	pthread_cond_signal(&data->cond);
	pthread_mutex_unlock(&data->mutex);

	pthread_exit(&data->thread);
}

int PThreadPool::getFreeThreadIndex() const noexcept
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

	static unsigned waitIndex = 0;
	ThreadTaskData* data = &data_[waitIndex];
	waitIndex = (waitIndex + 1) % threadCount_;

	timespec waitTime;
#if defined (_MSC_VER)
	auto errorCode = timespec_get(&waitTime, TIME_UTC);
	if (errorCode != 0) {
		waitTime.tv_sec += 1;
	}
	else {
		throw std::runtime_error("timespec_get failed");
	}
#else
	waitTime.tv_nsec = 0;
	waitTime.tv_sec = time(NULL) + 1;
#endif
		
	while (data->errorCode == ErrorCode::IN_PROGRESS)
	{
		pthread_mutex_lock(&data->mutex);
		int err = pthread_cond_timedwait(&data->cond, &data->mutex, &waitTime);
		pthread_mutex_unlock(&data->mutex);

		if (err == ETIMEDOUT) {
			return waitFreedThread();
		}
	}

	data->thread = nullptr;
	return waitFreedThread();
}

PThreadPool::PThreadPool(unsigned count)
	: threadCount_(count)
	, data_(new ThreadTaskData[count])
{}

PThreadPool::~PThreadPool()
{
	waitTasksFinished();
	
	delete[] data_;
	pthread_exit(0);
}

bool PThreadPool::addTaskToRun(const Func& func, void* arg /*= nullptr*/, unsigned argSize /*= 0*/)
{
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
	
	tasksCount++;
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
