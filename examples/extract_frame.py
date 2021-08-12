import argparse
import sys

from PIL import Image
from pybraw import _pybraw


def argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=str, required=True,
                        help='input BRAW video file')
    parser.add_argument('--frame', type=int, default=0,
                        help='frame index (0-based)')
    parser.add_argument('--output', type=str, required=True,
                        help='output image file')
    return parser


def checked_result(return_values, expected_result=_pybraw.S_OK):
    assert return_values[0] == expected_result
    if len(return_values[1:]) == 1:
        return return_values[1]
    return return_values[1:]


class MyCallback(_pybraw.BlackmagicRawCallback):
    def ReadComplete(self, job, result, frame):
        frame.AddRef()
        self.frame = frame

    def ProcessComplete(self, job, result, processed_image):
        processed_image.AddRef()
        self.processed_image = processed_image


def main(args):
    opts = argument_parser().parse_args(args)

    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    codec = checked_result(factory.CreateCodec())
    clip = checked_result(codec.OpenClip(opts.input))

    frame_count = checked_result(clip.GetFrameCount())

    if opts.frame < 0 or opts.frame >= frame_count:
        raise ValueError(f'Frame out of range')

    callback = MyCallback()
    callback.AddRef()
    codec.SetCallback(callback)
    callback.Release()

    read_job = checked_result(clip.CreateJobReadFrame(opts.frame))
    read_job.Submit()
    read_job.Release()
    codec.FlushJobs()

    callback.frame.SetResourceFormat(_pybraw.blackmagicRawResourceFormatRGBAU8)
    process_job = checked_result(callback.frame.CreateJobDecodeAndProcessFrame())
    callback.frame.Release()
    del callback.frame
    process_job.Submit()
    process_job.Release()
    codec.FlushJobs()

    resource_type = checked_result(callback.processed_image.GetResourceType())
    assert resource_type == _pybraw.blackmagicRawResourceTypeBufferCPU
    np_image = callback.processed_image.numpy()
    callback.processed_image.Release()
    del callback.processed_image

    pil_image = Image.fromarray(np_image[..., :3])
    pil_image.save(opts.output)

    clip.Release()
    codec.Release()
    factory.Release()


if __name__ == '__main__':
    main(sys.argv[1:])
