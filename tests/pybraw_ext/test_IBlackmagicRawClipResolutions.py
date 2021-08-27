import pytest

from pybraw import verify, ResolutionScale


@pytest.fixture
def clip_resolutions(clip):
    return verify(clip.as_IBlackmagicRawClipResolutions())


def test_GetResolutionCount(clip_resolutions):
    assert verify(clip_resolutions.GetResolutionCount()) == 4


@pytest.mark.parametrize('index,width,height', [
    (0, 4096, 2160),
    (1, 2048, 1080),
    (2, 1024, 540),
    (3, 512, 270),
])
def test_GetResolution(clip_resolutions, index, width, height):
    assert verify(clip_resolutions.GetResolution(index)) == (width, height)


@pytest.mark.parametrize('width,height,flipped,scale', [
    (4096, 2160, False, ResolutionScale.Full),
    (4096, 2160, True, ResolutionScale.Full_Flipped),
    (4095, 2159, False, ResolutionScale.Half),
])
def test_GetClosestScaleForResolution(clip_resolutions, width, height, flipped, scale):
    actual = ResolutionScale(verify(clip_resolutions.GetClosestScaleForResolution(width, height, flipped)))
    assert actual == scale
