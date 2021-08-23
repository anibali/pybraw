import gc
from contextlib import contextmanager

from pybraw import _pybraw


@contextmanager
def releases_last_reference(obj: _pybraw.IUnknown):
    """Checks whether deleting an IUnknown object releases the final reference.
    """
    # Create a weak reference to `obj` that allows us to call methods on the underlying object even
    # after the original Python wrapper has been garbage collected.
    obj_weakref = _pybraw._IUnknownWeakref(obj)
    # Delete our reference to the original Python wrapper. We expect at this point that the caller
    # holds the only reference to the original Python wrapper.
    del obj
    # Add a reference to the underlying object. There should now be two references: the one held
    # by the original Python wrapper, and the one we just added.
    refs_before = obj_weakref.AddRef()
    assert refs_before == 2, 'not the last reference'
    # Yield so that the caller can delete the final reference to the original Python wrapper, thus
    # releasing one of the references to the underlying object.
    yield
    gc.collect()
    # Release the reference to the underlying object we previously added with AddRef. This is
    # expected to be the last reference.
    refs_after = obj_weakref.Release()
    assert refs_after == 0, 'last reference not released'
