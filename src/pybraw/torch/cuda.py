import ctypes

from pybraw import _pybraw


CUDA_LIBRARY_NAMES = ['libcuda.so', 'libcuda.dylib', 'nvcuda.dll']

for name in CUDA_LIBRARY_NAMES:
    try:
        cuda_lib = ctypes.CDLL(name)
        break
    except OSError:
        pass
else:
    cuda_lib = None


def get_current_cuda_context():
    if cuda_lib is None:
        raise RuntimeError('Failed to load the CUDA library.')
    context = ctypes.c_void_p()
    assert cuda_lib.cuCtxGetCurrent(ctypes.byref(context)) == 0
    if not context:
        return None
    return _pybraw.PointerCTypesToPyBind(context)
