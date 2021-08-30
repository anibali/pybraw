from pybraw import _pybraw, verify


def test_GetClipProcessingAttributeRange(constants):
    value_min, value_max, is_readonly = verify(constants.GetClipProcessingAttributeRange(
        'Blackmagic Pocket Cinema Camera 4K',
        _pybraw.blackmagicRawClipProcessingAttributeToneCurveWhiteLevel,
    ))
    assert value_min.to_py() == 0.0
    assert value_max.to_py() == 2.0
    assert is_readonly is False


def test_GetClipProcessingAttributeList(constants):
    array, array_len, is_readonly = verify(constants.GetClipProcessingAttributeList(
        'Blackmagic Pocket Cinema Camera 4K',
        _pybraw.blackmagicRawClipProcessingAttributeColorScienceGen,
    ))
    actual = [value.to_py() for value in array]
    expected = [4, 5]
    assert array_len == len(expected)
    assert actual == expected
    assert is_readonly is False


def test_GetFrameProcessingAttributeRange(constants):
    value_min, value_max, is_readonly = verify(constants.GetFrameProcessingAttributeRange(
        'Blackmagic Pocket Cinema Camera 4K',
        _pybraw.blackmagicRawFrameProcessingAttributeWhiteBalanceKelvin,
    ))
    assert value_min.to_py() == 2000
    assert value_max.to_py() == 50000
    assert is_readonly is False


def test_GetFrameProcessingAttributeList(constants):
    array, array_len, is_readonly = verify(constants.GetFrameProcessingAttributeList(
        'Blackmagic Pocket Cinema Camera 4K',
        _pybraw.blackmagicRawFrameProcessingAttributeISO,
    ))
    actual = [value.to_py() for value in array]
    expected = [100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000]
    assert array_len == len(expected)
    assert actual == expected
    assert is_readonly is False


def test_GetISOListForAnalogGain(constants):
    array, array_len, is_readonly = verify(constants.GetISOListForAnalogGain(
        'Blackmagic Pocket Cinema Camera 4K',
        1.0,
        False,
    ))
    expected = [100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000]
    assert array_len == len(expected)
    assert array == expected
    assert is_readonly is True
