#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <queue>
#include <thread>
#include <unistd.h>


enum TaskStatus
{
	TaskStarted = 0,
	TaskErrorAlreadyStarted,
	TaskErrorForkingFailed,
	TaskErrorWaitingFailed,
	TaskFinishedWithExitCode,
	TaskTerminatedBySignal,
	TaskErrorUnknown = 999,
};

class Task
{
	const std::string shell_cmd;
	pid_t pid;
	std::unique_ptr<std::thread> thread;
	int exitCode = 0, signalCode = 0;

public :

	typedef void (*TaskStatusChangedCallback)(TaskStatus status, Task& task, void* userData);

private :

	TaskStatusChangedCallback callback;

public :

	int getExitCode() const { return exitCode; }
	
	int getSignalCode() const { return signalCode; }
	
	int getPID() const { return pid; }

	Task(const std::string& shell_cmd_, TaskStatusChangedCallback callback_) :
		shell_cmd(shell_cmd_), callback(callback_) { }
	
	TaskStatus start(void* userData)
	{
		if (thread.get())
			return TaskErrorAlreadyStarted;

		pid = fork();
		if (pid == -1)
		{
		    // Error occurred while forking
		    return TaskErrorForkingFailed;
		}

		if (pid == 0)
		{
		    // Child process
		    // Perform the desired task in the child process
		    exit(system(shell_cmd.c_str()));
		}

	    // Parent process
		thread.reset(new std::thread([this, userData]()
		{
			// Monitor the child process until it exits
			int status;
			if (waitpid(pid, &status, 0) == -1)
			{
				callback(TaskErrorWaitingFailed, *this, userData);
			}
			else if (WIFEXITED(status))
			{
			    // Child process exited normally
			    exitCode = WEXITSTATUS(status);
			    callback(TaskFinishedWithExitCode, *this, userData);
			}
			else if (WIFSIGNALED(status))
			{
			    // Child process terminated by a signal
			    signalCode = WTERMSIG(status);
			    callback(TaskTerminatedBySignal, *this, userData);
			}
			else
			{
				callback(TaskErrorUnknown, *this, userData);
			}
		}));

		return TaskStarted;
	}
	
	void stop()
	{
		if (thread.get())
		{
			// Send terminate signal to PID and wait for
			// the monitoring thread to join.
			kill(pid, SIGTERM);
			
			// TODO Use boost::thread::try_join_for() for time-limited joining.
			thread->join();
			
			thread.reset();
		}
	}
	
	~Task()
	{
		stop();
	}
};

class TaskManager;

struct ManagedTask
{
	Task&& task;
	TaskManager* manager;
	std::list<ManagedTask>::iterator it;
	
	ManagedTask(Task&& task_, TaskManager* manager_) : manager(manager_), task(std::move(task_)) { }
};

struct UserTask
{
	std::list<ManagedTask>::iterator it;
};

struct TaskEvent
{
	UserTask& task;
	TaskStatus status;
};

class TaskManager
{
	std::list<ManagedTask> tasks;
	std::queue<TaskEvent> events;
	std::mutex tasksMtx, eventsMtx;

	static void taskStatusChangeHandler(TaskStatus status, Task& task, void* userData)
	{
		if (!userData) return;
		
		auto it = *reinterpret_cast<std::list<ManagedTask>::iterator*>(userData);
		
		ManagedTask& managedTask = *it;
		auto userTask = *reinterpret_cast<UserTask*>(&managedTask.it);

		// Handle some of the events ourselves.
		switch (status)
		{
		case TaskErrorWaitingFailed :
		case TaskFinishedWithExitCode :
		case TaskTerminatedBySignal :
		case TaskErrorUnknown :
			{
				std::scoped_lock lock{managedTask.manager->tasksMtx};
				
				// Remove task from the managed tasks list.
				managedTask.task.stop();
				managedTask.manager->tasks.erase(it);
			}
			break;
		}

		// Publish the collected event to the queue of events.
		{		
			std::scoped_lock lock{managedTask.manager->eventsMtx};
			TaskEvent event{userTask, status};
			managedTask.manager->events.emplace(std::move(event));
		}
	}

public :

	TaskManager() { }
	
	std::pair<TaskStatus, UserTask*> startTask(const std::string& shell_cmd)
	{
		// Create a task.
		Task task(shell_cmd, taskStatusChangeHandler);
		
		// Create a task managed container.
		ManagedTask managedTask(std::move(task), this);
		{
			std::scoped_lock lock{managedTask.manager->tasksMtx};
			tasks.emplace_front(std::move(managedTask));
			managedTask.it = tasks.begin();
		}
		
		// Start the task, referring to a managed container as a context.
		auto userTask = reinterpret_cast<void*>(&managedTask.it);
		TaskStatus status = task.start(userTask);
		
		// If the task is started successfully, return the iterator to the user.
		if (status == TaskStarted)
		{
			return std::make_pair(status, reinterpret_cast<UserTask*>(&managedTask.it));
		}
		
		// Otherwise, erase the container and return an error.
		tasks.erase(managedTask.it);
		return std::make_pair(status, nullptr);
	}
	
	bool stopTask(UserTask* userTask)
	{
		if (!userTask) return false;
		
		// Stop & remove the managed task, if it exists.
		// Note we have to make this check to ensure the iterator is valid.
		for (auto it = tasks.begin(); it != tasks.end(); ++it)
		{
		    if (it != userTask->it) continue;

	    	it->task.stop();
	        tasks.erase(it);
	        return true;
	    }
		
		return false;
	}
	
	// Evict the collected events from the event queue.
	// Return true if at least one event has been ruturned;
	// otherwise, return false.
	bool tryPopTaskEvent(std::vector<TaskEvent>& eventsOutput)
	{
		std::scoped_lock lock{eventsMtx};

		size_t size = events.size();
		if (!size) return false;
		
		eventsOutput.reserve(size);
		eventsOutput.clear();
		for (size_t i = 0; i < size; i++)
		{
        	eventsOutput.emplace_back(std::move(events.front()));
        	events.pop();
	    }
	    return true;
    }
};

int main()
{
	return 0;
}

