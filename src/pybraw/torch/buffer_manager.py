from abc import ABC, abstractmethod
from math import ceil
from typing import Optional, Sequence

import torch
from pybraw import verify, _pybraw, PixelFormat, ResolutionScale
from torch.nn.functional import interpolate


def _create_storage(pixel_type, device, size):
    device = torch.device(device)
    if device.type == 'cpu':
        if pixel_type == 'U8':
            return torch.ByteStorage(size)
        elif pixel_type == 'U16':
            # This is a bit wonky since ShortStorage in PyTorch is for _signed_ numbers.
            return torch.ShortStorage(size)
        elif pixel_type == 'F32':
            return torch.FloatStorage(size)
    elif device.type == 'cuda':
        with torch.cuda.device(device):
            if pixel_type == 'U8':
                return torch.cuda.ByteStorage(0)
            elif pixel_type == 'U16':
                # This is a bit wonky since ShortStorage in PyTorch is for _signed_ numbers.
                return torch.cuda.ShortStorage(0)
            elif pixel_type == 'F32':
                return torch.cuda.FloatStorage(0)
    raise NotImplementedError(f'Unsupported storage type: {device}, {pixel_type}')


def _storage_to_tensor(storage):
    device = storage.device
    dtype = storage.dtype
    if device.type == 'cpu':
        if dtype == torch.uint8:
            return torch.ByteTensor(storage)
        elif dtype == torch.int16:
            return torch.ShortTensor(storage)
        elif dtype == torch.float32:
            return torch.FloatTensor(storage)
    if device.type == 'cuda':
        with torch.cuda.device(device):
            if dtype == torch.uint8:
                return torch.cuda.ByteTensor(storage)
            elif dtype == torch.int16:
                return torch.cuda.ShortTensor(storage)
            elif dtype == torch.float32:
                return torch.cuda.FloatTensor(storage)
    raise NotImplementedError(f'Unsupported tensor type: {device}, {dtype}')


class BufferManager(ABC):
    def __init__(self, manual_decoder):
        self.manual_decoder = manual_decoder
        self.bit_stream = torch.ByteStorage(0)
        self.frame_state = torch.ByteStorage(0)
        self.decoded_buffer = torch.ByteStorage(0)

    @property
    def frame_state_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.frame_state.data_ptr())

    @property
    def bit_stream_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.bit_stream.data_ptr())

    @property
    def decoded_buffer_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.decoded_buffer.data_ptr())

    def populate_frame_state_buffer(self, frame):
        frame_state_size_bytes = verify(self.manual_decoder.GetFrameStateSizeBytes())
        self.frame_state.resize_(frame_state_size_bytes)
        verify(self.manual_decoder.PopulateFrameStateBuffer(frame, None, None, self.frame_state_resource, frame_state_size_bytes))

    def create_read_job(self, clip_ex, frame_index) -> _pybraw.IBlackmagicRawJob:
        bit_stream_size_bytes = verify(clip_ex.GetBitStreamSizeBytes(frame_index))
        self.bit_stream.resize_(bit_stream_size_bytes)
        read_job = verify(clip_ex.CreateJobReadFrame(frame_index, self.bit_stream_resource, bit_stream_size_bytes))
        return read_job

    @abstractmethod
    def create_decode_job(self) -> _pybraw.IBlackmagicRawJob:
        pass

    @abstractmethod
    def create_process_job(self) -> _pybraw.IBlackmagicRawJob:
        pass

    @abstractmethod
    def get_output_buffer(self):
        pass

    @abstractmethod
    def replace_output_buffer(self):
        pass

    def postprocess(
        self,
        processed_image: _pybraw.IBlackmagicRawProcessedImage,
        resolution_scale: ResolutionScale,
        device: torch.device = None,
        crop: Optional[Sequence[int]] = None,
        out_size: Optional[Sequence[int]] = None,
    ):
        output_buffer = self.get_output_buffer()

        ref_resource = verify(processed_image.GetResource())
        if output_buffer.data_ptr() != int(ref_resource):
            raise ValueError('Processed image does not match the buffer')

        if device is None:
            device = output_buffer.device

        if resolution_scale.is_flipped():
            raise NotImplementedError('Flipped images are currently not supported')
        scale_factor = resolution_scale.factor()

        width = verify(processed_image.GetWidth())
        height = verify(processed_image.GetHeight())
        resource_format = verify(processed_image.GetResourceFormat())
        pixel_format: PixelFormat = PixelFormat(resource_format)
        n_channels = len(pixel_format.channels())
        n_elements = n_channels * height * width

        image_tensor = _storage_to_tensor(output_buffer)
        image_tensor = image_tensor[:n_elements]
        image_tensor = image_tensor.to(device)

        if pixel_format.is_planar():
            print(n_channels, height, width)
            image_tensor = image_tensor.view(n_channels, height, width)
            height_axis = 1
            width_axis = 2
        else:
            image_tensor = image_tensor.view(height, width, n_channels)
            height_axis = 0
            width_axis = 1

        if crop is not None:
            x, y, w, h = crop
            x = round(x / scale_factor)
            y = round(y / scale_factor)
            w = round(w / scale_factor)
            h = round(h / scale_factor)
            image_tensor = image_tensor.narrow(width_axis, x, w).narrow(height_axis, y, h)

        if out_size is not None:
            out_width, out_height = out_size
            if not (out_width == image_tensor.shape[width_axis] and out_height == image_tensor.shape[height_axis]):
                if not pixel_format.is_planar():
                    raise NotImplementedError('Resizing is currently only supported for planar pixel formats')
                image_tensor = interpolate(image_tensor[None, ...], (out_height, out_width),
                                           mode='bilinear', align_corners=False)[0]

        # Replace the previous buffer if we are referencing its memory.
        # This prevents us from overwriting the data with subsequent reads.
        if image_tensor.storage().data_ptr() == output_buffer.data_ptr():
            self.replace_output_buffer()

        return image_tensor


class BufferManagerFlow1(BufferManager):
    def __init__(self, manual_decoder, post_3d_lut: Optional[torch.ByteStorage], pixel_format: PixelFormat):
        super().__init__(manual_decoder)
        self._post_3d_lut = post_3d_lut
        self._pixel_format = pixel_format
        self.replace_output_buffer()

    def get_output_buffer(self):
        return self.processed_buffer

    def replace_output_buffer(self):
        self.processed_buffer = _create_storage(self._pixel_format.data_type(), 'cpu', 0)

    @property
    def post_3d_lut_resource(self):
        if self._post_3d_lut is None:
            return _pybraw.CreateResourceNone()
        return _pybraw.CreateResourceFromIntPointer(self._post_3d_lut.data_ptr())

    @property
    def processed_buffer_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.processed_buffer.data_ptr())

    def create_decode_job(self):
        decoded_buffer_size_bytes = verify(self.manual_decoder.GetDecodedSizeBytes(self.frame_state_resource))
        self.decoded_buffer.resize_(decoded_buffer_size_bytes)
        decode_job = verify(self.manual_decoder.CreateJobDecode(self.frame_state_resource, self.bit_stream_resource, self.decoded_buffer_resource))
        return decode_job

    def create_process_job(self):
        processed_buffer_size_bytes = verify(self.manual_decoder.GetProcessedSizeBytes(self.frame_state_resource))
        self.processed_buffer.resize_(ceil(processed_buffer_size_bytes / self.processed_buffer.element_size()))
        process_job = verify(self.manual_decoder.CreateJobProcess(self.frame_state_resource, self.decoded_buffer_resource, self.processed_buffer_resource, self.post_3d_lut_resource))
        return process_job


class BufferManagerFlow2(BufferManager):
    def __init__(self, manual_decoder, post_3d_lut_gpu: Optional[torch.cuda.ByteStorage], pixel_format: PixelFormat, context, command_queue, processing_device: torch.device):
        super().__init__(manual_decoder)
        self.context = context
        self.command_queue = command_queue
        self._post_3d_lut_gpu = post_3d_lut_gpu
        self.processing_device = processing_device
        with torch.cuda.device(self.processing_device):
            self.decoded_buffer_gpu = torch.cuda.ByteStorage(0)
            self.working_buffer = torch.cuda.ByteStorage(0)
        self._pixel_format = pixel_format
        self.replace_output_buffer()

    def get_output_buffer(self):
        return self.processed_buffer

    def replace_output_buffer(self):
        self.processed_buffer = _create_storage(self._pixel_format.data_type(), self.processing_device, 0)

    @property
    def post_3d_lut_gpu_resource(self):
        if self._post_3d_lut_gpu is None:
            return _pybraw.CreateResourceNone()
        return _pybraw.CreateResourceFromIntPointer(self._post_3d_lut_gpu.data_ptr())

    @property
    def decoded_buffer_gpu_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.decoded_buffer_gpu.data_ptr())

    @property
    def working_buffer_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.working_buffer.data_ptr())

    @property
    def processed_buffer_resource(self):
        return _pybraw.CreateResourceFromIntPointer(self.processed_buffer.data_ptr())

    def create_decode_job(self) -> _pybraw.IBlackmagicRawJob:
        decoded_buffer_size_bytes = verify(self.manual_decoder.GetDecodedSizeBytes(self.frame_state_resource))
        self.decoded_buffer.resize_(decoded_buffer_size_bytes)
        self.decoded_buffer_gpu.resize_(decoded_buffer_size_bytes)
        decode_job = verify(self.manual_decoder.CreateJobDecode(self.frame_state_resource, self.bit_stream_resource, self.decoded_buffer_resource))
        return decode_job

    def create_process_job(self) -> _pybraw.IBlackmagicRawJob:
        working_buffer_size_bytes = verify(self.manual_decoder.GetWorkingSizeBytes(self.frame_state_resource))
        self.working_buffer.resize_(working_buffer_size_bytes)
        processed_buffer_size_bytes = verify(self.manual_decoder.GetProcessedSizeBytes(self.frame_state_resource))
        self.processed_buffer.resize_(ceil(processed_buffer_size_bytes / self.processed_buffer.element_size()))
        self.decoded_buffer_gpu.copy_(self.decoded_buffer)
        process_job = verify(self.manual_decoder.CreateJobProcess(
            self.context, self.command_queue, self.frame_state_resource,
            self.decoded_buffer_gpu_resource, self.working_buffer_resource,
            self.processed_buffer_resource, self.post_3d_lut_gpu_resource))
        return process_job
