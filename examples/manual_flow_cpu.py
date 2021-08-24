import argparse
import logging
import sys
from dataclasses import dataclass
from threading import Condition

from pybraw import _pybraw, verify
from pybraw.logger import log

RESOURCE_FORMAT = _pybraw.blackmagicRawResourceFormatRGBAU8


def argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=str, required=True,
                        help='input BRAW video file')
    return parser


class JobCounter:
    def __init__(self, max_jobs, cur_jobs: int = 0):
        self._condition = Condition()
        self._max_jobs = max_jobs
        self._cur_jobs = cur_jobs

    @property
    def max_jobs(self):
        return self._max_jobs

    def start_job(self):
        with self._condition:
            while self._cur_jobs >= self._max_jobs:
                self._condition.wait()
            self._cur_jobs += 1

    def end_job(self):
        with self._condition:
            self._cur_jobs -= 1
            self._condition.notify()

    def wait_while_jobs_running(self):
        with self._condition:
            while self._cur_jobs > 0:
                self._condition.wait()


class BufferManagerFlow1:
    def __init__(self, resource_manager, manual_decoder, post_3d_lut_buffer_cpu):
        self.resource_manager = resource_manager
        self.manual_decoder = manual_decoder
        self.post_3d_lut_buffer_cpu = post_3d_lut_buffer_cpu
        self.bit_stream_size_bytes = 0
        self.bit_stream = None
        self.frame_state_size_bytes = 0
        self.frame_state = None
        self.decoded_buffer_size_bytes = 0
        self.decoded_buffer = None
        self.processed_buffer_size_bytes = 0
        self.processed_buffer = None

    def __del__(self):
        for buf in [self.bit_stream, self.frame_state, self.decoded_buffer, self.processed_buffer]:
            if buf is not None:
                verify(self.resource_manager.ReleaseResource(None, None, buf, _pybraw.blackmagicRawResourceTypeBufferCPU))

    def populate_frame_state_buffer(self, frame):
        frame_state_size_bytes = verify(self.manual_decoder.GetFrameStateSizeBytes())
        if frame_state_size_bytes > self.frame_state_size_bytes:
            if self.frame_state is not None:
                verify(self.resource_manager.ReleaseResource(None, None, self.frame_state, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.frame_state = verify(self.resource_manager.CreateResource(None, None, frame_state_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.frame_state_size_bytes = frame_state_size_bytes
        verify(self.manual_decoder.PopulateFrameStateBuffer(frame, None, None, self.frame_state, frame_state_size_bytes))

    def create_read_job(self, clip_ex, frame_index) -> _pybraw.IBlackmagicRawJob:
        bit_stream_size_bytes = verify(clip_ex.GetBitStreamSizeBytes(frame_index))
        if bit_stream_size_bytes > self.bit_stream_size_bytes:
            if self.bit_stream is not None:
                verify(self.resource_manager.ReleaseResource(None, None, self.bit_stream, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.bit_stream = verify(self.resource_manager.CreateResource(None, None, bit_stream_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.bit_stream_size_bytes = bit_stream_size_bytes
        read_job = verify(clip_ex.CreateJobReadFrame(frame_index, self.bit_stream, bit_stream_size_bytes))
        return read_job

    def create_decode_job(self) -> _pybraw.IBlackmagicRawJob:
        decoded_buffer_size_bytes = verify(self.manual_decoder.GetDecodedSizeBytes(self.frame_state))
        if decoded_buffer_size_bytes > self.decoded_buffer_size_bytes:
            if self.decoded_buffer is not None:
                verify(self.resource_manager.ReleaseResource(None, None, self.decoded_buffer, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.decoded_buffer = verify(self.resource_manager.CreateResource(None, None, decoded_buffer_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.decoded_buffer_size_bytes = decoded_buffer_size_bytes
        decode_job = verify(self.manual_decoder.CreateJobDecode(self.frame_state, self.bit_stream, self.decoded_buffer))
        return decode_job

    def create_process_job(self) -> _pybraw.IBlackmagicRawJob:
        processed_buffer_size_bytes = verify(self.manual_decoder.GetProcessedSizeBytes(self.frame_state))
        if processed_buffer_size_bytes > self.processed_buffer_size_bytes:
            if self.processed_buffer is not None:
                verify(self.resource_manager.ReleaseResource(None, None, self.processed_buffer, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.processed_buffer = verify(self.resource_manager.CreateResource(None, None, processed_buffer_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.processed_buffer_size_bytes = processed_buffer_size_bytes
        process_job = verify(self.manual_decoder.CreateJobProcess(self.frame_state, self.decoded_buffer, self.processed_buffer, self.post_3d_lut_buffer_cpu))
        return process_job


@dataclass(frozen=True)
class UserData:
    buffer_manager: BufferManagerFlow1
    frame_index: int


class CameraCodecCallback(_pybraw.BlackmagicRawCallback):
    def __init__(self, job_counter: JobCounter):
        super().__init__()
        self.job_counter = job_counter

    def ReadComplete(self, read_job, result, frame):
        user_data: UserData = verify(read_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            log.info(f'Read frame index: {user_data.frame_index}')
        else:
            log.error(f'Failed to read frame index: {user_data.frame_index}')
            self.job_counter.end_job()
            return

        verify(frame.SetResourceFormat(RESOURCE_FORMAT))
        buffer_manager = user_data.buffer_manager
        buffer_manager.populate_frame_state_buffer(frame)

        decode_job = buffer_manager.create_decode_job()
        verify(decode_job.put_py_user_data(user_data))
        verify(decode_job.Submit())
        decode_job.Release()

    def DecodeComplete(self, decode_job, result):
        user_data: UserData = verify(decode_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            log.info(f'Decoded frame index: {user_data.frame_index}')
        else:
            log.error(f'Failed to decode frame index: {user_data.frame_index}')
            self.job_counter.end_job()
            return

        buffer_manager = user_data.buffer_manager
        process_job = buffer_manager.create_process_job()
        verify(process_job.put_py_user_data(user_data))
        verify(process_job.Submit())
        process_job.Release()

    def ProcessComplete(self, process_job, result, processed_image):
        user_data: UserData = verify(process_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            log.info(f'Processed frame index: {user_data.frame_index}')
        else:
            log.error(f'Failed to process frame index: {user_data.frame_index}')

        self.job_counter.end_job()


def process_clip_manual(clip: _pybraw.IBlackmagicRawClip, resource_manager, manual_decoder, job_counter: JobCounter):
    clip_ex = verify(clip.as_IBlackmagicRawClipEx())
    clip_processing_attributes = verify(clip.as_IBlackmagicRawClipProcessingAttributes())
    frame_count = verify(clip.GetFrameCount())
    clip_post_3d_lut = verify(clip_processing_attributes.GetPost3DLUT())
    if clip_post_3d_lut is not None:
        post_3d_lut_buffer_cpu = verify(clip_post_3d_lut.GetResourceCPU())
    else:
        post_3d_lut_buffer_cpu = _pybraw.CreateResourceNone()
    buffer_manager_pool = [
        BufferManagerFlow1(resource_manager, manual_decoder, post_3d_lut_buffer_cpu)
        for _ in range(job_counter.max_jobs)
    ]
    for frame_index in range(frame_count):
        job_counter.start_job()
        buffer_manager = buffer_manager_pool[frame_index % len(buffer_manager_pool)]
        read_job = buffer_manager.create_read_job(clip_ex, frame_index)
        user_data = UserData(buffer_manager, frame_index)
        verify(read_job.put_py_user_data(user_data))
        verify(read_job.Submit())
        read_job.Release()

    job_counter.wait_while_jobs_running()


def main(args):
    opts = argument_parser().parse_args(args)
    log.setLevel(logging.DEBUG)

    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    codec = verify(factory.CreateCodec())
    configuration_ex = verify(codec.as_IBlackmagicRawConfigurationEx())
    resource_manager = verify(configuration_ex.GetResourceManager())
    manual_decoder = verify(codec.as_IBlackmagicRawManualDecoderFlow1())
    clip = verify(codec.OpenClip(opts.input))
    job_counter = JobCounter(max_jobs=2)
    callback = CameraCodecCallback(job_counter)
    verify(codec.SetCallback(callback))
    process_clip_manual(clip, resource_manager, manual_decoder, job_counter)
    verify(codec.FlushJobs())


if __name__ == '__main__':
    main(sys.argv[1:])
