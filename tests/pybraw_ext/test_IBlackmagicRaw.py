import sys

from pybraw import _pybraw, verify, ResultCode
from .helpers import releases_last_reference


def test_automatic_release(factory):
    codec = verify(factory.CreateCodec())
    with releases_last_reference(codec):
        del codec


def test_GetResourceManager(codec):
    conf = verify(codec.as_IBlackmagicRawConfigurationEx())
    resource_manager = verify(conf.GetResourceManager())
    assert isinstance(resource_manager, _pybraw.IBlackmagicRawResourceManager)


def test_GetInstructionSet(codec):
    conf = verify(codec.as_IBlackmagicRawConfigurationEx())
    instruction_set = verify(conf.GetInstructionSet())
    possibilities = [e.value for e in _pybraw._BlackmagicRawInstructionSet.__members__.values()]
    assert instruction_set in possibilities


def test_PreparePipeline(codec):
    class PreparePipelineCompleteCallback(_pybraw.BlackmagicRawCallback):
        def PreparePipelineComplete(self, user_data, result):
            self.user_data = user_data
            self.result = result

    callback = PreparePipelineCompleteCallback()
    verify(codec.SetCallback(callback))
    user_data = object()
    verify(codec.PreparePipeline(_pybraw.blackmagicRawPipelineCPU, None, None, user_data))
    assert sys.getrefcount(user_data) == 3

    verify(codec.FlushJobs())
    assert callback.user_data == user_data
    assert ResultCode.is_success(callback.result)

    del callback
    assert sys.getrefcount(user_data) == 2
