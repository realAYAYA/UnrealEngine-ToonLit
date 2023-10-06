#include "ort_model_optimizer_api.h"

#include "core/common/logging/logging.h"
#include "core/common/logging/isink.h"
#include "core/common/logging/capture.h"

#include "core/session/onnxruntime_session_options_config_keys.h"

#include "core/graph/model.h"
#include "core/graph/graph.h"
#include "core/graph/graph_viewer.h"
#include "core/optimizer/graph_transformer_mgr.h"
#include "core/graph/onnx_protobuf.h"
#include "onnx/defs/operator_sets.h"

#include "core/optimizer/graph_transformer_utils.h"

#include "core/providers/cpu/cpu_execution_provider.h"

#include "core/framework/tensorprotoutils.h"
#include "core/framework/endian_utils.h"

#include <memory>
#include <string>
#include <sstream>

using namespace onnxruntime::common;
using namespace Ort;

#define CHECK(expr) \
	do {                                                                                       \
		auto _status = (expr);                                                                 \
		if ((!_status.IsOK())) {                                                               \
			return _status;                                                                    \
		}                                                                                      \
	} while (0)


//#define CHECK(expr) \
//  do {                                                                                       \
//    auto _status = (expr);                                                                   \
//    if ((!_status.IsOK())) {                                                                 \
//      ::onnxruntime::LogRuntimeError(0, _status, __FILE__, __FUNCTION__, __LINE__); \
//      return _status;                                                                          \
//    }                                                                                        \
//  } while (0)

namespace onnxruntime {


namespace logging {
class OptimizerCallbackSink : public ISink {
public:

	OptimizerCallbackSink(Ort::LogCallbackFn callback) 
		: callback_(callback) 
	{}

	virtual void SendImpl(const Timestamp& timestamp, const std::string& logger_id, const Capture& message) override {
		//timestamp;

		if (callback_) {
			
			std::ostringstream msg;

			msg << message.SeverityPrefix() << ":" << message.Category() << ":" << logger_id << ", "
				<< message.Location().ToString() << "] " << message.Message() << "\n";

			callback_(msg.str().c_str());
		}
	}

	Ort::LogCallbackFn	callback_;
};

} //namespace logging 

//
//
//
template<typename T, typename RepeatedFieldT>
bool ReadFieldData(const RepeatedFieldT& field, T* data, size_t count) {

	if (count != field.size())
		return false;

	int idx = 0;
	
	for (auto it = field.cbegin(); it != field.cend(); ++it, ++idx) {
        data[idx] = static_cast<T>(*it);
	}

	return true;
}

#define CASE_READ(TensorDataType, Type, RepeatedField) \
	case ONNX_NAMESPACE::TensorProto_DataType_##TensorDataType: \
		return ReadFieldData<Type>(RepeatedField, (Type*) data, size / sizeof(Type))

//
//
//
bool ReadTensorData(const ONNX_NAMESPACE::TensorProto& tensor, void* data, size_t size, size_t offset = 0) {
  
	//auto dataName = ONNX_NAMESPACE::TensorProto::DataType_Name(tensor.data_type());
	//offset;

	if (tensor.has_raw_data()) {
		
		auto srcData = (const uint8_t*) tensor.raw_data().c_str();
        auto dstData = (uint8_t*) data;

		auto status = utils::ReadLittleEndian(
			gsl::make_span(srcData, tensor.raw_data().size()),
			gsl::make_span(dstData + offset, size));
		
		return status.IsOK();
	}
	else {
        
		switch (tensor.data_type()) {
			
			CASE_READ(FLOAT,	float,		tensor.float_data());
            CASE_READ(DOUBLE,	double,		tensor.double_data());
            CASE_READ(BOOL,		uint8_t,	tensor.int32_data());
			CASE_READ(INT8,		int8_t,		tensor.int32_data());
            CASE_READ(INT16,	int16_t,	tensor.int32_data());
            CASE_READ(INT32,	int32_t,	tensor.int32_data());
			CASE_READ(INT64,	int64_t,	tensor.int64_data());
			CASE_READ(UINT8,	uint8_t,	tensor.int32_data());
			CASE_READ(UINT16,	uint16_t,	tensor.int32_data());
            CASE_READ(UINT32,	uint32_t,	tensor.uint64_data());
			CASE_READ(UINT64,	uint64_t,	tensor.uint64_data());
			// TODO: FIXME: Implement this
            //CASE_READ(FLOAT16,	MLFloat16,	tensor.int32_data());
            //CASE_READ(BFLOAT16,	BFloat16,	tensor.int32_data());
            
			default:
                assert(0);
				return false;
		}
	}
}

#undef CASE_READ

//
//
//
static void FillTensorInfo(GraphTensorInfo& info, const NodeArg* arg) {
	
	info.name = arg->Name().c_str();
	
	const ONNX_NAMESPACE::TensorShapeProto* shape = arg->Shape();

	if (shape) {
		info.shapeLen = shape->dim_size();

		for (int c = 0; c < GraphTensorInfo::cMaxDim; ++c)
			info.shape[c] = c < info.shapeLen ? shape->dim().Get(c).dim_value() : 0;
	}
	else {
		info.shapeLen = 0;
	}

    info.dataType = (Ort::GraphTensorDataType)arg->TypeAsProto()->tensor_type().elem_type();
}

//
//
//
struct GraphNodeImpl {

	GraphNodeImpl() :
		node(nullptr) {
	}

	GraphNodeImpl(const Node* nodeArg) : 
		node(nodeArg),
        attribNames(node->GetAttributes().size()) {

		int c = 0;
		for (const auto& it : node->GetAttributes()) {
            attribNames[c++] = it.first.c_str();	
        }
	}

	GraphNodeInfo GetInfo() const {
        
		return 
			GraphNodeInfo{
				node->OpType().c_str(),
				node->Name().c_str(),
				(int) node->InputDefs().size(),
				(int) node->OutputDefs().size(),
				(int) node->GetAttributes().size()
			};
	}

	GraphTensorInfo GetInput(int index) const {
        
		GraphTensorInfo info{};

		const auto& inputs = node->InputDefs();

		if (index < (int) inputs.size()) {
			
			const NodeArg* arg = inputs[index];
			
			FillTensorInfo(info, arg);
		}

		return info;
	}

	GraphTensorInfo GetOutput(int index) const {
        
		GraphTensorInfo info{};

		const auto& outputs = node->OutputDefs();

		if (index < (int) outputs.size()) {
			
			const NodeArg* arg = outputs[index];
			
			FillTensorInfo(info, arg);
		}

		return info;
	}

	GraphAttributeInfo GetAttribute(int index) const {
        
		GraphAttributeInfo info {};

		if (index < (int)attribNames.size()) {
		
			const auto& attrMap = node->GetAttributes();
            auto it = attrMap.find(attribNames[index]);

			if (it != attrMap.end()) {
				const auto& proto = it->second;
              
				info.name = it->first.c_str();
				info.type = (GraphAttributeType) proto.type();

				// TODO: Check if attribute type is supported
                
				GraphAttributeValue value = GetAttributeValue(index);
				//value;
				
				int dbg;
				dbg = 0;
			}
		}

		return info;
	}

	GraphAttributeValue GetAttributeValue(int index) const {
		
		GraphAttributeValue value;

		memset(&value, 0, sizeof(value));
		value.count = 0;
		value.type = GraphAttributeValue::kUndefined;

		const auto& attrMap = node->GetAttributes();
        auto it = attrMap.find(attribNames[index]);

        if (it != attrMap.end()) {
            const auto& proto = it->second;

            switch (proto.type()) {
				case ONNX_NAMESPACE::AttributeProto_AttributeType_FLOAT: {
					value.f = proto.f();
					value.count = 1;
					value.type = GraphAttributeValue::kFloat;
					break;
				}

				case ONNX_NAMESPACE::AttributeProto_AttributeType_INT: {
                    value.i = proto.i();
                    value.count = 1;
                    value.type = GraphAttributeValue::kInt;
					break;
				}

				case ONNX_NAMESPACE::AttributeProto_AttributeType_STRING: {
                    value.s = proto.s().c_str();
					value.count = 1;
                    value.type = GraphAttributeValue::kString;
					break;
				}

				case ONNX_NAMESPACE::AttributeProto_AttributeType_FLOATS: {
                    value.floats = (float*) proto.floats().data();
                    value.count = proto.floats().size();
                    value.type = GraphAttributeValue::kFloats;
					break;
				}

				case ONNX_NAMESPACE::AttributeProto_AttributeType_INTS: {
                    value.ints = (int64_t*) proto.ints().data();
                    value.count = proto.ints().size();
                    value.type = GraphAttributeValue::kInts;
					break;
				}

				case ONNX_NAMESPACE::AttributeProto_AttributeType_STRINGS: {
                    value.count = proto.strings().size();
					value.type = GraphAttributeValue::kStrings;
                    value.strings = (const std::string* const*) proto.strings().data();
					break;
				}
            }
		}

		return value;
	}

	//GraphAttributeInfo GetAttribute(const char* name) const {
	//       
	//	GraphAttributeInfo info {};

	//	const auto& attrMap = node->GetAttributes();
	//	auto it = attrMap.find(name);

	//	if (it != attrMap.end()) {
	//		info.name = it->first.c_str();
	//		info.type = (GraphAttributeType) it->second.type();
	//	}

	//	return info;
	//}

	const Node*					node;
	std::vector<const char*>	attribNames;
};

struct GraphTensorInitializerImpl {
	
	const ONNX_NAMESPACE::TensorProto*	proto;
	uint64_t							dataSize;
};

//
//
//
class ModelGraph : public Ort::IModelGraph {

public:

	bool Init(std::shared_ptr<Model> model) {
		
		model_ = model;

		const auto& graph = model_->MainGraph();
        graphView_.reset(new GraphViewer(graph));
        
		const auto& graphNodeInds = graphView_->GetNodesInTopologicalOrder();
		graphNodes_.reserve(graphNodeInds.size());

		for (auto& idx : graphNodeInds)
            graphNodes_.push_back(GraphNodeImpl(graph.GetNode(idx)));

		for (const auto& it : graphView_->GetAllInitializedTensors()) {
            
			if (it.second->data_location() == ONNX_NAMESPACE::TensorProto_DataLocation_EXTERNAL) {
				
				// TODO: Use logger
				printf("!!! - Tensor:%s has external data location\n", it.first.c_str());
                assert(0);

				return false;
			}

			GraphTensorInitializerImpl initializer {};

			initializer.proto = it.second;
			
			const auto& proto = *it.second;

			// auto status = utils::GetSizeInBytesFromTensorProto<0>(proto, &initializer.dataSize);
			size_t dataSize_aux = 0;
			auto status = utils::GetSizeInBytesFromTensorProto<0>(proto, &dataSize_aux);
			initializer.dataSize = static_cast<uint64_t>(dataSize_aux);

			tensorInitializers_.push_back(initializer);
		}

		return true;
	}

	virtual const char* GetModelName() const {
          return model_->MainGraph().Name().c_str();
	}

	// TODO:
    // model_->Domain().c_str();
    // model_->ProducerName().c_str();
    // model_->ProducerVersion().c_str();
    // model_->DocString().c_str();
	// IrVersion()

	virtual GraphInfo GetGraphInfo() const {
		return 
			GraphInfo {
				model_->MainGraph().Name().c_str(),	
				(int) graphView_->GetInputs().size(),
				(int) graphView_->GetOutputs().size(),
				(int) graphNodes_.size(),
				(int) tensorInitializers_.size()
			};
	}

	virtual GraphTensorInfo GetGraphInput(int index) const {
 
		GraphTensorInfo info{};

		const auto& inputs = graphView_->GetInputs();

		if (index < (int) inputs.size()) {
			
			const NodeArg* arg = inputs[index];
			
			FillTensorInfo(info, arg);
		}

		return info;
	}

	virtual GraphTensorInfo GetGraphOutput(int index) const {
        
		GraphTensorInfo info{};

		const auto& outputs = graphView_->GetOutputs();

        if (index < (int) outputs.size()) {
            const NodeArg* arg = outputs[index];

            FillTensorInfo(info, arg);
        }

		return info;
	}

	virtual GraphNode GetNode(int index) const {
        return index < (int) graphNodes_.size() ? &graphNodes_[index] : nullptr;
	}

	virtual GraphNodeInfo GetNodeInfo(GraphNode node) const {
        if (node)
            return node->GetInfo();

		return GraphNodeInfo {};
	}

	virtual GraphAttributeInfo GetNodeAttribute(GraphNode node, int index) const {
        if (node) {
			return node->GetAttribute(index);
        }

        return GraphAttributeInfo{};
	}
	
	virtual GraphAttributeValue GetNodeAttributeValue(GraphNode node, int index) const {
		
		if (node) {
			return node->GetAttributeValue(index);
        }

        return GraphAttributeValue{};
	}

	virtual GraphTensorInfo GetNodeInput(GraphNode node, int index) const {
        if (node) {
            return node->GetInput(index);
		}

		return GraphTensorInfo {};
	}

	virtual GraphTensorInfo GetNodeOutput(GraphNode node, int index) const {
        if (node) {
            return node->GetOutput(index);
		}

		return GraphTensorInfo {};
	}

	virtual GraphTensorInitializer GetTensorInitializer(int index) const {

		if (index < (int) tensorInitializers_.size()) {
			return &tensorInitializers_[index];
		}

		return nullptr;
	}

	virtual GraphTensorInitializer GetTensorInitializer(const char* tensorName) const {

		for (int idx = 0; idx < (int) tensorInitializers_.size(); ++idx) {
			
			const auto& tensorInit = tensorInitializers_[idx];

			if (tensorInit.proto->name().compare(tensorName) == 0) {
				
				return &tensorInitializers_[idx];
			}
		}

		return nullptr;
	}

	virtual const char* GetTensorName(GraphTensorInitializer tensorInit) const {
	
		return tensorInit ? tensorInit->proto->name().c_str() : nullptr;
	}

	virtual size_t GetTensorDataSize(GraphTensorInitializer tensorInit) const {
	
		size_t dataSize = 0;

		if (tensorInit) 
			dataSize = tensorInit->dataSize;

		return dataSize;
	}

	virtual int GetTensorData(GraphTensorInitializer tensorInit, void* data, size_t size, size_t offset) const {
	
		if (tensorInit && offset + size <= tensorInit->dataSize) {
				
			// Return 0 on success
			return ReadTensorData(*tensorInit->proto, data, size, offset) ? 0 : -1;
        }

		return -1;
	}

private:

	std::shared_ptr<Model>						model_;
	std::unique_ptr<GraphViewer>				graphView_;
	std::vector<GraphNodeImpl>					graphNodes_;
	std::vector<GraphTensorInitializerImpl>		tensorInitializers_;
};

//
// ModelOptimizer implementation
//
class ModelOptimizer {
 public:

	ModelOptimizer(const ModelOptimizeOptions& options);
	
	virtual ~ModelOptimizer();

	common::Status Optimize(const ORTCHAR_T* path, Ort::IModelGraph** graph);

    common::Status Optimize(const void* data, size_t size, Ort::IModelGraph** graph);

private:

	common::Status Optimize(std::shared_ptr<Model> model, Ort::IModelGraph** graph);

	const Ort::ModelOptimizeOptions&				options_;
	std::unique_ptr<logging::LoggingManager>		logMgr_;
	std::unique_ptr<logging::Logger>				logger_;
};



ModelOptimizer::ModelOptimizer(const ModelOptimizeOptions& options) 
	: options_(options) {

	std::string logId = "";

	auto sink = std::make_unique<logging::OptimizerCallbackSink>(options.logCallback);
	
	logMgr_ = std::make_unique<logging::LoggingManager>(
					std::move(sink),
					logging::Severity::kFATAL,
					false,
					logging::LoggingManager::InstanceType::Temporal,
					&logId);

	logger_ = logMgr_->CreateLogger(logId);
}

ModelOptimizer::~ModelOptimizer() {
}

common::Status ModelOptimizer::Optimize(const ORTCHAR_T* path, IModelGraph** graph) {

	std::shared_ptr<Model> model;

    CHECK(Model::Load(path, model, nullptr, *logger_.get()));

	return Optimize(model, graph);
}

common::Status ModelOptimizer::Optimize(const void* data, size_t size, IModelGraph** graph) {
	
	std::shared_ptr<Model> model;

	CHECK(Model::LoadFromBytes((int) size, const_cast<void*>(data), model, nullptr, *logger_.get()));

	return Optimize(model, graph);
}

common::Status ModelOptimizer::Optimize(std::shared_ptr<Model> model, IModelGraph** graph) {
	
	if (!model)
        return common::Status(common::NONE, common::INVALID_ARGUMENT, "ModelOptimizer:Model is not loaded");

	// NOTE: The max. number of steps is taken from InferenceSession::ConstructorCommon() and SessionOptions
	GraphTransformerManager	graph_transformation_mgr(10);

	// Create default options
	SessionOptions sessionOpts{};

	// CPU execution provider is required for eliminating constant values
    CPUExecutionProviderInfo epi{sessionOpts.enable_cpu_mem_arena};
    
	auto cpu_ep = std::make_unique<CPUExecutionProvider>(epi);

	// TODO: No quantization by default
    //sessionOpts.config_options.GetConfigOrDefault(kOrtSessionOptionsDisableQuantQDQ, "0");
    
	// Register L1 transformations
    if (options_.applyL1Transformations) {
		auto transformers = optimizer_utils::GenerateTransformers(TransformerLevel::Level1, sessionOpts, *cpu_ep);

		for (auto& transformer : transformers) {
            CHECK(graph_transformation_mgr.Register(std::move(transformer), TransformerLevel::Level1));
		}
	}

	// Register DML/HLSL fused activation here
    if (options_.applyL2Transformations) {
		// TODO: Implement this
	}

	// Model is loaded we can optimize it now
	if (options_.applyL1Transformations)
        CHECK(graph_transformation_mgr.ApplyTransformers(model->MainGraph(), TransformerLevel::Level1, *logger_.get()));

	// Apply DML/HLSL fused activation here
    if (options_.applyL2Transformations)
        CHECK(graph_transformation_mgr.ApplyTransformers(model->MainGraph(), TransformerLevel::Level2, *logger_.get()));

	// Graph is optimized here and it's ready for client's traversal
	auto modelGraph = std::make_unique<ModelGraph>();
	
	if (!modelGraph->Init(model)) {
          return common::Status(common::NONE, common::INVALID_GRAPH, "Failed to create ModelGraph");
	}

	*graph = modelGraph.release();

	return common::Status();
}

} // onnxruntime

//
// Optimize model from path
//
extern "C" ORT_EXPORT Ort::IModelGraph* OrtOptimizeModelFromFile(const ORTCHAR_T* path, const Ort::ModelOptimizeOptions& options) {
	
	Ort::IModelGraph* graph = nullptr;

	auto optimizer = std::make_unique<onnxruntime::ModelOptimizer>(options);

	onnxruntime::common::Status status = optimizer->Optimize(path, &graph);

	if (!status.IsOK()) {
        if (options.logCallback) {
            options.logCallback(status.ToString().c_str());
		}
	}

	return graph;
}

//
// Optimize model from memory
//
extern "C" ORT_EXPORT Ort::IModelGraph* OrtOptimizeModelFromMemory(const void* data, size_t size, const Ort::ModelOptimizeOptions& options) {
	
	Ort::IModelGraph* graph = nullptr;

	auto optimizer = std::make_unique<onnxruntime::ModelOptimizer>(options);

	onnxruntime::common::Status status = optimizer->Optimize(data, size, &graph);

	if (!status.IsOK()) {
        if (options.logCallback) {
            options.logCallback(status.ToString().c_str());
		}
	}

	return graph;
}

#include "core/session/ort_apis.h"

// Validate model in memory
extern "C" ORT_EXPORT OrtStatusPtr OrtValidateModelFromMemory(const void* data, size_t size) {

    using namespace onnxruntime;

	ORT_TRY {
		auto logger = logging::LoggingManager::DefaultLogger();

		std::shared_ptr<Model> model;

		auto status = Model::LoadFromBytes((int) size, const_cast<void*>(data), model, nullptr, logger);

		if (!status.IsOK())
			return OrtApis::CreateStatus(ORT_FAIL, status.ErrorMessage().c_str());
	}
	ORT_CATCH(const std::exception & ex) {

#if WITH_EDITOR
		return OrtApis::CreateStatus(ORT_FAIL, ex.what());
#else
		return OrtApis::CreateStatus(ORT_FAIL, "Internal error");
#endif

	}
	
	return nullptr;
}
