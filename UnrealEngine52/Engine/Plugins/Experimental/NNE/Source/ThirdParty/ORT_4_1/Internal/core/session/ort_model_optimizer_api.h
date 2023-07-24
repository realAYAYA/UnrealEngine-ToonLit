#pragma once

#include "onnxruntime_c_api.h"

#ifdef __cplusplus

namespace onnxruntime {
    struct GraphNodeImpl;
    struct GraphTensorInitializerImpl;
}

namespace Ort {

typedef void (*LogCallbackFn)(const char* msg);

//enum class ModelOptimizationLevel : uint8 {
//    Level1,
//
//};

struct ModelOptimizeOptions {
    LogCallbackFn   logCallback;

    uint32_t applyL1Transformations  : 1;
    uint32_t applyL2Transformations  : 1;

    ModelOptimizeOptions()
        : logCallback{}
        , applyL1Transformations(1)
        , applyL2Transformations(1) 
    {}
};

struct IModelGraph;
struct ModelOptimizeOptions;

struct ModelInfo {
    const char* name;
    const char* domain;
    const char* producerName;
    const char* producerVersion;
    const char* docString;
};

struct GraphInfo {
    const char* name;
    int         inputCount;
    int         outputCount;
    int         nodeCount;
    int         tensorInitializerCount;
};

struct GraphNodeInfo {
    const char* opName;
    const char* name;
    int         inputCount;
    int         outputCount;
    int         attributeCount;
};

// Same as TensorProto_DataType
enum class GraphTensorDataType : uint8_t {
    kUndefined = 0,
    kFloat = 1,
    kUInt8 = 2,
    kInt8 = 3,
    kUInt16 = 4,
    kInt16 = 5,
    kInt32 = 6,
    kInt64 = 7,
    kString = 8,
    kBool = 9,
    kFloat16 = 10,
    kDouble = 11,
    kUInt32 = 12,
    kUInt64 = 13,
    kComplex64 = 14,
    kComplex128 = 15,
    kBFloat16 = 16
};

struct GraphTensorInfo {
    static constexpr int cMaxDim = 8;

    const char*         name;
    int32_t             shape[cMaxDim];
    int                 shapeLen;

    GraphTensorDataType dataType;
};

// Same as AttributeProto_AttributeType
enum class GraphAttributeType : uint8_t {
    kUndefined = 0,
    kFloat = 1,
    kInt = 2,
    kString = 3,
    kTensor = 4,
    kGraph = 5,
    kSparseTensor = 11,
    kFloats = 6,
    kInts = 7,
    kStrings = 8,
    kTensors = 9,
    kGraphs = 10,
    kSparseTensors = 12
};

struct GraphAttributeInfo {
    const char*         name;
    GraphAttributeType  type;
};

struct GraphTensorData {
    void*   data;
    size_t  size;
};

struct GraphAttributeValue {

	enum Type {
		kUndefined,
		kFloat,
		kInt,
		kString,
		kFloats,
		kInts,
		kStrings
	};

	union {
		float			f;
		int64_t			i;
		const char*		s;
		
		float*			floats;
		int64_t*		ints;
		const char**	strings;
	};

	int		count;
	Type	type;
};

typedef const onnxruntime::GraphNodeImpl* GraphNode;
typedef const onnxruntime::GraphTensorInitializerImpl* GraphTensorInitializer;

struct IModelGraph {

    virtual ~IModelGraph() = default;

    virtual const char* GetModelName() const = 0;

    virtual GraphInfo GetGraphInfo() const = 0;

    virtual GraphTensorInfo GetGraphInput(int index) const = 0;
    virtual GraphTensorInfo GetGraphOutput(int index) const = 0;

    virtual GraphNode GetNode(int index) const = 0;
    virtual GraphNodeInfo GetNodeInfo(GraphNode node) const = 0;
    virtual GraphAttributeInfo GetNodeAttribute(GraphNode node, int index) const = 0;
    virtual GraphAttributeValue GetNodeAttributeValue(GraphNode node, int index) const = 0;
    virtual GraphTensorInfo GetNodeInput(GraphNode node, int inputIndex) const = 0;
    virtual GraphTensorInfo GetNodeOutput(GraphNode node, int outputIndex) const = 0;

    virtual GraphTensorInitializer GetTensorInitializer(int index) const = 0;
    virtual GraphTensorInitializer GetTensorInitializer(const char* tensorName) const = 0;
    virtual const char* GetTensorName(GraphTensorInitializer tensorInit) const = 0;
    virtual size_t GetTensorDataSize(GraphTensorInitializer tensorInit) const = 0;
    virtual int GetTensorData(GraphTensorInitializer tensorInit, void* data, size_t size, size_t offset) const = 0;
};

} // namespace Ort

extern "C" ORT_EXPORT Ort::IModelGraph* OrtOptimizeModelFromFile(const ORTCHAR_T* path, const Ort::ModelOptimizeOptions& opts);

extern "C" ORT_EXPORT Ort::IModelGraph* OrtOptimizeModelFromMemory(const void* data, size_t size, const Ort::ModelOptimizeOptions& opts);

extern "C" ORT_EXPORT OrtStatusPtr OrtValidateModelFromMemory(const void* data, size_t size);

namespace Ort {

//  Utility function
inline const char* GraphTensorDataTypeToString(GraphTensorDataType type) {
    switch (type) {
        case GraphTensorDataType::kUndefined :
            return "UNDEFINED";

        case GraphTensorDataType::kFloat :
            return "FLOAT";
        
        case GraphTensorDataType::kUInt8 :
            return "UINT8";

        case GraphTensorDataType::kInt8 :
            return "INT8";

        case GraphTensorDataType::kUInt16 :
            return "UINT16";

        case GraphTensorDataType::kInt16 :
            return "INT16";

        case GraphTensorDataType::kInt32 :
            return "INT32";

        case GraphTensorDataType::kInt64 :
            return "INT64";

        case GraphTensorDataType::kString :
            return "STRING";

        case GraphTensorDataType::kBool :
            return "BOOL";

        case GraphTensorDataType::kFloat16 :
            return "FLOAT16";

        case GraphTensorDataType::kDouble :
            return "DOUBLE";

        case GraphTensorDataType::kUInt32 :
            return "UINT32";

        case GraphTensorDataType::kUInt64 :
            return "UINT64";

        case GraphTensorDataType::kComplex64 :
            return "COMPLEX64";

        case GraphTensorDataType::kComplex128 :
            return "COMPLEX128";

        case GraphTensorDataType::kBFloat16 :
            return "BFLOAT16";
    }

    return "UNDEFINED";
}

}


#endif