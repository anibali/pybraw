import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw


def checked_result(return_values, expected_result=_pybraw.S_OK):
    assert return_values[0] == expected_result
    if len(return_values[1:]) == 1:
        return return_values[1]
    return return_values[1:]


def test_CreateBlackmagicRawFactoryInstance():
    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    assert factory
    factory.Release()


class TestIBlackmagicRawPipelineIterator:
    @pytest.fixture
    def factory(self):
        factory = _pybraw.CreateBlackmagicRawFactoryInstance()
        yield factory
        factory.Release()

    @pytest.fixture
    def pipeline_iterator(self, factory):
        pipeline_iterator = checked_result(factory.CreatePipelineIterator(_pybraw.blackmagicRawInteropNone))
        yield pipeline_iterator
        pipeline_iterator.Release()

    def test_GetName(self, pipeline_iterator):
        pipeline_names = []
        while True:
            result, pipeline_name = pipeline_iterator.GetName()
            if result == _pybraw.E_FAIL:
                break
            assert result == _pybraw.S_OK
            pipeline_names.append(pipeline_name)
            pipeline_iterator.Next()
        assert 'CPU' in pipeline_names

    def test_GetInterop(self, pipeline_iterator):
        interop = checked_result(pipeline_iterator.GetInterop())
        assert interop == _pybraw.blackmagicRawInteropNone

    def test_GetPipeline(self, pipeline_iterator):
        pipelines = []
        while True:
            result, pipeline = pipeline_iterator.GetPipeline()
            if result == _pybraw.E_FAIL:
                break
            assert result == _pybraw.S_OK
            pipelines.append(pipeline)
            pipeline_iterator.Next()
        assert _pybraw.blackmagicRawPipelineCPU in pipelines


class TestIBlackmagicRawPipelineDeviceIterator:
    @pytest.fixture
    def factory(self):
        factory = _pybraw.CreateBlackmagicRawFactoryInstance()
        yield factory
        factory.Release()

    @pytest.fixture
    def device_iterator(self, factory):
        device_iterator = checked_result(factory.CreatePipelineDeviceIterator(_pybraw.blackmagicRawPipelineCPU, _pybraw.blackmagicRawInteropNone))
        yield device_iterator
        device_iterator.Release()

    def test_GetPipeline(self, device_iterator):
        pipeline = checked_result(device_iterator.GetPipeline())
        assert pipeline == _pybraw.blackmagicRawPipelineCPU

    def test_GetInterop(self, device_iterator):
        interop = checked_result(device_iterator.GetInterop())
        assert interop == _pybraw.blackmagicRawInteropNone

    def test_CreateDevice(self, device_iterator):
        pipeline_device = checked_result(device_iterator.CreateDevice())
        assert pipeline_device
        pipeline_name = checked_result(pipeline_device.GetPipelineName())
        assert pipeline_name == 'CPU'
        pipeline_device.Release()


class TestIBlackmagicRawClip:
    @pytest.fixture
    def factory(self):
        factory = _pybraw.CreateBlackmagicRawFactoryInstance()
        yield factory
        factory.Release()

    @pytest.fixture
    def codec(self, factory):
        codec = checked_result(factory.CreateCodec())
        yield codec
        codec.Release()

    @pytest.fixture
    def clip(self, codec, sample_filename):
        clip = checked_result(codec.OpenClip(sample_filename))
        yield clip
        clip.Release()

    def test_GetWidth(self, clip):
        width = checked_result(clip.GetWidth())
        assert width == 4096

    def test_GetHeight(self, clip):
        height = checked_result(clip.GetHeight())
        assert height == 2160

    def test_GetFrameRate(self, clip):
        frame_rate = checked_result(clip.GetFrameRate())
        assert frame_rate == 25.0

    def test_GetFrameCount(self, clip):
        frame_count = checked_result(clip.GetFrameCount())
        assert frame_count == 418

    def test_GetTimecodeForFrame(self, clip):
        timecode = checked_result(clip.GetTimecodeForFrame(0))
        assert timecode == '14:39:00:23'

    def test_GetMetadataIterator_lowlevel(self, clip):
        iterator = checked_result(clip.GetMetadataIterator())
        metadata = {}
        while True:
            result, key = iterator.GetKey()
            if result == _pybraw.E_FAIL:
                break
            assert result == _pybraw.S_OK
            data = checked_result(iterator.GetData())
            metadata[key] = data
            result = iterator.Next()
            assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
        assert 'firmware_version' in metadata
        assert metadata['firmware_version'].vt == _pybraw.blackmagicRawVariantTypeString
        assert metadata['firmware_version'].bstrVal == '6.2'
        assert metadata['crop_origin'].vt == _pybraw.blackmagicRawVariantTypeSafeArray
        assert_allclose(metadata['crop_origin'].parray.numpy(), np.array([16.0, 8.0]))
        for value in metadata.values():
            _pybraw.VariantClear(value)
        iterator.Release()

    def test_GetMetadataIterator_midlevel(self, clip):
        iterator = checked_result(clip.GetMetadataIterator())
        metadata = {}
        while True:
            result, key = iterator.GetKey()
            if result == _pybraw.E_FAIL:
                break
            assert result == _pybraw.S_OK
            data = checked_result(iterator.GetData())
            metadata[key] = data.to_py()
            # We expect the data to be copied when calling to_py(), so it should be safe to clear
            # the variant here.
            _pybraw.VariantClear(data)
            result = iterator.Next()
            assert result in {_pybraw.S_OK, _pybraw.S_FALSE}
        iterator.Release()
        assert 'firmware_version' in metadata
        assert metadata['firmware_version'] == '6.2'
        assert_allclose(metadata['crop_origin'], np.array([16.0, 8.0]))

    def test_read_frame(self, codec, clip):
        class MyCallback(_pybraw.BlackmagicRawCallback):
            def ReadComplete(self, job, result, frame):
                frame.AddRef()
                self.frame = frame

            def ProcessComplete(self, job, result, processed_image):
                processed_image.AddRef()
                self.processed_image = processed_image

        # NOTE: SetCallback _must_ be called before CreateJobReadFrame
        callback = MyCallback()
        callback.AddRef()
        codec.SetCallback(callback)
        callback.Release()
        read_job = checked_result(clip.CreateJobReadFrame(12))
        read_job.Submit()
        read_job.Release()
        codec.FlushJobs()
        process_job = checked_result(callback.frame.CreateJobDecodeAndProcessFrame())
        callback.frame.Release()
        del callback.frame
        process_job.Submit()
        process_job.Release()
        codec.FlushJobs()
        width = checked_result(callback.processed_image.GetWidth())
        assert width == 4096
        height = checked_result(callback.processed_image.GetHeight())
        assert height == 2160
        resource_type = checked_result(callback.processed_image.GetResourceType())
        assert resource_type == _pybraw.blackmagicRawResourceTypeBufferCPU
        resource_format = checked_result(callback.processed_image.GetResourceFormat())
        assert resource_format == _pybraw.blackmagicRawResourceFormatRGBAU8
        resource = checked_result(callback.processed_image.GetResource())
        np_image = resource.reshape(height, width, 4)
        assert_allclose(np_image[100, 200], np.array([126, 131, 129, 255]))
        # import matplotlib.pyplot as plt
        # plt.imshow(np_image)
        # plt.show()
        callback.processed_image.Release()
        del callback.processed_image


class TestBlackmagicRawVariantType:
    def test_blackmagicRawVariantTypeU32(self):
        assert _pybraw.blackmagicRawVariantTypeU32 == 5
