import numpy as np
import pytest
from numpy.testing import assert_allclose
from pybraw import _pybraw


def test_VariantCreateString():
    data = 'Test string'
    value = _pybraw.VariantCreateString(data)
    assert value.to_py() == data


@pytest.mark.parametrize('array', [
    np.asarray([1.1, 2.2, 3.3, 4.4], dtype=np.float32),
    np.asarray([10, 20, 30], dtype=np.uint32),
    np.asarray([-5, 5], dtype=np.int16),
])
def test_VariantCreateSafeArray(array):
    value = _pybraw.VariantCreateSafeArray(array)
    assert_allclose(value.to_py(), array)
