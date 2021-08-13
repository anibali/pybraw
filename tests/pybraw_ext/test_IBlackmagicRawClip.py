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


def test_GetMetadataIterator(clip):
    iterator = checked_result(clip.GetMetadataIterator())
    metadata = {}
    while True:
        result, key = iterator.GetKey()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        metadata[key] = checked_result(iterator.GetData()).to_py()
        result = iterator.Next()
        assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
    assert 'firmware_version' in metadata
    assert metadata['firmware_version'] == '6.2'
    assert_allclose(metadata['crop_origin'], np.array([16.0, 8.0]))


def test_GetClipAttribute(clip):
    attributes = checked_result(clip.as_IBlackmagicRawClipProcessingAttributes())
    gamut = checked_result(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeGamut))
    assert gamut.to_py() == 'Blackmagic Design'


def test_SetClipAttribute(clip):
    attributes = checked_result(clip.as_IBlackmagicRawClipProcessingAttributes())
    value = _pybraw.VariantCreateFloat32(0.25)
    checked_result(attributes.SetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeToneCurveBlackLevel, value))
    black_level = checked_result(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeToneCurveBlackLevel))
    assert black_level.to_py() == 0.25


def test_GetPost3DLUT(clip):
    attributes = checked_result(clip.as_IBlackmagicRawClipProcessingAttributes())
    lut = checked_result(attributes.GetPost3DLUT())
    assert lut == None
