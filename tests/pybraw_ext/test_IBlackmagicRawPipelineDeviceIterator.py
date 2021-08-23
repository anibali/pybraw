from pybraw import _pybraw, verify


def test_GetPipeline(cpu_device_iterator):
    pipeline = verify(cpu_device_iterator.GetPipeline())
    assert pipeline == _pybraw.blackmagicRawPipelineCPU


def test_GetInterop(cpu_device_iterator):
    interop = verify(cpu_device_iterator.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_CreateDevice(cpu_device_iterator):
    pipeline_device = verify(cpu_device_iterator.CreateDevice())
    assert isinstance(pipeline_device, _pybraw.IBlackmagicRawPipelineDevice)
