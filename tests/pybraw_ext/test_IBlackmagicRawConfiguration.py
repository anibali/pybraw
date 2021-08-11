from .helpers import checked_result


def test_GetCPUThreads(configuration):
    thread_count = checked_result(configuration.GetCPUThreads())
    assert thread_count > 0


def test_GetMaxCPUThreadCount(configuration):
    thread_count = checked_result(configuration.GetMaxCPUThreadCount())
    assert thread_count > 0


def test_SetCPUThreads(configuration):
    checked_result(configuration.SetCPUThreads(1))
    thread_count = checked_result(configuration.GetCPUThreads())
    assert thread_count == 1


def test_SetFromDevice(configuration, cpu_device):
    checked_result(configuration.SetFromDevice(cpu_device))
    # TODO: Use GetPipeline to check pipelines match.
