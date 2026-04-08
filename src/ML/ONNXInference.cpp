#if SPECTRALZ_ENABLE_ML

#include "ONNXInference.h"
#include <algorithm>
#include <thread>

namespace spectralz
{

ONNXInference::ONNXInference()
{
    initializeEnvironment();
}

ONNXInference::~ONNXInference()
{
    unloadModel();
}

ONNXInference::ONNXInference(ONNXInference&& other) noexcept
    : env(std::move(other.env))
    , session(std::move(other.session))
    , sessionOptions(std::move(other.sessionOptions))
    , inputNames(std::move(other.inputNames))
    , outputNames(std::move(other.outputNames))
    , inputNamesCStr(std::move(other.inputNamesCStr))
    , outputNamesCStr(std::move(other.outputNamesCStr))
    , inputShapes(std::move(other.inputShapes))
    , outputShapes(std::move(other.outputShapes))
    , modelPath(std::move(other.modelPath))
    , sessionLoaded(other.sessionLoaded)
{
    other.sessionLoaded = false;
}

ONNXInference& ONNXInference::operator=(ONNXInference&& other) noexcept
{
    if (this != &other)
    {
        unloadModel();

        env = std::move(other.env);
        session = std::move(other.session);
        sessionOptions = std::move(other.sessionOptions);
        inputNames = std::move(other.inputNames);
        outputNames = std::move(other.outputNames);
        inputNamesCStr = std::move(other.inputNamesCStr);
        outputNamesCStr = std::move(other.outputNamesCStr);
        inputShapes = std::move(other.inputShapes);
        outputShapes = std::move(other.outputShapes);
        modelPath = std::move(other.modelPath);
        sessionLoaded = other.sessionLoaded;

        other.sessionLoaded = false;
    }
    return *this;
}

void ONNXInference::initializeEnvironment()
{
    try
    {
        env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "SpectralzML");
    }
    catch (const Ort::Exception& e)
    {
        DBG("ONNX Runtime environment initialization failed: " << e.what());
    }
}

void ONNXInference::configureSessionOptions()
{
    sessionOptions = std::make_unique<Ort::SessionOptions>();

    // Optimize for inference
    sessionOptions->SetIntraOpNumThreads(std::max(1, static_cast<int>(std::thread::hardware_concurrency() / 2)));
    sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    // Enable memory pattern optimization
    sessionOptions->EnableMemPattern();

    // Disable CPU memory arena for more predictable memory usage
    sessionOptions->DisableCpuMemArena();
}

bool ONNXInference::loadModel(const juce::File& path)
{
    DBG("ONNXInference::loadModel ENTER - " << path.getFileName());

    std::lock_guard<std::mutex> lock(sessionMutex);
    DBG("  Lock acquired");

    if (!path.existsAsFile())
    {
        DBG("Model file not found: " << path.getFullPathName());
        return false;
    }
    DBG("  File exists, size: " << path.getSize() << " bytes");

    // Unload existing model
    if (sessionLoaded)
    {
        DBG("  Unloading existing model...");
        unloadModelInternal();  // Use internal version - already holding lock
        DBG("  Existing model unloaded");
    }

    try
    {
        if (!env)
        {
            DBG("  Initializing ONNX environment...");
            initializeEnvironment();
            if (!env)
            {
                DBG("Failed to initialize ONNX environment");
                return false;
            }
        }
        DBG("  ONNX environment ready");

        DBG("  Configuring session options...");
        configureSessionOptions();
        DBG("  Session options configured");

        // Check for external data file (.onnx.data)
        auto dataFile = juce::File(path.getFullPathName() + ".data");
        if (dataFile.existsAsFile())
        {
            DBG("Model has external data file: " << dataFile.getFileName());
            DBG("  Data file size: " << dataFile.getSize() << " bytes");
        }
        else
        {
            DBG("  No external data file found");
        }

        DBG("Creating ONNX session for: " << path.getFullPathName());
        DBG("  About to call Ort::Session constructor...");

        // Create session from model file
#ifdef _WIN32
        std::wstring modelPathW = path.getFullPathName().toWideCharPointer();
        session = std::make_unique<Ort::Session>(*env, modelPathW.c_str(), *sessionOptions);
#else
        session = std::make_unique<Ort::Session>(*env, path.getFullPathName().toRawUTF8(), *sessionOptions);
#endif
        DBG("  Ort::Session constructor completed successfully");

        modelPath = path;
        sessionLoaded = true;

        // Extract model metadata
        DBG("  Extracting model metadata...");
        extractModelMetadata();
        DBG("  Metadata extracted");

        DBG("Model loaded successfully: " << path.getFileName());
        DBG("  Inputs: " << static_cast<int>(inputNames.size()));
        for (size_t i = 0; i < inputNames.size(); ++i)
        {
            DBG("    Input " << i << ": " << inputNames[i]);
        }
        DBG("  Outputs: " << static_cast<int>(outputNames.size()));

        return true;
    }
    catch (const Ort::Exception& e)
    {
        DBG("ONNX Runtime Exception: " << e.what());
        sessionLoaded = false;
        session.reset();
        return false;
    }
    catch (const std::exception& e)
    {
        DBG("Standard Exception during model load: " << e.what());
        sessionLoaded = false;
        session.reset();
        return false;
    }
    catch (...)
    {
        DBG("Unknown exception during model load");
        sessionLoaded = false;
        session.reset();
        return false;
    }
}

void ONNXInference::unloadModel()
{
    std::lock_guard<std::mutex> lock(sessionMutex);
    unloadModelInternal();
}

void ONNXInference::unloadModelInternal()
{
    // Called with mutex already held - do not lock again!
    session.reset();
    sessionLoaded = false;

    inputNames.clear();
    outputNames.clear();
    inputNamesCStr.clear();
    outputNamesCStr.clear();
    inputShapes.clear();
    outputShapes.clear();
}

void ONNXInference::extractModelMetadata()
{
    if (!session) return;

    inputNames.clear();
    outputNames.clear();
    inputNamesCStr.clear();
    outputNamesCStr.clear();
    inputShapes.clear();
    outputShapes.clear();

    // Get input info
    size_t numInputs = session->GetInputCount();
    for (size_t i = 0; i < numInputs; ++i)
    {
        auto nameAlloc = session->GetInputNameAllocated(i, allocator);
        inputNames.push_back(nameAlloc.get());

        auto typeInfo = session->GetInputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();

        TensorShape shape;
        shape.dims = tensorInfo.GetShape();
        inputShapes.push_back(shape);
    }

    // Get output info
    size_t numOutputs = session->GetOutputCount();
    for (size_t i = 0; i < numOutputs; ++i)
    {
        auto nameAlloc = session->GetOutputNameAllocated(i, allocator);
        outputNames.push_back(nameAlloc.get());

        auto typeInfo = session->GetOutputTypeInfo(i);
        auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();

        TensorShape shape;
        shape.dims = tensorInfo.GetShape();
        outputShapes.push_back(shape);
    }

    // Update C-string pointers
    inputNamesCStr.clear();
    for (const auto& name : inputNames)
        inputNamesCStr.push_back(name.c_str());

    outputNamesCStr.clear();
    for (const auto& name : outputNames)
        outputNamesCStr.push_back(name.c_str());
}

std::vector<std::string> ONNXInference::getInputNames() const
{
    return inputNames;
}

std::vector<std::string> ONNXInference::getOutputNames() const
{
    return outputNames;
}

TensorShape ONNXInference::getInputShape(size_t index) const
{
    if (index < inputShapes.size())
        return inputShapes[index];
    return {};
}

TensorShape ONNXInference::getOutputShape(size_t index) const
{
    if (index < outputShapes.size())
        return outputShapes[index];
    return {};
}

InferenceResult ONNXInference::run(
    const std::vector<float>& inputData,
    const TensorShape& inputShape)
{
    DBG("ONNXInference::run(single) ENTER - wrapping in vectors...");
    DBG("  inputData size: " << inputData.size());
    DBG("  inputShape dims: " << inputShape.dims.size());
    std::cerr.flush();

    // Create vectors on the heap to avoid stack issues
    DBG("  Creating vector wrappers...");
    std::vector<std::vector<float>> inputsVec;
    inputsVec.push_back(inputData);
    DBG("  inputsVec created, size: " << inputsVec.size());

    std::vector<TensorShape> shapesVec;
    shapesVec.push_back(inputShape);
    DBG("  shapesVec created, size: " << shapesVec.size());

    DBG("  Calling multi-input run()...");
    std::cerr.flush();
    return run(inputsVec, shapesVec);
}

InferenceResult ONNXInference::run(
    const std::vector<std::vector<float>>& inputs,
    const std::vector<TensorShape>& shapes)
{
    DBG("ONNXInference::run(multi) ENTER - BEFORE any operations");
    std::cerr.flush();
    DBG("  inputs.size(): " << inputs.size());
    DBG("  shapes.size(): " << shapes.size());
    if (!inputs.empty())
    {
        DBG("  inputs[0].size(): " << inputs[0].size());
    }
    DBG("  About to acquire lock...");
    std::cerr.flush();
    std::lock_guard<std::mutex> lock(sessionMutex);
    DBG("  Lock acquired");

    InferenceResult result;
    result.success = false;

    if (!sessionLoaded || !session)
    {
        DBG("  ERROR: No model loaded");
        result.errorMessage = "No model loaded";
        return result;
    }

    DBG("  Model has " << inputNames.size() << " inputs, we're providing " << inputs.size());
    if (inputs.size() != inputNames.size())
    {
        result.errorMessage = "Input count mismatch: expected " +
            std::to_string(inputNames.size()) + ", got " + std::to_string(inputs.size());
        DBG("  ERROR: " << result.errorMessage);
        return result;
    }

    // Debug: print expected vs provided shapes
    for (size_t i = 0; i < inputShapes.size() && i < shapes.size(); ++i)
    {
        std::string expectedStr = "[";
        for (size_t d = 0; d < inputShapes[i].dims.size(); ++d)
        {
            if (d > 0) expectedStr += ", ";
            expectedStr += std::to_string(inputShapes[i].dims[d]);
        }
        expectedStr += "]";

        std::string providedStr = "[";
        for (size_t d = 0; d < shapes[i].dims.size(); ++d)
        {
            if (d > 0) providedStr += ", ";
            providedStr += std::to_string(shapes[i].dims[d]);
        }
        providedStr += "]";

        DBG("  Input " << i << " (" << inputNames[i] << "): model expects " << expectedStr << ", we provide " << providedStr);
    }

    try
    {
        DBG("  Creating memory info...");
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
            OrtArenaAllocator, OrtMemTypeDefault);
        DBG("  Memory info created");

        // Create input tensors
        std::vector<Ort::Value> inputTensors;
        inputTensors.reserve(inputs.size());

        for (size_t i = 0; i < inputs.size(); ++i)
        {
            const auto& inputData = inputs[i];
            const auto& shape = shapes[i];

            // Validate data size matches shape
            size_t expectedSize = shape.elementCount();
            DBG("  Input " << i << ": data size=" << inputData.size() << ", expected from shape=" << expectedSize);
            if (inputData.size() != expectedSize)
            {
                result.errorMessage = "Input " + std::to_string(i) + " size mismatch: expected " +
                    std::to_string(expectedSize) + ", got " + std::to_string(inputData.size());
                DBG("  ERROR: " << result.errorMessage);
                return result;
            }

            DBG("  Creating tensor " << i << "...");
            inputTensors.push_back(Ort::Value::CreateTensor<float>(
                memoryInfo,
                const_cast<float*>(inputData.data()),
                inputData.size(),
                shape.dims.data(),
                shape.dims.size()));
            DBG("  Tensor " << i << " created");
        }

        // Run inference
        DBG("  Running session->Run()...");
        auto outputTensors = session->Run(
            Ort::RunOptions{nullptr},
            inputNamesCStr.data(),
            inputTensors.data(),
            inputTensors.size(),
            outputNamesCStr.data(),
            outputNamesCStr.size());

        // Extract outputs
        result.outputs.reserve(outputTensors.size());
        result.outputShapes.reserve(outputTensors.size());

        for (auto& tensor : outputTensors)
        {
            auto tensorInfo = tensor.GetTensorTypeAndShapeInfo();
            auto shape = tensorInfo.GetShape();

            TensorShape outShape;
            outShape.dims = shape;
            result.outputShapes.push_back(outShape);

            size_t elementCount = outShape.elementCount();
            const float* data = tensor.GetTensorData<float>();

            result.outputs.emplace_back(data, data + elementCount);
        }

        result.success = true;
    }
    catch (const Ort::Exception& e)
    {
        result.errorMessage = std::string("Inference failed: ") + e.what();
        DBG("ONNX Inference error: " << result.errorMessage);
    }

    return result;
}

size_t ONNXInference::getApproximateMemoryUsage() const
{
    // Rough estimate based on model size and typical runtime overhead
    if (!sessionLoaded || modelPath.getFullPathName().isEmpty())
        return 0;

    // Model file size as base, multiply by ~2 for runtime overhead
    return static_cast<size_t>(modelPath.getSize() * 2);
}

} // namespace spectralz

#endif // SPECTRALZ_ENABLE_ML
