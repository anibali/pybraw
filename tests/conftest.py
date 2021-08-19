import os.path

import pytest


@pytest.fixture
def test_data_dir():
    return os.path.normpath(os.path.join(__file__, '..', 'data'))


@pytest.fixture
def sample_filename(test_data_dir):
    # Sample file is from: https://filmplusgear.com/blackmagic-raw-testfile-3
    return os.path.join(test_data_dir, 'Filmplusgear-skiers-Samnaun-2019-dci-Q5.braw')


@pytest.fixture
def bw_sidecar_filename(test_data_dir):
    # Sample file is from: https://filmplusgear.com/blackmagic-raw-testfile-3
    return os.path.join(test_data_dir, 'bw.sidecar')


@pytest.fixture
def bw_filename(tmp_path, test_data_dir, sample_filename, bw_sidecar_filename):
    filename = tmp_path.joinpath('bw.braw')
    filename.symlink_to(sample_filename)
    tmp_path.joinpath('bw.sidecar').symlink_to(bw_sidecar_filename)
    return str(filename)
