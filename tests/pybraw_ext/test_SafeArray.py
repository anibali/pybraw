import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw


@pytest.mark.parametrize('array', [
    np.asarray([1.1, 2.2, 3.3, 4.4], dtype=np.float32),
    np.asarray([10, 20, 30], dtype=np.uint32),
    np.asarray([-5, 5], dtype=np.int16),
])
def test_SafeArrayCreateFromNumpy(array):
    value = _pybraw.SafeArrayCreateFromNumpy(array)
    actual = value.to_py()
    del value
    assert_allclose(actual, array)
