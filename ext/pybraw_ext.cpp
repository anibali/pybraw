#include "BlackmagicRawAPI.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include <atomic>

namespace py = pybind11;
using namespace pybind11::literals;


#define DEF_QUERY_INTERFACE(S, T)\
.def("as_"#T, [](S& self) {\
    LPVOID pv = nullptr;\
    HRESULT result = self.QueryInterface(IID_##T, &pv);\
    return std::make_tuple(result, (T*)pv);\
}, "Get the "#T" interface to this "#S)


void* UserDataCreate(py::object object) {
    // Add one new reference to the Python object.
    py::object* ref = new py::object(object);
    // Return the new reference as a void*.
    return static_cast<void*>(ref);
}


py::object UserDataToPython(void* userData, bool release) {
    if(userData == nullptr) {
        return py::none();
    }
    py::object* ref = static_cast<py::object*>(userData);
    py::object object = *ref;
    // Decrease the reference count on the Python object.
    if(release) {
        delete ref;
    }
    // Return the Python object.
    return object;
}


class Releaser {
public:
    void operator() (IUnknown* obj) {
        obj->Release();
    }
};


class VariantClearer {
public:
    void operator() (Variant* variant) {
        VariantClear(variant);
    }
};


class SafeArrayDestroyer {
public:
    void operator() (SafeArray* safe_array) {
        SafeArrayDestroy(safe_array);
    }
};


struct Resource {
    void* data;
};


// Trampoline helper class which enables subclassing IBlackmagicRawResourceManager from Python.
class BlackmagicRawResourceManager : public IBlackmagicRawResourceManager {
private:
    std::atomic_ulong m_refCount = {0};
protected:
    virtual ~BlackmagicRawResourceManager() {
        assert(m_refCount == 0);
    }
public:
    BlackmagicRawResourceManager() {
        AddRef();
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID*) { return E_NOTIMPL; }

    virtual ULONG STDMETHODCALLTYPE AddRef(void) {
        return m_refCount.fetch_add(1) + 1;
    }

    virtual ULONG STDMETHODCALLTYPE Release(void) {
        ULONG oldRefCount = m_refCount.fetch_sub(1);
        assert(oldRefCount > 0);
        if(oldRefCount == 1) {
            delete this;
        }
        return oldRefCount - 1;
    }

    HRESULT CreateResource(void* context, void* commandQueue, uint32_t sizeBytes, BlackmagicRawResourceType type, BlackmagicRawResourceUsage usage, void** resource) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "CreateResource");
        if(pyfunc) {
            py::object ret = pyfunc(context, commandQueue, sizeBytes, type, usage);
            if(!py::isinstance<py::tuple>(ret)) {
                py::pybind11_fail("Expected \"IBlackmagicRawResourceManager::CreateResource\" to return a tuple");
            }
            py::tuple tuple = ret.cast<py::tuple>();
            if(!py::isinstance<py::int_>(tuple[0])) {
                py::pybind11_fail("Expected first return value to be a HRESULT");
            }
            if(!py::isinstance<Resource>(tuple[1])) {
                py::pybind11_fail("Expected second return value to be a Resource");
            }
            *resource = tuple[1].cast<Resource>().data;
            return tuple[0].cast<HRESULT>();
        }
        py::pybind11_fail("Tried to call pure virtual function \"IBlackmagicRawResourceManager::CreateResource\"");
    }

    HRESULT ReleaseResource(void* context, void* commandQueue, void* resource, BlackmagicRawResourceType type) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "ReleaseResource");
        if(pyfunc) {
            Resource resource_obj = {};
            resource_obj.data = resource;
            py::object ret = pyfunc(context, commandQueue, resource_obj, type);
            if(!py::isinstance<py::int_>(ret)) {
                py::pybind11_fail("Expected \"IBlackmagicRawResourceManager::CreateResource\" to return a HRESULT");
            }
            return ret.cast<HRESULT>();
        }
        py::pybind11_fail("Tried to call pure virtual function \"IBlackmagicRawResourceManager::ReleaseResource\"");
    }

    HRESULT CopyResource(void* context, void* commandQueue, void* source, BlackmagicRawResourceType sourceType, void* destination, BlackmagicRawResourceType destinationType, uint32_t sizeBytes, bool copyAsync) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "CopyResource");
        if(pyfunc) {
            Resource source_resource = {};
            source_resource.data = source;
            Resource destination_resource = {};
            destination_resource.data = destination;
            py::object ret = pyfunc(context, commandQueue, source_resource, sourceType, destination_resource, destinationType, sizeBytes, copyAsync);
            if(!py::isinstance<py::int_>(ret)) {
                py::pybind11_fail("Expected \"IBlackmagicRawResourceManager::CreateResource\" to return a HRESULT");
            }
            return ret.cast<HRESULT>();
        }
        py::pybind11_fail("Tried to call pure virtual function \"IBlackmagicRawResourceManager::CopyResource\"");
    }

    HRESULT GetResourceHostPointer(void* context, void* commandQueue, void* resource, BlackmagicRawResourceType resourceType, void** hostPointer) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "GetResourceHostPointer");
        if(pyfunc) {
            Resource resource_obj = {};
            resource_obj.data = resource;
            py::object ret = pyfunc(context, commandQueue, resource_obj, resourceType);
            if(!py::isinstance<py::tuple>(ret)) {
                py::pybind11_fail("Expected \"IBlackmagicRawResourceManager::CreateResource\" to return a tuple");
            }
            py::tuple tuple = ret.cast<py::tuple>();
            if(!py::isinstance<py::int_>(tuple[0])) {
                py::pybind11_fail("Expected first return value to be a HRESULT");
            }
            if(!py::isinstance<Resource>(tuple[1])) {
                py::pybind11_fail("Expected second return value to be a Resource");
            }
            *hostPointer = tuple[1].cast<Resource>().data;
            return tuple[0].cast<HRESULT>();
        }
        py::pybind11_fail("Tried to call pure virtual function \"IBlackmagicRawResourceManager::GetResourceHostPointer\"");
    }
};


// Trampoline helper class which enables subclassing IBlackmagicRawCallback from Python.
class BlackmagicRawCallback : public IBlackmagicRawCallback {
private:
    std::atomic_ulong m_refCount = {0};
protected:
    virtual ~BlackmagicRawCallback() {
        assert(m_refCount == 0);
    }
public:
    BlackmagicRawCallback() {
        AddRef();
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID*) { return E_NOTIMPL; }

    virtual ULONG STDMETHODCALLTYPE AddRef(void) {
        return m_refCount.fetch_add(1) + 1;
    }

    virtual ULONG STDMETHODCALLTYPE Release(void) {
        ULONG oldRefCount = m_refCount.fetch_sub(1);
        assert(oldRefCount > 0);
        if(oldRefCount == 1) {
            delete this;
        }
        return oldRefCount - 1;
    }

    void ReadComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawFrame* frame) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "ReadComplete");
        if(pyfunc) {
            frame->AddRef();
            pyfunc(
                job,
                result,
                py::cast(frame, py::return_value_policy::take_ownership)
            );
        }
    }

    void ProcessComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawProcessedImage* processedImage) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "ProcessComplete");
        if(pyfunc) {
            processedImage->AddRef();
            pyfunc(
                job,
                result,
                py::cast(processedImage, py::return_value_policy::take_ownership)
            );
        }
    }

    void DecodeComplete(IBlackmagicRawJob* job, HRESULT result) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "DecodeComplete");
        if(pyfunc) {
            pyfunc(job, result);
        }
    }

    void TrimProgress(IBlackmagicRawJob* job, float progress) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "TrimProgress");
        if(pyfunc) {
            pyfunc(job, progress);
        }
    }

    void TrimComplete(IBlackmagicRawJob* job, HRESULT result) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "TrimComplete");
        if(pyfunc) {
            pyfunc(job, result);
        }
    }

    void SidecarMetadataParseWarning(IBlackmagicRawClip* clip, const char* fileName, uint32_t lineNumber, const char* info) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "SidecarMetadataParseWarning");
        if(pyfunc) {
            clip->AddRef();
            pyfunc(
                py::cast(clip, py::return_value_policy::take_ownership),
                fileName,
                lineNumber,
                info
            );
        }
    }

    void SidecarMetadataParseError(IBlackmagicRawClip* clip, const char* fileName, uint32_t lineNumber, const char* info) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "SidecarMetadataParseError");
        if(pyfunc) {
            clip->AddRef();
            pyfunc(
                py::cast(clip, py::return_value_policy::take_ownership),
                fileName,
                lineNumber,
                info
            );
        }
    }

    void PreparePipelineComplete(void* userData, HRESULT result) override {
        py::gil_scoped_acquire gil;
        py::function pyfunc = py::get_override(this, "PreparePipelineComplete");
        py::object userDataPy = UserDataToPython(userData, true);
        if(pyfunc) {
            pyfunc(userDataPy, result);
        }
    }
};


template<typename T>
py::array_t<T> _resource_to_numpy(std::vector<size_t> shape, uint32_t sizeBytes, void* resource, py::handle base) {
    std::vector<size_t> stride;
    size_t prod = sizeof(T);
    for(auto it = shape.rbegin(); it != shape.rend(); ++it) {
        stride.insert(stride.begin(), prod);
        prod *= *it;
    }
    if(prod != sizeBytes) {
        throw py::buffer_error("mismatched resource size");
    }
    return py::array_t<T>(shape, stride, (T*)resource, base);
}


template<typename T>
py::array_t<T> _safe_array_to_numpy(SafeArray* safe_array, py::handle base) {
    if(safe_array->cDims != 1) {
        throw py::buffer_error("only 1D SafeArray instances are supported");
    }
    void* ptr;
    SafeArrayAccessData(safe_array, &ptr);
    py::array_t<T> array = py::array_t<T>({safe_array->bounds.cElements}, {sizeof(T)}, &((T*)ptr)[safe_array->bounds.lLbound], base);
    SafeArrayUnaccessData(safe_array);
    return array;
}


py::array convert_safe_array_to_numpy(SafeArray* safe_array, py::handle base) {
    switch(safe_array->variantType) {
        case blackmagicRawVariantTypeU8:
            return _safe_array_to_numpy<uint8_t>(safe_array, base);
        case blackmagicRawVariantTypeS16:
            return _safe_array_to_numpy<int16_t>(safe_array, base);
        case blackmagicRawVariantTypeU16:
            return _safe_array_to_numpy<uint16_t>(safe_array, base);
        case blackmagicRawVariantTypeS32:
            return _safe_array_to_numpy<int32_t>(safe_array, base);
        case blackmagicRawVariantTypeU32:
            return _safe_array_to_numpy<uint32_t>(safe_array, base);
        case blackmagicRawVariantTypeFloat32:
            return _safe_array_to_numpy<float_t>(safe_array, base);
        default:
            throw py::buffer_error("unsupported variantType for SafeArray");
    }
}


SafeArray* convert_numpy_to_safe_array(py::array array) {
    py::array buf = py::array::ensure(array);
    if(!buf) {
        throw py::buffer_error("not a numpy array");
    }
    BlackmagicRawVariantType vt = blackmagicRawVariantTypeEmpty;
    if(py::isinstance<py::array_t<uint8_t>>(buf)) {
        vt = blackmagicRawVariantTypeU8;
    } else if(py::isinstance<py::array_t<int16_t>>(buf)) {
        vt = blackmagicRawVariantTypeS16;
    } else if(py::isinstance<py::array_t<uint16_t>>(buf)) {
        vt = blackmagicRawVariantTypeU16;
    } else if(py::isinstance<py::array_t<int32_t>>(buf)) {
        vt = blackmagicRawVariantTypeS32;
    } else if(py::isinstance<py::array_t<uint32_t>>(buf)) {
        vt = blackmagicRawVariantTypeU32;
    } else if(py::isinstance<py::array_t<float_t>>(buf)) {
        vt = blackmagicRawVariantTypeFloat32;
    } else {
        throw py::buffer_error("unsupported data type");
    }
    void* data = malloc(buf.nbytes());
    memcpy(data, buf.data(), buf.nbytes());
    // NOTE: We aren't using SafeArrayCreate because it segfaults for signed integer types.
    SafeArray* parray = new SafeArray();
    parray->variantType = vt;
    parray->cDims = 1;
    parray->data = data;
    parray->bounds.lLbound = 0;
    parray->bounds.cElements = (uint32_t)buf.size();
    return parray;
}


Variant VariantCreateS16(int16_t value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeS16;
    variant.iVal = value;
    return variant;
}


Variant VariantCreateU16(uint16_t value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeU16;
    variant.uiVal = value;
    return variant;
}


Variant VariantCreateS32(int32_t value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeS32;
    variant.intVal = value;
    return variant;
}


Variant VariantCreateU32(uint32_t value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeU32;
    variant.uintVal = value;
    return variant;
}


Variant VariantCreateFloat32(float_t value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeFloat32;
    variant.fltVal = value;
    return variant;
}


Variant VariantCreateString(const char* value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeString;
    variant.bstrVal = strdup(value);
    return variant;
}


Variant VariantCreateSafeArray(py::array value) {
    Variant variant;
    VariantInit(&variant);
    variant.vt = blackmagicRawVariantTypeSafeArray;
    variant.parray = convert_numpy_to_safe_array(value);
    return variant;
}


PYBIND11_MODULE(_pybraw, m) {
    m.doc() = "Python bindings for Blackmagic RAW SDK";
    m.def("CreateBlackmagicRawFactoryInstance", &CreateBlackmagicRawFactoryInstance);

//    m.def("VariantInit", &VariantInit);
//    m.def("VariantClear", &VariantClear);
    m.def("VariantCreateS16", &VariantCreateS16);
    m.def("VariantCreateU16", &VariantCreateU16);
    m.def("VariantCreateS32", &VariantCreateS32);
    m.def("VariantCreateU32", &VariantCreateU32);
    m.def("VariantCreateFloat32", &VariantCreateFloat32);
    m.def("VariantCreateString", &VariantCreateString);
    m.def("VariantCreateSafeArray", &VariantCreateSafeArray);

    m.def("SafeArrayCreateFromNumpy", &convert_numpy_to_safe_array);

    m.def("CreateResourceNone", []() {
        Resource resource = {};
        return resource;
    });

    m.def("CreateResourceFromIntPointer", [](uintptr_t int_pointer) {
        Resource resource = {};
        resource.data = reinterpret_cast<void*>(int_pointer);
        return resource;
    });

    m.def("PointerCTypesToPyBind", [](py::object p) {
        auto ctypes = py::module::import("ctypes");
        if(py::isinstance(p, ctypes.attr("c_void_p"))) {
            PyObject* int_pointer = PyObject_GetAttr(p.ptr(), PyUnicode_FromString("value"));
            return PyLong_AsVoidPtr(int_pointer);
        } else {
            throw std::invalid_argument("expected argument to be a c_void_p");
        }
    });

    // This function is currently used for testing purposes only.
    m.def("_IUnknownWeakref", [](IUnknown* obj) {
        return obj;
    }, py::return_value_policy::reference);

    // HRESULT constants.
    m.attr("S_OK") = py::int_(S_OK);
    m.attr("S_FALSE") = py::int_(S_FALSE);
    m.attr("E_UNEXPECTED") = py::int_(E_UNEXPECTED);
    m.attr("E_NOTIMPL") = py::int_(E_NOTIMPL);
    m.attr("E_OUTOFMEMORY") = py::int_(E_OUTOFMEMORY);
    m.attr("E_INVALIDARG") = py::int_(E_INVALIDARG);
    m.attr("E_NOINTERFACE") = py::int_(E_NOINTERFACE);
    m.attr("E_POINTER") = py::int_(E_POINTER);
    m.attr("E_HANDLE") = py::int_(E_HANDLE);
    m.attr("E_ABORT") = py::int_(E_ABORT);
    m.attr("E_FAIL") = py::int_(E_FAIL);
    m.attr("E_ACCESSDENIED") = py::int_(E_ACCESSDENIED);

    py::enum_<_BlackmagicRawVariantType>(m, "_BlackmagicRawVariantType")
        .value("blackmagicRawVariantTypeEmpty", blackmagicRawVariantTypeEmpty)
        .value("blackmagicRawVariantTypeU8", blackmagicRawVariantTypeU8)
        .value("blackmagicRawVariantTypeS16", blackmagicRawVariantTypeS16)
        .value("blackmagicRawVariantTypeU16", blackmagicRawVariantTypeU16)
        .value("blackmagicRawVariantTypeS32", blackmagicRawVariantTypeS32)
        .value("blackmagicRawVariantTypeU32", blackmagicRawVariantTypeU32)
        .value("blackmagicRawVariantTypeFloat32", blackmagicRawVariantTypeFloat32)
        .value("blackmagicRawVariantTypeString", blackmagicRawVariantTypeString)
        .value("blackmagicRawVariantTypeSafeArray", blackmagicRawVariantTypeSafeArray)
        .export_values()
    ;

    py::enum_<_BlackmagicRawResourceType>(m, "_BlackmagicRawResourceType")
        .value("blackmagicRawResourceTypeBufferCPU", blackmagicRawResourceTypeBufferCPU)
        .value("blackmagicRawResourceTypeBufferMetal", blackmagicRawResourceTypeBufferMetal)
        .value("blackmagicRawResourceTypeBufferCUDA", blackmagicRawResourceTypeBufferCUDA)
        .value("blackmagicRawResourceTypeBufferOpenCL", blackmagicRawResourceTypeBufferOpenCL)
        .export_values()
    ;

    py::enum_<_BlackmagicRawResourceFormat>(m, "_BlackmagicRawResourceFormat")
        .value("blackmagicRawResourceFormatRGBAU8", blackmagicRawResourceFormatRGBAU8)
        .value("blackmagicRawResourceFormatBGRAU8", blackmagicRawResourceFormatBGRAU8)
        .value("blackmagicRawResourceFormatRGBU16", blackmagicRawResourceFormatRGBU16)
        .value("blackmagicRawResourceFormatRGBAU16", blackmagicRawResourceFormatRGBAU16)
        .value("blackmagicRawResourceFormatBGRAU16", blackmagicRawResourceFormatBGRAU16)
        .value("blackmagicRawResourceFormatRGBU16Planar", blackmagicRawResourceFormatRGBU16Planar)
        .value("blackmagicRawResourceFormatRGBF32", blackmagicRawResourceFormatRGBF32)
        .value("blackmagicRawResourceFormatRGBF32Planar", blackmagicRawResourceFormatRGBF32Planar)
        .value("blackmagicRawResourceFormatBGRAF32", blackmagicRawResourceFormatBGRAF32)
        .export_values()
    ;

    py::enum_<_BlackmagicRawResourceUsage>(m, "_BlackmagicRawResourceUsage")
        .value("blackmagicRawResourceUsageReadCPUWriteCPU", blackmagicRawResourceUsageReadCPUWriteCPU)
        .value("blackmagicRawResourceUsageReadGPUWriteGPU", blackmagicRawResourceUsageReadGPUWriteGPU)
        .value("blackmagicRawResourceUsageReadGPUWriteCPU", blackmagicRawResourceUsageReadGPUWriteCPU)
        .value("blackmagicRawResourceUsageReadCPUWriteGPU", blackmagicRawResourceUsageReadCPUWriteGPU)
        .export_values()
    ;

    py::enum_<_BlackmagicRawPipeline>(m, "_BlackmagicRawPipeline")
        .value("blackmagicRawPipelineCPU", blackmagicRawPipelineCPU)
        .value("blackmagicRawPipelineCUDA", blackmagicRawPipelineCUDA)
        .value("blackmagicRawPipelineMetal", blackmagicRawPipelineMetal)
        .value("blackmagicRawPipelineOpenCL", blackmagicRawPipelineOpenCL)
        .export_values()
    ;

    py::enum_<_BlackmagicRawInstructionSet>(m, "_BlackmagicRawInstructionSet")
        .value("blackmagicRawInstructionSetSSE41", blackmagicRawInstructionSetSSE41)
        .value("blackmagicRawInstructionSetAVX", blackmagicRawInstructionSetAVX)
        .value("blackmagicRawInstructionSetAVX2", blackmagicRawInstructionSetAVX2)
        .value("blackmagicRawInstructionSetNEON", blackmagicRawInstructionSetNEON)
        .export_values()
    ;

    py::enum_<_BlackmagicRawResolutionScale>(m, "_BlackmagicRawResolutionScale")
        .value("blackmagicRawResolutionScaleFull", blackmagicRawResolutionScaleFull)
        .value("blackmagicRawResolutionScaleHalf", blackmagicRawResolutionScaleHalf)
        .value("blackmagicRawResolutionScaleQuarter", blackmagicRawResolutionScaleQuarter)
        .value("blackmagicRawResolutionScaleEighth", blackmagicRawResolutionScaleEighth)
        .value("blackmagicRawResolutionScaleFullUpsideDown", blackmagicRawResolutionScaleFullUpsideDown)
        .value("blackmagicRawResolutionScaleHalfUpsideDown", blackmagicRawResolutionScaleHalfUpsideDown)
        .value("blackmagicRawResolutionScaleQuarterUpsideDown", blackmagicRawResolutionScaleQuarterUpsideDown)
        .value("blackmagicRawResolutionScaleEighthUpsideDown", blackmagicRawResolutionScaleEighthUpsideDown)
        .export_values()
    ;

    py::enum_<_BlackmagicRawClipProcessingAttribute>(m, "_BlackmagicRawClipProcessingAttribute")
        .value("blackmagicRawClipProcessingAttributeColorScienceGen", blackmagicRawClipProcessingAttributeColorScienceGen)
        .value("blackmagicRawClipProcessingAttributeGamma", blackmagicRawClipProcessingAttributeGamma)
        .value("blackmagicRawClipProcessingAttributeGamut", blackmagicRawClipProcessingAttributeGamut)
        .value("blackmagicRawClipProcessingAttributeToneCurveContrast", blackmagicRawClipProcessingAttributeToneCurveContrast)
        .value("blackmagicRawClipProcessingAttributeToneCurveSaturation", blackmagicRawClipProcessingAttributeToneCurveSaturation)
        .value("blackmagicRawClipProcessingAttributeToneCurveMidpoint", blackmagicRawClipProcessingAttributeToneCurveMidpoint)
        .value("blackmagicRawClipProcessingAttributeToneCurveHighlights", blackmagicRawClipProcessingAttributeToneCurveHighlights)
        .value("blackmagicRawClipProcessingAttributeToneCurveShadows", blackmagicRawClipProcessingAttributeToneCurveShadows)
        .value("blackmagicRawClipProcessingAttributeToneCurveVideoBlackLevel", blackmagicRawClipProcessingAttributeToneCurveVideoBlackLevel)
        .value("blackmagicRawClipProcessingAttributeToneCurveBlackLevel", blackmagicRawClipProcessingAttributeToneCurveBlackLevel)
        .value("blackmagicRawClipProcessingAttributeToneCurveWhiteLevel", blackmagicRawClipProcessingAttributeToneCurveWhiteLevel)
        .value("blackmagicRawClipProcessingAttributeHighlightRecovery", blackmagicRawClipProcessingAttributeHighlightRecovery)
        .value("blackmagicRawClipProcessingAttributeAnalogGainIsConstant", blackmagicRawClipProcessingAttributeAnalogGainIsConstant)
        .value("blackmagicRawClipProcessingAttributeAnalogGain", blackmagicRawClipProcessingAttributeAnalogGain)
        .value("blackmagicRawClipProcessingAttributePost3DLUTMode", blackmagicRawClipProcessingAttributePost3DLUTMode)
        .value("blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTName", blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTName)
        .value("blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTTitle", blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTTitle)
        .value("blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTSize", blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTSize)
        .value("blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTData", blackmagicRawClipProcessingAttributeEmbeddedPost3DLUTData)
        .value("blackmagicRawClipProcessingAttributeSidecarPost3DLUTName", blackmagicRawClipProcessingAttributeSidecarPost3DLUTName)
        .value("blackmagicRawClipProcessingAttributeSidecarPost3DLUTTitle", blackmagicRawClipProcessingAttributeSidecarPost3DLUTTitle)
        .value("blackmagicRawClipProcessingAttributeSidecarPost3DLUTSize", blackmagicRawClipProcessingAttributeSidecarPost3DLUTSize)
        .value("blackmagicRawClipProcessingAttributeSidecarPost3DLUTData", blackmagicRawClipProcessingAttributeSidecarPost3DLUTData)
        .value("blackmagicRawClipProcessingAttributeGamutCompressionEnable", blackmagicRawClipProcessingAttributeGamutCompressionEnable)
        .export_values()
    ;

    py::enum_<_BlackmagicRawFrameProcessingAttribute>(m, "_BlackmagicRawFrameProcessingAttribute")
        .value("blackmagicRawFrameProcessingAttributeWhiteBalanceKelvin", blackmagicRawFrameProcessingAttributeWhiteBalanceKelvin)
        .value("blackmagicRawFrameProcessingAttributeWhiteBalanceTint", blackmagicRawFrameProcessingAttributeWhiteBalanceTint)
        .value("blackmagicRawFrameProcessingAttributeExposure", blackmagicRawFrameProcessingAttributeExposure)
        .value("blackmagicRawFrameProcessingAttributeISO", blackmagicRawFrameProcessingAttributeISO)
        .value("blackmagicRawFrameProcessingAttributeAnalogGain", blackmagicRawFrameProcessingAttributeAnalogGain)
        .export_values()
    ;

    py::enum_<_BlackmagicRawInterop>(m, "_BlackmagicRawInterop")
        .value("blackmagicRawInteropNone", blackmagicRawInteropNone)
        .value("blackmagicRawInteropOpenGL", blackmagicRawInteropOpenGL)
        .export_values()
    ;

    py::class_<SafeArrayBound>(m, "SafeArrayBound")
        .def_readwrite("lLbound", &SafeArrayBound::lLbound)
        .def_readwrite("cElements", &SafeArrayBound::cElements)
    ;

    py::class_<SafeArray,std::unique_ptr<SafeArray,SafeArrayDestroyer>>(m, "SafeArray")
        .def_readwrite("variantType", &SafeArray::variantType)
        .def_readwrite("cDims", &SafeArray::cDims)
        .def_readwrite("data", &SafeArray::data)
        .def_readwrite("bounds", &SafeArray::bounds)
        .def("to_py", [](SafeArray* self) -> py::array {
            return convert_safe_array_to_numpy(self, py::handle());
        }, "Return a copy of this SafeArray as a Numpy array.")
//        .def("to_py_nocopy", [](SafeArray* self) -> py::array {
//            // WARN: This is set up such that data is not copied. So if the SafeArray is freed,
//            //       it is not safe to continue using this array.
//            return convert_safe_array_to_numpy(self, py::none());
//        })
    ;

    py::class_<Variant,std::unique_ptr<Variant,VariantClearer>>(m, "Variant")
//        .def_readwrite("vt", &Variant::vt)
//        .def_readwrite("iVal", &Variant::iVal)
//        .def_readwrite("uiVal", &Variant::uiVal)
//        .def_readwrite("intVal", &Variant::intVal)
//        .def_readwrite("uintVal", &Variant::uintVal)
//        .def_readwrite("fltVal", &Variant::fltVal)
//        .def_readwrite("bstrVal", &Variant::bstrVal)
//        .def_readwrite("parray", &Variant::parray)
        .def("to_py", [](Variant& self) -> py::object {
            switch(self.vt) {
                case blackmagicRawVariantTypeS16:
                    return py::cast(self.iVal);
                case blackmagicRawVariantTypeU16:
                    return py::cast(self.uiVal);
                case blackmagicRawVariantTypeS32:
                    return py::cast(self.intVal);
                case blackmagicRawVariantTypeU32:
                    return py::cast(self.uintVal);
                case blackmagicRawVariantTypeFloat32:
                    return py::cast(self.fltVal);
                case blackmagicRawVariantTypeString:
                    return py::cast(self.bstrVal);
                case blackmagicRawVariantTypeSafeArray:
                    return convert_safe_array_to_numpy(self.parray, py::handle());
                default:
                    throw py::value_error("unsupported variantType for Variant");
            }
        }, "Return a copy of this Variant as a Python object.")
    ;

    py::class_<Resource>(m, "Resource")
        .def("to_py_nocopy", [](Resource& self, size_t size_bytes) -> py::array {
            return py::array_t<uint8_t>({size_bytes}, {sizeof(uint8_t)}, ((uint8_t*)self.data), py::none());
        }, "Return a view of this resource as a Numpy array. The view will not be valid after the resource is released.")
        .def("__int__", [](Resource& self) {
            return (uintptr_t)self.data;
        })
        .def("__hash__", [](Resource& self) {
            return (uintptr_t)self.data;
        })
        .def("__eq__", [](Resource& self, py::object other) {
            if(!py::isinstance<Resource>(other)) {
                return false;
            }
            return py::hash(py::cast(self)) == py::hash(other);
        })
    ;

    py::class_<IUnknown>(m, "IUnknown")
        // Instead of binding QueryInterface directly, we add as_IBlackmagicXXX methods where
        // appropriate.
        .def("AddRef", &IUnknown::AddRef)
        .def("Release", &IUnknown::Release)
    ;

    py::class_<IBlackmagicRawCallback,IUnknown,std::unique_ptr<IBlackmagicRawCallback,Releaser>>(m, "IBlackmagicRawCallback")
        .def("ReadComplete", &IBlackmagicRawCallback::ReadComplete)
        .def("ProcessComplete", &IBlackmagicRawCallback::ProcessComplete)
        .def("DecodeComplete", &IBlackmagicRawCallback::DecodeComplete)
        .def("TrimProgress", &IBlackmagicRawCallback::TrimProgress)
        .def("TrimComplete", &IBlackmagicRawCallback::TrimComplete)
        .def("SidecarMetadataParseWarning", &IBlackmagicRawCallback::SidecarMetadataParseWarning)
        .def("SidecarMetadataParseError", &IBlackmagicRawCallback::SidecarMetadataParseError)
        .def("PreparePipelineComplete", &IBlackmagicRawCallback::PreparePipelineComplete)
    ;

    py::class_<BlackmagicRawCallback,IBlackmagicRawCallback,std::unique_ptr<BlackmagicRawCallback,Releaser>>(m, "BlackmagicRawCallback")
        .def(py::init<>())
    ;

    py::class_<IBlackmagicRawClipEx,IUnknown,std::unique_ptr<IBlackmagicRawClipEx,Releaser>>(m, "IBlackmagicRawClipEx")
        .def("GetMaxBitStreamSizeBytes",
            [](IBlackmagicRawClipEx& self) {
                uint32_t maxBitStreamSizeBytes = 0;
                HRESULT result = self.GetMaxBitStreamSizeBytes(&maxBitStreamSizeBytes);
                return std::make_tuple(result, maxBitStreamSizeBytes);
            },
            "Inspect all frames and return the maximum bit stream size encountered."
        )
        .def("GetBitStreamSizeBytes",
            [](IBlackmagicRawClipEx& self, uint64_t frameIndex) {
                uint32_t bitStreamSizeBytes = 0;
                HRESULT result = self.GetBitStreamSizeBytes(frameIndex, &bitStreamSizeBytes);
                return std::make_tuple(result, bitStreamSizeBytes);
            },
            "Return the bit stream size for the provided frame.",
            "frameIndex"_a
        )
        .def("CreateJobReadFrame",
            [](IBlackmagicRawClipEx& self, uint64_t frameIndex, Resource bitStream, uint32_t bitStreamSizeBytes) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobReadFrame(frameIndex, bitStream.data, bitStreamSizeBytes, &job);
                return std::make_tuple(result, job);
            },
            "Create a job that will read the frame's bit stream into memory.",
            "frameIndex"_a, "bitStream"_a, "bitStreamSizeBytes"_a
        )
        .def("QueryTimecodeInfo",
            [](IBlackmagicRawClipEx& self) {
                uint32_t baseFrameIndex = 0;
                bool isDropFrameTimecode = false;
                HRESULT result = self.QueryTimecodeInfo(&baseFrameIndex, &isDropFrameTimecode);
                return std::make_tuple(result, baseFrameIndex, isDropFrameTimecode);
            },
            "Query the timecode info for the clip."
        )
    ;

    py::class_<IBlackmagicRawClipAudio,IUnknown,std::unique_ptr<IBlackmagicRawClipAudio,Releaser>>(m, "IBlackmagicRawClipAudio")
        // TODO: Add missing bindings.
    ;

    py::class_<IBlackmagicRawClipResolutions,IUnknown,std::unique_ptr<IBlackmagicRawClipResolutions,Releaser>>(m, "IBlackmagicRawClipResolutions")
        .def("GetResolutionCount",
            [](IBlackmagicRawClipResolutions& self) {
                uint32_t resolutionCount = 0;
                HRESULT result = self.GetResolutionCount(&resolutionCount);
                return std::make_tuple(result, resolutionCount);
            },
            "Return the number of resolutions at which the clip may be processed."
        )
        .def("GetResolution",
            [](IBlackmagicRawClipResolutions& self, uint32_t resolutionIndex) {
                uint32_t resolutionWidthPixels = 0;
                uint32_t resolutionHeightPixels = 0;
                HRESULT result = self.GetResolution(resolutionIndex, &resolutionWidthPixels, &resolutionHeightPixels);
                return std::make_tuple(result, resolutionWidthPixels, resolutionHeightPixels);
            },
            "Return a resolution at which the clip may be processed.",
            "resolutionIndex"_a
        )
        .def("GetClosestScaleForResolution",
            [](IBlackmagicRawClipResolutions& self, uint32_t resolutionWidthPixels, uint32_t resolutionHeightPixels, bool requestUpsideDown) {
                BlackmagicRawResolutionScale resolutionScale = 0;
                HRESULT result = self.GetClosestScaleForResolution(resolutionWidthPixels, resolutionHeightPixels, requestUpsideDown, &resolutionScale);
                return std::make_tuple(result, resolutionScale);
            },
            "Return a scale which most closely matches the given resolution.",
            "resolutionWidthPixels"_a, "resolutionHeightPixels"_a, "requestUpsideDown"_a
        )
    ;

    py::class_<IBlackmagicRawPost3DLUT,IUnknown,std::unique_ptr<IBlackmagicRawPost3DLUT,Releaser>>(m, "IBlackmagicRawPost3DLUT")
        .def("GetName",
            [](IBlackmagicRawPost3DLUT& self) {
                const char* name = nullptr;
                HRESULT result = self.GetName(&name);
                return std::make_tuple(result, name);
            },
            "Get the name of the 3D LUT."
        )
        .def("GetTitle",
            [](IBlackmagicRawPost3DLUT& self) {
                const char* title = nullptr;
                HRESULT result = self.GetTitle(&title);
                return std::make_tuple(result, title);
            },
            "Get the title of the 3D LUT."
        )
        .def("GetSize",
            [](IBlackmagicRawPost3DLUT& self) {
                uint32_t size = 0;
                HRESULT result = self.GetSize(&size);
                return std::make_tuple(result, size);
            },
            "Get the size of the LUT (e.g. 17 for a 17x17x17 LUT)."
        )
        .def("GetResourceGPU",
            [](IBlackmagicRawPost3DLUT& self, void* context, void* commandQueue) {
                Resource resource = {};
                BlackmagicRawResourceType type = 0;
                HRESULT result = self.GetResourceGPU(context, commandQueue, &type, &resource.data);
                return std::make_tuple(result, type, resource);
            },
            "Get the GPU resource the LUT is stored in.",
            "context"_a, "commandQueue"_a
        )
        .def("GetResourceCPU",
            [](IBlackmagicRawPost3DLUT& self) {
                Resource resource = {};
                HRESULT result = self.GetResourceCPU(&resource.data);
                return std::make_tuple(result, resource);
            },
            "Get the CPU resource the LUT is stored in."
        )
        .def("GetResourceSizeBytes",
            [](IBlackmagicRawPost3DLUT& self) {
                uint32_t sizeBytes = 0;
                HRESULT result = self.GetResourceSizeBytes(&sizeBytes);
                return std::make_tuple(result, sizeBytes);
            },
            "Get the size of the resource in bytes."
        )
    ;

    py::class_<IBlackmagicRawClipProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawClipProcessingAttributes,Releaser>>(m, "IBlackmagicRawClipProcessingAttributes")
        .def("GetClipAttribute",
            [](IBlackmagicRawClipProcessingAttributes& self, BlackmagicRawClipProcessingAttribute attribute) {
                Variant value;
                VariantInit(&value);
                HRESULT result = self.GetClipAttribute(attribute, &value);
                return std::make_tuple(result, value);
            },
            "Get the attribute.",
            "attribute"_a
        )
        .def("SetClipAttribute",
            &IBlackmagicRawClipProcessingAttributes::SetClipAttribute,
            "Set the attribute.",
            "attribute"_a, "value"_a
        )
        .def("GetPost3DLUT",
            [](IBlackmagicRawClipProcessingAttributes& self) {
                IBlackmagicRawPost3DLUT* lut = nullptr;
                HRESULT result = self.GetPost3DLUT(&lut);
                return std::make_tuple(result, lut);
            },
            "Get the active 3D LUT."
        )
    ;

    py::class_<IBlackmagicRawFrameProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawFrameProcessingAttributes,Releaser>>(m, "IBlackmagicRawFrameProcessingAttributes")
        .def("GetFrameAttribute",
            [](IBlackmagicRawFrameProcessingAttributes& self, BlackmagicRawFrameProcessingAttribute attribute) {
                Variant value;
                VariantInit(&value);
                HRESULT result = self.GetFrameAttribute(attribute, &value);
                return std::make_tuple(result, value);
            },
            "Get the attribute.",
            "attribute"_a
        )
        .def("SetFrameAttribute",
            &IBlackmagicRawFrameProcessingAttributes::SetFrameAttribute,
            "Set the attribute.",
            "attribute"_a, "value"_a
        )
    ;

    py::class_<IBlackmagicRawFrame,IUnknown,std::unique_ptr<IBlackmagicRawFrame,Releaser>>(m, "IBlackmagicRawFrame")
        .def("GetFrameIndex",
            [](IBlackmagicRawFrame& self) {
                uint64_t frameIndex = 0;
                HRESULT result = self.GetFrameIndex(&frameIndex);
                return std::make_tuple(result, frameIndex);
            },
            "Get the frame index."
        )
        .def("GetTimecode",
            [](IBlackmagicRawFrame& self) {
                const char* timecode = nullptr;
                HRESULT result = self.GetTimecode(&timecode);
                return std::make_tuple(result, timecode);
            },
            "Get a formatted timecode for this frame."
        )
        .def("GetMetadataIterator",
            [](IBlackmagicRawFrame& self) {
                IBlackmagicRawMetadataIterator* iterator = nullptr;
                HRESULT result = self.GetMetadataIterator(&iterator);
                return std::make_tuple(result, iterator);
            },
            "Create a metadata iterator for this frame."
        )
        .def("GetMetadata",
            [](IBlackmagicRawFrame& self, const char* key) {
                Variant value;
                VariantInit(&value);
                HRESULT result = self.GetMetadata(key, &value);
                return std::make_tuple(result, value);
            },
            "Query a single frame metadata value by key.",
            "key"_a
        )
        .def("SetMetadata",
            &IBlackmagicRawFrame::SetMetadata,
            "Set metadata to this frame.",
            "key"_a, "value"_a
        )
        .def("CloneFrameProcessingAttributes",
            [](IBlackmagicRawFrame& self) {
                IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes = nullptr;
                HRESULT result = self.CloneFrameProcessingAttributes(&frameProcessingAttributes);
                return std::make_tuple(result, frameProcessingAttributes);
            },
            "Create a copy of this frame's processing attributes."
        )
        .def("SetResolutionScale",
            &IBlackmagicRawFrame::SetResolutionScale,
            "Set the resolution scale we want to decode this image to."
            "resolutionScale"_a
        )
        .def("GetResolutionScale",
            [](IBlackmagicRawFrame& self) {
                BlackmagicRawResolutionScale resolutionScale = 0;
                HRESULT result = self.GetResolutionScale(&resolutionScale);
                return std::make_tuple(result, resolutionScale);
            },
            "Get the resolution scale set to the frame."
        )
        .def("SetResourceFormat",
            &IBlackmagicRawFrame::SetResourceFormat,
            "Set the desired resource format that we want to process this frame into.",
            "resourceFormat"_a
        )
        .def("GetResourceFormat",
            [](IBlackmagicRawFrame& self) {
                BlackmagicRawResourceFormat resourceFormat = 0;
                HRESULT result = self.GetResourceFormat(&resourceFormat);
                return std::make_tuple(result, resourceFormat);
            },
            "Get the resource format this frame will be processed into."
        )
        .def("CreateJobDecodeAndProcessFrame",
            [](IBlackmagicRawFrame& self, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobDecodeAndProcessFrame(clipProcessingAttributes, frameProcessingAttributes, &job);
                return std::make_tuple(result, job);
            },
            "Create a job that will decode and process our image.",
            "clipProcessingAttributes"_a = nullptr, "frameProcessingAttributes"_a = nullptr
        )
    ;

    py::class_<IBlackmagicRawProcessedImage,IUnknown,std::unique_ptr<IBlackmagicRawProcessedImage,Releaser>>(m, "IBlackmagicRawProcessedImage")
        .def("GetWidth",
            [](IBlackmagicRawProcessedImage& self) {
                uint32_t width = 0;
                HRESULT result = self.GetWidth(&width);
                return std::make_tuple(result, width);
            },
            "Get the width of the processed image."
        )
        .def("GetHeight",
            [](IBlackmagicRawProcessedImage& self) {
                uint32_t height = 0;
                HRESULT result = self.GetHeight(&height);
                return std::make_tuple(result, height);
            },
            "Get the height of the processed image."
        )
        .def("GetResource",
            [](IBlackmagicRawProcessedImage& self) {
                Resource resource = {};
                HRESULT result = self.GetResource(&resource.data);
                return std::make_tuple(result, resource);
            },
            "Get the resource the image is stored in."
        )
        .def("to_py",
            [](IBlackmagicRawProcessedImage& self) -> py::array {
                HRESULT result;
                BlackmagicRawResourceType type = 0;
                result = self.GetResourceType(&type);
                if(result != S_OK) {
                    throw py::buffer_error("failed to query resource type");
                }
                if(type != blackmagicRawResourceTypeBufferCPU) {
                    throw py::buffer_error("not a CPU resource");
                }
                uint32_t sizeBytes = 0;
                result = self.GetResourceSizeBytes(&sizeBytes);
                if(result != S_OK) {
                    throw py::buffer_error("failed to query resource size");
                }
                void* resource = nullptr;
                result = self.GetResource(&resource);
                if(result != S_OK) {
                    throw py::buffer_error("failed to get resource pointer");
                }
                BlackmagicRawResourceFormat format = 0;
                result = self.GetResourceFormat(&format);
                if(result != S_OK) {
                    throw py::buffer_error("failed to query resource format");
                }
                uint32_t width = 0;
                result = self.GetWidth(&width);
                if(result != S_OK) {
                    throw py::buffer_error("failed to query image width");
                }
                uint32_t height = 0;
                result = self.GetHeight(&height);
                if(result != S_OK) {
                    throw py::buffer_error("failed to query image height");
                }
                // The use of a capsule makes this safe. We increment the reference count for the
                // processed frame and make it the base for the array. This will keep the processed
                // frame alive for at least as long as the array viewing its data.
                self.AddRef();
                py::capsule caps(&self, [](void* ptr) {
                    IBlackmagicRawProcessedImage* self = (IBlackmagicRawProcessedImage*)ptr;
                    self->Release();
                });
                switch(format) {
                    case blackmagicRawResourceFormatRGBAU8:
                    case blackmagicRawResourceFormatBGRAU8:
                        return _resource_to_numpy<uint8_t>(std::vector<size_t>{height, width, 4}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatRGBU16:
                        return _resource_to_numpy<uint16_t>(std::vector<size_t>{height, width, 3}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatRGBAU16:
                    case blackmagicRawResourceFormatBGRAU16:
                        return _resource_to_numpy<uint16_t>(std::vector<size_t>{height, width, 4}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatRGBU16Planar:
                        return _resource_to_numpy<uint16_t>(std::vector<size_t>{3, height, width}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatRGBF32:
                        return _resource_to_numpy<float_t>(std::vector<size_t>{height, width, 3}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatRGBF32Planar:
                        return _resource_to_numpy<float_t>(std::vector<size_t>{3, height, width}, sizeBytes, resource, caps);
                    case blackmagicRawResourceFormatBGRAF32:
                        return _resource_to_numpy<float_t>(std::vector<size_t>{height, width, 4}, sizeBytes, resource, caps);
                }
                throw py::buffer_error("unsupported resource format");
            },
            "Get the image as a Numpy array."
        )
        .def("GetResourceType",
            [](IBlackmagicRawProcessedImage& self) {
                BlackmagicRawResourceType type = 0;
                HRESULT result = self.GetResourceType(&type);
                return std::make_tuple(result, type);
            },
            "Get the memory type of the resource."
        )
        .def("GetResourceFormat",
            [](IBlackmagicRawProcessedImage& self) {
                BlackmagicRawResourceFormat format = 0;
                HRESULT result = self.GetResourceFormat(&format);
                return std::make_tuple(result, format);
            },
            "Get the pixel format of the resource."
        )
        .def("GetResourceSizeBytes",
            [](IBlackmagicRawProcessedImage& self) {
                uint32_t sizeBytes = 0;
                HRESULT result = self.GetResourceSizeBytes(&sizeBytes);
                return std::make_tuple(result, sizeBytes);
            },
            "Get the size of the resource in bytes."
        )
        .def("GetResourceContextAndCommandQueue",
            [](IBlackmagicRawProcessedImage& self) {
                void* context = nullptr;
                void* commandQueue = nullptr;
                HRESULT result = self.GetResourceContextAndCommandQueue(&context, &commandQueue);
                return std::make_tuple(result, context, commandQueue);
            },
            "Get the context and command queue that the resource was created on."
        )
    ;

    py::class_<IBlackmagicRawMetadataIterator,IUnknown,std::unique_ptr<IBlackmagicRawMetadataIterator,Releaser>>(m, "IBlackmagicRawMetadataIterator")
        .def("Next",
            &IBlackmagicRawMetadataIterator::Next,
            "Step to the next metadata entry."
        )
        .def("GetKey",
            [](IBlackmagicRawMetadataIterator& self) {
                const char* key = nullptr;
                HRESULT result = self.GetKey(&key);
                return std::make_tuple(result, key);
            },
            "Query the key name of this metadata entry."
        )
        .def("GetData",
            [](IBlackmagicRawMetadataIterator& self) {
                Variant data;
                VariantInit(&data);
                HRESULT result = self.GetData(&data);
                return std::make_tuple(result, data);
            },
            "Query the data in this metadata entry."
        )
    ;

    py::class_<IBlackmagicRawJob,IUnknown,std::unique_ptr<IBlackmagicRawJob,py::nodelete>>(m, "IBlackmagicRawJob")
        .def("Submit",
            &IBlackmagicRawJob::Submit,
            "Submit the job to the decoder, placing it in the decoder's internal queue."
        )
        .def("Abort",
            &IBlackmagicRawJob::Abort,
            "Abort the job."
        )
        .def("SetUserData",
            [](IBlackmagicRawJob& self, py::object object) {
                // If there is already user data attached to the job, release it.
                void* userData = nullptr;
                self.GetUserData(&userData);
                UserDataToPython(userData, true);
                // Set the user data.
                return self.SetUserData(UserDataCreate(object));
            },
            "Attach a generic Python object attached to the job."
            "\n\n"
            "This will cause a memory leak if the job is deleted with user data still attached."
            "You can avoid this by ensuring that `PopUserData()` is called prior to the deletion"
            "of the job object.",
            "userData"_a
        )
        .def("GetUserData",
            [](IBlackmagicRawJob& self) {
                void* userData = nullptr;
                HRESULT result = self.GetUserData(&userData);
                py::object object = UserDataToPython(userData, false);
                return std::make_tuple(result, object);
            },
            "Retrieve the generic Python object attached to the job."
        )
        .def("PopUserData",
            [](IBlackmagicRawJob& self) -> std::tuple<int, py::object> {
                void* userData = nullptr;
                HRESULT result = self.GetUserData(&userData);
                py::object object = UserDataToPython(userData, true);
                HRESULT resultSet = self.SetUserData(nullptr);
                if(SUCCEEDED(result)) {
                    result = resultSet;
                }
                return std::make_tuple(result, object);
            },
            "Retrieve and detach the generic Python object attached to the job."
        )
    ;

    py::class_<IBlackmagicRawClip,IUnknown,std::unique_ptr<IBlackmagicRawClip,Releaser>>(m, "IBlackmagicRawClip")
        .def("GetWidth",
            [](IBlackmagicRawClip& self) {
                uint32_t width = 0;
                HRESULT result = self.GetWidth(&width);
                return std::make_tuple(result, width);
            },
            "Get the width of frames in the clip."
        )
        .def("GetHeight",
            [](IBlackmagicRawClip& self) {
                uint32_t height = 0;
                HRESULT result = self.GetHeight(&height);
                return std::make_tuple(result, height);
            },
            "Get the height of frames in the clip."
        )
        .def("GetFrameRate",
            [](IBlackmagicRawClip& self) {
                float frameRate = 0;
                HRESULT result = self.GetFrameRate(&frameRate);
                return std::make_tuple(result, frameRate);
            },
            "Get the frame rate of the clip in frames per second."
        )
        .def("GetFrameCount",
            [](IBlackmagicRawClip& self) {
                uint64_t frameCount = 0;
                HRESULT result = self.GetFrameCount(&frameCount);
                return std::make_tuple(result, frameCount);
            },
            "Get the number of frames in the clip."
        )
        .def("GetTimecodeForFrame",
            [](IBlackmagicRawClip& self, uint64_t frameIndex) {
                const char* timecode = nullptr;
                HRESULT result = self.GetTimecodeForFrame(frameIndex, &timecode);
                return std::make_tuple(result, timecode);
            },
            "Get the timecode for the specified frame.",
            "frameIndex"_a
        )
        .def("GetMetadataIterator",
            [](IBlackmagicRawClip& self) {
                IBlackmagicRawMetadataIterator* iterator = nullptr;
                HRESULT result = self.GetMetadataIterator(&iterator);
                return std::make_tuple(result, iterator);
            },
            "Create a metadata iterator for this clip."
        )
        .def("GetMetadata",
            [](IBlackmagicRawClip& self, const char* key) {
                Variant value;
                VariantInit(&value);
                HRESULT result = self.GetMetadata(key, &value);
                return std::make_tuple(result, value);
            },
            "Query a single clip metadata value by key.",
            "key"_a
        )
        .def("SetMetadata",
            &IBlackmagicRawClip::SetMetadata,
            "Set metadata to this clip.",
            "key"_a, "value"_a
        )
        .def("GetCameraType",
            [](IBlackmagicRawClip& self) {
                const char* cameraType = nullptr;
                HRESULT result = self.GetCameraType(&cameraType);
                return std::make_tuple(result, cameraType);
            },
            "Get the camera type that this clip was recorded on."
        )
        .def("CloneClipProcessingAttributes",
            [](IBlackmagicRawClip& self) {
                IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes = nullptr;
                HRESULT result = self.CloneClipProcessingAttributes(&clipProcessingAttributes);
                return std::make_tuple(result, clipProcessingAttributes);
            },
            "Create a copy of this clip's processing attributes."
        )
        .def("GetMulticardFileCount",
            [](IBlackmagicRawClip& self) {
                uint32_t multicardFileCount = 0;
                HRESULT result = self.GetMulticardFileCount(&multicardFileCount);
                return std::make_tuple(result, multicardFileCount);
            },
            "Query how many cards this movie was originally recorded onto."
        )
        .def("IsMulticardFilePresent",
            [](IBlackmagicRawClip& self, uint32_t index) {
                bool isMulticardFilePresent = false;
                HRESULT result = self.IsMulticardFilePresent(index, &isMulticardFilePresent);
                return std::make_tuple(result, isMulticardFilePresent);
            },
            "Query if a particular card file from the original recording is present.",
            "index"_a
        )
        .def("GetSidecarFileAttached",
            [](IBlackmagicRawClip& self) {
                bool isSidecarFileAttached = false;
                HRESULT result = self.GetSidecarFileAttached(&isSidecarFileAttached);
                return std::make_tuple(result, isSidecarFileAttached);
            },
            "Return whether a relevant .sidecar file was present on disk."
        )
        .def("SaveSidecarFile",
            &IBlackmagicRawClip::SaveSidecarFile,
            "Save all set metadata and processing attributes to the .sidecar file on disk."
        )
        .def("ReloadSidecarFile",
            &IBlackmagicRawClip::ReloadSidecarFile,
            "Reload the .sidecar file, replacing unsaved metadata and processing attributes."
        )
        .def("CreateJobReadFrame",
            [](IBlackmagicRawClip& self, uint64_t frameIndex) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobReadFrame(frameIndex, &job);
                return std::make_tuple(result, job);
            },
            "Create a job that will read the frame's bit stream into memory.",
            "frameIndex"_a
        )
        .def("CreateJobTrim",
            [](IBlackmagicRawClip& self, const char* fileName, uint64_t frameIndex, uint64_t frameCount, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobTrim(fileName, frameIndex, frameCount, clipProcessingAttributes, frameProcessingAttributes, &job);
                return std::make_tuple(result, job);
            },
            "Create a job that will export part of the clip into a new .braw file.",
            "fileName"_a, "frameIndex"_a, "frameCount"_a,
            "clipProcessingAttributes"_a = nullptr, "frameProcessingAttributes"_a = nullptr
        )
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipEx)
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipAudio)
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipProcessingAttributes)
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipResolutions)
    ;

    py::class_<IBlackmagicRawConfiguration,IUnknown,std::unique_ptr<IBlackmagicRawConfiguration,Releaser>>(m, "IBlackmagicRawConfiguration")
        .def("SetPipeline",
            &IBlackmagicRawConfiguration::SetPipeline,
            "Set the pipeline to use for decoding.",
            "pipeline"_a, "pipelineContext"_a, "pipelineCommandQueue"_a
        )
        .def("GetPipeline",
            [](IBlackmagicRawConfiguration& self) {
                BlackmagicRawPipeline pipeline = 0;
                void* pipelineContextOut = nullptr;
                void* pipelineCommandQueueOut = nullptr;
                HRESULT result = self.GetPipeline(&pipeline, &pipelineContextOut, &pipelineCommandQueueOut);
                return std::make_tuple(result, pipeline, pipelineContextOut, pipelineCommandQueueOut);
            },
            "Get the pipeline used for decoding."
        )
        .def("IsPipelineSupported",
            [](IBlackmagicRawConfiguration& self, BlackmagicRawPipeline pipeline) {
                bool pipelineSupported = 0;
                HRESULT result = self.IsPipelineSupported(pipeline, &pipelineSupported);
                return std::make_tuple(result, pipelineSupported);
            },
            "Determine if a pipeline is supported by this machine.",
            "pipeline"_a
        )
        .def("SetCPUThreads",
            &IBlackmagicRawConfiguration::SetCPUThreads,
            "Set the number of CPU threads to use while decoding.",
            "threadCount"_a
        )
        .def("GetCPUThreads",
            [](IBlackmagicRawConfiguration& self) {
                uint32_t threadCount = 0;
                HRESULT result = self.GetCPUThreads(&threadCount);
                return std::make_tuple(result, threadCount);
            },
            "Get the number of CPU threads to use while decoding."
        )
        .def("GetMaxCPUThreadCount",
            [](IBlackmagicRawConfiguration& self) {
                uint32_t threadCount = 0;
                HRESULT result = self.GetMaxCPUThreadCount(&threadCount);
                return std::make_tuple(result, threadCount);
            },
            "Query the number of hardware threads available on the system."
        )
        .def("SetWriteMetadataPerFrame",
            &IBlackmagicRawConfiguration::SetWriteMetadataPerFrame,
            "Set whether per-frame metadata will be written to only the relevant frame.",
            "writePerFrame"_a
        )
        .def("GetWriteMetadataPerFrame",
            [](IBlackmagicRawConfiguration& self) {
                bool writePerFrame = 0;
                HRESULT result = self.GetWriteMetadataPerFrame(&writePerFrame);
                return std::make_tuple(result, writePerFrame);
            },
            "Check whether per-frame metadata will be written to only the relevant frame."
        )
        .def("SetFromDevice",
            &IBlackmagicRawConfiguration::SetFromDevice,
            "Set the instruction set, pipeline, context, and command queue from the device.",
            "pipelineDevice"_a
        )
    ;

    py::class_<IBlackmagicRawResourceManager,IUnknown,std::unique_ptr<IBlackmagicRawResourceManager,Releaser>>(m, "IBlackmagicRawResourceManager")
        .def("CreateResource",
            [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, uint32_t sizeBytes, BlackmagicRawResourceType type, BlackmagicRawResourceUsage usage) {
                Resource resource = {};
                HRESULT result = self.CreateResource(context, commandQueue, sizeBytes, type, usage, &resource.data);
                return std::make_tuple(result, resource);
            },
            "Create a new resource.",
            "context"_a, "commandQueue"_a, "sizeBytes"_a, "type"_a, "usage"_a
        )
        .def("ReleaseResource",
            [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, Resource resource, BlackmagicRawResourceType type) {
                HRESULT result = self.ReleaseResource(context, commandQueue, resource.data, type);
                return result;
            },
            "Release a resource.",
            "context"_a, "commandQueue"_a, "resource"_a, "type"_a
        )
        .def("CopyResource",
            [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, Resource source, BlackmagicRawResourceType sourceType, Resource destination, BlackmagicRawResourceType destinationType, uint32_t sizeBytes, bool copyAsync) {
                HRESULT result = self.CopyResource(context, commandQueue, source.data, sourceType, destination.data, destinationType, sizeBytes, copyAsync);
                return result;
            },
            "Copy a resource",
            "context"_a, "commandQueue"_a, "source"_a, "sourceType"_a, "destination"_a,
            "destinationType"_a, "sizeBytes"_a, "copyAsync"_a
        )
        .def("GetResourceHostPointer",
            [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, Resource resource, BlackmagicRawResourceType resourceType) {
                void* hostPointer = nullptr;
                HRESULT result = self.GetResourceHostPointer(context, commandQueue, resource.data, resourceType, &hostPointer);
                return std::make_tuple(result, hostPointer);
            },
            "Obtain a pointer to a resource's host addressable memory.",
            "context"_a, "commandQueue"_a, "resource"_a, "resourceType"_a
        )
    ;

    py::class_<BlackmagicRawResourceManager,IBlackmagicRawResourceManager,std::unique_ptr<BlackmagicRawResourceManager,Releaser>>(m, "BlackmagicRawResourceManager")
        .def(py::init<>())
    ;

    py::class_<IBlackmagicRawConfigurationEx,IUnknown,std::unique_ptr<IBlackmagicRawConfigurationEx,Releaser>>(m, "IBlackmagicRawConfigurationEx")
        .def("GetResourceManager",
            [](IBlackmagicRawConfigurationEx& self) {
                IBlackmagicRawResourceManager* resourceManager = nullptr;
                HRESULT result = self.GetResourceManager(&resourceManager);
                return std::make_tuple(result, resourceManager);
            },
            "Get the current resource manager."
        )
        .def("SetResourceManager",
            &IBlackmagicRawConfigurationEx::SetResourceManager,
            "Set the current resource manager.",
            "resourceManager"_a
        )
        .def("GetInstructionSet",
            [](IBlackmagicRawConfigurationEx& self) {
                BlackmagicRawInstructionSet instructionSet = 0;
                HRESULT result = self.GetInstructionSet(&instructionSet);
                return std::make_tuple(result, instructionSet);
            },
            "Get the CPU instruction set used by the decoder."
        )
        .def("SetInstructionSet",
            &IBlackmagicRawConfigurationEx::SetInstructionSet,
            "Set the CPU instruction set used by the decoder.",
            "instructionSet"_a
        )
    ;

    py::class_<IBlackmagicRawConstants,IUnknown,std::unique_ptr<IBlackmagicRawConstants,Releaser>>(m, "IBlackmagicRawConstants")
        .def("GetClipProcessingAttributeRange",
            [](IBlackmagicRawConstants& self, const char* cameraType, BlackmagicRawClipProcessingAttribute attribute) {
                Variant valueMin, valueMax;
                bool isReadOnly = false;
                VariantInit(&valueMin);
                VariantInit(&valueMax);
                HRESULT result = self.GetClipProcessingAttributeRange(cameraType, attribute, &valueMin, &valueMax, &isReadOnly);
                return std::make_tuple(result, valueMin, valueMax, isReadOnly);
            },
            "Get the clip processing attribute range for the specified attribute.",
            "cameraType"_a, "attribute"_a
        )
        .def("GetClipProcessingAttributeList",
            [](IBlackmagicRawConstants& self, const char* cameraType, BlackmagicRawClipProcessingAttribute attribute) {
                std::vector<Variant> array;
                bool isReadOnly = false;
                uint32_t arrayElementCount = 0;
                HRESULT result = self.GetClipProcessingAttributeList(cameraType, attribute, nullptr, &arrayElementCount, &isReadOnly);
                if(SUCCEEDED(result)) {
                    array.resize(arrayElementCount);
                    for(uint32_t i = 0; i < arrayElementCount; ++i) {
                        VariantInit(&array[i]);
                    }
                    result = self.GetClipProcessingAttributeList(cameraType, attribute, &array[0], &arrayElementCount, &isReadOnly);
                }
                return std::make_tuple(result, array, arrayElementCount, isReadOnly);
            },
            "Get the clip processing attribute value list for the specified attribute.",
            "cameraType"_a, "attribute"_a
        )
        .def("GetFrameProcessingAttributeRange",
            [](IBlackmagicRawConstants& self, const char* cameraType, BlackmagicRawFrameProcessingAttribute attribute) {
                Variant valueMin, valueMax;
                bool isReadOnly = false;
                VariantInit(&valueMin);
                VariantInit(&valueMax);
                HRESULT result = self.GetFrameProcessingAttributeRange(cameraType, attribute, &valueMin, &valueMax, &isReadOnly);
                return std::make_tuple(result, valueMin, valueMax, isReadOnly);
            },
            "Get the frame processing attribute range for the specified attribute.",
            "cameraType"_a, "attribute"_a
        )
        .def("GetFrameProcessingAttributeList",
            [](IBlackmagicRawConstants& self, const char* cameraType, BlackmagicRawFrameProcessingAttribute attribute) {
                std::vector<Variant> array;
                bool isReadOnly = false;
                uint32_t arrayElementCount = 0;
                HRESULT result = self.GetFrameProcessingAttributeList(cameraType, attribute, nullptr, &arrayElementCount, &isReadOnly);
                if(SUCCEEDED(result)) {
                    array.resize(arrayElementCount);
                    for(uint32_t i = 0; i < arrayElementCount; ++i) {
                        VariantInit(&array[i]);
                    }
                    result = self.GetFrameProcessingAttributeList(cameraType, attribute, &array[0], &arrayElementCount, &isReadOnly);
                }
                return std::make_tuple(result, array, arrayElementCount, isReadOnly);
            },
            "Get the frame processing attribute value list for the specified attribute.",
            "cameraType"_a, "attribute"_a
        )
        .def("GetISOListForAnalogGain",
            [](IBlackmagicRawConstants& self, const char* cameraType, float analogGain, bool analogGainIsConstant) {
                std::vector<uint32_t> array;
                bool isReadOnly = false;
                uint32_t arrayElementCount = 0;
                HRESULT result = self.GetISOListForAnalogGain(cameraType, analogGain, analogGainIsConstant, nullptr, &arrayElementCount, &isReadOnly);
                if(SUCCEEDED(result)) {
                    array.resize(arrayElementCount);
                    std::fill(array.begin(), array.end(), 0);
                    result = self.GetISOListForAnalogGain(cameraType, analogGain, analogGainIsConstant, &array[0], &arrayElementCount, &isReadOnly);
                }
                return std::make_tuple(result, array, arrayElementCount, isReadOnly);
            },
            "Get the frame processing attribute value list for the specified attribute.",
            "cameraType"_a, "analogGain"_a, "analogGainIsConstant"_a
        )
    ;

    py::class_<IBlackmagicRawManualDecoderFlow1,IUnknown,std::unique_ptr<IBlackmagicRawManualDecoderFlow1,Releaser>>(m, "IBlackmagicRawManualDecoderFlow1")
        .def("PopulateFrameStateBuffer",
            [](IBlackmagicRawManualDecoderFlow1& self, IBlackmagicRawFrame* frame, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes, Resource frameState, uint32_t frameStateSizeBytes) {
                HRESULT result = self.PopulateFrameStateBuffer(frame, clipProcessingAttributes, frameProcessingAttributes, frameState.data, frameStateSizeBytes);
                return result;
            },
            "Convert the internal state of an IBlackmagicRawFrame to a frame state buffer.",
            "frame"_a, "clipProcessingAttributes"_a, "frameProcessingAttributes"_a, "frameState"_a, "frameStateSizeBytes"_a
        )
        .def("GetFrameStateSizeBytes",
            [](IBlackmagicRawManualDecoderFlow1& self) {
                uint32_t frameStateSizeBytes = 0;
                HRESULT result = self.GetFrameStateSizeBytes(&frameStateSizeBytes);
                return std::make_tuple(result, frameStateSizeBytes);
            },
            "Query the size of the frame state buffer in bytes."
        )
        .def("GetDecodedSizeBytes",
            [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
                uint32_t decodedSizeBytes = 0;
                HRESULT result = self.GetDecodedSizeBytes(frameStateBufferCPU.data, &decodedSizeBytes);
                return std::make_tuple(result, decodedSizeBytes);
            },
            "Query the size of the decoded buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("GetProcessedSizeBytes",
            [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
                uint32_t processedSizeBytes = 0;
                HRESULT result = self.GetProcessedSizeBytes(frameStateBufferCPU.data, &processedSizeBytes);
                return std::make_tuple(result, processedSizeBytes);
            },
            "Query the size of the processed buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("GetPost3DLUTSizeBytes",
            [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
                uint32_t post3DLUTSizeBytes = 0;
                HRESULT result = self.GetPost3DLUTSizeBytes(frameStateBufferCPU.data, &post3DLUTSizeBytes);
                return std::make_tuple(result, post3DLUTSizeBytes);
            },
            "Query the size of the post 3D LUT buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("CreateJobDecode",
            [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU, Resource bitStreamBufferCPU, Resource decodedBufferCPU) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobDecode(frameStateBufferCPU.data, bitStreamBufferCPU.data, decodedBufferCPU.data, &job);
                return std::make_tuple(result, job);
            },
            "Create a job to decode a frame.",
            "frameStateBufferCPU"_a, "bitStreamBufferCPU"_a, "decodedBufferCPU"_a
        )
        .def("CreateJobProcess",
            [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU, Resource decodedBufferCPU, Resource processedBufferCPU, Resource post3DLUTBufferCPU) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobProcess(frameStateBufferCPU.data, decodedBufferCPU.data, processedBufferCPU.data, post3DLUTBufferCPU.data, &job);
                return std::make_tuple(result, job);
            },
            "Create a job to process a frame.",
            "frameStateBufferCPU"_a, "decodedBufferCPU"_a, "processedBufferCPU"_a, "post3DLUTBufferCPU"_a
        )
    ;

    py::class_<IBlackmagicRawManualDecoderFlow2,IUnknown,std::unique_ptr<IBlackmagicRawManualDecoderFlow2,Releaser>>(m, "IBlackmagicRawManualDecoderFlow2")
        .def("PopulateFrameStateBuffer",
            [](IBlackmagicRawManualDecoderFlow2& self, IBlackmagicRawFrame* frame, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes, Resource frameState, uint32_t frameStateSizeBytes) {
                HRESULT result = self.PopulateFrameStateBuffer(frame, clipProcessingAttributes, frameProcessingAttributes, frameState.data, frameStateSizeBytes);
                return result;
            },
            "Convert the internal state of an IBlackmagicRawFrame to a frame state buffer.",
            "frame"_a, "clipProcessingAttributes"_a, "frameProcessingAttributes"_a, "frameState"_a, "frameStateSizeBytes"_a
        )
        .def("GetFrameStateSizeBytes",
            [](IBlackmagicRawManualDecoderFlow2& self) {
                uint32_t frameStateSizeBytes = 0;
                HRESULT result = self.GetFrameStateSizeBytes(&frameStateSizeBytes);
                return std::make_tuple(result, frameStateSizeBytes);
            },
            "Query the size of the frame state buffer in bytes."
        )
        .def("GetDecodedSizeBytes",
            [](IBlackmagicRawManualDecoderFlow2& self, Resource frameStateBufferCPU) {
                uint32_t decodedSizeBytes = 0;
                HRESULT result = self.GetDecodedSizeBytes(frameStateBufferCPU.data, &decodedSizeBytes);
                return std::make_tuple(result, decodedSizeBytes);
            },
            "Query the size of the decoded buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("GetWorkingSizeBytes",
            [](IBlackmagicRawManualDecoderFlow2& self, Resource frameStateBufferCPU) {
                uint32_t workingSizeBytes = 0;
                HRESULT result = self.GetWorkingSizeBytes(frameStateBufferCPU.data, &workingSizeBytes);
                return std::make_tuple(result, workingSizeBytes);
            },
            "Query the size of the working buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("GetProcessedSizeBytes",
            [](IBlackmagicRawManualDecoderFlow2& self, Resource frameStateBufferCPU) {
                uint32_t processedSizeBytes = 0;
                HRESULT result = self.GetProcessedSizeBytes(frameStateBufferCPU.data, &processedSizeBytes);
                return std::make_tuple(result, processedSizeBytes);
            },
            "Query the size of the processed buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("GetPost3DLUTSizeBytes",
            [](IBlackmagicRawManualDecoderFlow2& self, Resource frameStateBufferCPU) {
                uint32_t post3DLUTSizeBytes = 0;
                HRESULT result = self.GetPost3DLUTSizeBytes(frameStateBufferCPU.data, &post3DLUTSizeBytes);
                return std::make_tuple(result, post3DLUTSizeBytes);
            },
            "Query the size of the post 3D LUT buffer in bytes.",
            "frameStateBufferCPU"_a
        )
        .def("CreateJobDecode",
            [](IBlackmagicRawManualDecoderFlow2& self, Resource frameStateBufferCPU, Resource bitStreamBufferCPU, Resource decodedBufferCPU) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobDecode(frameStateBufferCPU.data, bitStreamBufferCPU.data, decodedBufferCPU.data, &job);
                return std::make_tuple(result, job);
            },
            "Create a job to decode a frame.",
            "frameStateBufferCPU"_a, "bitStreamBufferCPU"_a, "decodedBufferCPU"_a
        )
        .def("CreateJobProcess",
            [](IBlackmagicRawManualDecoderFlow2& self, void* context, void* commandQueue, Resource frameStateBufferCPU, Resource decodedBufferGPU, Resource workingBufferGpu, Resource processedBufferGPU, Resource post3DLUTBufferGPU) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobProcess(context, commandQueue, frameStateBufferCPU.data, decodedBufferGPU.data, workingBufferGpu.data, processedBufferGPU.data, post3DLUTBufferGPU.data, &job);
                return std::make_tuple(result, job);
            },
            "Create a job to process a frame.",
            "context"_a, "commandQueue"_a, "frameStateBufferCPU"_a, "decodedBufferGPU"_a,
            "workingBufferGPU"_a, "processedBufferGPU"_a, "post3DLUTBufferGPU"_a
        )
    ;

    py::class_<IBlackmagicRawToneCurve,IUnknown,std::unique_ptr<IBlackmagicRawToneCurve,Releaser>>(m, "IBlackmagicRawToneCurve")
        // TODO: Add missing bindings.
    ;

    py::class_<IBlackmagicRaw,IUnknown,std::unique_ptr<IBlackmagicRaw,Releaser>>(m, "IBlackmagicRaw")
        .def("OpenClip",
            [](IBlackmagicRaw& self, const char* fileName) {
                IBlackmagicRawClip* clip = nullptr;
                HRESULT result = self.OpenClip(fileName, &clip);
                return std::make_tuple(result, clip);
            },
            "Open a clip.",
            "fileName"_a
        )
        .def("SetCallback",
            &IBlackmagicRaw::SetCallback,
            "Register a callback with the codec object.",
            "callback"_a
        )
        .def("PreparePipeline",
            [](IBlackmagicRaw& self, BlackmagicRawPipeline pipeline, void* pipelineContext, void* pipelineCommandQueue, py::object userData) {
                return self.PreparePipeline(pipeline, pipelineContext, pipelineCommandQueue, UserDataCreate(userData));
            },
            "Asynchronously prepare the pipeline for decoding.",
            "pipeline"_a, "pipelineContext"_a, "pipelineCommandQueue"_a, "userData"_a
        )
        .def("PreparePipelineForDevice",
            [](IBlackmagicRaw& self, IBlackmagicRawPipelineDevice* pipelineDevice, py::object userData) {
                return self.PreparePipelineForDevice(pipelineDevice, UserDataCreate(userData));
            },
            "Asynchronously prepare the pipeline for decoding.",
            "pipelineDevice"_a, "userData"_a
        )
        .def("FlushJobs",
            &IBlackmagicRaw::FlushJobs,
            py::call_guard<py::gil_scoped_release>(),
            "Wait for all jobs to complete."
        )
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawConfiguration)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawConfigurationEx)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawConstants)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawManualDecoderFlow1)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawManualDecoderFlow2)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawToneCurve)
    ;

    py::class_<IBlackmagicRawPipelineIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineIterator,Releaser>>(m, "IBlackmagicRawPipelineIterator")
        .def("Next",
            &IBlackmagicRawPipelineIterator::Next,
            "Step to the next pipeline entry."
        )
        .def("GetName",
            [](IBlackmagicRawPipelineIterator& self) {
                const char* pipelineName = nullptr;
                HRESULT result = self.GetName(&pipelineName);
                return std::make_tuple(result, pipelineName);
            },
            "Get the name of the pipeline."
        )
        .def("GetInterop",
            [](IBlackmagicRawPipelineIterator& self) {
                BlackmagicRawInterop interop = 0;
                HRESULT result = self.GetInterop(&interop);
                return std::make_tuple(result, interop);
            },
            "Get the interoperability of the pipeline."
        )
        .def("GetPipeline",
            [](IBlackmagicRawPipelineIterator& self) {
                BlackmagicRawPipeline pipeline = 0;
                HRESULT result = self.GetPipeline(&pipeline);
                return std::make_tuple(result, pipeline);
            },
            "Get the pipeline."
        )
    ;

    py::class_<IBlackmagicRawOpenGLInteropHelper,IUnknown,std::unique_ptr<IBlackmagicRawOpenGLInteropHelper,Releaser>>(m, "IBlackmagicRawOpenGLInteropHelper")
        .def("GetPreferredResourceFormat",
            [](IBlackmagicRawOpenGLInteropHelper& self) {
                BlackmagicRawResourceFormat preferredFormat = 0;
                HRESULT result = self.GetPreferredResourceFormat(&preferredFormat);
                return std::make_tuple(result, preferredFormat);
            },
            "Get the preferred resource format for interaction between the device and OpenGL."
        )
        .def("SetImage",
            [](IBlackmagicRawOpenGLInteropHelper& self, IBlackmagicRawProcessedImage* processedImage) {
                uint32_t openGLTextureName = 0;
                int32_t openGLTextureTarget = 0;
                HRESULT result = self.SetImage(processedImage, &openGLTextureName, &openGLTextureTarget);
                return std::make_tuple(result, openGLTextureName, openGLTextureTarget);
            },
            "Copy the processed image into an OpenGL texture.",
            "processedImage"_a
        )
    ;

    py::class_<IBlackmagicRawPipelineDevice,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDevice,Releaser>>(m, "IBlackmagicRawPipelineDevice")
        .def("SetBestInstructionSet",
            &IBlackmagicRawPipelineDevice::SetBestInstructionSet,
            "Set the CPU instruction set of the device according to the best system capabilities."
        )
        .def("SetInstructionSet",
            &IBlackmagicRawPipelineDevice::SetInstructionSet,
            "Set the CPU instruction set to use for the device.",
            "instructionSet"_a
        )
        .def("GetInstructionSet",
            [](IBlackmagicRawPipelineDevice& self) {
                BlackmagicRawInstructionSet instructionSet = 0;
                HRESULT result = self.GetInstructionSet(&instructionSet);
                return std::make_tuple(result, instructionSet);
            },
            "Get the CPU instruction set of the device."
        )
        .def("GetIndex",
            [](IBlackmagicRawPipelineDevice& self) {
                uint32_t deviceIndex = 0;
                HRESULT result = self.GetIndex(&deviceIndex);
                return std::make_tuple(result, deviceIndex);
            },
            "Get the index of the device in the pipeline's device list."
        )
        .def("GetName",
            [](IBlackmagicRawPipelineDevice& self) {
                const char* name = nullptr;
                HRESULT result = self.GetName(&name);
                return std::make_tuple(result, name);
            },
            "Get the name of the device."
        )
        .def("GetInterop",
            [](IBlackmagicRawPipelineDevice& self) {
                BlackmagicRawInterop interop = 0;
                HRESULT result = self.GetInterop(&interop);
                return std::make_tuple(result, interop);
            },
            "Get the API interoperability of the device."
        )
        .def("GetPipeline",
            [](IBlackmagicRawPipelineDevice& self) {
                BlackmagicRawPipeline pipeline = 0;
                void* context = nullptr;
                void* commandQueue = nullptr;
                HRESULT result = self.GetPipeline(&pipeline, &context, &commandQueue);
                return std::make_tuple(result, pipeline, context, commandQueue);
            },
            "Get the pipeline configuration information associated with the device."
        )
        .def("GetPipelineName",
            [](IBlackmagicRawPipelineDevice& self) {
                const char* pipelineName = nullptr;
                HRESULT result = self.GetPipelineName(&pipelineName);
                return std::make_tuple(result, pipelineName);
            },
            "Get the name of the pipeline associated with the device."
        )
        .def("GetOpenGLInteropHelper",
            [](IBlackmagicRawPipelineDevice& self) {
                IBlackmagicRawOpenGLInteropHelper* interopHelper = nullptr;
                HRESULT result = self.GetOpenGLInteropHelper(&interopHelper);
                return std::make_tuple(result, interopHelper);
            },
            "Create a helper to get the results of a processed image as an OpenGL texture."
        )
    ;

    py::class_<IBlackmagicRawPipelineDeviceIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDeviceIterator,Releaser>>(m, "IBlackmagicRawPipelineDeviceIterator")
        .def("Next",
            &IBlackmagicRawPipelineDeviceIterator::Next,
            "Step to the next device entry."
        )
        .def("GetPipeline",
            [](IBlackmagicRawPipelineDeviceIterator& self) {
                BlackmagicRawPipeline pipeline = 0;
                HRESULT result = self.GetPipeline(&pipeline);
                return std::make_tuple(result, pipeline);
            },
            "Get the pipeline."
        )
        .def("GetInterop",
            [](IBlackmagicRawPipelineDeviceIterator& self) {
                BlackmagicRawInterop interop = 0;
                HRESULT result = self.GetInterop(&interop);
                return std::make_tuple(result, interop);
            },
            "Get the interoperability of the device's pipeline."
        )
        .def("CreateDevice",
            [](IBlackmagicRawPipelineDeviceIterator& self) {
                IBlackmagicRawPipelineDevice* pipelineDevice = nullptr;
                HRESULT result = self.CreateDevice(&pipelineDevice);
                return std::make_tuple(result, pipelineDevice);
            },
            "Create the pipeline device (container for context and command queue)."
        )
    ;

    py::class_<IBlackmagicRawFactory,IUnknown,std::unique_ptr<IBlackmagicRawFactory,Releaser>>(m, "IBlackmagicRawFactory")
        .def("CreateCodec",
            [](IBlackmagicRawFactory& self) {
                IBlackmagicRaw* codec = nullptr;
                HRESULT result = self.CreateCodec(&codec);
                return std::make_tuple(result, codec);
            },
            "Create a codec from the factory."
        )
        .def("CreatePipelineIterator",
            [](IBlackmagicRawFactory& self, BlackmagicRawInterop interop) {
                IBlackmagicRawPipelineIterator* pipelineIterator = nullptr;
                HRESULT result = self.CreatePipelineIterator(interop, &pipelineIterator);
                return std::make_tuple(result, pipelineIterator);
            },
            "Create a pipeline iterator from the factory.",
            "interop"_a
        )
        .def("CreatePipelineDeviceIterator",
            [](IBlackmagicRawFactory& self, BlackmagicRawPipeline pipeline, BlackmagicRawInterop interop) {
                IBlackmagicRawPipelineDeviceIterator* deviceIterator = nullptr;
                HRESULT result = self.CreatePipelineDeviceIterator(pipeline, interop, &deviceIterator);
                return std::make_tuple(result, deviceIterator);
            },
            "Create a pipeline device iterator from the factory.",
            "pipeline"_a, "interop"_a
        )
    ;
}
