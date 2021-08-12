import pytest
from pybraw import _pybraw

from .helpers import checked_result


def test_IsPipelineSupported(configuration):
    pipeline_supported = checked_result(configuration.IsPipelineSupported(_pybraw.blackmagicRawPipelineCPU))
    assert pipeline_supported == True


def test_GetCPUThreads(configuration):
    thread_count = checked_result(configuration.GetCPUThreads())
    assert thread_count > 0


def test_GetMaxCPUThreadCount(configuration):
    thread_count = checked_result(configuration.GetMaxCPUThreadCount())
    assert thread_count > 0


@pytest.mark.parametrize('thread_count', [1, 2, 4])
def test_SetCPUThreads(configuration, thread_count):
    checked_result(configuration.SetCPUThreads(thread_count))
    actual = checked_result(configuration.GetCPUThreads())
    assert actual == thread_count


@pytest.mark.parametrize('write_per_frame', [True, False])
def test_SetWriteMetadataPerFrame(configuration, write_per_frame):
    checked_result(configuration.SetWriteMetadataPerFrame(write_per_frame))
    actual = checked_result(configuration.GetWriteMetadataPerFrame())
    assert actual == write_per_frame


def test_SetFromDevice(configuration, cpu_device):
    checked_result(configuration.SetFromDevice(cpu_device))
    # TODO: Use GetPipeline to check pipelines match.
