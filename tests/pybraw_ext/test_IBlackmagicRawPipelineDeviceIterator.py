from pybraw import _pybraw

from .helpers import checked_result


def test_GetPipeline(device_iterator):
    pipeline = checked_result(device_iterator.GetPipeline())
    assert pipeline == _pybraw.blackmagicRawPipelineCPU


def test_GetInterop(device_iterator):
    interop = checked_result(device_iterator.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_CreateDevice(device_iterator):
    pipeline_device = checked_result(device_iterator.CreateDevice())
    assert pipeline_device
    pipeline_name = checked_result(pipeline_device.GetPipelineName())
    assert pipeline_name == 'CPU'
    assert pipeline_device.Release() == 0
