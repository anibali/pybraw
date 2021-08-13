from pybraw import _pybraw

from .helpers import checked_result


def test_GetResourceManager(codec):
    conf = checked_result(codec.as_IBlackmagicRawConfigurationEx())
    resource_manager = checked_result(conf.GetResourceManager())
    assert isinstance(resource_manager, _pybraw.IBlackmagicRawResourceManager)


def test_GetInstructionSet(codec):
    conf = checked_result(codec.as_IBlackmagicRawConfigurationEx())
    instruction_set = checked_result(conf.GetInstructionSet())
    possibilities = [e.value for e in _pybraw._BlackmagicRawInstructionSet.__members__.values()]
    assert instruction_set in possibilities
