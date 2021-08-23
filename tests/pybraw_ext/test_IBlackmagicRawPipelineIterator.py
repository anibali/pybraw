from pybraw import _pybraw, verify


def test_GetName(pipeline_iterator):
    pipeline_names = []
    while True:
        result, pipeline_name = pipeline_iterator.GetName()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        pipeline_names.append(pipeline_name)
        verify(pipeline_iterator.Next())
    assert 'CPU' in pipeline_names


def test_GetInterop(pipeline_iterator):
    interop = verify(pipeline_iterator.GetInterop())
    assert interop == _pybraw.blackmagicRawInteropNone


def test_GetPipeline(pipeline_iterator):
    pipelines = []
    while True:
        result, pipeline = pipeline_iterator.GetPipeline()
        if result == _pybraw.E_FAIL:
            break
        assert result == _pybraw.S_OK
        pipelines.append(pipeline)
        verify(pipeline_iterator.Next())
    assert _pybraw.blackmagicRawPipelineCPU in pipelines
