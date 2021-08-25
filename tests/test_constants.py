from pybraw.constants import ResultCode, PixelFormat, ResolutionScale


class TestResultCode:
    def test_is_success(self):
        assert ResultCode.S_OK.is_success() == True
        assert ResultCode.S_FALSE.is_success() == True
        assert ResultCode.E_POINTER.is_success() == False


class TestPixelFormat:
    def test_channels(self):
        assert PixelFormat.RGBA_U8_Packed.channels() == 'RGBA'
        assert PixelFormat.BGRA_F32_Packed.channels() == 'BGRA'
        assert PixelFormat.RGB_F32_Planar.channels() == 'RGB'


class TestResolutionScale:
    def test_factor(self):
        assert ResolutionScale.Full.factor() == 1
        assert ResolutionScale.Eighth_Flipped.factor() == 8
