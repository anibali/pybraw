from pybraw import _pybraw, verify


def test_GetIndex(cpu_device):
    device_index = verify(cpu_device.GetIndex())
    assert device_index == 0


def test_GetName(cpu_device):
    name = verify(cpu_device.GetName())
    assert isinstance(name, str)


def test_GetInterop(cpu_device):
    interop = verify(cpu_device.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_GetPipelineName(cpu_device):
    pipeline_name = verify(cpu_device.GetPipelineName())
    assert pipeline_name == 'CPU'


def test_GetOpenGLInteropHelper(cpu_device):
    interop_helper = verify(cpu_device.GetOpenGLInteropHelper())
    assert isinstance(interop_helper, _pybraw.IBlackmagicRawOpenGLInteropHelper)
    preferred_format = verify(interop_helper.GetPreferredResourceFormat())
    possibilities = [e.value for e in _pybraw._BlackmagicRawResourceFormat.__members__.values()]
    assert preferred_format in possibilities
