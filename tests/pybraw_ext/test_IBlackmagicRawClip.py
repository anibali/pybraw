import numpy as np
from numpy.testing import assert_allclose
from pybraw import _pybraw

from .helpers import checked_result


def test_GetWidth(clip):
    width = checked_result(clip.GetWidth())
    assert width == 4096


def test_GetHeight(clip):
    height = checked_result(clip.GetHeight())
    assert height == 2160


def test_GetFrameRate(clip):
    frame_rate = checked_result(clip.GetFrameRate())
    assert frame_rate == 25.0


def test_GetFrameCount(clip):
    frame_count = checked_result(clip.GetFrameCount())
    assert frame_count == 418


def test_GetTimecodeForFrame(clip):
    timecode = checked_result(clip.GetTimecodeForFrame(0))
    assert timecode == '14:39:00:23'


def test_GetMetadataIterator_lowlevel(clip):
    iterator = checked_result(clip.GetMetadataIterator())
    metadata = {}
    while True:
        result, key = iterator.GetKey()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        data = checked_result(iterator.GetData())
        metadata[key] = data
        result = iterator.Next()
        assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
    assert 'firmware_version' in metadata
    assert metadata['firmware_version'].vt == _pybraw.blackmagicRawVariantTypeString
    assert metadata['firmware_version'].bstrVal == '6.2'
    assert metadata['crop_origin'].vt == _pybraw.blackmagicRawVariantTypeSafeArray
    assert_allclose(metadata['crop_origin'].parray.numpy(), np.array([16.0, 8.0]))
    for value in metadata.values():
        _pybraw.VariantClear(value)
    assert iterator.Release() == 0


def test_GetMetadataIterator_midlevel(clip):
    iterator = checked_result(clip.GetMetadataIterator())
    metadata = {}
    while True:
        result, key = iterator.GetKey()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        data = checked_result(iterator.GetData())
        metadata[key] = data.to_py()
        # We expect the data to be copied when calling to_py(), so it should be safe to clear
        # the variant here.
        _pybraw.VariantClear(data)
        result = iterator.Next()
        assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
    assert iterator.Release() == 0
    assert 'firmware_version' in metadata
    assert metadata['firmware_version'] == '6.2'
    assert_allclose(metadata['crop_origin'], np.array([16.0, 8.0]))


def test_read_frame(codec, clip):
    class MyCallback(_pybraw.BlackmagicRawCallback):
        def ReadComplete(self, job, result, frame):
            frame.AddRef()
            self.frame = frame

        def ProcessComplete(self, job, result, processed_image):
            processed_image.AddRef()
            self.processed_image = processed_image

    # NOTE: SetCallback _must_ be called before CreateJobReadFrame
    callback = MyCallback()
    callback.AddRef()
    codec.SetCallback(callback)
    assert callback.Release() == 1
    read_job = checked_result(clip.CreateJobReadFrame(12))
    read_job.Submit()
    assert read_job.Release() == 1
    codec.FlushJobs()
    process_job = checked_result(callback.frame.CreateJobDecodeAndProcessFrame())
    assert callback.frame.Release() == 1
    del callback.frame
    process_job.Submit()
    assert process_job.Release() == 1
    codec.FlushJobs()
    width = checked_result(callback.processed_image.GetWidth())
    assert width == 4096
    height = checked_result(callback.processed_image.GetHeight())
    assert height == 2160
    resource_type = checked_result(callback.processed_image.GetResourceType())
    assert resource_type == _pybraw.blackmagicRawResourceTypeBufferCPU
    resource_format = checked_result(callback.processed_image.GetResourceFormat())
    assert resource_format == _pybraw.blackmagicRawResourceFormatRGBAU8
    resource = checked_result(callback.processed_image.GetResource())
    np_image = resource.reshape(height, width, 4)
    assert callback.processed_image.Release() == 1  # Reference still held by np_image
    del callback.processed_image
    assert_allclose(np_image[100, 200], np.array([126, 131, 129, 255]))
    # import matplotlib.pyplot as plt
    # plt.imshow(np_image)
    # plt.show()
