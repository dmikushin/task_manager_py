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
