import pytest
from pybraw import _pybraw, verify

from .helpers import releases_last_reference


def test_automatic_release(factory):
    codec = verify(factory.CreateCodec())
    configuration = verify(codec.as_IBlackmagicRawConfiguration())
    del codec
    with releases_last_reference(configuration):
        del configuration


def test_IsPipelineSupported(configuration):
    pipeline_supported = verify(configuration.IsPipelineSupported(_pybraw.blackmagicRawPipelineCPU))
    assert pipeline_supported == True


def test_GetCPUThreads(configuration):
    thread_count = verify(configuration.GetCPUThreads())
    assert thread_count > 0


def test_GetMaxCPUThreadCount(configuration):
    thread_count = verify(configuration.GetMaxCPUThreadCount())
    assert thread_count > 0


@pytest.mark.parametrize('thread_count', [1, 2, 4])
def test_SetCPUThreads(configuration, thread_count):
    verify(configuration.SetCPUThreads(thread_count))
    actual = verify(configuration.GetCPUThreads())
    assert actual == thread_count


@pytest.mark.parametrize('write_per_frame', [True, False])
def test_SetWriteMetadataPerFrame(configuration, write_per_frame):
    verify(configuration.SetWriteMetadataPerFrame(write_per_frame))
    actual = verify(configuration.GetWriteMetadataPerFrame())
    assert actual == write_per_frame


def test_SetFromDevice(configuration, cpu_device):
    verify(configuration.SetFromDevice(cpu_device))
    actual = verify(configuration.GetPipeline())
    assert actual == (_pybraw.blackmagicRawPipelineCPU, None, None)
