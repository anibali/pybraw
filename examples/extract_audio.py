"""Extract the audio from a BRAW video file and write it to an uncompressed WAVE file.
"""

import argparse
import sys

from pybraw import _pybraw, verify


def argument_parser():
    parser = argparse.ArgumentParser('Extract the audio from a BRAW video.')
    parser.add_argument('--input', type=str, required=True,
                        help='input BRAW video file')
    parser.add_argument('--output', type=str, required=True,
                        help='output WAVE file')
    return parser


def main(args):
    opts = argument_parser().parse_args(args)

    factory = _pybraw.CreateBlackmagicRawFactoryInstance()
    codec = verify(factory.CreateCodec())
    clip = verify(codec.OpenClip(opts.input))
    audio: _pybraw.IBlackmagicRawClipAudio = verify(clip.as_IBlackmagicRawClipAudio())

    audio_format = verify(audio.GetAudioFormat())
    assert audio_format == _pybraw.blackmagicRawAudioFormatPCMLittleEndian

    num_samples = verify(audio.GetAudioSampleCount())
    num_channels = verify(audio.GetAudioChannelCount())
    bits_per_sample = verify(audio.GetAudioBitDepth())
    sample_rate = verify(audio.GetAudioSampleRate())

    bytes_per_sample = bits_per_sample // 8
    byte_rate = sample_rate * num_channels * bytes_per_sample
    subchunk_1_size = 16
    subchunk_2_size = num_samples * num_channels * bytes_per_sample

    # Documentation for the WAVE PCM soundfile format can be found here:
    # http://soundfile.sapp.org/doc/WaveFormat/
    with open(opts.output, 'wb') as f:
        # RIFF header
        f.write(b'RIFF')
        f.write((4 + (8 + subchunk_1_size) + (8 + subchunk_2_size)).to_bytes(4, 'little'))
        f.write(b'WAVE')

        # "fmt " subchunk
        f.write(b'fmt ')
        f.write(subchunk_1_size.to_bytes(4, 'little'))
        f.write((1).to_bytes(2, 'little'))
        f.write(num_channels.to_bytes(2, 'little'))
        f.write(sample_rate.to_bytes(4, 'little'))
        f.write(byte_rate.to_bytes(4, 'little'))
        f.write((num_channels * bytes_per_sample).to_bytes(2, 'little'))
        f.write(bits_per_sample.to_bytes(2, 'little'))

        # "data" subchunk
        f.write(b'data')
        f.write(subchunk_2_size.to_bytes(4, 'little'))
        buffer = bytearray(4096)
        samples_per_buffer = len(buffer) // (bytes_per_sample * num_channels)
        for sample_frame_index in range(0, num_samples, samples_per_buffer):
            _, bytes_read = verify(audio.GetAudioSamples(sample_frame_index, buffer, samples_per_buffer))
            f.write(buffer[:bytes_read])


if __name__ == '__main__':
    main(sys.argv[1:])
