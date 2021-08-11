from pybraw import _pybraw

from .helpers import checked_result


def test_GetIndex(cpu_device):
    device_index = checked_result(cpu_device.GetIndex())
    assert device_index == 0


def test_GetInterop(cpu_device):
    interop = checked_result(cpu_device.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_GetPipelineName(cpu_device):
    pipeline_name = checked_result(cpu_device.GetPipelineName())
    assert pipeline_name == 'CPU'
