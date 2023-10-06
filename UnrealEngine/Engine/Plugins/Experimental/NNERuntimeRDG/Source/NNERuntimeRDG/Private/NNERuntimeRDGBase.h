// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NNEAttributeMap.h"
#include "NNEAttributeValue.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNE.h"
#include "NNEModelOptimizerInterface.h"
#include "NNERuntimeFormat.h"
#include "RenderGraphResources.h"
#include "Serialization/MemoryReader.h"
#include "ShaderParameterUtils.h"



BEGIN_SHADER_PARAMETER_STRUCT(FNNETensorReadbackParameters, )
	RDG_BUFFER_ACCESS(Buffer, ERHIAccess::CopySrc)
END_SHADER_PARAMETER_STRUCT()

class FRDGBuilder;
struct FNNEModelRaw;

namespace UE::NNE { class FTensorDesc; }

namespace UE::NNERuntimeRDG::Private
{

/**
 * Interface for all operators to prepare the model tensors at scheduling time
 */
struct IPrepareOperator
{
	virtual ~IPrepareOperator() = default;
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const = 0;
};

/**
* Interface for all ML operators running on the RDG 
*/
struct IOperatorRDG
{
	virtual ~IOperatorRDG() = default;
};

class FTensorRDG : public NNE::Internal::FTensor
{
	FRDGBufferRef Buffer{};
	
public:
	static FTensorRDG Make(const NNE::FTensorDesc& TensorDesc, const NNE::FTensorShape& Shape, FRDGBufferRef Buffer)
	{
		check(Shape.IsCompatibleWith(TensorDesc.GetShape()));
		FTensorRDG TensorRDG;
		TensorRDG.Buffer = nullptr;
		TensorRDG.Name = TensorDesc.GetName();
		TensorRDG.DataType = TensorDesc.GetDataType();
		TensorRDG.Shape = Shape;
		TensorRDG.Volume = Shape.Volume();
		TensorRDG.DataSize = (uint64)NNE::GetTensorDataTypeSizeInBytes(TensorRDG.DataType) * TensorRDG.Volume;
		TensorRDG.Buffer = Buffer;
		check(TensorRDG.Volume <= TNumericLimits<uint32>::Max());
		return TensorRDG;
	}

	bool HasBuffer() const { return Buffer != FRDGBufferRef{}; }
	void SetBuffer(FRDGBufferRef Inbuffer){ Buffer = Inbuffer; }
	FRDGBufferRef GetBuffer() const { return Buffer; }
};

using FTensorRDGRef = FTensorRDG*;
using FTensorRDGArray = TArray<FTensorRDG, TInlineAllocator<16>>;
using FTensorRDGRefArray = TArray<FTensorRDGRef, TInlineAllocator<64>>;
using FIntArray = TArray<int32, TInlineAllocator<16>>;

bool AlwaysValidValidationFunction(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes);

class FInputValidator
{
public:
	FInputValidator();
	void AddRequired(int32 TemplateIdx = 0);
	void AddOptional(int32 TemplateIdx = 0);
	void SetTemplateCount(int32 TemplateCount);
	void AddSupportedType(ENNETensorDataType Type, int32 TemplateIdx = 0);
	bool Validate(TConstArrayView<ENNETensorDataType> InputTypes);

private:
	TArray<TArray<ENNETensorDataType>> TemplateTypes;
	TArray<int32> InputTemplateIndices;
	int32 NumRequiredInput;
	int32 NumOptionalInput;
};

class FAttributeValidator
{
public:
	void AddOptional(const FString& Name, ENNEAttributeDataType Type);
	void AddRequired(const FString& Name, ENNEAttributeDataType Type);
	bool Validate(const NNE::FAttributeMap& AttributesToValidate);

private:
	struct FEntry
	{
		FEntry(const FString& InName, ENNEAttributeDataType InType)
			: Name(InName), Type(InType)
		{
		}

		//Idea: we could extended as needed by operator to support more validation especially around the range of the values.
		//An example is ConvTranspose `auto_pad` enum style string that can only take a few values
		//In the same direction we might only support a range of value for a float (for example
		//we only support integer but the type is float, or only positive values for an int32)
		FString Name;
		ENNEAttributeDataType Type;
	};

	TArray<FEntry> RequiredAttributes;
	TArray<FEntry> OptionalAttributes;
};

/**
 * Registry for RDG ML operators
 */
template<class TOperatorType>
class TOperatorRegistryRDG
{
public:

	typedef TOperatorType* (*OperatorCreateFunc)();
	typedef bool (*OperatorValidateFunc)(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes);

	static TOperatorRegistryRDG* Get()
	{
		static TOperatorRegistryRDG Inst;

		return &Inst;
	}

	OperatorValidateFunc OpFindValidation(const FString& Name)
	{
		OperatorValidateFunc* Fn = OperatorValidations.Find(Name);

		if (!Fn)
		{
			UE_LOG(LogNNE, Warning, TEXT("RDG MLOperator:%s is not registered"), *Name);
			return nullptr;
		}

		return *Fn;
	}

	OperatorCreateFunc OpFind(const FString& Name)
	{
		OperatorCreateFunc* Fn = Operators.Find(Name);

		if (!Fn)
		{
			UE_LOG(LogNNE, Warning, TEXT("RDG MLOperator:%s is not registered"), *Name);
			return nullptr;
		}

		return *Fn;
	}

	bool OpAdd(const FString& Name, OperatorCreateFunc Func, OperatorValidateFunc ValidateFunc = AlwaysValidValidationFunction)
	{
		if (Operators.Find(Name) != nullptr)
		{
			UE_LOG(LogNNE, Warning, TEXT("RDG MLOperator is already registered:%s"), *Name);
			return false;
		}

		Operators.Add(Name, Func);
		OperatorValidations.Add(Name, ValidateFunc);
		return true;
	}

private:

	TMap<FString, OperatorCreateFunc> Operators;
	TMap<FString, OperatorValidateFunc> OperatorValidations;
};

/**
 * Validator for RDG ML operators
 */
template<class TOperatorType>
class TModelValidatorRDG : public UE::NNE::Internal::IModelValidator
{
public:
	virtual FString GetName() const 
	{
		return TEXT("RDG Model validator");
	}
	
	virtual bool ValidateModel(const FNNEModelRaw& InputModel, const UE::NNE::Internal::FOptimizerOptionsMap& Options) const override
	{
		FNNERuntimeFormat	Format;

		ENNEInferenceFormat FormatType = InputModel.Format;
		if (FormatType != ENNEInferenceFormat::NNERT)
		{
			UE_LOG(LogNNE, Warning, TEXT("Unsupported format type for validator %s"), *GetName());
			return false;
		}

		FMemoryReader Reader(InputModel.Data);
		FNNERuntimeFormat::StaticStruct()->SerializeBin(Reader, &Format);

		TOperatorRegistryRDG<TOperatorType>* Registry = TOperatorRegistryRDG<TOperatorType>::Get();
		check(Registry != nullptr);

		for (int32 Idx = 0; Idx < Format.Operators.Num(); ++Idx)
		{
			TArray<ENNETensorDataType> InputTensorTypes;
			TArray<NNE::FSymbolicTensorShape> InputTensorShapes;
			NNE::FAttributeMap AttributeMap;
			
			for (int32 InputTensorIndex: Format.Operators[Idx].InTensors)
			{
				InputTensorTypes.Add(Format.Tensors[InputTensorIndex].DataType);
				InputTensorShapes.Add(NNE::FSymbolicTensorShape::Make(Format.Tensors[InputTensorIndex].Shape));
			}
			for (const FNNEFormatAttributeDesc& Desc : Format.Operators[Idx].Attributes)
			{
				AttributeMap.SetAttribute(Desc.Name, Desc.Value);
			}

			const FString& OpType = Format.Operators[Idx].TypeName;
			
			typename TOperatorRegistryRDG<TOperatorType>::OperatorValidateFunc ValidationFn = Registry->OpFindValidation(OpType);

			if (!ValidationFn)
			{
				UE_LOG(LogNNE, Warning, TEXT("RDG MLOperatorRegistry failed to find validation for operator:%s"), *OpType);
				return false;
			}
			
			if (!ValidationFn(AttributeMap, InputTensorTypes, InputTensorShapes))
			{
				UE_LOG(LogNNE, Warning, TEXT("RDG MLOperatorRegistry failed to validate operator:%s"), *OpType);
				return false;
			}
		}

		return true;
	}
};

} // UE::NNERuntimeRDG::Private