#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include <string>
#include <vector>

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

struct UserTask
{
	void* impl;

	const std::string& getName() const;
	
	void setName(const std::string name_);

	int getExitCode() const;
	
	int getSignalCode() const;
};

class TaskManagerImpl;

class TaskManager
{
	TaskManagerImpl* impl;

public :

	TaskManager();
	
	~TaskManager();

	size_t runningTasksCount();
	
	std::pair<TaskStatus, UserTask*> startTask(const std::string& shell_cmd, const std::string name = "");
	
	bool stopTask(UserTask* userTask);
	
	// Evict the collected events from the event queue.
	// Return true if at least one event has been ruturned;
	// otherwise, return false.
	bool tryPopTaskEvent(std::vector<std::pair<TaskStatus, UserTask*>>& eventsOutput);
};

#endif // TASK_MANAGER_H

