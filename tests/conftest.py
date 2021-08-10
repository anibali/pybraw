import os.path

import pytest


@pytest.fixture
def test_data_dir():
    return os.path.normpath(os.path.join(__file__, '..', 'data'))


@pytest.fixture
def sample_filename(test_data_dir):
    # Sample file is from: https://filmplusgear.com/blackmagic-raw-testfile-3
    return os.path.join(test_data_dir, 'Filmplusgear-skiers-Samnaun-2019-dci-Q5.braw')
