from pybraw import _pybraw

from .helpers import checked_result


def test_GetIndex(cpu_device):
    device_index = checked_result(cpu_device.GetIndex())
    assert device_index == 0


def test_GetName(cpu_device):
    name = checked_result(cpu_device.GetName())
    assert isinstance(name, str)


def test_GetInterop(cpu_device):
    interop = checked_result(cpu_device.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_GetPipelineName(cpu_device):
    pipeline_name = checked_result(cpu_device.GetPipelineName())
    assert pipeline_name == 'CPU'


def test_GetOpenGLInteropHelper(cpu_device):
    interop_helper = checked_result(cpu_device.GetOpenGLInteropHelper())
    assert isinstance(interop_helper, _pybraw.IBlackmagicRawOpenGLInteropHelper)
    preferred_format = checked_result(interop_helper.GetPreferredResourceFormat())
    possibilities = [e.value for e in _pybraw._BlackmagicRawResourceFormat.__members__.values()]
    assert preferred_format in possibilities
