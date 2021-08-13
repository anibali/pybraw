import pytest
from pybraw import _pybraw

from .helpers import checked_result


@pytest.fixture
def factory():
    return _pybraw.CreateBlackmagicRawFactoryInstance()


@pytest.fixture
def pipeline_iterator(factory):
    return checked_result(factory.CreatePipelineIterator(_pybraw.blackmagicRawInteropNone))


@pytest.fixture
def cpu_device_iterator(factory):
    return checked_result(factory.CreatePipelineDeviceIterator(_pybraw.blackmagicRawPipelineCPU, _pybraw.blackmagicRawInteropNone))


@pytest.fixture
def cpu_device(cpu_device_iterator):
    return checked_result(cpu_device_iterator.CreateDevice())


@pytest.fixture
def codec(factory):
    return checked_result(factory.CreateCodec())


@pytest.fixture
def configuration(codec):
    return checked_result(codec.as_IBlackmagicRawConfiguration())


@pytest.fixture
def clip(codec, sample_filename):
    return checked_result(codec.OpenClip(sample_filename))
