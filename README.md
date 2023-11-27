# Task manager for Python

Start external processes and monitor their status with event loop:

```python3
import task_manager_py

task_manager = task_manager_py.TaskManager()
task_manager.startTask("ls", "List Files")
task_manager.startTask("echo 'Hello, World!'", "Print Message")

events = []
while task_manager.tryPopTaskEvent(events):
    for event in events:
        print(f"Task '{event.task.getName()}' finished with status {event.status}")
    events.clear()
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
