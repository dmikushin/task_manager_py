# Task manager for Python

Start external processes and monitor their status with event loop:

```python3
import task_manager_py

task_manager = task_manager_py.TaskManager()
task_manager.startTask("ls", "List Files")
task_manager.startTask("echo 'Hello, World!'", "Print Message")

while True:
    events = task_manager.tryPopTaskEvent()
    for event in events:
        status = event[0]
        task = event[1]
        if status == task_manager_py.TaskStatus.TaskFinishedWithExitCode:
            print(f"Task '{task.getName()}' finished with status {task.getExitCode()}")
        elif status == task_manager_py.TaskStatus.TaskTerminatedBySignal:
            print(f"Task '{task.getName()}' terminated by signal {task.getSignalCode()}")

    if task_manager.runningTasksCount() == 0:
        print(f"All tasks finished")
        break
```

Sample output:

```
Hello, World!
bin             CMakeFiles           lib                Makefile                                         test_task_manager
CMakeCache.txt  cmake_install.cmake  libtask_manager.a  task_manager_py.cpython-310-x86_64-linux-gnu.so  ThirdParty
Task 'Print Message' finished with status 0
Task 'List Files' finished with status 0
All tasks finished
```

## Building

```
mkdir build
cd build
cmake ..
make
```

## Usage

```
cd build
PYTHONPATH=. python3 ../example/example.py
```
