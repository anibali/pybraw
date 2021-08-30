import pytest
from pybraw import _pybraw, verify


@pytest.fixture
def factory():
    return _pybraw.CreateBlackmagicRawFactoryInstance()


@pytest.fixture
def pipeline_iterator(factory):
    return verify(factory.CreatePipelineIterator(_pybraw.blackmagicRawInteropNone))


@pytest.fixture
def cpu_device_iterator(factory):
    return verify(factory.CreatePipelineDeviceIterator(_pybraw.blackmagicRawPipelineCPU, _pybraw.blackmagicRawInteropNone))


@pytest.fixture
def cpu_device(cpu_device_iterator):
    return verify(cpu_device_iterator.CreateDevice())


@pytest.fixture
def codec(factory):
    return verify(factory.CreateCodec())


@pytest.fixture
def configuration(codec):
    return verify(codec.as_IBlackmagicRawConfiguration())


@pytest.fixture
def constants(codec):
    return verify(codec.as_IBlackmagicRawConstants())


@pytest.fixture
def clip(codec, sample_filename):
    return verify(codec.OpenClip(sample_filename))


@pytest.fixture
def bw_clip(codec, bw_filename):
    return verify(codec.OpenClip(bw_filename))
