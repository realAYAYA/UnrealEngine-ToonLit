// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NNETypes.h"
#include "NNETensor.h"

namespace UE::NNE { class FAttributeMap; }
struct FNNEAttributeValue;
struct FNNEModelRaw;

namespace UE::NNEUtils::Internal
{
using FTensor = NNE::Internal::FTensor;

class IModelBuilder
{
public:

	enum class HandleType : uint8
	{
		Invalid,
		Tensor,
		Operator
	};

	template<typename Tag>
	struct Handle
	{
		void*		Ptr { nullptr };
		HandleType	Type { HandleType::Invalid };
	};

	typedef struct Handle<struct TensorTag>		HTensor;
	typedef struct Handle<struct OperatorTag>	HOperator;

	template<typename TensorT>
	HTensor MakeTensorHandle(TensorT* TensorPtr)
	{
		return HTensor{ TensorPtr, HandleType::Tensor };
	}

	template<typename OperatorT>
	HOperator MakeOperatorHandle(OperatorT* OperatorPtr)
	{
		return HOperator{ OperatorPtr, HandleType::Operator };
	}

	virtual ~IModelBuilder() = default;

	/** Initialize the model builder */
	virtual bool Begin(const FString& Name = TEXT("main")) = 0;

	/** Serialize the model to given array */
	virtual bool End(TArray<uint8>& Data) = 0;

	/** Add tensor */
	virtual HTensor AddTensor(const FString& Name, ENNETensorDataType DataType, TArrayView<const int32> Shape, const void* Data = nullptr, uint64 DataSize = 0) = 0;

	/** Add model input */
	virtual bool AddInput(HTensor InTensor) = 0;

	/** Add model output */
	virtual bool AddOutput(HTensor OutTensor) = 0;

	/** Add operator */
	virtual HOperator AddOperator(const FString& Type, const FString& Name = TEXT("")) = 0;

	/** Add operator input */
	virtual bool AddOperatorInput(HOperator Op, HTensor Tensor) = 0;

	/** Add operator attribute */
	virtual bool AddOperatorAttribute(HOperator Op, const FString& Name, const FNNEAttributeValue& Value) = 0;
	
	/** Add operator output */
	virtual bool AddOperatorOutput(HOperator Op, HTensor Tensor) = 0;
};

/** Default ONNX IR version */
static constexpr int64 OnnxIrVersion = 7;

/** Default ONNX operator set version */
static constexpr int64 OnnxOpsetVersion = 15;

/**
 * Create an instance of ONNX model builder that creates ONNX models in memory
 */
NNEUTILS_API IModelBuilder* CreateONNXModelBuilder(int64 IrVersion = OnnxIrVersion, int64 OpsetVersion = OnnxOpsetVersion);

/**
 * Utility functions to create single layer NN for operator testing with optional attributes
 */
NNEUTILS_API bool CreateONNXModelForOperator(bool UseVariadicShapeForModel, const FString& OperatorName, 
	TConstArrayView<FTensor> InInputTensors, TConstArrayView<FTensor> InOutputTensors,
	TConstArrayView<FTensor> InWeightTensors, TConstArrayView<TArray<char>> InWeightTensorsData,
	const UE::NNE::FAttributeMap& Attributes, FNNEModelRaw& ModelData);

/**
 * Create an instance of NNE model builder that creates NNE model/format in memory
 */
NNEUTILS_API IModelBuilder* CreateNNEModelBuilder();

} // UE::NNEUtils::Internal

