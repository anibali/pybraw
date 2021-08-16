import argparse
import sys
from dataclasses import dataclass
from threading import Lock
from time import sleep

from pybraw import _pybraw

RESOURCE_FORMAT = _pybraw.blackmagicRawResourceFormatRGBAU8
MAX_JOBS_IN_FLIGHT = 2


def argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=str, required=True,
                        help='input BRAW video file')
    return parser


class AtomicInt:
    def __init__(self, value: int = 0):
        self.value = value
        self.lock = Lock()

    def fetch_add(self):
        with self.lock:
            old_value = self.value
            self.value += 1
            return old_value

    def fetch_sub(self):
        with self.lock:
            old_value = self.value
            self.value -= 1
            return old_value


def checked_result(return_values, expected_result=_pybraw.S_OK):
    if isinstance(return_values, int):
        result = return_values
        unwrapped = None
    else:
        result = return_values[0]
        if len(return_values) == 1:
            unwrapped = None
        elif len(return_values) == 2:
            unwrapped = return_values[1]
        else:
            unwrapped = return_values[1:]
    assert result == expected_result, f'expected result {expected_result}, got {result}'
    return unwrapped


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
                checked_result(self.resource_manager.ReleaseResource(None, None, buf, _pybraw.blackmagicRawResourceTypeBufferCPU))

    def populate_frame_state_buffer(self, frame):
        frame_state_size_bytes = checked_result(self.manual_decoder.GetFrameStateSizeBytes())
        if frame_state_size_bytes > self.frame_state_size_bytes:
            if self.frame_state is not None:
                checked_result(self.resource_manager.ReleaseResource(None, None, self.frame_state, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.frame_state = checked_result(self.resource_manager.CreateResource(None, None, frame_state_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.frame_state_size_bytes = frame_state_size_bytes
        checked_result(self.manual_decoder.PopulateFrameStateBuffer(frame, None, None, self.frame_state, self.frame_state_size_bytes))

    def create_read_job(self, clip_ex, frame_index) -> _pybraw.IBlackmagicRawJob:
        bit_stream_size_bytes = checked_result(clip_ex.GetBitStreamSizeBytes(frame_index))
        if bit_stream_size_bytes > self.bit_stream_size_bytes:
            if self.bit_stream is not None:
                checked_result(self.resource_manager.ReleaseResource(None, None, self.bit_stream, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.bit_stream = checked_result(self.resource_manager.CreateResource(None, None, bit_stream_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.bit_stream_size_bytes = bit_stream_size_bytes
        read_job = checked_result(clip_ex.CreateJobReadFrame(frame_index, self.bit_stream, bit_stream_size_bytes))
        return read_job

    def create_decode_job(self) -> _pybraw.IBlackmagicRawJob:
        decoded_buffer_size_bytes = checked_result(self.manual_decoder.GetDecodedSizeBytes(self.frame_state))
        if decoded_buffer_size_bytes > self.decoded_buffer_size_bytes:
            if self.decoded_buffer is not None:
                checked_result(self.resource_manager.ReleaseResource(None, None, self.decoded_buffer, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.decoded_buffer = checked_result(self.resource_manager.CreateResource(None, None, decoded_buffer_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.decoded_buffer_size_bytes = decoded_buffer_size_bytes
        decode_job = checked_result(self.manual_decoder.CreateJobDecode(self.frame_state, self.bit_stream, self.decoded_buffer))
        return decode_job

    def create_process_job(self) -> _pybraw.IBlackmagicRawJob:
        processed_buffer_size_bytes = checked_result(self.manual_decoder.GetProcessedSizeBytes(self.frame_state))
        if processed_buffer_size_bytes > self.processed_buffer_size_bytes:
            if self.processed_buffer is not None:
                checked_result(self.resource_manager.ReleaseResource(None, None, self.processed_buffer, _pybraw.blackmagicRawResourceTypeBufferCPU))
            self.processed_buffer = checked_result(self.resource_manager.CreateResource(None, None, processed_buffer_size_bytes, _pybraw.blackmagicRawResourceTypeBufferCPU, _pybraw.blackmagicRawResourceUsageReadCPUWriteCPU))
            self.processed_buffer_size_bytes = processed_buffer_size_bytes
        process_job = checked_result(self.manual_decoder.CreateJobProcess(self.frame_state, self.decoded_buffer, self.processed_buffer, self.post_3d_lut_buffer_cpu))
        return process_job


@dataclass(frozen=True)
class UserData:
    buffer_manager: BufferManagerFlow1
    frame_index: int


class CameraCodecCallback(_pybraw.BlackmagicRawCallback):
    def __init__(self, jobs_in_flight):
        super().__init__()
        self.jobs_in_flight = jobs_in_flight

    def ReadComplete(self, read_job, result, frame):
        user_data: UserData = checked_result(read_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            print(f'Read frame index: {user_data.frame_index}')
        else:
            print(f'Failed to read frame index: {user_data.frame_index}')
            self.jobs_in_flight.fetch_sub()
            return

        checked_result(frame.SetResourceFormat(RESOURCE_FORMAT))
        buffer_manager = user_data.buffer_manager
        buffer_manager.populate_frame_state_buffer(frame)

        decode_job = buffer_manager.create_decode_job()
        checked_result(decode_job.put_py_user_data(user_data))
        checked_result(decode_job.Submit())
        decode_job.Release()

    def DecodeComplete(self, decode_job, result):
        user_data: UserData = checked_result(decode_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            print(f'Decoded frame index: {user_data.frame_index}')
        else:
            print(f'Failed to decode frame index: {user_data.frame_index}')
            self.jobs_in_flight.fetch_sub()
            return

        buffer_manager = user_data.buffer_manager
        process_job = buffer_manager.create_process_job()
        checked_result(process_job.put_py_user_data(user_data))
        checked_result(process_job.Submit())
        process_job.Release()

    def ProcessComplete(self, process_job, result, processed_image):
        user_data: UserData = checked_result(process_job.pop_py_user_data())

        if result == _pybraw.S_OK:
            print(f'Processed frame index: {user_data.frame_index}')
        else:
            print(f'Failed to process frame index: {user_data.frame_index}')

        self.jobs_in_flight.fetch_sub()


def process_clip_manual(clip: _pybraw.IBlackmagicRawClip, resource_manager, manual_decoder, jobs_in_flight):
    clip_ex = checked_result(clip.as_IBlackmagicRawClipEx())
    clip_processing_attributes = checked_result(clip.as_IBlackmagicRawClipProcessingAttributes())
    frame_count = checked_result(clip.GetFrameCount())
    clip_post_3d_lut = checked_result(clip_processing_attributes.GetPost3DLUT())
    if clip_post_3d_lut is not None:
        post_3d_lut_buffer_cpu = checked_result(clip_post_3d_lut.GetResourceCPU())
    else:
        post_3d_lut_buffer_cpu = None
    buffer_manager_pool = [
        BufferManagerFlow1(resource_manager, manual_decoder, post_3d_lut_buffer_cpu)
        for _ in range(MAX_JOBS_IN_FLIGHT)
    ]
    for frame_index in range(frame_count):
        while jobs_in_flight.value >= MAX_JOBS_IN_FLIGHT:
            sleep(0.001)

        buffer_manager = buffer_manager_pool[frame_index % MAX_JOBS_IN_FLIGHT]
        read_job = buffer_manager.create_read_job(clip_ex, frame_index)
        user_data = UserData(buffer_manager, frame_index)
        checked_result(read_job.put_py_user_data(user_data))
        checked_result(read_job.Submit())
        jobs_in_flight.fetch_add()
        read_job.Release()

    while jobs_in_flight.value > 0:
        sleep(0.001)


def main(args):
    opts = argument_parser().parse_args(args)

    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    codec = checked_result(factory.CreateCodec())
    configuration_ex = checked_result(codec.as_IBlackmagicRawConfigurationEx())
    resource_manager = checked_result(configuration_ex.GetResourceManager())
    manual_decoder = checked_result(codec.as_IBlackmagicRawManualDecoderFlow1())
    clip = checked_result(codec.OpenClip(opts.input))
    jobs_in_flight = AtomicInt()
    callback = CameraCodecCallback(jobs_in_flight)
    checked_result(codec.SetCallback(callback))
    process_clip_manual(clip, resource_manager, manual_decoder, jobs_in_flight)
    codec.FlushJobs()


if __name__ == '__main__':
    main(sys.argv[1:])
