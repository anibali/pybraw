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

    // Default implementations of callback functions.
    // Subclasses can selectively override these.
    virtual void ReadComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawFrame* frame) {}
    virtual void ProcessComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawProcessedImage* processedImage) {}
    virtual void DecodeComplete(IBlackmagicRawJob* job, HRESULT result) {}
    virtual void TrimProgress(IBlackmagicRawJob* job, float progress) {}
    virtual void TrimComplete(IBlackmagicRawJob* job, HRESULT result) {}
    virtual void SidecarMetadataParseWarning(IBlackmagicRawClip* clip, const char* fileName, uint32_t lineNumber, const char* info) {}
    virtual void SidecarMetadataParseError(IBlackmagicRawClip* clip, const char* fileName, uint32_t lineNumber, const char* info) {}
    virtual void PreparePipelineComplete(void* userData, HRESULT result) {}
};


class PyBrawCallback : public BlackmagicRawCallback {
public:
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
    // TODO: Enable overriding of other methods.
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
py::array_t<T> _safe_array_to_numpy(const SafeArray& safe_array, py::handle base) {
    if(safe_array.cDims != 1) {
        throw py::buffer_error("only 1D SafeArray instances are supported");
    }
    return py::array_t<T>({safe_array.bounds.cElements}, {sizeof(T)}, &((T*)safe_array.data)[safe_array.bounds.lLbound], base);
}


py::array convert_safe_array_to_numpy(const SafeArray& safe_array, py::handle base) {
    switch(safe_array.variantType) {
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


PYBIND11_MODULE(_pybraw, m) {
    m.doc() = "Python bindings for Blackmagic RAW SDK";
    m.def("CreateBlackmagicRawFactoryInstance", &CreateBlackmagicRawFactoryInstance);

    m.def("VariantInit", &VariantInit);
    m.def("VariantClear", &VariantClear);

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

    py::enum_<_BlackmagicRawInterop>(m, "_BlackmagicRawInterop")
        .value("blackmagicRawInteropNone", blackmagicRawInteropNone)
        .value("blackmagicRawInteropOpenGL", blackmagicRawInteropOpenGL)
        .export_values()
    ;

    py::class_<SafeArrayBound>(m, "SafeArrayBound")
        .def_readwrite("lLbound", &SafeArrayBound::lLbound)
        .def_readwrite("cElements", &SafeArrayBound::cElements)
    ;

    py::class_<SafeArray>(m, "SafeArray")
        .def_readwrite("variantType", &SafeArray::variantType)
        .def_readwrite("cDims", &SafeArray::cDims)
        .def_readwrite("data", &SafeArray::data)
        .def_readwrite("bounds", &SafeArray::bounds)
        .def("numpy", [](SafeArray& self) -> py::array {
            // WARN: This is set up such that data is not copied. So if the SafeArray is freed,
            //       it is not safe to continue using this array.
            return convert_safe_array_to_numpy(self, py::none());
        }, "Return a view of this SafeArray as a NumPy array.")
    ;

    py::class_<Variant>(m, "Variant")
        .def(py::init<>())
        .def_readwrite("vt", &Variant::vt)
        .def_readwrite("iVal", &Variant::iVal)
        .def_readwrite("uiVal", &Variant::uiVal)
        .def_readwrite("intVal", &Variant::intVal)
        .def_readwrite("uintVal", &Variant::uintVal)
        .def_readwrite("fltVal", &Variant::fltVal)
        .def_readwrite("bstrVal", &Variant::bstrVal)
        .def_readwrite("parray", &Variant::parray)
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
                    // Data will be copied, so it is safe to clear the Variant and continue
                    // using this array.
                    return convert_safe_array_to_numpy(*self.parray, py::handle());
                default:
                    throw py::value_error("unsupported variantType for Variant");
            }
        }, "Return a copy of this Variant as a Python object.")
    ;

    py::class_<IUnknown>(m, "IUnknown")
        // TODO: Add missing bindings
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

    py::class_<BlackmagicRawCallback,PyBrawCallback,IBlackmagicRawCallback,std::unique_ptr<BlackmagicRawCallback,Releaser>>(m, "BlackmagicRawCallback")
        .def(py::init<>())
    ;

    py::class_<IBlackmagicRawClipEx,IUnknown,std::unique_ptr<IBlackmagicRawClipEx,Releaser>>(m, "IBlackmagicRawClipEx")
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawClipProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawClipProcessingAttributes,Releaser>>(m, "IBlackmagicRawClipProcessingAttributes")
        .def("GetClipAttribute", [](IBlackmagicRawClipProcessingAttributes& self, BlackmagicRawClipProcessingAttribute attribute) {
            Variant value;
            VariantInit(&value);
            HRESULT result = self.GetClipAttribute(attribute, &value);
            return std::make_tuple(result, value);
        })
        .def("SetClipAttribute", &IBlackmagicRawClipProcessingAttributes::SetClipAttribute)
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawFrameProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawFrameProcessingAttributes,Releaser>>(m, "IBlackmagicRawFrameProcessingAttributes")
        // TODO: Add missing bindings
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
        // TODO: Add missing bindings
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
//        .def("GetResource", [](IBlackmagicRawProcessedImage& self) {
//            void* resource = nullptr;
//            HRESULT result = self.GetResource(&resource);
//            return std::make_tuple(result, (size_t)resource);
//        })
        .def("numpy", [](IBlackmagicRawProcessedImage& self) -> py::array {
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
        // TODO: Add missing bindings
    ;

    // https://pybind11.readthedocs.io/en/stable/advanced/classes.html#non-public-destructors
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
        // TODO: Add missing bindings
        .def("CreateJobReadFrame", [](IBlackmagicRawClip& self, uint64_t frameIndex) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobReadFrame(frameIndex, &job);
            return std::make_tuple(result, job);
        })
        // TODO: Add missing bindings
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

    py::class_<IBlackmagicRawConfigurationEx,IUnknown,std::unique_ptr<IBlackmagicRawConfigurationEx,Releaser>>(m, "IBlackmagicRawConfigurationEx")
        // TODO: Add missing bindings
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
        // TODO: Add missing bindings
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
