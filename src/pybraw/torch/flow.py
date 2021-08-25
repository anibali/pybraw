from concurrent.futures import Future
from dataclasses import dataclass
from threading import Condition, RLock

from pybraw import _pybraw, verify, ResultCode, ResolutionScale, PixelFormat
from pybraw.logger import log
from pybraw.torch.buffer_manager import BufferManager


class TaskConsumedError(Exception):
    """An exception indicating that an invalid operation was performed on a consumed task."""
    pass


class Task:
    def __init__(self, task_manager, frame_index: int, pixel_format: PixelFormat, resolution_scale: ResolutionScale, postprocess_kwargs: dict):
        self.task_manager = task_manager
        self.frame_index = frame_index
        self.pixel_format = pixel_format
        self.resolution_scale = resolution_scale
        self.postprocess_kwargs = postprocess_kwargs
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
            self.task_manager.end_task(self)

    def is_complete(self):
        """Check whether the result of this task is available.
        """
        if self.is_consumed():
            raise TaskConsumedError
        return self._future.done()

    def on_complete(self, callback):
        """Register a callback for when the task completes.

        The callback will be called with two arguments: the completed task, and a boolean
        indicating whether the task was successful.
        """
        if self.is_consumed():
            raise TaskConsumedError
        if self.is_complete():
            exception = self._future.exception()
            if exception is None:
                callback(self, True)
            else:
                callback(self, False)
        self._callbacks.append(callback)


@dataclass(frozen=True)
class UserData:
    buffer_manager: BufferManager
    task: Task


class TaskManager:
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
            task.on_complete(self._on_task_complete)

        def __iter__(self):
            return self

        def __next__(self):
            with self._condition:
                while len(self._completed_tasks) <= 0 and len(self._running_tasks) > 0:
                    self._condition.wait()
                if len(self._completed_tasks) == 0:
                    raise StopIteration
                return self._completed_tasks.pop(0)

    def __init__(self, buffer_manager_pool, clip_ex, pixel_format):
        self.pixel_format = pixel_format
        self._clip_ex = clip_ex
        self._lock = RLock()
        self._max_running_tasks = len(buffer_manager_pool)
        self._cur_running_tasks = 0
        self._queued_tasks = []
        self._available_buffer_managers = list(buffer_manager_pool)
        self._unavailable_buffer_managers = {}
        self._completed_task_iterator = self._CompletedTaskIterator()

    def as_completed(self):
        """Return an iterator which returns tasks as they are completed.

        Note that tasks will not generally be returned in the order that they were started in.
        """
        return self._completed_task_iterator

    @property
    def max_running_tasks(self):
        return self._max_running_tasks

    def _try_start_task(self):
        with self._lock:
            if self._cur_running_tasks < self._max_running_tasks and len(self._queued_tasks) > 0:
                self._cur_running_tasks += 1
                task = self._queued_tasks.pop(0)
                self._completed_task_iterator._register_task(task)
                buffer_manager = self._available_buffer_managers.pop()
                self._unavailable_buffer_managers[task] = buffer_manager
                read_job = buffer_manager.create_read_job(self._clip_ex, task.frame_index)
                verify(read_job.put_py_user_data(UserData(buffer_manager, task)))
                verify(read_job.Submit())
                read_job.Release()

    def enqueue_task(self, frame_index, *, resolution_scale=ResolutionScale.Full, **postprocess_kwargs):
        task = Task(self, frame_index, self.pixel_format, resolution_scale, postprocess_kwargs)
        with self._lock:
            self._queued_tasks.append(task)
            self._try_start_task()
        return task

    def end_task(self, task):
        with self._lock:
            buffer_manager = self._unavailable_buffer_managers[task]
            del self._unavailable_buffer_managers[task]
            self._available_buffer_managers.append(buffer_manager)
            self._cur_running_tasks -= 1
            self._try_start_task()


class ManualFlowCallback(_pybraw.BlackmagicRawCallback):
    def _format_result(self, result):
        return f'{ResultCode.to_hex(result)} "{ResultCode.to_string(result)}"'

    def ReadComplete(self, read_job, result, frame):
        user_data: UserData = verify(read_job.pop_py_user_data())
        task = user_data.task

        if ResultCode.is_success(result):
            log.debug(f'Read frame index {task.frame_index}')
        else:
            task.reject(RuntimeError(f'Failed to read frame ({self._format_result(result)})'))
            return

        verify(frame.SetResolutionScale(task.resolution_scale))
        verify(frame.SetResourceFormat(task.pixel_format))
        buffer_manager = user_data.buffer_manager
        buffer_manager.populate_frame_state_buffer(frame)

        decode_job = buffer_manager.create_decode_job()
        verify(decode_job.put_py_user_data(user_data))
        verify(decode_job.Submit())
        decode_job.Release()

    def DecodeComplete(self, decode_job, result):
        user_data: UserData = verify(decode_job.pop_py_user_data())
        task = user_data.task

        if ResultCode.is_success(result):
            log.debug(f'Decoded frame index {task.frame_index}')
        else:
            task.reject(RuntimeError(f'Failed to decode frame ({self._format_result(result)})'))
            return

        buffer_manager = user_data.buffer_manager
        process_job = buffer_manager.create_process_job()
        verify(process_job.put_py_user_data(user_data))
        verify(process_job.Submit())
        process_job.Release()

    def ProcessComplete(self, process_job, result, processed_image):
        user_data: UserData = verify(process_job.pop_py_user_data())
        task = user_data.task

        if ResultCode.is_success(result):
            log.debug(f'Processed frame index {task.frame_index}')
        else:
            task.reject(RuntimeError(f'Failed to process frame ({self._format_result(result)})'))

        task.resolve(user_data.buffer_manager.postprocess(processed_image, task.resolution_scale, **task.postprocess_kwargs))
