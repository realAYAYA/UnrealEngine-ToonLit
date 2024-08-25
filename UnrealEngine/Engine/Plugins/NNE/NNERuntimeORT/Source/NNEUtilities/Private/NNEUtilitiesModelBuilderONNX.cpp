// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEUtilitiesModelBuilderONNX.h"

#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNERuntimeFormat.h"

#include "NNEUtilitiesThirdPartyWarningDisabler.h"
NNE_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT

#include "onnx/checker.h"
#include "onnx/common/ir.h"
#include "onnx/common/constants.h"
#include "onnx/defs/operator_sets.h"

NNE_THIRD_PARTY_INCLUDES_END

namespace UE::NNEUtilities::Internal
{

namespace ModelBuilderONNXHelper
{
	onnx::ValueInfoProto* OnnxTensorCast(IModelBuilder::FHTensor& Handle)
	{
		if (Handle.Type == IModelBuilder::EHandleType::Tensor)
		{
			return reinterpret_cast<onnx::ValueInfoProto*>(Handle.Ptr);
		}

		return nullptr;
	}

	onnx::NodeProto* OnnxOperatorCast(IModelBuilder::FHOperator& Handle)
	{
		if (Handle.Type == IModelBuilder::EHandleType::Operator)
		{
			return reinterpret_cast<onnx::NodeProto*>(Handle.Ptr);
		}

		return nullptr;
	}

	void BuildShapeForModel(bool ConvertToVariadicShape, const NNE::FTensorShape& InShape, TArray<int32>& OutShape)
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
} // namespace ModelBuilderONNXHelper

class FModelBuilderONNX : public IModelBuilder
{
	static constexpr const char* kOnnxDomain = onnx::ONNX_DOMAIN;

	onnx::ModelProto	Model;
	onnx::GraphProto*	Graph{ nullptr };
	int64				IrVersion;
	int64				OpsetVersion;
	
public:

	FModelBuilderONNX(int64 InIrVersion, int64 InOpsetVersion)
		: IrVersion(InIrVersion) , OpsetVersion(InOpsetVersion) {}
	
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

	virtual bool End(TArray<uint8>& Data) override
	{
		const int Size = (int)Model.ByteSizeLong();

		Data.SetNum(Size);

		bool res = Model.SerializeToArray(Data.GetData(), Data.Num());

		if (res)
		{
#ifdef ONNX_NO_EXCEPTIONS
			static_assert(false, "ONNX_NO_EXCEPTIONS is defined meaning onnx check_model would abort the program in case of validation failure.");
#else
			try
			{
				onnx::checker::check_model(Model);
			}
			catch (onnx::checker::ValidationError& e)
			{
				UE_LOG(LogNNE, Warning, TEXT("ModelBuilder error:%s"), ANSI_TO_TCHAR(e.what()));
				return false;
			}
#endif
		}

		return res;
	}

	virtual FHTensor AddTensor(const FString& Name, ENNETensorDataType DataType, TArrayView<const int32> Shape, const void* Data, uint64 DataSize) override
	{
		onnx::ValueInfoProto* Value = onnx::ValueInfoProto().New(Graph->GetArena());
		
		if (!SetValue(Value, Name, DataType, Shape))
		{
			return FHTensor();
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

		return MakeHandle<EHandleType::Tensor>(Value);
	}

	virtual bool AddInput(FHTensor Handle) override
	{
		onnx::ValueInfoProto* Value = ModelBuilderONNXHelper::OnnxTensorCast(Handle);

		if (!Value)
		{
			return false;
		}

		*Graph->mutable_input()->Add() = *Value;

		return true;
	}

	virtual bool AddOutput(FHTensor Handle) override
	{
		onnx::ValueInfoProto* Value = ModelBuilderONNXHelper::OnnxTensorCast(Handle);

		if (!Value)
		{
			return false;
		}

		*Graph->mutable_output()->Add() = *Value;

		return true;
	}

	virtual FHOperator AddOperator(const FString& TypeName, const FString& Domain, TOptional<uint32> Version, const FString& Name) override
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

		return MakeHandle<EHandleType::Operator>(Node);
	}

	virtual bool AddOperatorInput(FHOperator Op, FHTensor Tensor) override
	{
		onnx::ValueInfoProto* Value = ModelBuilderONNXHelper::OnnxTensorCast(Tensor);
		onnx::NodeProto* NodeOp = ModelBuilderONNXHelper::OnnxOperatorCast(Op);
		
		*(NodeOp->mutable_input()->Add()) = Value->name();
		
		return true;
	}

	virtual bool AddOperatorOutput(FHOperator Op, FHTensor Tensor) override
	{
		onnx::ValueInfoProto* Value = ModelBuilderONNXHelper::OnnxTensorCast(Tensor);
		onnx::NodeProto* NodeOp = ModelBuilderONNXHelper::OnnxOperatorCast(Op);

		*(NodeOp->mutable_output()->Add()) = Value->name();

		return true;
	}

	virtual bool AddOperatorAttribute(FHOperator Op, const FString& Name, const FNNEAttributeValue& Value) override
	{
		onnx::NodeProto* NodeOp = ModelBuilderONNXHelper::OnnxOperatorCast(Op);
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
			onnx::TensorShapeProto_Dimension* Dim = Shape->add_dim();
			if (InShape[Idx] != -1)
			{
				//see https://github.com/onnx/onnx/issues/4272
				Dim->set_dim_value(InShape[Idx]);
			}
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

bool CreateONNXModelForOperator(const FString& OperatorName, int32 IrVersion, int32 OpsetVersion, bool bUseVariadicShapeForModel,
	TConstArrayView<NNE::Internal::FTensor> InInputTensors, TConstArrayView<NNE::Internal::FTensor> InOutputTensors,
	TConstArrayView<NNE::Internal::FTensor> InWeightTensors, TConstArrayView<TConstArrayView<uint8>> InWeightTensorsData,
	const UE::NNE::FAttributeMap& Attributes, FNNEModelRaw& Model)
{
	Model = FNNEModelRaw{};

	TUniquePtr<IModelBuilder> Builder(CreateONNXModelBuilder((int64) IrVersion, (int64) OpsetVersion));

	Builder->Begin();

	TArray<int32> ShapeForModel;
	TArray<IModelBuilder::FHTensor> InputTensors;
	
	for (int32 Idx = 0; Idx < InInputTensors.Num(); ++Idx)
	{
		const NNE::Internal::FTensor& Desc = InInputTensors[Idx];
		ModelBuilderONNXHelper::BuildShapeForModel(bUseVariadicShapeForModel, Desc.GetShape(), ShapeForModel);
		IModelBuilder::FHTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel);

		InputTensors.Emplace(Tensor);
		Builder->AddInput(Tensor);
	}

	TArray<IModelBuilder::FHTensor> OutputTensors;

	for (int32 Idx = 0; Idx < InOutputTensors.Num(); ++Idx)
	{
		const NNE::Internal::FTensor& Desc = InOutputTensors[Idx];
		ModelBuilderONNXHelper::BuildShapeForModel(bUseVariadicShapeForModel, Desc.GetShape(), ShapeForModel);
		IModelBuilder::FHTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel);

		OutputTensors.Emplace(Tensor);
		Builder->AddOutput(Tensor);
	}

	checkf(InWeightTensors.Num() == InWeightTensorsData.Num(), TEXT("Invalid weight tensors data"));
	TArray<IModelBuilder::FHTensor> WeightTensors;

	for (int32 Idx = 0; Idx < InWeightTensors.Num(); ++Idx)
	{
		const NNE::Internal::FTensor& Desc = InWeightTensors[Idx];
		const TConstArrayView<uint8>& Data = InWeightTensorsData[Idx];
		check(Data.Num() == Desc.GetDataSize());
		ModelBuilderONNXHelper::BuildShapeForModel(false, Desc.GetShape(), ShapeForModel);
		IModelBuilder::FHTensor Tensor = Builder->AddTensor(Desc.GetName(), Desc.GetDataType(), ShapeForModel, Data.GetData(), Data.Num());

		WeightTensors.Emplace(Tensor);
	}

	IModelBuilder::FHOperator Op = Builder->AddOperator(OperatorName, OnnxDomainName, (uint32) OpsetVersion);

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

TUniquePtr<IModelBuilder> CreateONNXModelBuilder(int64 IrVersion, int64 OpsetVersion)
{
	return MakeUnique<FModelBuilderONNX>(IrVersion, OpsetVersion);
}

} // namespace UE::NNEUtilities::Internal
