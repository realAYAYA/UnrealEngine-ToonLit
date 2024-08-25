// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNEUtilitiesModelBuilderNNE.h"

#include "NNE.h"
#include "NNEAttributeMap.h"
#include "NNERuntimeFormat.h"
#include "Misc/StringBuilder.h"
#include "Serialization/MemoryWriter.h"

namespace UE::NNEUtilities::Internal
{

namespace ModelBuilderNNEHelper
{
	int32 NNETensorCast(IModelBuilder::FHTensor& Handle)
	{
		if (Handle.Type == IModelBuilder::EHandleType::Tensor)
		{
			return int32(reinterpret_cast<int64>(Handle.Ptr));
		}

		return -1;
	}

	int32 NNEOperatorCast(IModelBuilder::FHOperator& Handle)
	{
		if (Handle.Type == IModelBuilder::EHandleType::Operator)
		{
			return int32(reinterpret_cast<int64>(Handle.Ptr));
		}

		return -1;
	}
} //namespace ModelBuilderNNEHelper

class FModelBuilderNNE : public IModelBuilder
{
public:
	virtual bool Begin(const FString& Name) override
	{
		return true;
	}

	virtual bool End(TArray<uint8>& Data) override
	{
		FMemoryWriter Writer(Data);

		Format.Serialize(Writer);

		return !Data.IsEmpty();
	}

	virtual FHTensor AddTensor(const FString& Name, ENNETensorDataType DataType, TArrayView<const int32> Shape, const void* Data, uint64 DataSize)
	{
		TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>> NNEShape;
		for (int32 i = 0; i < Shape.Num(); ++i)
		{
			//Allow caller to use 0 for variable dimensions for inputs/outputs, NNE use -1.
			//RDG not supporting 0 sized dimension at the moment.
			NNEShape.Emplace(!Data && Shape[i] == 0 ? -1 : Shape[i]);
		}

		int32 Idx = AddTensor(Name, NNEShape, DataType, Data, DataSize);

		return MakeHandle<EHandleType::Tensor>(reinterpret_cast<void*>((int64)Idx));
	}

	virtual bool AddInput(FHTensor Tensor) override
	{
		int32 Idx = ModelBuilderNNEHelper::NNETensorCast(Tensor);

		if (Idx < 0 || Idx >= Format.Tensors.Num())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add input tensor, invalid tensor index"));
			return false;
		}

		if (Format.Tensors[Idx].Type != ENNEFormatTensorType::None)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add input tensor, tensor usage already set up"));
			return false;
		}

		Format.Tensors[Idx].Type = ENNEFormatTensorType::Input;

		return true;
	}

	virtual bool AddOutput(FHTensor Tensor) override
	{
		int32 Idx = ModelBuilderNNEHelper::NNETensorCast(Tensor);

		if (Idx < 0 || Idx >= Format.Tensors.Num())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add output tensor, invalid tensor index"));
			return false;
		}

		if (Format.Tensors[Idx].Type != ENNEFormatTensorType::None)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add output tensor, tensor usage already set up"));
			return false;
		}

		Format.Tensors[Idx].Type = ENNEFormatTensorType::Output;

		return true;
	}

	virtual FHOperator AddOperator(const FString& TypeName, const FString& Domain, TOptional<uint32> Version, const FString& Name = TEXT("")) override
	{
		int32 Idx = Format.Operators.Num();

		FNNEFormatOperatorDesc	Operator{};

		Operator.TypeName = TypeName;
		Operator.DomainName = Domain;
		Operator.Version = Version;
		Format.Operators.Emplace(Operator);

		return MakeHandle<EHandleType::Operator>(reinterpret_cast<void*>((int64)Idx));
	}

	virtual bool AddOperatorInput(FHOperator Op, FHTensor Tensor) override
	{
		int32 OpIdx = ModelBuilderNNEHelper::NNEOperatorCast(Op);
		int32 TensorIdx = ModelBuilderNNEHelper::NNETensorCast(Tensor);

		if (TensorIdx < 0 || TensorIdx >= Format.Tensors.Num())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add operator input tensor, invalid tensor index"));
			return false;
		}

		Format.Operators[OpIdx].InTensors.Emplace(TensorIdx);

		return true;
	}

	virtual bool AddOperatorOutput(FHOperator Op, FHTensor Tensor) override
	{
		int32 OpIdx = ModelBuilderNNEHelper::NNEOperatorCast(Op);
		int32 TensorIdx = ModelBuilderNNEHelper::NNETensorCast(Tensor);

		if (TensorIdx < 0 || TensorIdx >= Format.Tensors.Num())
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add operator output tensor, invalid tensor index"));
			return false;
		}

		if (Format.Tensors[TensorIdx].Type == ENNEFormatTensorType::Input)
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to add output tensor, tensor usage already set up to input"));
			return false;
		}

		if (Format.Tensors[TensorIdx].Type == ENNEFormatTensorType::None)
		{
			Format.Tensors[TensorIdx].Type = ENNEFormatTensorType::Intermediate;
		}

		Format.Operators[OpIdx].OutTensors.Emplace(TensorIdx);

		return true;
	}

	virtual bool AddOperatorAttribute(FHOperator Op, const FString& Name, const FNNEAttributeValue& Value) override
	{
		int32 OpIdx = ModelBuilderNNEHelper::NNEOperatorCast(Op);

		FNNEFormatAttributeDesc& Attribute = Format.Operators[OpIdx].Attributes.Emplace_GetRef();
		Attribute.Name = Name;
		Attribute.Value = Value;

		return true;
	}

private:

	int32 AddTensor(const FString& InName, TArrayView<const int32> InShape, ENNETensorDataType InDataType, const void* Data, uint64 DataSize)
	{
		int32 Idx = -1;

		int32* Val = TensorMap.Find(InName);

		if (Val)
		{
			Idx = *Val;
		}
		else
		{
			FNNEFormatTensorDesc	Desc{};

			Desc.Name = InName;
			Desc.Shape = InShape;
			Desc.Type = ENNEFormatTensorType::None;
			Desc.DataType = InDataType;

			if (Data)
			{
				Desc.Type = ENNEFormatTensorType::Initializer;

				// Handle empty data initializers, i.e. when DataSize is 0
				if (DataSize)
				{
					Desc.DataOffset = Format.TensorData.AddUninitialized(DataSize);
					Desc.DataSize = DataSize;

					FMemory::Memcpy(Format.TensorData.GetData() + Desc.DataOffset, Data, DataSize);
				}
			}
			else
			{
				if (Desc.DataType == ENNETensorDataType::None && InName.IsEmpty())
				{
					Desc.Type = ENNEFormatTensorType::Empty;
				}
			}

			Format.Tensors.Add(Desc);
			Idx = Format.Tensors.Num() - 1;

			TensorMap.Add(InName, Idx);
		}

		return Idx;
	}


	FNNERuntimeFormat	Format;
	TMap<FString, int32>	TensorMap;
};

TUniquePtr<IModelBuilder> CreateNNEModelBuilder()
{
	return MakeUnique<FModelBuilderNNE>();
}

} // namespace UE::NNEUtilities::Internal
