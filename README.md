# PyBraw

Python bindings for the Blackmagic RAW SDK.

# Setup

Set up and activate a Conda environment with all required dependencies:

```shell
conda env create -f environment.yml
conda activate pybraw
```

Download the [Blackmagic RAW 2.1 SDK for Linux](https://www.blackmagicdesign.com/support/download/ea11ce9660c642879612f363ca387c7f/Linux)
and copy `libBlackmagicRawAPI.so` into your Conda environment's library path. For example:

```
sudo cp libBlackmagicRawAPI.so "$CONDA_PREFIX/lib/"
```

You may also need other libraries depending on your requirements. For example, `libDecoderCUDA.so`
is required for CUDA GPU decoding.

Install the pybraw package:

```shell
pip install --no-build-isolation -U .
```

## PyTorch integration

The `pybraw.torch` module contains a simple interface for reading image frames directly into
PyTorch tensors. This works for both CPU and CUDA tensors. Here's some sample code:

```python
from pybraw import PixelFormat, ResolutionScale
from pybraw.torch.reader import FrameImageReader

file_name = 'tests/data/Filmplusgear-skiers-Samnaun-2019-dci-Q5.braw'
# Create a helper for reading video image frames.
reader = FrameImageReader(file_name, processing_device='cuda')
# Get the number of frames in the video.
frame_count = reader.frame_count()

# Prepare the reader.
with reader.run_flow(PixelFormat.RGB_F32_Planar, max_running_tasks=3) as task_manager:
    # Enqueue all tasks. A task specifies a frame index to read, along with other options
    # like the resolution to read it at, the crop region, and the output size.
    tasks = [task_manager.enqueue_task(frame_index, resolution_scale=ResolutionScale.Quarter)
             for frame_index in range(frame_count)]

    for task in tasks:
        # Wait for the task to finish, get its result, and mark it as completed so the next
        # queued task can begin.
        image_tensor = task.consume()
        # Output image info.
        shape = tuple(image_tensor.shape)
        pixel_mean = ','.join([f'{x:.4f}' for x in image_tensor.mean([1, 2])])
        print(f'[Frame {task.frame_index:3d}] shape={shape} pixel_mean={pixel_mean}')
```

Note that PyTorch is _not_ a hard dependency for this project. If you don't import `pybraw.torch`,
you don't need to have PyTorch installed.

## Low-level bindings

Low-level bindings are available in the `pybraw._pybraw` module. These bindings adhere to the
original Blackmagic RAW SDK API closely. Please refer to
[the official developer manual](https://documents.blackmagicdesign.com/DeveloperManuals/BlackmagicRAW-SDK.pdf)
for more details.

You can use the low-level bindings to do things like read video frames into NumPy arrays with
fine-grained control over the Blackmagic RAW SDK functionality. You can take a look at
`examples/extract_frame.py` for a basic example showing how to read frames as PIL images,
or `examples/manual_flow_cpu.py` and `examples/manual_flow_gpu.py` for more complex manual decoder
flow examples.

## Tests

In order to run the tests you will need to download the sample BRAW file from
https://filmplusgear.com/blackmagic-raw-testfile-3 and move it to
`tests/data/Filmplusgear-skiers-Samnaun-2019-dci-Q5.braw`.

Run the tests using PyTest:

```shell
pytest tests
```
