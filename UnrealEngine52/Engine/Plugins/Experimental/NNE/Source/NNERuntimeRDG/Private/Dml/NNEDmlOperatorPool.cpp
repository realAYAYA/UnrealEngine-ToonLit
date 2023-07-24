// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

#include "Algo/AnyOf.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
	* MaxPool
	*/
template <typename TDmlPoolOpDesc, DML_OPERATOR_TYPE DmlOpType, bool UseGlobalPooling>
class FOperatorDmlPool : public FOperatorDml
{
	using FIntArray = TArray<int32>;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlPool<TDmlPoolOpDesc, DmlOpType, UseGlobalPooling>();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		//TODO: int64 attributes
		check(InputTensors.Num() == 1);
		TConstArrayView<uint32> InputShape = InputTensors[0].GetShape().GetData();
		check(InputShape.Num() >= 2);
		check(OutputTensors.Num() == 1 || OutputTensors.Num() == 2);

		const uint32 NumSpatialDimensions = InputShape.Num() - NonspatialDimensionCount;
		if (NumSpatialDimensions != 2 && NumSpatialDimensions != 3)
		{
			UE_LOG(LogNNE, Error, TEXT("Number of spatial dimensions must be in range [2, 3] for DML inference."));
			return false;
		}

		DmlUtil::FSmallUIntArray StartPadding, EndPadding, KernelShape, Strides, Dilations;

		Strides.Init(1u, NumSpatialDimensions);
		if constexpr(!UseGlobalPooling)
		{
			if (!DmlUtil::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("strides")), Strides, TConstArrayView<uint32>(Strides)))
			{
				UE_LOG(LogNNE, Error, TEXT("Strides attribute cast led to overflow"));
				return false;
			}
		}
		check(Strides.Num() == NumSpatialDimensions);

		Dilations.Init(1u, NumSpatialDimensions);
		if constexpr(!UseGlobalPooling)
		{
			if (!DmlUtil::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("dilations")), Dilations, TConstArrayView<uint32>(Dilations)))
			{
				UE_LOG(LogNNE, Error, TEXT("Dilations attribute cast led to overflow"));
				return false;
			}
		}
		check(Dilations.Num() == NumSpatialDimensions);
		
		if constexpr(UseGlobalPooling)
		{
			KernelShape = DmlUtil::FSmallUIntArray{ InputShape.RightChop(NonspatialDimensionCount) };
		}
		else
		{
			if (!DmlUtil::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("kernel_shape")), KernelShape))
			{
				UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute cast led to overflow"));
				return false;
			}
		}
		
		if (KernelShape.IsEmpty())
		{
			UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute is required for pooling operators"));
			return false;
		}
		check(KernelShape.Num() == NumSpatialDimensions);

		if constexpr(UseGlobalPooling)
		{
			StartPadding.Init(0u, NumSpatialDimensions);
			EndPadding.Init(0u, NumSpatialDimensions);
		}
		else
		{
			ComputeStartEndPaddings(
				InputShape,
				Attributes,
				StartPadding,
				EndPadding,
				KernelPadding(InputShape, KernelShape, Dilations, Strides)
			);
		}
		
		for (int Idx = 0; Idx < OutputTensors.Num(); ++Idx)
		{
			check(OutputTensors[Idx].GetShape().Rank() == InputShape.Num());
			check(OutputTensors[Idx].GetShape().GetData()[0] == InputShape[0]);
			check(OutputTensors[Idx].GetShape().GetData()[1] == InputShape[1]);

			for (uint32 Dim = 0; Dim < NumSpatialDimensions; ++Dim) {
				uint32 PaddedSize = InputShape[Dim + NonspatialDimensionCount] + StartPadding[Dim] + EndPadding[Dim];
				check(OutputTensors[Idx].GetShape().GetData()[Dim + NonspatialDimensionCount] == (PaddedSize - KernelShape[Dim]) / Strides[Dim] + 1);
			}
		}

		//storage_order is not supported
		int32 StorageOrder =
			Attributes.GetValueOrDefault<int32>(
				TEXT("storage_order"),
				0);
		if(StorageOrder != 0)
		{
			UE_LOG(LogNNE, Error, TEXT("storage_order != 0 is not supported for DML inference"));
			return false;
		}


		DmlUtil::FTensorDesc DmlInputTensorDesc;
		if (!DmlInputTensorDesc.InitFromTensor(InputTensors[0], 4))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's input tensor for DML inference"));
			return false;
		}

		DmlUtil::FTensorDesc DmlOutputTensorDesc;
		if (!DmlOutputTensorDesc.InitFromTensor(OutputTensors[0], 4))
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's output tensor for DML inference"));
			return false;
		}

		auto FillPoolingDesc = [&](auto& PoolingDesc)
		{
			PoolingDesc.InputTensor = &DmlInputTensorDesc.Desc;
			PoolingDesc.OutputTensor = &DmlOutputTensorDesc.Desc;
			PoolingDesc.DimensionCount = NumSpatialDimensions;
			PoolingDesc.WindowSize = KernelShape.GetData();
			PoolingDesc.Strides = Strides.GetData();
			PoolingDesc.StartPadding = StartPadding.GetData();
			PoolingDesc.EndPadding = EndPadding.GetData();
		};

		const bool bHasDilations =
			Algo::AnyOf(
				Dilations,
				[](auto Dim) {return Dim != 1u; }
		);
		const bool bHasOutputIndices = OutputTensors.Num() > 1;

		TDmlPoolOpDesc DmlPoolOpDesc = {};
		FillPoolingDesc(DmlPoolOpDesc);

		if (DmlOpType == DML_OPERATOR_MAX_POOLING && (bHasOutputIndices || bHasDilations))
		{
			DML_MAX_POOLING2_OPERATOR_DESC DmlMaxPoolOpDesc = {};

			if (bHasOutputIndices)
			{
				DmlUtil::FTensorDesc DmlIndicesTensorDesc;
				if (!DmlIndicesTensorDesc.InitFromTensor(OutputTensors[1], 4))
				{
					UE_LOG(LogNNE, Warning, TEXT("Failed to initialize MaxPool's indices output tensor for DML inference"));
					return false;
				}
				DmlIndicesTensorDesc.BuffDesc.DataType = DML_TENSOR_DATA_TYPE::DML_TENSOR_DATA_TYPE_UINT64;
				DmlMaxPoolOpDesc.OutputIndicesTensor = &DmlIndicesTensorDesc.Desc;
			}

			DmlMaxPoolOpDesc.Dilations = Dilations.GetData();
			FillPoolingDesc(DmlMaxPoolOpDesc);
			return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MAX_POOLING2, &DmlMaxPoolOpDesc });
		}

		if constexpr (DmlOpType == DML_OPERATOR_AVERAGE_POOLING)
		{
			int32 IncludePadding = Attributes.GetValueOrDefault(TEXT("count_include_pad"), 0);
			DmlPoolOpDesc.IncludePadding = static_cast<BOOL>(IncludePadding);
		}

		if constexpr (DmlOpType == DML_OPERATOR_LP_POOLING)
		{
			DmlPoolOpDesc.P = (UINT) Attributes.GetValueOrDefault<int>(TEXT("p"), 2);
		}
			
		return CreateOperator(Device, DML_OPERATOR_DESC{ DmlOpType, &DmlPoolOpDesc });
	}
};

#define NNE_DML_REGISTER_POOLING_OP(OpName, DmlPrefix, UseGlobalPooling) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), FOperatorDmlPool<DML_##DmlPrefix##_POOLING_OPERATOR_DESC, DML_OPERATOR_##DmlPrefix##_POOLING, UseGlobalPooling>::Create); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;

// Register pooling operator on Module startup
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false)
NNE_DML_REGISTER_POOLING_OP(GlobalMaxPool, MAX, true)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false)
NNE_DML_REGISTER_POOLING_OP(GlobalAveragePool, AVERAGE, true)
NNE_DML_REGISTER_POOLING_OP(LpPool, LP, false)
NNE_DML_REGISTER_POOLING_OP(GlobalLpPool, LP, true)

#undef NNE_DML_REGISTER_POOLING_OP

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
