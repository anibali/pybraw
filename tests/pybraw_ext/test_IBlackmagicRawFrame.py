import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw

from .helpers import checked_result


class CapturingCallback(_pybraw.BlackmagicRawCallback):
    def ReadComplete(self, job, result, frame):
        self.frame = frame

    def ProcessComplete(self, job, result, processed_image):
        self.processed_image = processed_image


@pytest.fixture
def callback(codec):
    callback = CapturingCallback()
    codec.SetCallback(callback)
    return callback


@pytest.fixture
def frame(codec, clip, callback):
    read_job = checked_result(clip.CreateJobReadFrame(12))
    read_job.Submit()
    read_job.Release()
    codec.FlushJobs()
    return callback.frame


@pytest.mark.parametrize('format,max_val,is_planar,channels', [
    (_pybraw.blackmagicRawResourceFormatBGRAU8, 2**8, False, [2, 1, 0, 3]),
    (_pybraw.blackmagicRawResourceFormatRGBF32Planar, 1, True, [0, 1, 2]),
    (_pybraw.blackmagicRawResourceFormatRGBU16Planar, 2**16, True, [0, 1, 2]),
])
def test_SetResourceFormat(frame, codec, callback, format, max_val, is_planar, channels):
    checked_result(frame.SetResourceFormat(format))
    process_job = checked_result(frame.CreateJobDecodeAndProcessFrame())
    process_job.Submit()
    process_job.Release()
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


def test_SetResolutionScale(frame, codec, callback):
    checked_result(frame.SetResolutionScale(_pybraw.blackmagicRawResolutionScaleQuarter))
    process_job = checked_result(frame.CreateJobDecodeAndProcessFrame())
    process_job.Submit()
    process_job.Release()
    codec.FlushJobs()

    # Check that the resolution is one quarter of the original DCI full frame 4K.
    width = checked_result(callback.processed_image.GetWidth())
    assert width == 1024
    height = checked_result(callback.processed_image.GetHeight())
    assert height == 540

    # from PIL import Image
    # pil_image = Image.fromarray(callback.processed_image.numpy()[..., :3])
    # pil_image.show()


def test_CloneFrameProcessingAttributes(frame):
    attributes = checked_result(frame.CloneFrameProcessingAttributes())
    assert isinstance(attributes, _pybraw.IBlackmagicRawFrameProcessingAttributes)
    iso = checked_result(attributes.GetFrameAttribute(_pybraw.blackmagicRawFrameProcessingAttributeISO)).to_py()
    assert iso == 400


def test_GetMetadataIterator(frame):
    iterator = checked_result(frame.GetMetadataIterator())
    metadata = {}
    while True:
        result, key = iterator.GetKey()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        metadata[key] = checked_result(iterator.GetData()).to_py()
        result = iterator.Next()
        assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
    assert metadata['white_balance_kelvin'] == 5600
    assert_allclose(metadata['sensor_rate'], np.array([25, 1]))


def test_GetMetadata(frame):
    white_balance = checked_result(frame.GetMetadata('white_balance_kelvin'))
    assert white_balance.to_py() == 5600


def test_SetMetadata(frame):
    checked_result(frame.SetMetadata('white_balance_kelvin', _pybraw.VariantCreateU32(2800)))
    white_balance = checked_result(frame.GetMetadata('white_balance_kelvin'))
    assert white_balance.to_py() == 2800
