#ifndef P_THREAD_POOL_H
#define P_THREAD_POOL_H

#include <functional>
#include <atomic>

struct threadData;

class PThreadPool
{
	const unsigned threadCount_;
	threadData* data_;

	/* returns -1 if all threads are busy */
	int getFreeThreadIndex() const; 

	/**
	* returns free thread index immediately if has any, 
	* otherwise checks if there is free thread with 1 second interval 
	*/
	unsigned waitFreedThread() const; 

public:
	PThreadPool(unsigned count);
	~PThreadPool();

	/**
	* runs task in separate thread from the pool
	* if all threads were busy, function would block current thread until one of threads has become freed to run this task.
	*/
	bool addTaskToRun(std::function<void(void*)> func, void* arg = nullptr, unsigned argSize = 0);
	void waitTasksFinished() const;
};

#endif //!P_THREAD_POOL_H