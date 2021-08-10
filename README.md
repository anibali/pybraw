# PyBraw

Python bindings for the Blackmagic RAW SDK.

# Setup

Install the following:

```shell
sudo apt install libc++1 libc++abi1
```

Download the [Blackmagic RAW 2.1 SDK](https://www.blackmagicdesign.com/au/developer/product/camera)
and copy `libBlackmagicRawAPI.so` into your system library path. For example:

```
sudo cp libBlackmagicRawAPI.so /usr/lib/
```

Set up and activate a Conda environment with all required Python packages:

```shell
conda env create -f environment.yml
conda activate pybraw
```

Install the pybraw package:

```shell
pip install -U .
```

## Tests

In order to run the tests you will need to download the sample BRAW file from
https://filmplusgear.com/blackmagic-raw-testfile-3 and move it to
`tests/data/Filmplusgear-skiers-Samnaun-2019-dci-Q5.braw`.

Run the tests using PyTest:

```shell
pytest tests
```
