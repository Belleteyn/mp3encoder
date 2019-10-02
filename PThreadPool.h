#ifndef P_THREAD_POOL_H
#define P_THREAD_POOL_H

#include <functional>
#include <atomic>

struct ThreadTaskData;

enum class ErrorCode { IN_PROGRESS, FINISHED_SUCCESSFULLY, ERROR };
using Func = std::function<ErrorCode(void*)>;

class PThreadPool
{
	const unsigned threadCount_;
	ThreadTaskData* data_;

	/* returns -1 if all threads are busy */
	int getFreeThreadIndex() const noexcept;

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
	bool addTaskToRun(const Func& func, void* arg = nullptr, unsigned argSize = 0);
	
	/**
	 * waits all tasks to finish, keeps program alive
	 */
	void waitTasksFinished() const;
	
	/**
	 * immediately terminates all threads
	 */
	void terminate() const;
};

#endif //!P_THREAD_POOL_H