from pybraw import _pybraw, verify

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
