#include "task_manager.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

namespace py = pybind11;

class PyTaskManager;

class PyUserTask {
public:
    PyUserTask(UserTask& task) : task(task) {}

    const std::string& getName() const {
        return task.getName();
    }

    void setName(const std::string& name) {
        task.setName(name);
    }

private:
    UserTask& task;
    
    friend class PyTaskManager;
};

class PyTaskManager {
public:
    PyTaskManager() : manager(new TaskManager()) {}

    ~PyTaskManager() {
        delete manager;
    }

    size_t runningTasksCount() {
        return manager->runningTasksCount();
    }

    std::pair<TaskStatus, PyUserTask> startTask(const std::string& shell_cmd, const std::string& name = "") {
        std::pair<TaskStatus, UserTask*> result = manager->startTask(shell_cmd, name);
        return {result.first, PyUserTask(*result.second)};
    }

    bool stopTask(PyUserTask& userTask) {
        return manager->stopTask(&userTask.task);
    }

    bool tryPopTaskEvent(std::vector<TaskEvent>& eventsOutput) {
        return manager->tryPopTaskEvent(eventsOutput);
    }

private:
    TaskManager* manager;
};

PYBIND11_MODULE(task_manager_py, m) {
    py::enum_<TaskStatus>(m, "TaskStatus")
        .value("TaskStarted", TaskStatus::TaskStarted)
        .value("TaskErrorAlreadyStarted", TaskStatus::TaskErrorAlreadyStarted)
        .value("TaskErrorForkingFailed", TaskStatus::TaskErrorForkingFailed)
        .value("TaskErrorWaitingFailed", TaskStatus::TaskErrorWaitingFailed)
        .value("TaskFinishedWithExitCode", TaskStatus::TaskFinishedWithExitCode)
        .value("TaskTerminatedBySignal", TaskStatus::TaskTerminatedBySignal)
        .value("TaskErrorUnknown", TaskStatus::TaskErrorUnknown)
        .export_values();

    py::class_<PyUserTask>(m, "UserTask")
        .def("getName", &PyUserTask::getName)
        .def("setName", &PyUserTask::setName);

    py::class_<PyTaskManager>(m, "TaskManager")
        .def(py::init<>())
        .def("runningTasksCount", &PyTaskManager::runningTasksCount)
        .def("startTask", &PyTaskManager::startTask)
        .def("stopTask", &PyTaskManager::stopTask)
        .def("tryPopTaskEvent", &PyTaskManager::tryPopTaskEvent);
}

