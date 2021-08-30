import os
from contextlib import contextmanager

import torch

from pybraw import _pybraw, verify
from pybraw.torch.buffer_manager import BufferManagerFlow1, BufferManagerFlow2
from pybraw.torch.cuda import get_current_cuda_context
from pybraw.torch.flow import ReadTaskManager, ManualFlowCallback


class FrameImageReader:
    def __init__(self, video_path, processing_device='cpu'):
        self.video_path = os.fspath(video_path)
        self.processing_device = torch.device(processing_device)

        self.factory = _pybraw.CreateBlackmagicRawFactoryInstance()
        self.codec = verify(self.factory.CreateCodec())

        if self.processing_device.type == 'cuda':
            pipeline = _pybraw.blackmagicRawPipelineCUDA
        elif self.processing_device.type == 'cpu':
            pipeline = _pybraw.blackmagicRawPipelineCPU
        else:
            raise ValueError(f'Unsupported processing device: {self.processing_device}')

        configuration: _pybraw.IBlackmagicRawConfiguration = verify(self.codec.as_IBlackmagicRawConfiguration())
        if not verify(configuration.IsPipelineSupported(pipeline)):
            raise ValueError(f'Pipeline {pipeline.name} is not supported by this machine')

        if pipeline == _pybraw.blackmagicRawPipelineCUDA:
            with torch.cuda.device(self.processing_device):
                self.context = get_current_cuda_context()
            self.command_queue = None
            self.manual_decoder = verify(self.codec.as_IBlackmagicRawManualDecoderFlow2())
        else:
            self.context = None
            self.command_queue = None
            self.manual_decoder = verify(self.codec.as_IBlackmagicRawManualDecoderFlow1())

        verify(configuration.SetPipeline(pipeline, self.context, self.command_queue))
        self.clip = verify(self.codec.OpenClip(self.video_path))

    def frame_count(self):
        return verify(self.clip.GetFrameCount())

    def frame_width(self):
        return verify(self.clip.GetWidth())

    def frame_height(self):
        return verify(self.clip.GetHeight())

    def frame_rate(self):
        return verify(self.clip.GetFrameRate())

    def _get_post_3d_lut_buffer(self):
        clip_processing_attributes = verify(self.clip.as_IBlackmagicRawClipProcessingAttributes())
        clip_post_3d_lut = verify(clip_processing_attributes.GetPost3DLUT())
        if clip_post_3d_lut is None:
            return None
        post_3d_lut_resource = verify(clip_post_3d_lut.GetResourceCPU())
        lut_size_bytes = verify(clip_post_3d_lut.GetResourceSizeBytes())
        post_3d_lut_buffer = torch.as_tensor(post_3d_lut_resource.to_py_nocopy(lut_size_bytes), dtype=torch.uint8).storage()
        return post_3d_lut_buffer.to(self.processing_device)

    @contextmanager
    def run_flow(self, pixel_format, max_running_tasks=3):
        post_3d_lut_buffer = self._get_post_3d_lut_buffer()

        if self.processing_device.type == 'cuda':
            buffer_manager_pool = [
                BufferManagerFlow2(self.manual_decoder, post_3d_lut_buffer, pixel_format, self.context, self.command_queue, self.processing_device)
                for _ in range(max_running_tasks)
            ]
        elif self.processing_device.type == 'cpu':
            buffer_manager_pool = [
                BufferManagerFlow1(self.manual_decoder, post_3d_lut_buffer, pixel_format)
                for _ in range(max_running_tasks)
            ]
        else:
            raise NotImplementedError(f'Unsupported processing device: {self.processing_device}')

        clip_ex = verify(self.clip.as_IBlackmagicRawClipEx())
        task_manager = ReadTaskManager(buffer_manager_pool, clip_ex, pixel_format)
        callback = ManualFlowCallback()
        verify(self.codec.SetCallback(callback))

        yield task_manager

        # Cancel pending tasks.
        task_manager.clear_queue()
        # Cancel running tasks.
        callback.cancel()
        # Consume completed tasks.
        task_manager.consume_remaining()

        self.codec.FlushJobs()
        verify(self.codec.SetCallback(None))
