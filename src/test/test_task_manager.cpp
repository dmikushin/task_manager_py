#include "task_manager.h"

#include <cassert>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TEST(TaskManagerTest, TestTaskManager)
{
    TaskManager taskManager;

    // Start two tasks
    auto task1 = taskManager.startTask("sleep 2", "task1");
    auto task2 = taskManager.startTask("sleep 3", "task2");

    ASSERT_EQ(task1.first, TaskStarted);
    ASSERT_EQ(task2.first, TaskStarted);

    std::vector<UserTask*> userTasks = { task1.second, task2.second };

    // Wait for the tasks to finish
    while (!userTasks.empty())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::vector<std::pair<TaskStatus, UserTask*>> events;
        if (taskManager.tryPopTaskEvent(events))
        {
            for (const auto& event : events)
            {
            	auto& status = event.first;
                if (status == TaskFinishedWithExitCode || status == TaskTerminatedBySignal)
                {
                	auto& task = event.second;
                    auto it = std::find(userTasks.begin(), userTasks.end(), task);
                    if (it != userTasks.end())
                    {
                    	ASSERT_TRUE(((*it)->getName() == "task1") || ((*it)->getName() == "task2"));
                        userTasks.erase(it);
                    }
                }
            }
        }
    }

    std::vector<std::pair<TaskStatus, UserTask*>> events;
    ASSERT_FALSE(taskManager.tryPopTaskEvent(events));

    // Ensure the task list is empty
    ASSERT_EQ(taskManager.runningTasksCount(), 0);
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

