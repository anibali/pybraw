import pytest
import torch
from pytest_lazyfixture import lazy_fixture

from pybraw import PixelFormat, ResolutionScale
from pybraw.torch.reader import FrameImageReader


@pytest.fixture
def device_cpu():
    return torch.device('cpu')


@pytest.fixture
def device_cuda0():
    if torch.cuda.is_available() and 0 < torch.cuda.device_count():
        return torch.device('cuda:0')
    pytest.skip('device not available (cuda:0)')


@pytest.fixture
def device_cuda1():
    if torch.cuda.is_available() and 1 < torch.cuda.device_count():
        return torch.device('cuda:1')
    pytest.skip('device not available (cuda:1)')


@pytest.fixture
def reader_cpu(sample_filename, device_cpu):
    return FrameImageReader(sample_filename, processing_device=device_cpu)


@pytest.mark.parametrize('processing_device', [
    lazy_fixture('device_cpu'),
    lazy_fixture('device_cuda0'),
    lazy_fixture('device_cuda1'),
])
def test_read_frames(sample_filename, processing_device):
    reader = FrameImageReader(sample_filename, processing_device=processing_device)
    expected = [0.516379, 0.515850, 0.515255, 0.514853, 0.514609, 0.514260, 0.514031, 0.514378]

    with reader.run_flow(PixelFormat.RGB_F32_Planar, max_running_tasks=3) as task_manager:
        tasks = [task_manager.enqueue_task(frame_index, resolution_scale=ResolutionScale.Eighth)
                 for frame_index in range(8)]

        for i, task in enumerate(tasks):
            image_tensor = task.consume()
            assert image_tensor.device == processing_device
            assert float(image_tensor.mean()) == pytest.approx(expected[i], abs=1e-4)


def test_automatic_cancellation(reader_cpu):
    with reader_cpu.run_flow(PixelFormat.RGB_F32_Planar, max_running_tasks=3) as task_manager:
        tasks = [task_manager.enqueue_task(frame_index) for frame_index in range(100)]

    # We expect for all tasks to be either consumed or cancelled after the run_flow context manager
    # exits.
    for task in tasks:
        assert task.is_consumed() or task.is_cancelled()


def test_postprocessing_crop(reader_cpu):
    with reader_cpu.run_flow(PixelFormat.RGBA_U8_Packed, max_running_tasks=1) as task_manager:
        task = task_manager.enqueue_task(
            frame_index=0,
            resolution_scale=ResolutionScale.Quarter,
            crop=(0, 0, 800, 400),
        )
        image_tensor = task.consume()
    assert image_tensor.shape == (100, 200, 4)


@pytest.mark.parametrize('out_device', [
    lazy_fixture('device_cpu'),
    lazy_fixture('device_cuda0'),
    lazy_fixture('device_cuda1'),
])
def test_postprocessing_out_device(reader_cpu, out_device):
    with reader_cpu.run_flow(PixelFormat.RGBA_U8_Packed, max_running_tasks=1) as task_manager:
        task = task_manager.enqueue_task(
            frame_index=0,
            out_device=out_device,
        )
        image_tensor = task.consume()
    assert image_tensor.device == out_device
