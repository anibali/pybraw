import pytest
from pybraw import _pybraw

from .helpers import checked_result


@pytest.fixture
def factory():
    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    yield factory
    assert factory.Release() == 0


@pytest.fixture
def pipeline_iterator(factory):
    pipeline_iterator = checked_result(factory.CreatePipelineIterator(_pybraw.blackmagicRawInteropNone))
    yield pipeline_iterator
    assert pipeline_iterator.Release() == 0


@pytest.fixture
def cpu_device_iterator(factory):
    device_iterator = checked_result(factory.CreatePipelineDeviceIterator(_pybraw.blackmagicRawPipelineCPU, _pybraw.blackmagicRawInteropNone))
    yield device_iterator
    assert device_iterator.Release() == 0


@pytest.fixture
def cpu_device(cpu_device_iterator):
    pipeline_device = checked_result(cpu_device_iterator.CreateDevice())
    yield pipeline_device
    assert pipeline_device.Release() == 0


@pytest.fixture
def codec(factory):
    codec = checked_result(factory.CreateCodec())
    yield codec
    assert codec.Release() == 0


@pytest.fixture
def clip(codec, sample_filename):
    clip = checked_result(codec.OpenClip(sample_filename))
    yield clip
    assert clip.Release() == 0
