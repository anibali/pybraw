import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw

from .helpers import checked_result


@pytest.mark.parametrize('format,max_val,is_planar,channels', [
    (_pybraw.blackmagicRawResourceFormatBGRAU8, 2**8, False, [2, 1, 0, 3]),
    (_pybraw.blackmagicRawResourceFormatRGBF32Planar, 1, True, [0, 1, 2]),
    (_pybraw.blackmagicRawResourceFormatRGBU16Planar, 2**16, True, [0, 1, 2]),
])
def test_SetResourceFormat(codec, clip, format, max_val, is_planar, channels):
    class MyCallback(_pybraw.BlackmagicRawCallback):
        def ReadComplete(self, job, result, frame):
            checked_result(frame.SetResourceFormat(format))
            process_job = checked_result(frame.CreateJobDecodeAndProcessFrame())
            process_job.Submit()
            process_job.Release()

        def ProcessComplete(self, job, result, processed_image):
            self.processed_image = processed_image

    # NOTE: SetCallback _must_ be called before CreateJobReadFrame
    callback = MyCallback()
    codec.SetCallback(callback)
    read_job = checked_result(clip.CreateJobReadFrame(12))
    read_job.Submit()
    read_job.Release()
    codec.FlushJobs()

    resource_type = checked_result(callback.processed_image.GetResourceType())
    assert resource_type == _pybraw.blackmagicRawResourceTypeBufferCPU
    resource_format = checked_result(callback.processed_image.GetResourceFormat())
    assert resource_format == format
    np_image = callback.processed_image.numpy()
    del callback.processed_image

    np_image = np_image / max_val
    if is_planar:
        np_image = np.transpose(np_image, (1, 2, 0))
    expected = np.array([126, 131, 129, 255])[channels] / 255
    assert_allclose(np_image[100, 200], expected, atol=1 / 255)


def test_SetResolutionScale(codec, clip):
    class MyCallback(_pybraw.BlackmagicRawCallback):
        def ReadComplete(self, job, result, frame):
            checked_result(frame.SetResolutionScale(_pybraw.blackmagicRawResolutionScaleQuarter))
            process_job = checked_result(frame.CreateJobDecodeAndProcessFrame())
            process_job.Submit()
            process_job.Release()

        def ProcessComplete(self, job, result, processed_image):
            self.processed_image = processed_image

    # NOTE: SetCallback _must_ be called before CreateJobReadFrame
    callback = MyCallback()
    codec.SetCallback(callback)
    read_job = checked_result(clip.CreateJobReadFrame(12))
    read_job.Submit()
    read_job.Release()
    codec.FlushJobs()

    # Check that the resolution is one quarter of the original DCI full frame 4K.
    width = checked_result(callback.processed_image.GetWidth())
    assert width == 1024
    height = checked_result(callback.processed_image.GetHeight())
    assert height == 540

    # from PIL import Image
    # pil_image = Image.fromarray(callback.processed_image.numpy()[..., :3])
    # pil_image.show()


def test_CloneFrameProcessingAttributes(codec, clip):
    class MyCallback(_pybraw.BlackmagicRawCallback):
        def ReadComplete(self, job, result, frame):
            attributes = checked_result(frame.CloneFrameProcessingAttributes())
            assert isinstance(attributes, _pybraw.IBlackmagicRawFrameProcessingAttributes)
            self.iso = checked_result(attributes.GetFrameAttribute(_pybraw.blackmagicRawFrameProcessingAttributeISO)).to_py()

    callback = MyCallback()
    codec.SetCallback(callback)
    read_job = checked_result(clip.CreateJobReadFrame(12))
    read_job.Submit()
    read_job.Release()
    codec.FlushJobs()

    assert callback.iso == 400
