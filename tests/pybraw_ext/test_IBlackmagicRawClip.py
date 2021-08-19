import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw
from pytest_lazyfixture import lazy_fixture

from .helpers import checked_result, releases_last_reference


def test_automatic_release(codec, sample_filename):
    clip = checked_result(codec.OpenClip(sample_filename))
    with releases_last_reference(clip):
        del clip


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


def test_GetMetadata(clip):
    day_night = checked_result(clip.GetMetadata('day_night'))
    assert day_night.to_py() == 'day'


def test_SetMetadata(clip):
    checked_result(clip.SetMetadata('day_night', _pybraw.VariantCreateString('night')))
    day_night = checked_result(clip.GetMetadata('day_night'))
    assert day_night.to_py() == 'night'


def test_GetCameraType(clip):
    camera_type = checked_result(clip.GetCameraType())
    assert camera_type == 'Blackmagic Pocket Cinema Camera 4K'


def test_CloneClipProcessingAttributes(clip):
    attributes = checked_result(clip.CloneClipProcessingAttributes())
    assert isinstance(attributes, _pybraw.IBlackmagicRawClipProcessingAttributes)
    gamut = checked_result(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeGamut))
    assert gamut.to_py() == 'Blackmagic Design'


@pytest.mark.parametrize('cur_clip,expected', [
    (lazy_fixture('clip'), False),
    (lazy_fixture('bw_clip'), True),
])
def test_GetSidecarFileAttached(cur_clip, expected):
    is_attached = checked_result(cur_clip.GetSidecarFileAttached())
    assert is_attached == expected


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


@pytest.mark.parametrize('cur_clip,lut_size', [
    (lazy_fixture('clip'), None),
    (lazy_fixture('bw_clip'), 17),
])
def test_GetPost3DLUT(cur_clip, lut_size):
    attributes = checked_result(cur_clip.as_IBlackmagicRawClipProcessingAttributes())
    lut = checked_result(attributes.GetPost3DLUT())
    if lut_size is None:
        assert lut is None
    else:
        assert checked_result(lut.GetSize()) == lut_size
