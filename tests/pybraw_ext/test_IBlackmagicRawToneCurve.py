import numpy as np
import pytest

from pybraw import verify


def test_GetToneCurve(tone_curve):
    camera_type = 'Blackmagic Pocket Cinema Camera 4K'
    gamma = 'Blackmagic Design Film'
    gen = 4
    actual = verify(tone_curve.GetToneCurve(camera_type, gamma, gen))
    expected = (1.0, 1.0, pytest.approx(0.3835616409778595, abs=1e-6), 1.0, 1.0, 0.0, 1.0, 0)
    assert actual == expected


def test_EvaluateToneCurve(tone_curve):
    camera_type = 'Blackmagic Pocket Cinema Camera 4K'
    gen = 4
    tone_curve_params = (1.0, 1.0, 0.3835616409778595, 1.0, 1.0, 0.2, 1.0, 0)
    array = np.ndarray((32,), float)
    verify(tone_curve.EvaluateToneCurve(camera_type, gen, *tone_curve_params, array))
