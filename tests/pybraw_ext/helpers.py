from pybraw import _pybraw


def checked_result(return_values, expected_result=_pybraw.S_OK):
    assert return_values[0] == expected_result
    if len(return_values[1:]) == 1:
        return return_values[1]
    return return_values[1:]
