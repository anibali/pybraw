import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw, verify
from pytest_lazyfixture import lazy_fixture

from .helpers import releases_last_reference


def test_automatic_release(codec, sample_filename):
    clip = verify(codec.OpenClip(sample_filename))
    with releases_last_reference(clip):
        del clip


def test_GetWidth(clip):
    width = verify(clip.GetWidth())
    assert width == 4096


def test_GetHeight(clip):
    height = verify(clip.GetHeight())
    assert height == 2160


def test_GetFrameRate(clip):
    frame_rate = verify(clip.GetFrameRate())
    assert frame_rate == 25.0


def test_GetFrameCount(clip):
    frame_count = verify(clip.GetFrameCount())
    assert frame_count == 418


def test_GetTimecodeForFrame(clip):
    timecode = verify(clip.GetTimecodeForFrame(0))
    assert timecode == '14:39:00:23'


def test_GetMetadataIterator(clip):
    iterator = verify(clip.GetMetadataIterator())
    metadata = {}
    while True:
        result, key = iterator.GetKey()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        metadata[key] = verify(iterator.GetData()).to_py()
        verify(iterator.Next())
    assert 'firmware_version' in metadata
    assert metadata['firmware_version'] == '6.2'
    assert_allclose(metadata['crop_origin'], np.array([16.0, 8.0]))


def test_GetMetadata(clip):
    day_night = verify(clip.GetMetadata('day_night'))
    assert day_night.to_py() == 'day'


def test_SetMetadata(clip):
    verify(clip.SetMetadata('day_night', _pybraw.VariantCreateString('night')))
    day_night = verify(clip.GetMetadata('day_night'))
    assert day_night.to_py() == 'night'


def test_GetCameraType(clip):
    camera_type = verify(clip.GetCameraType())
    assert camera_type == 'Blackmagic Pocket Cinema Camera 4K'


def test_CloneClipProcessingAttributes(clip):
    attributes = verify(clip.CloneClipProcessingAttributes())
    assert isinstance(attributes, _pybraw.IBlackmagicRawClipProcessingAttributes)
    gamut = verify(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeGamut))
    assert gamut.to_py() == 'Blackmagic Design'


@pytest.mark.parametrize('cur_clip,expected', [
    (lazy_fixture('clip'), False),
    (lazy_fixture('bw_clip'), True),
])
def test_GetSidecarFileAttached(cur_clip, expected):
    is_attached = verify(cur_clip.GetSidecarFileAttached())
    assert is_attached == expected


def test_GetClipAttribute(clip):
    attributes = verify(clip.as_IBlackmagicRawClipProcessingAttributes())
    gamut = verify(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeGamut))
    assert gamut.to_py() == 'Blackmagic Design'


def test_SetClipAttribute(clip):
    attributes = verify(clip.as_IBlackmagicRawClipProcessingAttributes())
    value = _pybraw.VariantCreateFloat32(0.25)
    verify(attributes.SetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeToneCurveBlackLevel, value))
    black_level = verify(attributes.GetClipAttribute(_pybraw.blackmagicRawClipProcessingAttributeToneCurveBlackLevel))
    assert black_level.to_py() == 0.25


@pytest.mark.parametrize('cur_clip,lut_size', [
    (lazy_fixture('clip'), None),
    (lazy_fixture('bw_clip'), 17),
])
def test_GetPost3DLUT(cur_clip, lut_size):
    attributes = verify(cur_clip.as_IBlackmagicRawClipProcessingAttributes())
    lut = verify(attributes.GetPost3DLUT())
    if lut_size is None:
        assert lut is None
    else:
        assert verify(lut.GetSize()) == lut_size
