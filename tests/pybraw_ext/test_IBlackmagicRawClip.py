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
