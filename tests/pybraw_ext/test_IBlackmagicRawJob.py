import sys

from pybraw import _pybraw, verify


def test_SetUserData(codec, sample_filename):
    verify(codec.SetCallback(_pybraw.BlackmagicRawCallback()))
    clip = verify(codec.OpenClip(sample_filename))
    read_job = verify(clip.CreateJobReadFrame(12))
    user_data1 = object()
    user_data2 = object()
    verify(read_job.SetUserData(user_data1))
    assert sys.getrefcount(user_data1) == 3
    assert sys.getrefcount(user_data2) == 2
    verify(read_job.SetUserData(user_data2))
    assert sys.getrefcount(user_data1) == 2
    assert sys.getrefcount(user_data2) == 3
    verify(read_job.SetUserData(None))
    assert sys.getrefcount(user_data1) == 2
    assert sys.getrefcount(user_data2) == 2
    read_job.Release()


def test_GetUserData(codec, sample_filename):
    verify(codec.SetCallback(_pybraw.BlackmagicRawCallback()))
    clip = verify(codec.OpenClip(sample_filename))
    read_job = verify(clip.CreateJobReadFrame(12))
    user_data = object()
    verify(read_job.SetUserData(user_data))
    assert sys.getrefcount(user_data) == 3
    actual = verify(read_job.GetUserData())
    assert actual == user_data
    read_job.Release()


def test_PopUserData(codec, sample_filename):
    class UserDataCallback(_pybraw.BlackmagicRawCallback):
        def ReadComplete(self, job, result, frame):
            self.pop1 = verify(job.PopUserData())
            self.pop2 = verify(job.PopUserData())

    callback = UserDataCallback()
    verify(codec.SetCallback(callback))
    clip = verify(codec.OpenClip(sample_filename))
    read_job = verify(clip.CreateJobReadFrame(12))
    user_data = object()
    verify(read_job.SetUserData(user_data))
    assert sys.getrefcount(user_data) == 3

    verify(read_job.Submit())
    read_job.Release()
    verify(codec.FlushJobs())
    assert callback.pop1 == user_data
    assert callback.pop2 == None

    del callback
    assert sys.getrefcount(user_data) == 2
