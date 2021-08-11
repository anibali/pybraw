#include "BlackmagicRawAPI.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>

#include <atomic>

namespace py = pybind11;


// Concrete subclass of IBlackmagicRawCallback which provides an implementation of reference
// counting and placeholders for each of the callback functions.
class BlackmagicRawCallback : public IBlackmagicRawCallback {
private:
    std::atomic<int32_t> m_refCount = {0};
public:
    virtual ~BlackmagicRawCallback() {
        assert(m_refCount == 0);
    }
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, LPVOID*) { return E_NOTIMPL; }
    virtual ULONG STDMETHODCALLTYPE AddRef(void) {
        return ++m_refCount;
    }
    virtual ULONG STDMETHODCALLTYPE Release(void) {
        const int32_t newRefValue = --m_refCount;
        if(newRefValue == 0) {
            delete this;
        }
        assert(newRefValue >= 0);
        return newRefValue;
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
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, ReadComplete, job, result, frame);
    }
    void ProcessComplete(IBlackmagicRawJob* job, HRESULT result, IBlackmagicRawProcessedImage* processedImage) override {
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, ProcessComplete, job, result, processedImage);
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
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, SidecarMetadataParseWarning, clip, fileName, lineNumber, info);
    }
    void SidecarMetadataParseError(IBlackmagicRawClip* clip, const char* fileName, uint32_t lineNumber, const char* info) override {
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, SidecarMetadataParseError, clip, fileName, lineNumber, info);
    }
    void PreparePipelineComplete(void* userData, HRESULT result) override {
        PYBIND11_OVERRIDE(void, BlackmagicRawCallback, PreparePipelineComplete, userData, result);
    }
};


template<typename T>
py::array_t<T> _safe_array_to_numpy(const SafeArray& safe_array, py::handle base) {
    if(safe_array.cDims != 1) {
        throw pybind11::value_error("only 1D SafeArray instances are supported");
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
            throw pybind11::value_error("unsupported variantType for SafeArray");
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
                    throw pybind11::value_error("unsupported variantType for Variant");
            }
        }, "Return a copy of this Variant as a Python object.")
    ;

    py::class_<IUnknown>(m, "IUnknown")
        // TODO: Add missing bindings
        .def("AddRef", &IUnknown::AddRef)
        .def("Release", &IUnknown::Release)
    ;

    py::class_<IBlackmagicRawCallback,IUnknown,std::unique_ptr<IBlackmagicRawCallback,py::nodelete>>(m, "IBlackmagicRawCallback")
        .def("ReadComplete", &IBlackmagicRawCallback::ReadComplete)
        .def("ProcessComplete", &IBlackmagicRawCallback::ProcessComplete)
        .def("DecodeComplete", &IBlackmagicRawCallback::DecodeComplete)
        .def("TrimProgress", &IBlackmagicRawCallback::TrimProgress)
        .def("TrimComplete", &IBlackmagicRawCallback::TrimComplete)
        .def("SidecarMetadataParseWarning", &IBlackmagicRawCallback::SidecarMetadataParseWarning)
        .def("SidecarMetadataParseError", &IBlackmagicRawCallback::SidecarMetadataParseError)
        .def("PreparePipelineComplete", &IBlackmagicRawCallback::PreparePipelineComplete)
    ;

    py::class_<BlackmagicRawCallback,PyBrawCallback,IBlackmagicRawCallback,std::unique_ptr<BlackmagicRawCallback,py::nodelete>>(m, "BlackmagicRawCallback")
        .def(py::init<>())
    ;

    py::class_<IBlackmagicRawClipProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawClipProcessingAttributes,py::nodelete>>(m, "IBlackmagicRawClipProcessingAttributes")
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawFrameProcessingAttributes,IUnknown,std::unique_ptr<IBlackmagicRawFrameProcessingAttributes,py::nodelete>>(m, "IBlackmagicRawFrameProcessingAttributes")
        // TODO: Add missing bindings
    ;

    py::class_<IBlackmagicRawFrame,IUnknown,std::unique_ptr<IBlackmagicRawFrame,py::nodelete>>(m, "IBlackmagicRawFrame")
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
        .def("CreateJobDecodeAndProcessFrame", [](IBlackmagicRawFrame& self, IBlackmagicRawClipProcessingAttributes* clipProcessingAttributes, IBlackmagicRawFrameProcessingAttributes* frameProcessingAttributes) {
            IBlackmagicRawJob* job = nullptr;
            HRESULT result = self.CreateJobDecodeAndProcessFrame(clipProcessingAttributes, frameProcessingAttributes, &job);
            return std::make_tuple(result, job);
        }, "Create a job that will decode and process our image.",
        py::arg("clipProcessingAttributes").none(true) = nullptr, py::arg("frameProcessingAttributes").none(true) = nullptr)
    ;

    py::class_<IBlackmagicRawProcessedImage,IUnknown,std::unique_ptr<IBlackmagicRawProcessedImage,py::nodelete>>(m, "IBlackmagicRawProcessedImage")
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
        .def("GetResource", [](IBlackmagicRawProcessedImage& self) -> py::object {
            BlackmagicRawResourceType type = 0;
            HRESULT result = self.GetResourceType(&type);
            if(result != S_OK) {
                return py::cast(std::make_tuple(result, py::none()));
            }
            if(type != blackmagicRawResourceTypeBufferCPU) {
                throw pybind11::value_error("only CPU resources are supported");
            }
            uint32_t sizeBytes = 0;
            result = self.GetResourceSizeBytes(&sizeBytes);
            if(result != S_OK) {
                return py::cast(std::make_tuple(result, py::none()));
            }
            void* resource = nullptr;
            result = self.GetResource(&resource);
            if(result != S_OK) {
                return py::cast(std::make_tuple(result, py::none()));
            }
            // The use of a capsule makes this safe. We increment the reference count for the
            // processed frame and make it the base for the array. This will keep the processed
            // frame alive for at least as long as the array viewing its data.
            self.AddRef();
            py::capsule caps(&self, [](void* ptr) {
                IBlackmagicRawProcessedImage* self = (IBlackmagicRawProcessedImage*)ptr;
                self->Release();
            });
            auto array = py::array_t<uint8_t>({sizeBytes}, {sizeof(uint8_t)}, (uint8_t*)resource, caps);
            return py::cast(std::make_tuple(result, array));
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

    py::class_<IBlackmagicRawMetadataIterator,IUnknown,std::unique_ptr<IBlackmagicRawMetadataIterator,py::nodelete>>(m, "IBlackmagicRawMetadataIterator")
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
    py::class_<IBlackmagicRawClip,IUnknown,std::unique_ptr<IBlackmagicRawClip,py::nodelete>>(m, "IBlackmagicRawClip")
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
    ;

    py::class_<IBlackmagicRaw,IUnknown,std::unique_ptr<IBlackmagicRaw,py::nodelete>>(m, "IBlackmagicRaw")
        .def("OpenClip", [](IBlackmagicRaw& self, const char* fileName) {
            IBlackmagicRawClip* clip = nullptr;
            HRESULT result = self.OpenClip(fileName, &clip);
            return std::make_tuple(result, clip);
        })
        .def("SetCallback", &IBlackmagicRaw::SetCallback)
        // TODO: Add missing bindings
        .def("FlushJobs", &IBlackmagicRaw::FlushJobs, py::call_guard<py::gil_scoped_release>())
    ;

    py::class_<IBlackmagicRawPipelineIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineIterator,py::nodelete>>(m, "IBlackmagicRawPipelineIterator")
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

    py::class_<IBlackmagicRawPipelineDevice,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDevice,py::nodelete>>(m, "IBlackmagicRawPipelineDevice")
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

    py::class_<IBlackmagicRawPipelineDeviceIterator,IUnknown,std::unique_ptr<IBlackmagicRawPipelineDeviceIterator,py::nodelete>>(m, "IBlackmagicRawPipelineDeviceIterator")
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

    py::class_<IBlackmagicRawFactory,IUnknown,std::unique_ptr<IBlackmagicRawFactory,py::nodelete>>(m, "IBlackmagicRawFactory")
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
