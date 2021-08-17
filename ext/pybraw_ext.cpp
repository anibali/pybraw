#include "BlackmagicRawAPI.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <atomic>

namespace py = pybind11;


#define DEF_QUERY_INTERFACE(S, T)\
.def("as_"#T, [](S& self) {\
    LPVOID pv = nullptr;\
    HRESULT result = self.QueryInterface(IID_##T, &pv);\
    return std::make_tuple(result, (T*)pv);\
})


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


// Concrete subclass of IBlackmagicRawCallback which provides an implementation of reference
// counting and placeholders for each of the callback functions.
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
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, DecodeComplete, job, result);
    }

    void TrimProgress(IBlackmagicRawJob* job, float progress) override {
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, TrimProgress, job, progress);
    }

    void TrimComplete(IBlackmagicRawJob* job, HRESULT result) override {
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, TrimComplete, job, result);
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
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, PreparePipelineComplete, userData, result);
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
        })
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
        .def("GetMaxBitStreamSizeBytes", [](IBlackmagicRawClipEx& self) {
            uint32_t maxBitStreamSizeBytes = 0;
            HRESULT result = self.GetMaxBitStreamSizeBytes(&maxBitStreamSizeBytes);
            return std::make_tuple(result, maxBitStreamSizeBytes);
        })
        .def("GetBitStreamSizeBytes", [](IBlackmagicRawClipEx& self, uint64_t frameIndex) {
            uint32_t bitStreamSizeBytes = 0;
            HRESULT result = self.GetBitStreamSizeBytes(frameIndex, &bitStreamSizeBytes);
            return std::make_tuple(result, bitStreamSizeBytes);
        })
        .def("CreateJobReadFrame", [](IBlackmagicRawClipEx& self, uint64_t frameIndex, Resource bitStream, uint32_t bitStreamSizeBytes) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobReadFrame(frameIndex, bitStream.data, bitStreamSizeBytes, &job);
            return std::make_tuple(result, job);
        })
        .def("QueryTimecodeInfo", [](IBlackmagicRawClipEx& self) {
            uint32_t baseFrameIndex = 0;
            bool isDropFrameTimecode = false;
            HRESULT result = self.QueryTimecodeInfo(&baseFrameIndex, &isDropFrameTimecode);
            return std::make_tuple(result, baseFrameIndex, isDropFrameTimecode);
        })
    ;

    py::class_<IBlackmagicRawPost3DLUT,IUnknown,std::unique_ptr<IBlackmagicRawPost3DLUT,Releaser>>(m, "IBlackmagicRawPost3DLUT")
        .def("GetName", [](IBlackmagicRawPost3DLUT& self) {
            const char* name = nullptr;
            HRESULT result = self.GetName(&name);
            return std::make_tuple(result, name);
        })
        .def("GetTitle", [](IBlackmagicRawPost3DLUT& self) {
            const char* title = nullptr;
            HRESULT result = self.GetTitle(&title);
            return std::make_tuple(result, title);
        })
        .def("GetSize", [](IBlackmagicRawPost3DLUT& self) {
            uint32_t size = 0;
            HRESULT result = self.GetSize(&size);
            return std::make_tuple(result, size);
        })
        .def("GetResourceGPU", [](IBlackmagicRawPost3DLUT& self, void* context, void* commandQueue) {
            Resource resource = {};
            BlackmagicRawResourceType type = 0;
            HRESULT result = self.GetResourceGPU(context, commandQueue, &type, &resource.data);
            return std::make_tuple(result, type, resource);
        })
        .def("GetResourceCPU", [](IBlackmagicRawPost3DLUT& self) {
            Resource resource = {};
            HRESULT result = self.GetResourceCPU(&resource.data);
            return std::make_tuple(result, resource);
        })
        .def("GetResourceSizeBytes", [](IBlackmagicRawPost3DLUT& self) {
            uint32_t sizeBytes = 0;
            HRESULT result = self.GetResourceSizeBytes(&sizeBytes);
            return std::make_tuple(result, sizeBytes);
        })
    ;

    py::class_<IBlackmagicRawClipProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawClipProcessingAttributes,Releaser>>(m, "IBlackmagicRawClipProcessingAttributes")
        .def("GetClipAttribute", [](IBlackmagicRawClipProcessingAttributes& self, BlackmagicRawClipProcessingAttribute attribute) {
            Variant value;
            VariantInit(&value);
            HRESULT result = self.GetClipAttribute(attribute, &value);
            return std::make_tuple(result, value);
        })
        .def("SetClipAttribute", &IBlackmagicRawClipProcessingAttributes::SetClipAttribute)
        .def("GetPost3DLUT", [](IBlackmagicRawClipProcessingAttributes& self) {
            IBlackmagicRawPost3DLUT* lut = nullptr;
            HRESULT result = self.GetPost3DLUT(&lut);
            return std::make_tuple(result, lut);
        })
    ;

    py::class_<IBlackmagicRawFrameProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawFrameProcessingAttributes,Releaser>>(m, "IBlackmagicRawFrameProcessingAttributes")
        .def("GetFrameAttribute", [](IBlackmagicRawFrameProcessingAttributes& self, BlackmagicRawFrameProcessingAttribute attribute) {
            Variant value;
            VariantInit(&value);
            HRESULT result = self.GetFrameAttribute(attribute, &value);
            return std::make_tuple(result, value);
        })
        .def("SetFrameAttribute", &IBlackmagicRawFrameProcessingAttributes::SetFrameAttribute)
    ;

    py::class_<IBlackmagicRawFrame,IUnknown,std::unique_ptr<IBlackmagicRawFrame,Releaser>>(m, "IBlackmagicRawFrame")
        .def("GetFrameIndex", [](IBlackmagicRawFrame& self) {
            uint64_t frameIndex = 0;
            HRESULT result = self.GetFrameIndex(&frameIndex);
            return std::make_tuple(result, frameIndex);
        })
        .def("GetTimecode", [](IBlackmagicRawFrame& self) {
            const char* timecode = nullptr;
            HRESULT result = self.GetTimecode(&timecode);
            return std::make_tuple(result, timecode);
        })
        .def("GetMetadataIterator", [](IBlackmagicRawFrame& self) {
            IBlackmagicRawMetadataIterator* iterator = nullptr;
            HRESULT result = self.GetMetadataIterator(&iterator);
            return std::make_tuple(result, iterator);
        })
        .def("GetMetadata", [](IBlackmagicRawFrame& self, const char* key) {
            Variant value;
            VariantInit(&value);
            HRESULT result = self.GetMetadata(key, &value);
            return std::make_tuple(result, value);
        })
        .def("SetMetadata", &IBlackmagicRawFrame::SetMetadata)
        .def("CloneFrameProcessingAttributes", [](IBlackmagicRawFrame& self) {
            IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes = nullptr;
            HRESULT result = self.CloneFrameProcessingAttributes(&frameProcessingAttributes);
            return std::make_tuple(result, frameProcessingAttributes);
        })
        .def("SetResolutionScale", &IBlackmagicRawFrame::SetResolutionScale)
        .def("GetResolutionScale", [](IBlackmagicRawFrame& self) {
            BlackmagicRawResolutionScale resolutionScale = 0;
            HRESULT result = self.GetResolutionScale(&resolutionScale);
            return std::make_tuple(result, resolutionScale);
        })
        .def("SetResourceFormat", &IBlackmagicRawFrame::SetResourceFormat)
        .def("GetResourceFormat", [](IBlackmagicRawFrame& self) {
            BlackmagicRawResourceFormat resourceFormat = 0;
            HRESULT result = self.GetResourceFormat(&resourceFormat);
            return std::make_tuple(result, resourceFormat);
        })
        .def("CreateJobDecodeAndProcessFrame", [](IBlackmagicRawFrame& self, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobDecodeAndProcessFrame(clipProcessingAttributes, frameProcessingAttributes, &job);
            return std::make_tuple(result, job);
        }, "Create a job that will decode and process our image.",
        py::arg("clipProcessingAttributes").none(true) = nullptr, py::arg("frameProcessingAttributes").none(true) = nullptr)
    ;

    py::class_<IBlackmagicRawProcessedImage,IUnknown,std::unique_ptr<IBlackmagicRawProcessedImage,Releaser>>(m, "IBlackmagicRawProcessedImage")
        .def("GetWidth", [](IBlackmagicRawProcessedImage& self) {
            uint32_t width = 0;
            HRESULT result = self.GetWidth(&width);
            return std::make_tuple(result, width);
        })
        .def("GetHeight", [](IBlackmagicRawProcessedImage& self) {
            uint32_t height = 0;
            HRESULT result = self.GetHeight(&height);
            return std::make_tuple(result, height);
        })
        .def("GetResource", [](IBlackmagicRawProcessedImage& self) {
            Resource resource = {};
            HRESULT result = self.GetResource(&resource.data);
            return std::make_tuple(result, resource);
        })
        .def("to_py", [](IBlackmagicRawProcessedImage& self) -> py::array {
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
        })
        .def("GetResourceType", [](IBlackmagicRawProcessedImage& self) {
            BlackmagicRawResourceType type = 0;
            HRESULT result = self.GetResourceType(&type);
            return std::make_tuple(result, type);
        })
        .def("GetResourceFormat", [](IBlackmagicRawProcessedImage& self) {
            BlackmagicRawResourceFormat format = 0;
            HRESULT result = self.GetResourceFormat(&format);
            return std::make_tuple(result, format);
        })
        .def("GetResourceSizeBytes", [](IBlackmagicRawProcessedImage& self) {
            uint32_t sizeBytes = 0;
            HRESULT result = self.GetResourceSizeBytes(&sizeBytes);
            return std::make_tuple(result, sizeBytes);
        })
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawMetadataIterator,IUnknown,std::unique_ptr<IBlackmagicRawMetadataIterator,Releaser>>(m, "IBlackmagicRawMetadataIterator")
        .def("Next", &IBlackmagicRawMetadataIterator::Next)
        .def("GetKey", [](IBlackmagicRawMetadataIterator& self) {
            const char* key = nullptr;
            HRESULT result = self.GetKey(&key);
            return std::make_tuple(result, key);
        })
        .def("GetData", [](IBlackmagicRawMetadataIterator& self) {
            Variant data;
            VariantInit(&data);
            HRESULT result = self.GetData(&data);
            return std::make_tuple(result, data);
        })
    ;

    py::class_<IBlackmagicRawJob,IUnknown,std::unique_ptr<IBlackmagicRawJob,py::nodelete>>(m, "IBlackmagicRawJob")
        .def("Submit", &IBlackmagicRawJob::Submit)
        .def("Abort", &IBlackmagicRawJob::Abort)
//        .def("SetUserData", &IBlackmagicRawJob::SetUserData)
//        .def("GetUserData", [](IBlackmagicRawJob& self) {
//            void* userData = nullptr;
//            HRESULT result = self.GetUserData(&userData);
//            return std::make_tuple(result, userData);
//        })
        .def("put_py_user_data", [](IBlackmagicRawJob& self, py::object object) {
            // Increases the reference count for the Python object. A call to put_py_user_data
            // should be paired with a call to pop_py_user_data to avoid memory leaks.
            py::object* ref = new py::object(object);
            HRESULT result = self.SetUserData((void*)ref);
            return result;
        })
        .def("pop_py_user_data", [](IBlackmagicRawJob& self) -> std::tuple<int, py::object> {
            void* userData = nullptr;
            HRESULT result = self.GetUserData(&userData);
            if(result != S_OK or userData == nullptr) {
                return std::make_tuple(result, py::none());
            }
            py::object object = (*(py::object*)userData);
            result = self.SetUserData(nullptr);
            delete (py::object*)userData;
            return std::make_tuple(result, object);
        })
    ;

    py::class_<IBlackmagicRawClip,IUnknown,std::unique_ptr<IBlackmagicRawClip,Releaser>>(m, "IBlackmagicRawClip")
        .def("GetWidth", [](IBlackmagicRawClip& self) {
            uint32_t width = 0;
            HRESULT result = self.GetWidth(&width);
            return std::make_tuple(result, width);
        })
        .def("GetHeight", [](IBlackmagicRawClip& self) {
            uint32_t height = 0;
            HRESULT result = self.GetHeight(&height);
            return std::make_tuple(result, height);
        })
        .def("GetFrameRate", [](IBlackmagicRawClip& self) {
            float frameRate = 0;
            HRESULT result = self.GetFrameRate(&frameRate);
            return std::make_tuple(result, frameRate);
        })
        .def("GetFrameCount", [](IBlackmagicRawClip& self) {
            uint64_t frameCount = 0;
            HRESULT result = self.GetFrameCount(&frameCount);
            return std::make_tuple(result, frameCount);
        })
        .def("GetTimecodeForFrame", [](IBlackmagicRawClip& self, uint64_t frameIndex) {
            const char* timecode = nullptr;
            HRESULT result = self.GetTimecodeForFrame(frameIndex, &timecode);
            return std::make_tuple(result, timecode);
        })
        .def("GetMetadataIterator", [](IBlackmagicRawClip& self) {
            IBlackmagicRawMetadataIterator* iterator = nullptr;
            HRESULT result = self.GetMetadataIterator(&iterator);
            return std::make_tuple(result, iterator);
        })
        .def("GetMetadata", [](IBlackmagicRawClip& self, const char* key) {
            Variant value;
            VariantInit(&value);
            HRESULT result = self.GetMetadata(key, &value);
            return std::make_tuple(result, value);
        })
        .def("SetMetadata", &IBlackmagicRawClip::SetMetadata)
        .def("GetCameraType", [](IBlackmagicRawClip& self) {
            const char* cameraType = nullptr;
            HRESULT result = self.GetCameraType(&cameraType);
            return std::make_tuple(result, cameraType);
        })
        .def("CloneClipProcessingAttributes", [](IBlackmagicRawClip& self) {
            IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes = nullptr;
            HRESULT result = self.CloneClipProcessingAttributes(&clipProcessingAttributes);
            return std::make_tuple(result, clipProcessingAttributes);
        })
        .def("GetMulticardFileCount", [](IBlackmagicRawClip& self) {
            uint32_t multicardFileCount = 0;
            HRESULT result = self.GetMulticardFileCount(&multicardFileCount);
            return std::make_tuple(result, multicardFileCount);
        })
        .def("IsMulticardFilePresent", [](IBlackmagicRawClip& self, uint32_t index) {
            bool isMulticardFilePresent = false;
            HRESULT result = self.IsMulticardFilePresent(index, &isMulticardFilePresent);
            return std::make_tuple(result, isMulticardFilePresent);
        })
        .def("GetSidecarFileAttached", [](IBlackmagicRawClip& self) {
            bool isSidecarFileAttached = false;
            HRESULT result = self.GetSidecarFileAttached(&isSidecarFileAttached);
            return std::make_tuple(result, isSidecarFileAttached);
        })
        .def("SaveSidecarFile", &IBlackmagicRawClip::SaveSidecarFile)
        .def("ReloadSidecarFile", &IBlackmagicRawClip::ReloadSidecarFile)
        .def("CreateJobReadFrame", [](IBlackmagicRawClip& self, uint64_t frameIndex) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobReadFrame(frameIndex, &job);
            return std::make_tuple(result, job);
        })
        .def(
            "CreateJobTrim",
            [](IBlackmagicRawClip& self, const char* fileName, uint64_t frameIndex, uint64_t frameCount, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes) {
                IBlackmagicRawJob* job = nullptr;
                HRESULT result = self.CreateJobTrim(fileName, frameIndex, frameCount, clipProcessingAttributes, frameProcessingAttributes, &job);
                return std::make_tuple(result, job);
            },
            "Create a job that will export part of the clip into a new .braw file.",
            py::arg("fileName"),
            py::arg("frameIndex"),
            py::arg("frameCount"),
            py::arg("clipProcessingAttributes").none(true) = nullptr,
            py::arg("frameProcessingAttributes").none(true) = nullptr
        )
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipEx)
        DEF_QUERY_INTERFACE(IBlackmagicRawClip, IBlackmagicRawClipProcessingAttributes)
    ;

    py::class_<IBlackmagicRawConfiguration,IUnknown,std::unique_ptr<IBlackmagicRawConfiguration,Releaser>>(m, "IBlackmagicRawConfiguration")
        // TODO: Add missing bindings
        .def("IsPipelineSupported", [](IBlackmagicRawConfiguration& self, BlackmagicRawPipeline pipeline) {
            bool pipelineSupported = 0;
            HRESULT result = self.IsPipelineSupported(pipeline, &pipelineSupported);
            return std::make_tuple(result, pipelineSupported);
        })
        .def("SetCPUThreads", &IBlackmagicRawConfiguration::SetCPUThreads)
        .def("GetCPUThreads", [](IBlackmagicRawConfiguration& self) {
            uint32_t threadCount = 0;
            HRESULT result = self.GetCPUThreads(&threadCount);
            return std::make_tuple(result, threadCount);
        })
        .def("GetMaxCPUThreadCount", [](IBlackmagicRawConfiguration& self) {
            uint32_t threadCount = 0;
            HRESULT result = self.GetMaxCPUThreadCount(&threadCount);
            return std::make_tuple(result, threadCount);
        })
        .def("SetWriteMetadataPerFrame", &IBlackmagicRawConfiguration::SetWriteMetadataPerFrame)
        .def("GetWriteMetadataPerFrame", [](IBlackmagicRawConfiguration& self) {
            bool writePerFrame = 0;
            HRESULT result = self.GetWriteMetadataPerFrame(&writePerFrame);
            return std::make_tuple(result, writePerFrame);
        })
        .def("SetFromDevice", &IBlackmagicRawConfiguration::SetFromDevice)
    ;

    py::class_<IBlackmagicRawResourceManager,IUnknown,std::unique_ptr<IBlackmagicRawResourceManager,Releaser>>(m, "IBlackmagicRawResourceManager")
        .def("CreateResource", [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, uint32_t sizeBytes, BlackmagicRawResourceType type, BlackmagicRawResourceUsage usage) {
            Resource resource = {};
            HRESULT result = self.CreateResource(context, commandQueue, sizeBytes, type, usage, &resource.data);
            return std::make_tuple(result, resource);
        })
        .def("ReleaseResource", [](IBlackmagicRawResourceManager& self, void* context, void* commandQueue, Resource resource, BlackmagicRawResourceType type) {
            HRESULT result = self.ReleaseResource(context, commandQueue, resource.data, type);
            return result;
        })
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawConfigurationEx,IUnknown,std::unique_ptr<IBlackmagicRawConfigurationEx,Releaser>>(m, "IBlackmagicRawConfigurationEx")
        .def("GetResourceManager", [](IBlackmagicRawConfigurationEx& self) {
            IBlackmagicRawResourceManager* resourceManager = nullptr;
            HRESULT result = self.GetResourceManager(&resourceManager);
            return std::make_tuple(result, resourceManager);
        })
        .def("SetResourceManager", &IBlackmagicRawConfigurationEx::SetResourceManager)
        .def("GetInstructionSet", [](IBlackmagicRawConfigurationEx& self) {
            BlackmagicRawInstructionSet instructionSet = 0;
            HRESULT result = self.GetInstructionSet(&instructionSet);
            return std::make_tuple(result, instructionSet);
        })
        .def("SetInstructionSet", &IBlackmagicRawConfigurationEx::SetInstructionSet)
    ;

    py::class_<IBlackmagicRawManualDecoderFlow1,IUnknown,std::unique_ptr<IBlackmagicRawManualDecoderFlow1,Releaser>>(m, "IBlackmagicRawManualDecoderFlow1")
        .def("PopulateFrameStateBuffer", [](IBlackmagicRawManualDecoderFlow1& self, IBlackmagicRawFrame* frame, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes, Resource frameState, uint32_t frameStateSizeBytes) {
            HRESULT result = self.PopulateFrameStateBuffer(frame, clipProcessingAttributes, frameProcessingAttributes, frameState.data, frameStateSizeBytes);
            return result;
        })
        .def("GetFrameStateSizeBytes", [](IBlackmagicRawManualDecoderFlow1& self) {
            uint32_t frameStateSizeBytes = 0;
            HRESULT result = self.GetFrameStateSizeBytes(&frameStateSizeBytes);
            return std::make_tuple(result, frameStateSizeBytes);
        })
        .def("GetDecodedSizeBytes", [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
            uint32_t decodedSizeBytes = 0;
            HRESULT result = self.GetDecodedSizeBytes(frameStateBufferCPU.data, &decodedSizeBytes);
            return std::make_tuple(result, decodedSizeBytes);
        })
        .def("GetProcessedSizeBytes", [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
            uint32_t processedSizeBytes = 0;
            HRESULT result = self.GetProcessedSizeBytes(frameStateBufferCPU.data, &processedSizeBytes);
            return std::make_tuple(result, processedSizeBytes);
        })
        .def("GetPost3DLUTSizeBytes", [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU) {
            uint32_t post3DLUTSizeBytes = 0;
            HRESULT result = self.GetPost3DLUTSizeBytes(frameStateBufferCPU.data, &post3DLUTSizeBytes);
            return std::make_tuple(result, post3DLUTSizeBytes);
        })
        .def("CreateJobDecode", [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU, Resource bitStreamBufferCPU, Resource decodedBufferCPU) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobDecode(frameStateBufferCPU.data, bitStreamBufferCPU.data, decodedBufferCPU.data, &job);
            return std::make_tuple(result, job);
        })
        .def("CreateJobProcess", [](IBlackmagicRawManualDecoderFlow1& self, Resource frameStateBufferCPU, Resource decodedBufferCPU, Resource processedBufferCPU, Resource post3DLUTBufferCPU) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobProcess(frameStateBufferCPU.data, decodedBufferCPU.data, processedBufferCPU.data, post3DLUTBufferCPU.data, &job);
            return std::make_tuple(result, job);
        })
    ;

    py::class_<IBlackmagicRaw,IUnknown,std::unique_ptr<IBlackmagicRaw,Releaser>>(m, "IBlackmagicRaw")
        .def("OpenClip", [](IBlackmagicRaw& self, const char* fileName) {
            IBlackmagicRawClip* clip = nullptr;
            HRESULT result = self.OpenClip(fileName, &clip);
            return std::make_tuple(result, clip);
        })
        .def("SetCallback", &IBlackmagicRaw::SetCallback)
        // TODO: Add missing bindings
        .def("FlushJobs", &IBlackmagicRaw::FlushJobs, py::call_guard<py::gil_scoped_release>())
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawConfiguration)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawConfigurationEx)
        DEF_QUERY_INTERFACE(IBlackmagicRaw, IBlackmagicRawManualDecoderFlow1)
    ;

    py::class_<IBlackmagicRawPipelineIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineIterator,Releaser>>(m, "IBlackmagicRawPipelineIterator")
        .def("Next", &IBlackmagicRawPipelineIterator::Next)
        .def("GetName", [](IBlackmagicRawPipelineIterator& self) {
            const char* pipelineName = nullptr;
            HRESULT result = self.GetName(&pipelineName);
            return std::make_tuple(result, pipelineName);
        })
        .def("GetInterop", [](IBlackmagicRawPipelineIterator& self) {
            BlackmagicRawInterop interop = 0;
            HRESULT result = self.GetInterop(&interop);
            return std::make_tuple(result, interop);
        })
        .def("GetPipeline", [](IBlackmagicRawPipelineIterator& self) {
            BlackmagicRawPipeline pipeline = 0;
            HRESULT result = self.GetPipeline(&pipeline);
            return std::make_tuple(result, pipeline);
        })
    ;

    py::class_<IBlackmagicRawOpenGLInteropHelper,IUnknown,std::unique_ptr<IBlackmagicRawOpenGLInteropHelper,Releaser>>(m, "IBlackmagicRawOpenGLInteropHelper")
        .def("GetPreferredResourceFormat", [](IBlackmagicRawOpenGLInteropHelper& self) {
            BlackmagicRawResourceFormat preferredFormat = 0;
            HRESULT result = self.GetPreferredResourceFormat(&preferredFormat);
            return std::make_tuple(result, preferredFormat);
        })
        .def("SetImage", [](IBlackmagicRawOpenGLInteropHelper& self, IBlackmagicRawProcessedImage* processedImage) {
            uint32_t openGLTextureName = 0;
            int32_t openGLTextureTarget = 0;
            HRESULT result = self.SetImage(processedImage, &openGLTextureName, &openGLTextureTarget);
            return std::make_tuple(result, openGLTextureName, openGLTextureTarget);
        })
    ;

    py::class_<IBlackmagicRawPipelineDevice,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDevice,Releaser>>(m, "IBlackmagicRawPipelineDevice")
        .def("SetBestInstructionSet", &IBlackmagicRawPipelineDevice::SetBestInstructionSet)
        .def("SetInstructionSet", &IBlackmagicRawPipelineDevice::SetInstructionSet)
        .def("GetInstructionSet", [](IBlackmagicRawPipelineDevice& self) {
            BlackmagicRawInstructionSet instructionSet = 0;
            HRESULT result = self.GetInstructionSet(&instructionSet);
            return std::make_tuple(result, instructionSet);
        })
        .def("GetIndex", [](IBlackmagicRawPipelineDevice& self) {
            uint32_t deviceIndex = 0;
            HRESULT result = self.GetIndex(&deviceIndex);
            return std::make_tuple(result, deviceIndex);
        })
        .def("GetName", [](IBlackmagicRawPipelineDevice& self) {
            const char* name = nullptr;
            HRESULT result = self.GetName(&name);
            return std::make_tuple(result, name);
        })
        .def("GetInterop", [](IBlackmagicRawPipelineDevice& self) {
            BlackmagicRawInterop interop = 0;
            HRESULT result = self.GetInterop(&interop);
            return std::make_tuple(result, interop);
        })
        // TODO: Add missing bindings
        .def("GetPipelineName", [](IBlackmagicRawPipelineDevice& self) {
            const char* pipelineName = nullptr;
            HRESULT result = self.GetPipelineName(&pipelineName);
            return std::make_tuple(result, pipelineName);
        })
        .def("GetOpenGLInteropHelper", [](IBlackmagicRawPipelineDevice& self) {
            IBlackmagicRawOpenGLInteropHelper* interopHelper = nullptr;
            HRESULT result = self.GetOpenGLInteropHelper(&interopHelper);
            return std::make_tuple(result, interopHelper);
        })
    ;

    py::class_<IBlackmagicRawPipelineDeviceIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDeviceIterator,Releaser>>(m, "IBlackmagicRawPipelineDeviceIterator")
        .def("Next", &IBlackmagicRawPipelineDeviceIterator::Next)
        .def("GetPipeline", [](IBlackmagicRawPipelineDeviceIterator& self) {
            BlackmagicRawPipeline pipeline = 0;
            HRESULT result = self.GetPipeline(&pipeline);
            return std::make_tuple(result, pipeline);
        })
        .def("GetInterop", [](IBlackmagicRawPipelineDeviceIterator& self) {
            BlackmagicRawInterop interop = 0;
            HRESULT result = self.GetInterop(&interop);
            return std::make_tuple(result, interop);
        })
        .def("CreateDevice", [](IBlackmagicRawPipelineDeviceIterator& self) {
            IBlackmagicRawPipelineDevice* pipelineDevice = nullptr;
            HRESULT result = self.CreateDevice(&pipelineDevice);
            return std::make_tuple(result, pipelineDevice);
        })
    ;

    py::class_<IBlackmagicRawFactory,IUnknown,std::unique_ptr<IBlackmagicRawFactory,Releaser>>(m, "IBlackmagicRawFactory")
        .def("CreateCodec", [](IBlackmagicRawFactory& self) {
            IBlackmagicRaw* codec = nullptr;
            HRESULT result = self.CreateCodec(&codec);
            return std::make_tuple(result, codec);
        })
        .def("CreatePipelineIterator", [](IBlackmagicRawFactory& self, BlackmagicRawInterop interop) {
            IBlackmagicRawPipelineIterator* pipelineIterator = nullptr;
            HRESULT result = self.CreatePipelineIterator(interop, &pipelineIterator);
            return std::make_tuple(result, pipelineIterator);
        })
        .def("CreatePipelineDeviceIterator", [](IBlackmagicRawFactory& self, BlackmagicRawPipeline pipeline, BlackmagicRawInterop interop) {
            IBlackmagicRawPipelineDeviceIterator* deviceIterator = nullptr;
            HRESULT result = self.CreatePipelineDeviceIterator(pipeline, interop, &deviceIterator);
            return std::make_tuple(result, deviceIterator);
        })
    ;
}
