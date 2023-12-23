#include "task_manager.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <queue>
#include <thread>
#include <unistd.h>

namespace {

class Task
{
	const std::string cmd;
	std::vector<char*> args, envs;
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

	Task(const std::string& cmd_,
		const std::vector<std::string>& args_, const std::vector<std::string>& envs_,
		TaskStatusChangedCallback callback_) :
		cmd(cmd_), callback(callback_)
	{
		args.reserve(args_.size() + 2);
		char* arg = new char[cmd_.size() + 1];
		strcpy(arg, cmd_.c_str());
		args.push_back(arg);
		for (const std::string& arg_ : args_)
		{
			char* arg = new char[arg_.size() + 1];
			strcpy(arg, arg_.c_str());
			args.push_back(arg);
		}
		args.push_back(nullptr);

		envs.reserve(envs_.size() + 2);
		for (const std::string& env_ : envs_)
		{
			char* env = new char[env_.size() + 1];
			strcpy(env, env_.c_str());
			envs.push_back(env);
		}
		envs.push_back(nullptr);
	}

	TaskStatus start(void* userData)
	{
		if (thread.get())
			return TaskErrorAlreadyStarted;

		// Parent process
		thread.reset(new std::thread([this, userData]()
		{
			pid = fork();
			if (pid == -1)
			{
				// Error occurred while forking
				return; // TODO TaskErrorForkingFailed;
			}

			if (pid == 0)
			{
				// Child process
				// Perform the desired task in the child process
				if (execvpe(cmd.c_str(), args.data(), envs.data()) == -1)
				{
					fprintf(stderr, "execv() error = %d\n", errno);
					exit(EXIT_FAILURE);
				}
			}

			// Monitor the child process until it exits
			int status;
			while (waitpid(pid, &status, 0) == -1)
			{
				if (errno == EINTR)
				{
					// Parent interrrupted - restarting...
					continue;
				}

				callback(TaskErrorWaitingFailed, *this, userData);
				return;
			}
			if (WIFEXITED(status))
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

		for (char* arg : args)
			if (arg) delete[] arg;
	}
};

struct ManagedTask
{
	Task task;
	TaskManagerImpl* manager;
	std::list<ManagedTask>::iterator it;
	std::string name;
	ManagedTask* self = nullptr;
		
	ManagedTask(const std::string& cmd,
		const std::vector<std::string>& args, const std::vector<std::string>& env,
		Task::TaskStatusChangedCallback callback,
		TaskManagerImpl* manager_) : task(cmd, args, env, callback), manager(manager_), self(this) { }
};

} // namespace

class TaskManagerImpl
{
	std::list<ManagedTask> tasks;
	std::queue<std::pair<TaskStatus, UserTask*>> events;
	std::mutex tasksMtx, eventsMtx;

	static void taskStatusChangeHandler(TaskStatus status, Task& task, void* userData)
	{
		if (!userData) return;
		
		ManagedTask& managedTask = *reinterpret_cast<ManagedTask*>(userData);
		auto* userTask = reinterpret_cast<UserTask*>(&managedTask.self);

		// Publish the collected event to the queue of events.
		{		
			std::scoped_lock lock{managedTask.manager->eventsMtx};
			managedTask.manager->events.push(std::make_pair(status, userTask));
		}
	}

public :

	TaskManagerImpl() { }

	size_t runningTasksCount()
	{
		std::scoped_lock lock{tasksMtx};
		return tasks.size();
	}
	
	std::pair<TaskStatus, UserTask*> startTask(const std::string& cmd,
		const std::vector<std::string>& args, const std::vector<std::string>& env,
		const std::string name = "")
	{
		// Create a task within a managed container.
		std::list<ManagedTask>::iterator* it = nullptr;
		{
			std::scoped_lock lock{tasksMtx};
			tasks.emplace_front(cmd, args, env, taskStatusChangeHandler, this);
			it = &tasks.front().it;
			*it = tasks.begin();
		}
		
		// Start the task, referring to a managed container as a context.
		auto userTask = reinterpret_cast<void*>((*it)->self);
		TaskStatus status = (*it)->task.start(userTask);
		
		// If the task is started successfully, return the iterator to the user.
		if (status == TaskStarted)
		{
			(*it)->name = name;
			return std::make_pair(status, reinterpret_cast<UserTask*>(&(*it)->self));
		}
		
		// Otherwise, erase the container and return an error.
		tasks.erase(*it);
		return std::make_pair(status, nullptr);
	}
	
	bool stopTask(UserTask* userTask)
	{
		if (!userTask) return false;
		
		auto userit = reinterpret_cast<ManagedTask*>(userTask->impl)->it;
		
		// Stop & remove the managed task, if it exists.
		// Note we have to make this check to ensure the iterator is valid.
		for (auto it = tasks.begin(); it != tasks.end(); ++it)
		{
			if (it != userit) continue;

			it->task.stop();
			tasks.erase(it);
			return true;
		}
		
		return false;
	}
	
	// Evict the collected events from the event queue.
	// Return true if at least one event has been ruturned;
	// otherwise, return false.
	bool tryPopTaskEvent(std::vector<std::pair<TaskStatus, UserTask*>>& eventsOutput)
	{
		std::scoped_lock lock{eventsMtx};

		size_t size = events.size();
		if (!size) return false;
		
		eventsOutput.reserve(size);
		eventsOutput.clear();
		for (size_t i = 0; i < size; i++)
		{
			auto& event = events.front();
			
			// Handle some of the events ourselves.
			switch (event.first)
			{
			case TaskErrorWaitingFailed :
			case TaskFinishedWithExitCode :
			case TaskTerminatedBySignal :
			case TaskErrorUnknown :
				{
					std::scoped_lock lock{tasksMtx};
					
					// Remove task from the managed tasks list.
					ManagedTask& managedTask = *reinterpret_cast<ManagedTask*>(event.second->impl);
					managedTask.task.stop();
					tasks.erase(managedTask.it);
				}
				break;
			}

			eventsOutput.emplace_back(std::move(event));
			events.pop();
		}
		return true;
	}
};

const std::string& UserTask::getName() const { return reinterpret_cast<ManagedTask*>(impl)->name; }

void UserTask::setName(const std::string name_) { reinterpret_cast<ManagedTask*>(impl)->name = name_; }

int UserTask::getExitCode() const { return reinterpret_cast<ManagedTask*>(impl)->task.getExitCode(); }

int UserTask::getSignalCode() const { return reinterpret_cast<ManagedTask*>(impl)->task.getSignalCode(); }

TaskManager::TaskManager()
{
	impl = new TaskManagerImpl();
}

TaskManager::~TaskManager()
{
	delete impl;
}

size_t TaskManager::runningTasksCount()
{
	return impl->runningTasksCount();
}

std::pair<TaskStatus, UserTask*> TaskManager::startTask(const std::string& cmd,
	const std::vector<std::string>& args, const std::vector<std::string>& env,
	const std::string name)
{
	return impl->startTask(cmd, args, env, name);
}

bool TaskManager::stopTask(UserTask* userTask)
{
	return impl->stopTask(userTask);
}

// Evict the collected events from the event queue.
// Return true if at least one event has been ruturned;
// otherwise, return false.
bool TaskManager::tryPopTaskEvent(std::vector<std::pair<TaskStatus, UserTask*>>& eventsOutput)
{
	return impl->tryPopTaskEvent(eventsOutput);
}

