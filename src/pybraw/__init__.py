from typing import Union

from .constants import *


def verify(return_values: Union[int, tuple]):
    """Strip the result code from a library call, asserting success.

    Args:
        return_values: Values returned from the library function call. It is expected that the
            first return value is a HRESULT.

    Returns:
        The input return values excluding the result code.
        If return_values only contains the result code, `None` is returned.
        If return_values contains the result code and one other value, the other value is returned.
        If return_values contains the result code and multiple other values, the other values
        are returned as a tuple.
    """
    if isinstance(return_values, int):
        result = return_values
        unwrapped = None
    else:
        result = return_values[0]
        if len(return_values) == 1:
            unwrapped = None
        elif len(return_values) == 2:
            unwrapped = return_values[1]
        else:
            unwrapped = return_values[1:]
    result = ResultCode(result)
    assert result.is_success(), f'unsuccessful result code: {ResultCode(result).to_hex()} ({ResultCode(result).name})'
    return unwrapped
