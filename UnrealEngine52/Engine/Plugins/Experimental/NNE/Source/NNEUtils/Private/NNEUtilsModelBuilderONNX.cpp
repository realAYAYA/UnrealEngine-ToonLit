// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNECore.h"
#include "NNEUtilsModelBuilder.h"
#include "NNECoreAttributeMap.h"
#include "NNECoreRuntimeFormat.h"

#include "NNEThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT

#include "onnx/common/ir.h"
#include "onnx/common/constants.h"
#include "onnx/defs/operator_sets.h"

#include "core/session/ort_model_optimizer_api.h"	// For model validation
//#include "core/session/ort_env.h"
//#include "core/session/environment.h"
#include "core/session/onnxruntime_cxx_api.h"

NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNEUtils::Internal
{

inline onnx::ValueInfoProto* OnnxTensorCast(IModelBuilder::HTensor& Handle)
{
	if (Handle.Type == IModelBuilder::HandleType::Tensor)
	{
		return reinterpret_cast<onnx::ValueInfoProto*>(Handle.Ptr);
	}

	return nullptr;
}

inline onnx::NodeProto* OnnxOperatorCast(IModelBuilder::HOperator& Handle)
{
	if (Handle.Type == IModelBuilder::HandleType::Operator)
	{
		return reinterpret_cast<onnx::NodeProto*>(Handle.Ptr);
	}

	return nullptr;
}

/**
 * Builds an ONNX model in memory.
 * NOTE:
 * - We plan to use this only for generating simple networks for testing operators and simple models
 */
class FModelBuilderONNX : public IModelBuilder
{
	static constexpr const char* kOnnxDomain = onnx::ONNX_DOMAIN;

	onnx::ModelProto	Model;
	onnx::GraphProto*	Graph{ nullptr };
	int64				IrVersion { OnnxIrVersion };
	int64				OpsetVersion{ OnnxOpsetVersion };
	
public:

	FModelBuilderONNX(int64 InIrVersion = OnnxIrVersion, int64 InOpsetVersion = OnnxOpsetVersion)
		: IrVersion(InIrVersion)
		, OpsetVersion(InOpsetVersion)
	{
	}
	
	/** Initialize the model builder */
	virtual bool Begin(const FString& Name) override
	{
		// Setup model
		Model.set_ir_version(IrVersion);
		Model.set_domain(kOnnxDomain);

		onnx::OperatorSetIdProto* OpsetProto = Model.add_opset_import();

		OpsetProto->set_domain(kOnnxDomain);
		OpsetProto->set_version(OpsetVersion);

		Graph = Model.mutable_graph();
		Graph->set_name(TCHAR_TO_ANSI(*Name));

		return true;
	}

	/** Serialize the model to given array */
	virtual bool End(TArray<uint8>& Data) override
	{
		const int Size = (int)Model.ByteSizeLong();

		//Data.r
		Data.SetNum(Size);

		bool res = Model.SerializeToArray(Data.GetData(), Data.Num());

		// Validate model
		if (res)
		{
			UE_LOG(LogNNE, Display, TEXT("OrtValidateModelFromMemory... (note: might abort if i.e. shapes are not correct!)"));

			OrtStatusPtr status = OrtValidateModelFromMemory(Data.GetData(), Data.Num());

			if (status)
			{
				UE_LOG(LogNNE, Warning, TEXT("ModelBuilder error:%s"), ANSI_TO_TCHAR(Ort::GetApi().GetErrorMessage(status)));
				return false;
			}
		}

		return res;
	}

	/** Add tensor */
	virtual HTensor AddTensor(const FString& Name, ENNETensorDataType DataType, TArrayView<const int32> Shape, const void* Data, uint64 DataSize) override
	{
		auto Value = onnx::ValueInfoProto().New(Graph->GetArena());
		
		if (!SetValue(Value, Name, DataType, Shape))
		{
			return HTensor();
		}

		if (Data)
		{
			onnx::TensorProto* Tensor = Graph->mutable_initializer()->Add();

			Tensor->set_name(TCHAR_TO_ANSI(*Name));
			Tensor->set_data_type(ToTensorProtoDataType(DataType));
			for (int32 Idx = 0; Idx < Shape.Num(); ++Idx)
			{
				Tensor->add_dims(Shape[Idx]);
			}

			checkf(DataType != ENNETensorDataType::Char, TEXT("Char tensors not supported yet!"));
			// raw_data is not supported for strings, when implementing those one will need to use string_data from TensorProto
			std::string* raw_data = Tensor->mutable_raw_data();
			raw_data->assign((const char*)Data, DataSize);
		}

		return MakeTensorHandle(Value);
	}

	/** Add model input */
	virtual bool AddInput(HTensor Handle) override
	{
		auto Value = OnnxTensorCast(Handle);

		if (!Value)
		{
			return false;
		}

		*Graph->mutable_input()->Add() = *Value;

		return true;
	}

	/** Add model output */
	virtual bool AddOutput(HTensor Handle) override
	{
		auto Value = OnnxTensorCast(Handle);

		if (!Value)
		{
			return false;
		}

		*Graph->mutable_output()->Add() = *Value;

		return true;
	}

	/** Add operator */
	virtual HOperator AddOperator(const FString& TypeName, const FString& Name) override
	{
		FTCHARToUTF8 Convert(*TypeName);

		std::string NodeType = onnx::Symbol(Convert.Get()).toString();

		onnx::NodeProto* Node = Graph->add_node();

		Node->set_op_type(NodeType);

		if (!Name.IsEmpty())
		{
			Node->set_name(TCHAR_TO_ANSI(*Name));
		}
		else
		{
			Node->set_name(TCHAR_TO_ANSI(*TypeName));
		}

		Node->set_domain(Model.domain());

		return MakeOperatorHandle(Node);
	}

	/** Add operator input */
	virtual bool AddOperatorInput(HOperator Op, HTensor Tensor) override
	{
		auto Value = OnnxTensorCast(Tensor);
		auto NodeOp = OnnxOperatorCast(Op);
		auto InName = NodeOp->mutable_input()->Add();
		
		*InName = Value->name();
		
		return true;
	}

	/** Add operator output */
	virtual bool AddOperatorOutput(HOperator Op, HTensor Tensor) override
	{
		auto Value = OnnxTensorCast(Tensor);
		auto NodeOp = OnnxOperatorCast(Op);
		auto OutName = NodeOp->mutable_output()->Add();

		*OutName = Value->name();

		return true;
	}

	/** Add operator attribute */
	virtual bool AddOperatorAttribute(HOperator Op, const FString& Name, const FNNEAttributeValue& Value) override
	{
		auto NodeOp = OnnxOperatorCast(Op);
		onnx::AttributeProto* Attribute = NodeOp->mutable_attribute()->Add();

		Attribute->set_name(TCHAR_TO_ANSI(*Name));
		if (Value.GetType() == ENNEAttributeDataType::Float)
		{
			Attribute->set_type(onnx::AttributeProto::FLOAT);
			Attribute->set_f(Value.GetValue<float>());
		}
		else if (Value.GetType() == ENNEAttributeDataType::Int32)
		{
			Attribute->set_type(onnx::AttributeProto::INT);
			Attribute->set_i(Value.GetValue<int32>());
		}
		else if (Value.GetType() == ENNEAttributeDataType::Int32Array)
		{
			Attribute->set_type(onnx::AttributeProto::INTS);
			for (int32 Val : Value.GetValue<TArray<int32>>())
			{
				Attribute->add_ints(Val);
			}
		}
		else if (Value.GetType() == ENNEAttributeDataType::String)
		{
			Attribute->set_type(onnx::AttributeProto::STRING);
			Attribute->set_s(TCHAR_TO_UTF8(*Value.GetValue<FString>()));
		}
		else if (Value.GetType() == ENNEAttributeDataType::StringArray)
		{
			Attribute->set_type(onnx::AttributeProto::STRINGS);
			for (const FString& Val : Value.GetValue<TArray<FString>>())
			{
				Attribute->add_strings(TCHAR_TO_UTF8(*Val));
			}
		}
		else if (Value.GetType() == ENNEAttributeDataType::FloatArray)
		{
			Attribute->set_type(onnx::AttributeProto::FLOATS);
			for (float Val : Value.GetValue<TArray<float>>())
			{
				Attribute->add_floats(Val);
			}
		}
		else
		{
			checkf(false, TEXT("not implemented"))
		}
		
		return true;
	}

private:

	bool SetValue(onnx::ValueInfoProto* Value, const FString& Name, ENNETensorDataType DataType, const TArrayView<const int32>& InShape)
	{
		onnx::TypeProto*		Type = Value->mutable_type();
		onnx::TypeProto_Tensor* TensorType = Type->mutable_tensor_type();
		onnx::TensorShapeProto* Shape = TensorType->mutable_shape();
		
		Value->set_name(TCHAR_TO_ANSI(*Name));
		TensorType->set_elem_type(ToTensorProtoDataType(DataType));

		for (int32 Idx = 0; Idx < InShape.Num(); ++Idx)
		{
			auto Dim = Shape->add_dim();

			Dim->set_dim_value(InShape[Idx]);
		}

		return true;
	}

	onnx::TensorProto_DataType ToTensorProtoDataType(ENNETensorDataType DataType)
	{
		switch (DataType)
		{
			case ENNETensorDataType::None: return onnx::TensorProto_DataType_UNDEFINED;
			case ENNETensorDataType::Float: return onnx::TensorProto_DataType_FLOAT;
			case ENNETensorDataType::UInt8: return onnx::TensorProto_DataType_UINT8;
			case ENNETensorDataType::Int8: return onnx::TensorProto_DataType_INT8;
			case ENNETensorDataType::UInt16: return onnx::TensorProto_DataType_UINT16;
			case ENNETensorDataType::Int16: return onnx::TensorProto_DataType_INT16;
			case ENNETensorDataType::Int32: return onnx::TensorProto_DataType_INT32;
			case ENNETensorDataType::Int64: return onnx::TensorProto_DataType_INT64;
			//case ENNETensorDataType::String: return onnx::TensorProto_DataType_STRING;
			case ENNETensorDataType::Boolean: return onnx::TensorProto_DataType_BOOL;
			case ENNETensorDataType::Half: return onnx::TensorProto_DataType_FLOAT16;
			case ENNETensorDataType::Double: return onnx::TensorProto_DataType_DOUBLE;
			case ENNETensorDataType::UInt32: return onnx::TensorProto_DataType_UINT32;
			case ENNETensorDataType::UInt64: return onnx::TensorProto_DataType_UINT64;
			case ENNETensorDataType::Complex64: return onnx::TensorProto_DataType_COMPLEX64;
			case ENNETensorDataType::Complex128: return onnx::TensorProto_DataType_COMPLEX128;
			case ENNETensorDataType::BFloat16: return onnx::TensorProto_DataType_BFLOAT16;
			default: return onnx::TensorProto_DataType_UNDEFINED;
		}
	}
};

//
//
//
void BuildShapeForModel(bool ConvertToVariadicShape, const NNECore::FTensorShape& InShape, TArray<int32>& OutShape)
{
	OutShape.Reset();
	for (int32 Idx = 0; Idx < InShape.Rank(); ++Idx)
	{
		int32 Dim = static_cast<int32>(InShape.GetData()[Idx]);
		if (ConvertToVariadicShape)
		{
			Dim = -1;
		}
		OutShape.Emplace(Dim);
	}
}

//
//
//
NNEUTILS_API bool CreateONNXModelForOperator(bool UseVariadicShapeForModel, const FString& OperatorName,
	TConstArrayView<FTensor> InInputTensors, TConstArrayView<FTensor> InOutputTensors,
	TConstArrayView<FTensor> InWeightTensors, TConstArrayView<TArray<char>> InWeightTensorsData,
	const UE::NNECore::FAttributeMap& Attributes, FNNEModelRaw& Model)
{
	Model = FNNEModelRaw{};
	
	int64 IrVersion = OnnxIrVersion;
	int64 OpsetVersion = OnnxOpsetVersion;

	if (OperatorName == TEXT("BatchNormalization") ||	// current implementation is opset 9 (next version is 14)
		OperatorName == TEXT("Clip") ||					// current implementation is opset 6 (next version is 11)
		OperatorName == TEXT("Pad") ||					// current implementation is opset 2 (next version is 11)
		OperatorName == TEXT("Split") ||				// current implementation is opset 2 (next version is 11)
		OperatorName == TEXT("Shape") ||				// current implementation is opset 1 (next version is 13)
		OperatorName == TEXT("Slice") ||				// current implementation is opset 1 (next version is 10)
		OperatorName == TEXT("Squeeze") ||				// current implementation is opset 1 (next version is 11)
		OperatorName == TEXT("Unsqueeze") ||			// current implementation is opset 1 (next version is 11)
		OperatorName == TEXT("Upsample")				// deprecated starting opset 10
		)				
	{
		OpsetVersion = 9;
	}
	else
	if (OperatorName == TEXT("ReduceL1") ||				// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceL2") ||				// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceLogSum") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceLogSumExp") ||		// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceLogMin") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceLogMax") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceMean") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceProd") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceSum") ||			// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("ReduceSumSquare") ||		// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("Resize") ||				// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("Squeeze") ||				// current implementation is opset 11 (next version is 13)
		OperatorName == TEXT("Unsqueeze")				// current implementation is opset 11 (next version is 13)
		)
	{
		OpsetVersion = 11;
	}

	TUniquePtr<IModelBuilder> Builder(CreateONNXModelBuilder(IrVersion, OpsetVersion));

	Builder->Begin();

	TArray<int32> ShapeForModel;
	TArray<IModelBuilder::HTensor> InputTensors;
	
	for (int32 Idx = 0; Idx < InInputTensors.Num(); ++Idx)
	{
		const FTensor& Desc = InInputTensors[Idx];
		BuildShapeForModel(UseVariadicShapeForModel, Desc.GetShape(), ShapeForModel);
		IModelBuilder::HTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel);

		InputTensors.Emplace(Tensor);
		Builder->AddInput(Tensor);
	}

	TArray<IModelBuilder::HTensor> OutputTensors;

	for (int32 Idx = 0; Idx < InOutputTensors.Num(); ++Idx)
	{
		const FTensor& Desc = InOutputTensors[Idx];
		BuildShapeForModel(UseVariadicShapeForModel, Desc.GetShape(), ShapeForModel);
		IModelBuilder::HTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel);

		OutputTensors.Emplace(Tensor);
		Builder->AddOutput(Tensor);
	}

	checkf(InWeightTensors.Num() == InWeightTensorsData.Num(), TEXT("Invalid weight tensors data"));
	TArray<IModelBuilder::HTensor> WeightTensors;

	for (int32 Idx = 0; Idx < InWeightTensors.Num(); ++Idx)
	{
		const FTensor& Desc = InWeightTensors[Idx];
		const TArray<char>& Data = InWeightTensorsData[Idx];
		check(Data.Num() == Desc.GetDataSize());
		BuildShapeForModel(false, Desc.GetShape(), ShapeForModel);
		IModelBuilder::HTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel, Data.GetData(), Data.Num());

		WeightTensors.Emplace(Tensor);
	}

	auto Op = Builder->AddOperator(OperatorName);

	for (int32 Idx = 0; Idx < InputTensors.Num(); ++Idx)
	{
		Builder->AddOperatorInput(Op, InputTensors[Idx]);
	}

	for (int32 Idx = 0; Idx < WeightTensors.Num(); ++Idx)
	{
		// For now weights are added after model inputs in the list of
		// operator inputs. This should be made more flexible if needed by future tests cases.
		Builder->AddOperatorInput(Op, WeightTensors[Idx]);
	}

	for (int32 Idx = 0; Idx < OutputTensors.Num(); ++Idx)
	{
		Builder->AddOperatorOutput(Op, OutputTensors[Idx]);
	}

	for (int32 Idx = 0; Idx < Attributes.Num(); ++Idx)
	{
		Builder->AddOperatorAttribute(Op, Attributes.GetName(Idx), Attributes.GetAttributeValue(Idx));
	}

	
	Builder->End(Model.Data);
	Model.Format = ENNEInferenceFormat::ONNX;
	
	return true;
}

/** Return instance of ONNX model builder */
NNEUTILS_API IModelBuilder* CreateONNXModelBuilder(int64 IrVersion, int64 OpsetVersion)
{
	return new FModelBuilderONNX(IrVersion, OpsetVersion);
}

} // namespace UE::NNEUtils::Internal
