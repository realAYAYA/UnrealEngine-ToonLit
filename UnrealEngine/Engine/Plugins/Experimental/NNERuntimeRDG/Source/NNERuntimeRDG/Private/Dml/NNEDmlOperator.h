// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef NNE_USE_DIRECTML

#include "NNEDmlCommon.h"
#include "NNETypes.h"
#include "NNETensor.h"
#include "NNEAttributeMap.h"
#include "NNERuntimeRDGBase.h"

#define NNE_DML_REGISTER_OP(OpName) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), FOperatorDml##OpName##::Create, FOperatorDml##OpName##::Validate); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;


namespace UE::NNERuntimeRDG::Private::Dml
{

static constexpr uint32_t NcdhwDimensionCount = 5;
static constexpr uint32_t NcdhwSpatialDimensionCount = 3;
static constexpr uint32_t NonspatialDimensionCount = 2; // The batch and channel dimensions of NCW, NCHW, NCDHW....

template<typename T>
inline TArrayView<T> MakeEmptyArrayView()
{
	return MakeArrayView(static_cast<T*>(nullptr), 0);
}

template<typename T>
inline TConstArrayView<T> MakeEmptyConstArrayView()
{
	return TConstArrayView<T>(static_cast<const T*>(nullptr), 0);
}

//
//
//
class FDmlDeviceContext
{
public:

	uint32							DeviceIndex;
	ID3D12Device*					D3D12Device{ nullptr }; // Borrowed reference from RHI
	TComPtr<IDMLDevice>				Device{ nullptr };
	TComPtr<IDMLCommandRecorder>	CmdRec{ nullptr };
};

class FTensorDescDml;

namespace Util
{
	
template<typename T>
using FSmallArray = TArray<T, TInlineAllocator<NNE::FTensorShape::MaxRank>>;
using FSmallIntArray = TArray<int32, TInlineAllocator<NNE::FTensorShape::MaxRank>>;
using FSmallUIntArray = TArray<uint32, TInlineAllocator<NNE::FTensorShape::MaxRank>>;

template<typename InputType, typename OutputType>
inline bool IsOverflowing(InputType Input)
{
	OutputType Output = static_cast<OutputType>(Input);
	if(Input != static_cast<InputType>(Output))
	{
		return true;
	}
	return false;
}

template<typename InputType, typename OutputType>
inline bool ConvertArrayViewNoOverflow(TConstArrayView<InputType> InputView, TArrayView<OutputType>& OutputView)
{
	OutputView = MakeArrayView((OutputType*) InputView.GetData(), InputView.Num());
	for(int32 Idx = 0; Idx < InputView.Num(); ++Idx)
	{
		if(InputView[Idx] != static_cast<InputType>(OutputView[Idx]))
		{
			return false;
		}
	}
	return true;
}

template<typename OutputType, typename AllocatorType>
inline bool GetArrayAttributeNoOverflow(
	const FNNEAttributeValue* Attr, 
	TArray<OutputType, AllocatorType>& OutputArray, 
	TConstArrayView<OutputType> DefaultValues = MakeEmptyConstArrayView<OutputType>()
	)
{
	if (Attr)
	{

		TArrayView<OutputType> ConvertedView;
		TArray<int32> IntArray;
		TArray<float> FloatArray;

		switch(Attr->GetType())
		{
		case ENNEAttributeDataType::Int32Array:
			{
				IntArray = Attr->GetValue<TArray<int32>>();
				if(!ConvertArrayViewNoOverflow(TConstArrayView<int32>(IntArray), ConvertedView))
				{
					return false;
				}
			}
			break;
		case ENNEAttributeDataType::FloatArray:
			{
				FloatArray = Attr->GetValue<TArray<float>>();
				if(!ConvertArrayViewNoOverflow(TConstArrayView<float>(FloatArray), ConvertedView))
				{
					return false;
				}
			}
			break;
		default:
			return false;
		}
			
		OutputArray = TArray<OutputType, AllocatorType>{ ConvertedView };
	}
	else
	{
		OutputArray = TArray<OutputType, AllocatorType>{ DefaultValues };
	}
	return true;
}

extern bool IsSameShape(const NNE::Internal::FTensor& Left, const NNE::Internal::FTensor& Right);
extern bool IsSameShape(const FTensorDescDml& Left, const FTensorDescDml& Right);
extern DML_TENSOR_DATA_TYPE GetTensorDataType(ENNETensorDataType DataType);

} // Util

class FTensorDescDml
{
public:

	FTensorDescDml();

	// Set tensor minimum and maximum rank.
	// By default minimum and maximum range is between 1 and DML_TENSOR_DIMENSION_COUNT_MAX1 (8).
	FTensorDescDml& SetTensorRank(int32 MinRank, int32 MaxRank);

	// Set tensor shape
	FTensorDescDml& SetShape(TConstArrayView<uint32> Shape);

	// Utility method that calls SetShape(Shape)
	FTensorDescDml& SetShape(const UE::NNE::FTensorShape& Shape)
	{
		return SetShape(Shape.GetData());
	}

	// Set shape that and make sure that it has the given rank
	FTensorDescDml& SetShape(TConstArrayView<uint32> Shape, int32 Rank);

	// Utility method that calls SetShape(Shape, Rank)
	FTensorDescDml& SetShape(const UE::NNE::FTensorShape& Shape, int32 Rank)
	{
		return SetShape(Shape.GetData(), Rank);
	}

	// Set shape and match the broadcast shape
	FTensorDescDml& SetShape(TConstArrayView<uint32> Shape, TConstArrayView<uint32> BroadcastShape);

	// Utility method that calles SetShape() from above
	FTensorDescDml& SetShape(const UE::NNE::FTensorShape& Shape, const UE::NNE::FTensorShape& BroadcastShape)
	{
		return SetShape(Shape.GetData(), BroadcastShape.GetData());
	}

	// Use for tensors that have shape [1, C, 1] or [1, C, 1, 1] etc.
	FTensorDescDml& SetShape1D(uint32 Dimension, int32 Rank);
	
	// Set strides computed from shape (used for broadcasting)
	FTensorDescDml& SetStridesFromShape(TConstArrayView<uint32> Shape);

	// Set strides computed from shape (used for broadcasting)
	FTensorDescDml& SetStridesFromShape(const UE::NNE::FTensorShape& Shape)
	{
		return SetStridesFromShape(Shape.GetData());
	}

	// Set computed strides
	FTensorDescDml& SetStrides(TConstArrayView<uint32> Strides);

	// Set tensor data type
	FTensorDescDml& SetDataType(ENNETensorDataType DataType);

	// Set tensor flags
	FTensorDescDml& SetDataOwnedByDml(bool bSetOwnedByDml);

	// Utility method to use FTensor to set all the members
	FTensorDescDml& SetFromTensor(const UE::NNE::Internal::FTensor& Tensor)
	{
		return SetShape(Tensor.GetShape())
			.SetDataType(Tensor.GetDataType())
			.SetDataOwnedByDml(Tensor.HasPreparedData());
	}

	// Utility method to use FTensor to set all the members
	FTensorDescDml& SetFromTensor(const UE::NNE::Internal::FTensor& Tensor, int32 Rank)
	{
		return SetShape(Tensor.GetShape(), Rank)
			.SetDataType(Tensor.GetDataType())
			.SetDataOwnedByDml(Tensor.HasPreparedData());
	}

	// Utility method to use FTensor to set all the members
	FTensorDescDml& SetFromTensorBroadcast(const UE::NNE::Internal::FTensor& Tensor, const UE::NNE::FTensorShape& BroadcastShape)
	{
		return SetShape(Tensor.GetShape(), BroadcastShape)
			.SetDataType(Tensor.GetDataType())
			.SetDataOwnedByDml(Tensor.HasPreparedData());
	}

	// Utility method to use FTensor to set all the members
	FTensorDescDml& SetFromTensor1D(const UE::NNE::Internal::FTensor& Tensor, int32 Rank)
	{
		return SetShape1D(Tensor.GetShape().GetData()[0], Rank)
			.SetDataType(Tensor.GetDataType())
			.SetDataOwnedByDml(Tensor.HasPreparedData());
	}

	// Return computed sizes
	TConstArrayView<uint32> GetSizes() const
	{
		return Sizes;
	}

	// Return computed strides
	TConstArrayView<uint32> GetStrides() const
	{
		return Strides;
	}

	// Return computed size rank
	int32 GetRank() const
	{
		return Sizes.Num();
	}

	// Validate the tensor descriptor, once it's validated any calls to SetXXX() will be ignored
	bool Validate();

	// Return filled DML tensor descriptor
	// NOTE: Call this method only after Validate() is called
	const DML_TENSOR_DESC* GetDmlDesc() const
	{
		check(bIsValidated);
		return &Desc;
	}

	// Return DML tensor data type
	// NOTE: Call this method only after Validate() is called
	DML_TENSOR_DATA_TYPE GetDmlDataType() const
	{
		check(bIsValidated);
		return BuffDesc.DataType;
	}

private:

	uint64 CalculateBufferSize(uint64 ElemSizeInBytes);

	Util::FSmallUIntArray	Sizes;
	Util::FSmallUIntArray	Strides;
	DML_BUFFER_TENSOR_DESC	BuffDesc;
	DML_TENSOR_DESC			Desc;
	int32					MinTensorRank;
	int32					MaxTensorRank;
	bool					bIsValidated;
};

//
// DirectML operator base class
//
class FOperatorDml
{
public:

	virtual ~FOperatorDml() = default;

	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNE::Internal::FTensor> InputTensors, TArrayView<const NNE::Internal::FTensor> OutputTensors, const NNE::FAttributeMap& Attributes) = 0;

	virtual TConstArrayView<int32> GetConstantCPUInputs() const;

	virtual TConstArrayView<int32> GetRemappedInputs() const;

	IDMLOperator* GetOperator();

protected:
	
	bool CreateOperator(IDMLDevice* Device, const DML_OPERATOR_DESC& DmlOpDesc);

	TComPtr<IDMLOperator>		DmlOp;
	Util::FSmallIntArray		ConstantCPUInputs;
	Util::FSmallIntArray		RemappedInputs;
};

/**
 * DirectML ML operator registry
 */
using FOperatorRegistryDml = TOperatorRegistryRDG<FOperatorDml>;
using FModelValidatorDml = TModelValidatorRDG<FOperatorDml>;

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
