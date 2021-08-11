from pybraw import _pybraw


def checked_result(return_values, expected_result=_pybraw.S_OK):
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
    assert result == expected_result
    return unwrapped
