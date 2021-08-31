from pybraw import _pybraw, verify


def test_GetAudioFormat(audio):
    format = verify(audio.GetAudioFormat())
    assert format == _pybraw.blackmagicRawAudioFormatPCMLittleEndian


def test_GetAudioBitDepth(audio):
    bit_depth = verify(audio.GetAudioBitDepth())
    assert bit_depth == 24


def test_GetAudioChannelCount(audio):
    channel_count = verify(audio.GetAudioChannelCount())
    assert channel_count == 2


def test_GetAudioSampleRate(audio):
    sample_rate = verify(audio.GetAudioSampleRate())
    assert sample_rate == 48000


def test_GetAudioSampleCount(audio):
    sample_count = verify(audio.GetAudioSampleCount())
    assert sample_count == 802560


def test_GetAudioSamples(audio):
    buffer = bytearray(1024)
    samples_read, bytes_read = verify(audio.GetAudioSamples(802560 - 4, buffer, 4))
    assert samples_read == 4
    assert bytes_read == 4 * 2 * 3
    expected = bytearray(b"f&\x00\xb7%\x00X\xfa\xff&$\x00\x1a\xf4\xff\xf2-\x00c\xc8\xff\x80\'\x00")
    assert buffer[:bytes_read] == expected
