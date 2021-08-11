from pybraw import _pybraw

from .helpers import checked_result


def test_GetPipeline(cpu_device_iterator):
    pipeline = checked_result(cpu_device_iterator.GetPipeline())
    assert pipeline == _pybraw.blackmagicRawPipelineCPU


def test_GetInterop(cpu_device_iterator):
    interop = checked_result(cpu_device_iterator.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_CreateDevice(cpu_device_iterator):
    pipeline_device = checked_result(cpu_device_iterator.CreateDevice())
    assert pipeline_device
    assert pipeline_device.Release() == 0
