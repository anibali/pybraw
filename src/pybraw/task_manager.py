from abc import ABC, abstractmethod
from concurrent.futures import Future
from threading import Condition, RLock


class TaskConsumedError(Exception):
    """An exception indicating that an invalid operation was performed on a consumed task."""
    pass


class Task:
    def __init__(self, task_manager):
        self.task_manager = task_manager
        self._future = Future()
        self._callbacks = []

    def reject(self, exception: BaseException):
        """Set the result of the task to an exception, making the task unsuccessful.
        """
        if self.is_consumed():
            raise TaskConsumedError
        self._future.set_exception(exception)
        for callback in self._callbacks:
            callback(self, False)

    def resolve(self, result):
        """Set the result of the task, making the task successful.
        """
        if self.is_consumed():
            raise TaskConsumedError
        self._future.set_result(result)
        for callback in self._callbacks:
            callback(self, True)

    def cancel(self):
        """Cancel the task, making the task unsuccessful.
        """
        if self.is_consumed():
            raise TaskConsumedError
        self._future.cancel()
        for callback in self._callbacks:
            callback(self, False)

    def is_consumed(self):
        """Check whether this task has been consumed.
        """
        return self._future is None

    def consume(self):
        """Return the image produced by this task and end the task.

        The function call will wait for the task to complete if it is still running. Consuming this
        task will allow another queued task to begin.
        """
        if self.is_consumed():
            raise TaskConsumedError
        try:
            result = self._future.result()
            self._future = None
            return result
        finally:
            self.task_manager._end_task(self)

    def is_done(self) -> bool:
        """Check whether the result of this task is available.

        Returns:
            `True` if the task has been resolved, rejected, or cancelled.
        """
        if self.is_consumed():
            raise TaskConsumedError
        return self._future.done()

    def is_cancelled(self) -> bool:
        """Check whether the task has been cancelled.
        """
        if self.is_consumed():
            raise TaskConsumedError
        return self._future.cancelled()

    def on_done(self, callback):
        """Register a callback for when the task is resolved, rejected, or cancelled.

        The callback will be called with two arguments: the completed task, and a boolean
        indicating whether the task was successful.
        """
        if self.is_consumed():
            raise TaskConsumedError
        if self.is_done():
            exception = self._future.exception()
            if exception is None:
                callback(self, True)
            else:
                callback(self, False)
        self._callbacks.append(callback)


class TaskManager(ABC):
    class _CompletedTaskIterator:
        def __init__(self):
            self._condition = Condition()
            self._running_tasks = []
            self._completed_tasks = []

        def _on_task_complete(self, task, is_success):
            with self._condition:
                self._running_tasks.remove(task)
                self._completed_tasks.append(task)
                self._condition.notify()

        def _register_task(self, task):
            with self._condition:
                self._running_tasks.append(task)
                self._condition.notify()
            task.on_done(self._on_task_complete)

        def __iter__(self):
            return self

        def __next__(self):
            with self._condition:
                while len(self._completed_tasks) <= 0 and len(self._running_tasks) > 0:
                    self._condition.wait()
                if len(self._completed_tasks) == 0:
                    raise StopIteration
                return self._completed_tasks.pop(0)

    def __init__(self, max_running_tasks):
        if max_running_tasks < 1:
            raise ValueError('max_running_tasks must be at least 1')
        self._lock = RLock()
        self._max_running_tasks = max_running_tasks
        self._cur_running_tasks = 0
        self._queued_tasks = []
        self._running_tasks = []
        self._completed_task_iterator = self._CompletedTaskIterator()

    def as_completed(self):
        """Return an iterator which returns tasks as they are completed.

        Note that tasks will not generally be returned in the order that they were started in.
        """
        return self._completed_task_iterator

    @property
    def max_running_tasks(self):
        """The maximum number of tasks that can be running at once.
        """
        return self._max_running_tasks

    @abstractmethod
    def _on_task_started(self, task):
        pass

    @abstractmethod
    def _on_task_ended(self, task):
        pass

    def _try_start_task(self):
        with self._lock:
            if self._cur_running_tasks < self._max_running_tasks and len(self._queued_tasks) > 0:
                self._cur_running_tasks += 1
                task = self._queued_tasks.pop(0)
                self._completed_task_iterator._register_task(task)
                self._running_tasks.append(task)
                self._on_task_started(task)

    def enqueue(self, task):
        """Add a new task to the processing queue.
        """
        with self._lock:
            self._queued_tasks.append(task)
            self._try_start_task()

    def clear_queue(self):
        """Remove all pending tasks from the queue.
        """
        with self._lock:
            for task in list(self._queued_tasks):
                task.cancel()
            self._queued_tasks.clear()

    def consume_remaining(self):
        """Consume all tasks that have been started but not yet consumed.
        """
        with self._lock:
            for task in list(self._running_tasks):
                try:
                    task.consume()
                except:
                    pass

    def _end_task(self, task):
        with self._lock:
            self._running_tasks.remove(task)
            self._on_task_ended(task)
