from dataclasses import dataclass
from typing import List

from pybraw import _pybraw, verify, ResultCode, ResolutionScale, PixelFormat
from pybraw.logger import log
from pybraw.task_manager import Task, TaskManager
from pybraw.torch.buffer_manager import BufferManager


class ReadTask(Task):
    def __init__(self, task_manager, frame_index: int, pixel_format: PixelFormat, resolution_scale: ResolutionScale, postprocess_kwargs: dict):
        super().__init__(task_manager)
        self.frame_index = frame_index
        self.pixel_format = pixel_format
        self.resolution_scale = resolution_scale
        self.postprocess_kwargs = postprocess_kwargs


@dataclass(frozen=True)
class UserData:
    buffer_manager: BufferManager
    task: ReadTask


class ReadTaskManager(TaskManager):
    def __init__(
        self,
        buffer_manager_pool: List[BufferManager],
        clip_ex: _pybraw.IBlackmagicRawClipEx,
        pixel_format: PixelFormat,
    ):
        super().__init__(len(buffer_manager_pool))
        self.pixel_format = pixel_format
        self._clip_ex = clip_ex
        self._available_buffer_managers = list(buffer_manager_pool)
        self._unavailable_buffer_managers = {}

    def _on_task_started(self, task):
        buffer_manager = self._available_buffer_managers.pop()
        self._unavailable_buffer_managers[task] = buffer_manager
        read_job = buffer_manager.create_read_job(self._clip_ex, task.frame_index)
        verify(read_job.SetUserData(UserData(buffer_manager, task)))
        verify(read_job.Submit())
        read_job.Release()

    def _on_task_ended(self, task):
        buffer_manager = self._unavailable_buffer_managers[task]
        del self._unavailable_buffer_managers[task]
        self._available_buffer_managers.append(buffer_manager)
        self._cur_running_tasks -= 1
        self._try_start_task()

    def enqueue_task(self, frame_index, *, resolution_scale=ResolutionScale.Full, **postprocess_kwargs) -> ReadTask:
        """Add a new task to the processing queue.

        Args:
            frame_index: The index of the frame to read, decode, and process.
            resolution_scale: The scale at which to decode the frame.
            **postprocess_kwargs: Keyword arguments which will be passed to
                `BufferManager.postprocess`.

        Returns:
            The newly created and enqueued task.
        """
        task = ReadTask(self, frame_index, self.pixel_format, resolution_scale, postprocess_kwargs)
        super().enqueue(task)
        return task


class ManualFlowCallback(_pybraw.BlackmagicRawCallback):
    """Callbacks for the PyTorch manual decoding flows.
    """
    def __init__(self):
        super().__init__()
        self._cancelled = False

    def cancel(self):
        self._cancelled = True

    def _format_result(self, result):
        return f'{ResultCode.to_hex(result)} "{ResultCode.to_string(result)}"'

    def ReadComplete(self, read_job, result, frame):
        user_data: UserData = verify(read_job.PopUserData())
        task = user_data.task

        if self._cancelled:
            task.cancel()
            return

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
        verify(decode_job.SetUserData(user_data))
        verify(decode_job.Submit())
        decode_job.Release()

    def DecodeComplete(self, decode_job, result):
        user_data: UserData = verify(decode_job.PopUserData())
        task = user_data.task

        if self._cancelled:
            task.cancel()
            return

        if ResultCode.is_success(result):
            log.debug(f'Decoded frame index {task.frame_index}')
        else:
            task.reject(RuntimeError(f'Failed to decode frame ({self._format_result(result)})'))
            return

        buffer_manager = user_data.buffer_manager
        process_job = buffer_manager.create_process_job()
        verify(process_job.SetUserData(user_data))
        verify(process_job.Submit())
        process_job.Release()

    def ProcessComplete(self, process_job, result, processed_image):
        user_data: UserData = verify(process_job.PopUserData())
        task = user_data.task

        if self._cancelled:
            task.cancel()
            return

        if ResultCode.is_success(result):
            log.debug(f'Processed frame index {task.frame_index}')
        else:
            task.reject(RuntimeError(f'Failed to process frame ({self._format_result(result)})'))

        task.resolve(user_data.buffer_manager.postprocess(processed_image, task.resolution_scale, **task.postprocess_kwargs))
