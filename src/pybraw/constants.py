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
