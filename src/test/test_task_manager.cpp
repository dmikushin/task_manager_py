#include "task_manager.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

int main()
{
    TaskManager taskManager;

    // Start two tasks
    auto task1 = taskManager.startTask("sleep 2");
    auto task2 = taskManager.startTask("sleep 3");

    assert(task1.first == TaskStarted);
    assert(task2.first == TaskStarted);

    std::vector<UserTask*> userTasks = { task1.second, task2.second };

    // Wait for the tasks to finish
    while (!userTasks.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::vector<TaskEvent> events;
        if (taskManager.tryPopTaskEvent(events))
        {
            for (const auto& event : events)
            {
                if (event.status == TaskFinishedWithExitCode || event.status == TaskTerminatedBySignal)
                {
                    auto it = std::find(userTasks.begin(), userTasks.end(), &event.task);
                    if (it != userTasks.end())
                        userTasks.erase(it);
                }
            }
        }
    }

    std::vector<TaskEvent> events;
    assert(!taskManager.tryPopTaskEvent(events));

    // Ensure the task list is empty
	assert(taskManager.runningTasksCount() == 0);

    return 0;
}

