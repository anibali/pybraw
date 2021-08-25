import argparse
import logging
import sys
from time import perf_counter

import torch

from pybraw import PixelFormat, ResolutionScale
from pybraw.logger import log
from pybraw.torch.reader import FrameImageReader


def argument_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=str, required=True,
                        help='input BRAW video file')
    parser.add_argument('--device', type=str, default='cuda',
                        help='processing device')
    parser.add_argument('--show', action='store_true', default=False,
                        help='show images in a GUI window')
    return parser


def _select_resolution_scale(frame_width, frame_height, crop, out_size) -> ResolutionScale:
    if crop is None:
        in_w, in_h = frame_width, frame_height
    else:
        in_w, in_h = crop[2:]
    if out_size is None:
        out_w, out_h = in_w, in_h
    else:
        out_w, out_h = out_size
    min_factor = min(in_w / out_w, in_h / out_h)
    if min_factor >= 8:
        return ResolutionScale.Eighth
    elif min_factor >= 4:
        return ResolutionScale.Quarter
    elif min_factor >= 2:
        return ResolutionScale.Half
    return ResolutionScale.Full


def main(args):
    opts = argument_parser().parse_args(args)
    log.setLevel(logging.DEBUG)

    if opts.show:
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            log.critical('The --show option requires Matplotlib, but it could not be imported')
            return

    reader = FrameImageReader(opts.input, processing_device=opts.device)
    frame_count = reader.frame_count()
    frame_width = reader.frame_width()
    frame_height = reader.frame_height()
    frame_time = 1 / reader.frame_rate()

    crop_w = 800
    crop_h = 800
    max_crop_x = frame_width - crop_w
    max_crop_y = frame_height - crop_h
    out_size = (100, 100)

    if opts.show:
        fig, ax = plt.subplots(1, 1)
        plt.show(block=False)
        im = None

    with reader.run_flow(PixelFormat.RGB_F32_Planar, max_running_tasks=3) as task_manager:
        tasks = []
        for frame_index in range(frame_count):
            # Move the crop region in a diagonal bouncing pattern, just like the way the logo moves
            # around in the classic DVD player screensavers.
            crop_x = max_crop_x - abs((frame_index * 20) % (2 * max_crop_x) - max_crop_x)
            crop_y = max_crop_y - abs((frame_index * 20) % (2 * max_crop_y) - max_crop_y)
            crop = (crop_x, crop_y, crop_w, crop_h)
            # Select the smallest possible read resolution scale which is larger than what we need.
            resolution_scale = _select_resolution_scale(frame_width, frame_height, crop, out_size)
            # Add the frame read, crop, and resize task to the processing queue.
            tasks.append(task_manager.enqueue_task(frame_index, resolution_scale=resolution_scale, crop=crop, out_size=out_size))

        prev_time = 0
        avg_fps = 0
        for task in tasks:
            # Wait for the task to finish, get its result, and mark it as completed so the next
            # queued task can begin.
            image_tensor = task.consume()

            log.info(f'Mean pixel value for frame {task.frame_index}: {image_tensor.mean([1, 2]).tolist()}')
            if opts.show:
                # Cap the FPS such that the display rate does not exceed the video frame rate.
                plt.pause(max(frame_time - (perf_counter() - prev_time), 0.001))
            cur_time = perf_counter()
            dt = cur_time - prev_time
            prev_time = cur_time
            cur_fps = 1 / dt
            if task.frame_index <= 2:
                avg_fps = cur_fps
            else:
                avg_fps = 0.9 * avg_fps + 0.1 * cur_fps
            log.debug(f'Average FPS: {avg_fps:.2f}')
            if opts.show:
                ax.set_title(f'Frame {task.frame_index:6d} ({avg_fps:.2f} FPS)')
                if im is None:
                    im = ax.imshow(image_tensor.mul(255).permute(1, 2, 0).to(dtype=torch.uint8, device='cpu'))
                else:
                    im.set_data(image_tensor.mul(255).permute(1, 2, 0).to(dtype=torch.uint8, device='cpu'))
                if not plt.fignum_exists(fig.number):
                    break


if __name__ == '__main__':
    main(sys.argv[1:])
