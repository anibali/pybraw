from pybraw import _pybraw, verify


class SimpleCallback(_pybraw.BlackmagicRawCallback):
    def ReadComplete(self, job, result, frame):
        process_job = verify(frame.CreateJobDecodeAndProcessFrame())
        process_job.Submit()
        process_job.Release()


class ProxyResourceManager(_pybraw.BlackmagicRawResourceManager):
    def __init__(self, resource_manager):
        super().__init__()
        self._rm = resource_manager
        self.n_created = 0
        self.n_released = 0

    def CreateResource(self, context, command_queue, size_bytes, type, usage):
        self.n_created += 1
        result, resource = self._rm.CreateResource(context, command_queue, size_bytes, type, usage)
        return result, resource

    def ReleaseResource(self, context, command_queue, resource, type):
        self.n_released += 1
        result = self._rm.ReleaseResource(context, command_queue, resource, type)
        return result

    def CopyResource(self, context, command_queue, source, source_type, destination,
                     destination_type, size_bytes, copy_async):
        result = self._rm.CopyResource(context, command_queue, source, source_type, destination,
                     destination_type, size_bytes, copy_async)
        return result

    def GetResourceHostPointer(self, context, command_queue, resource, resource_type):
        return self._rm.GetResourceHostPointer(context, command_queue, resource, resource_type)


def test_subclass_proxy(codec, sample_filename):
    configuration_ex = verify(codec.as_IBlackmagicRawConfigurationEx())
    resource_manager = verify(configuration_ex.GetResourceManager())
    proxy = ProxyResourceManager(resource_manager)
    verify(configuration_ex.SetResourceManager(proxy))

    callback = SimpleCallback()
    verify(codec.SetCallback(callback))

    clip = verify(codec.OpenClip(sample_filename))
    read_job = verify(clip.CreateJobReadFrame(0))
    verify(read_job.Submit())
    read_job.Release()
    verify(codec.FlushJobs())

    assert proxy.n_created == 2
    assert proxy.n_released == 2
