from pybraw import _pybraw
from enum import IntEnum


class ResultCode(IntEnum):
    """An enum representing HRESULT values.
    """
    S_OK = _pybraw.S_OK
    S_FALSE = _pybraw.S_FALSE
    E_UNEXPECTED = _pybraw.E_UNEXPECTED
    E_NOTIMPL = _pybraw.E_NOTIMPL
    E_OUTOFMEMORY = _pybraw.E_OUTOFMEMORY
    E_INVALIDARG = _pybraw.E_INVALIDARG
    E_NOINTERFACE = _pybraw.E_NOINTERFACE
    E_POINTER = _pybraw.E_POINTER
    E_HANDLE = _pybraw.E_HANDLE
    E_ABORT = _pybraw.E_ABORT
    E_FAIL = _pybraw.E_FAIL
    E_ACCESSDENIED = _pybraw.E_ACCESSDENIED

    def to_hex(self):
        """Convert the result code into a hex number.
        """
        return f'0x{self & ((1 << 32) - 1):08X}'

    def is_success(self):
        """Return whether the result code indicates success.

        Returns:
            `True` when the result code indicates success, `False` otherwise.
        """
        return (self & (1 << 31)) == 0

    @classmethod
    def to_string(cls, result):
        try:
            return cls(result).name
        except:
            return 'UNKNOWN'


class PixelFormat(IntEnum):
    """An enum representing the pixel format of an image resource.
    """
    RGBA_U8_Packed = _pybraw.blackmagicRawResourceFormatRGBAU8
    BGRA_U8_Packed = _pybraw.blackmagicRawResourceFormatBGRAU8
    RGB_U16_Packed = _pybraw.blackmagicRawResourceFormatRGBU16
    RGBA_U16_Packed = _pybraw.blackmagicRawResourceFormatRGBU16
    BGRA_U16_Packed = _pybraw.blackmagicRawResourceFormatBGRAU16
    RGB_U16_Planar = _pybraw.blackmagicRawResourceFormatRGBU16Planar
    RGB_F32_Packed = _pybraw.blackmagicRawResourceFormatRGBF32
    RGB_F32_Planar = _pybraw.blackmagicRawResourceFormatRGBF32Planar
    BGRA_F32_Packed = _pybraw.blackmagicRawResourceFormatBGRAF32

    def channels(self):
        parts = self.name.split('_')
        assert parts[0] in {'RGBA', 'BGRA', 'RGB'}
        return parts[0]

    def data_type(self):
        parts = self.name.split('_')
        assert parts[1] in {'U8', 'U16', 'F32'}
        return parts[1]

    def is_planar(self):
        parts = self.name.split('_')
        assert parts[2] in {'Packed', 'Planar'}
        return parts[2] == 'Planar'


class ResolutionScale(IntEnum):
    """An enum representing different resolution scaling factors.
    """
    Full = _pybraw.blackmagicRawResolutionScaleFull
    Half = _pybraw.blackmagicRawResolutionScaleHalf
    Quarter = _pybraw.blackmagicRawResolutionScaleQuarter
    Eighth = _pybraw.blackmagicRawResolutionScaleEighth
    Full_Flipped = _pybraw.blackmagicRawResolutionScaleFullUpsideDown
    Half_Flipped = _pybraw.blackmagicRawResolutionScaleHalfUpsideDown
    Quarter_Flipped = _pybraw.blackmagicRawResolutionScaleQuarterUpsideDown
    Eighth_Flipped = _pybraw.blackmagicRawResolutionScaleEighthUpsideDown

    def factor(self):
        parts = self.name.split('_')
        lookup = {'Full': 1, 'Half': 2, 'Quarter': 4, 'Eighth': 8}
        assert parts[0] in lookup
        return lookup[parts[0]]

    def is_flipped(self):
        parts = self.name.split('_')
        if len(parts) == 1:
            return False
        assert parts[1] == 'Flipped'
        return True
