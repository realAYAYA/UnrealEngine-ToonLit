// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

#include "Algo/AnyOf.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
 *  Pooling operators (local and global)
 */
template <typename TDmlPoolOpDesc, DML_OPERATOR_TYPE DmlOpType, bool UseGlobalPooling, TCHAR const *OpName>
class FOperatorDmlPool : public FOperatorDml
{
	using FIntArray = TArray<int32>;

	class FPoolingArgs : public FKernelArgs
	{
		Util::FSmallUIntArray	KernelShape;

	public:

		bool Init(const NNE::FAttributeMap& Attributes, const int32 InputShapeRank, bool bInIsGlobalKernel)
		{
			if (!FKernelArgs::Init(Attributes, InputShapeRank, UseGlobalPooling, /*bIsTransposed*/ false))
			{
				return false;
			}

			if (!bIsGlobalKernel)
			{
				if (!Util::GetArrayAttributeNoOverflow(Attributes.GetAttributeValue(TEXT("kernel_shape")), KernelShape))
				{
					UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute cast led to overflow"));
					return false;
				}

				if (KernelShape.IsEmpty())
				{
					UE_LOG(LogNNE, Error, TEXT("kernel_shape attribute is required for pooling operators"));
					return false;
				}
				
				check(KernelShape.Num() == NumDimensions);

				for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
				{
					WindowSize.Add(uint32(KernelShape[KernelShape.Num() - NumDimensions + Dim]));
				}
			}

			return true;
		}

		bool Evaluate(TConstArrayView<uint32> InputShape)
		{
			if (!bIsGlobalKernel)
			{
				FKernelArgs::Evaluate(InputShape, KernelPadding(InputShape, WindowSize, Dilations, Strides));
			}
			else
			{
				FKernelArgs::Evaluate(InputShape);
			}

			return true;
		}

		TConstArrayView<uint32> GetWindowSize() const
		{
			return WindowSize;
		}
	};

	mutable FPoolingArgs	Args;
	int32					IncludePadding;
	uint32					P;
	static constexpr uint32 NumAllowedInputTensors = 1, MinAllowedOutputTensors = 1, MaxAllowedOutputTensors = 2;
	static constexpr int32 	MinTensorRank = 4, MaxTensorRank = 5;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlPool<TDmlPoolOpDesc, DmlOpType, UseGlobalPooling, OpName>();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{

		if(InputShapes.Num() != NumAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), OpName, InputShapes.Num(), NumAllowedInputTensors);
			return false;
		}

		TSet<ENNETensorDataType> AllowedDataTypes;
		if constexpr (DmlOpType == DML_OPERATOR_MAX_POOLING)
		{
			AllowedDataTypes = 
					{ 	
						ENNETensorDataType::Float, ENNETensorDataType::Half, 
						ENNETensorDataType::Int64, ENNETensorDataType::Int32, ENNETensorDataType::Int16,
						ENNETensorDataType::Int8, ENNETensorDataType::UInt64, ENNETensorDataType::UInt32, 
						ENNETensorDataType::UInt16, ENNETensorDataType::UInt8
					};
		}
		else
		{
			AllowedDataTypes = 
					{ 	
						ENNETensorDataType::Float, ENNETensorDataType::Half
					};
		}

		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			AllowedDataTypes,
			MinTensorRank, MaxTensorRank
			))
		{
			return false;
		}


		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		// TODO: int64 attributes
		check(Inputs.Num() == NumAllowedInputTensors);
		check(Outputs.Num() >= MinAllowedOutputTensors && Outputs.Num() <= MaxAllowedOutputTensors);

		// Attribute storage_order is not supported
		const int32 StorageOrder = Attributes.GetValueOrDefault<int32>(TEXT("storage_order"), 0);
		if (StorageOrder != 0)
		{
			UE_LOG(LogNNE, Error, TEXT("storage_order != 0 is not supported for DML inference"));
			return false;
		}

		const int32 InputShapeRank = Inputs[0].GetShape().Rank();
		check(InputShapeRank >= 2);

		if (!Args.Init(Attributes, InputShapeRank, UseGlobalPooling))
		{
			return false;
		}

		if constexpr (DmlOpType == DML_OPERATOR_AVERAGE_POOLING)
		{
			IncludePadding = Attributes.GetValueOrDefault(TEXT("count_include_pad"), 0);
		}

		if constexpr (DmlOpType == DML_OPERATOR_LP_POOLING)
		{
			P = (uint32) Attributes.GetValueOrDefault<int>(TEXT("p"), 2);
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

		Args.Evaluate(InputShape);

		// Compute output shape(s)
		{
			TConstArrayView<uint32> OutputShape = Args.GetOutputShape();

			for (auto& OutputTensor : OutputTensors)
			{
				OutputTensor->SetShape(NNE::FTensorShape::Make(OutputShape));
			}
		}

		// Check output shape
		for (int Idx = 0; Idx < OutputTensors.Num(); ++Idx)
		{
			check(OutputTensors[Idx]->GetShape().Rank() == InputShape.Num());
			check(OutputTensors[Idx]->GetShape().GetData()[0] == InputShape[0]);
			check(OutputTensors[Idx]->GetShape().GetData()[1] == InputShape[1]);

			TConstArrayView<uint32> StartPadding = Args.GetStartPadding();
			TConstArrayView<uint32> EndPadding = Args.GetEndPadding();
			TConstArrayView<uint32> Strides = Args.GetStrides();
			TConstArrayView<uint32> Dilations = Args.GetDilations();
			TConstArrayView<uint32> WindowSize = Args.GetWindowSize();
			TConstArrayView<uint32> OutputShape = OutputTensors[Idx]->GetShape().GetData();

			for (uint32 Dim = 0; Dim < Args.GetNumDimensions(); ++Dim)
			{
				const uint32 PaddedSize = InputShape[Dim + NonspatialDimensionCount] + StartPadding[Dim] + EndPadding[Dim];
				float(*const RoundingFunction)(float) = 
							Args.GetCeilMode() ?
								(float(*)(float)) &FMath::CeilToFloat
							:
								(float(*)(float)) &FMath::FloorToFloat
							;
				check(OutputShape[Dim + NonspatialDimensionCount] == (uint32) RoundingFunction((float)(PaddedSize - ((WindowSize[Dim] - 1) * Dilations[Dim] + 1 )) / (float) Strides[Dim] + 1.0f));
			}
		}

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		TConstArrayView<uint32> InputShape = InputTensors[0]->GetShape().GetData();

		FTensorDescDml DmlInputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(*InputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's input tensor for DML inference"));
			return false;
		}

		FTensorDescDml DmlOutputTensorDesc;

		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(*OutputTensors[0])
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize pooling operator's output tensor for DML inference"));
			return false;
		}

		auto FillPoolingDesc = [&](auto& PoolingDesc)
		{
			PoolingDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
			PoolingDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
			PoolingDesc.DimensionCount = Args.GetNumDimensions();
			PoolingDesc.WindowSize = Args.GetWindowSize().GetData();
			PoolingDesc.Strides = Args.GetStrides().GetData();
			PoolingDesc.StartPadding = Args.GetStartPadding().GetData();
			PoolingDesc.EndPadding = Args.GetEndPadding().GetData();
		};

		const bool bHasDilations =
			Algo::AnyOf(
				Args.GetDilations(),
				[](auto Dim) 
				{
					return Dim != 1u; 
				}
		);

		const bool bHasOutputIndices = OutputTensors.Num() > 1;

		TDmlPoolOpDesc DmlPoolOpDesc = {};
		
		if (DmlOpType == DML_OPERATOR_MAX_POOLING && (bHasOutputIndices || bHasDilations))
		{
			DML_MAX_POOLING2_OPERATOR_DESC DmlMaxPoolOpDesc = {};

			if (bHasOutputIndices)
			{
				FTensorDescDml DmlIndicesTensorDesc;
				
				if (!DmlIndicesTensorDesc
						.SetTensorRank(MinTensorRank, MaxTensorRank)
						.SetFromTensor(*OutputTensors[1])
						.SetDataType(ENNETensorDataType::UInt64)
						.Validate())
				{
					UE_LOG(LogNNE, Warning, TEXT("Failed to initialize MaxPool's indices output tensor for DML inference"));
					return false;
				}
				
				DmlMaxPoolOpDesc.OutputIndicesTensor = DmlIndicesTensorDesc.GetDmlDesc();
			}

			DmlMaxPoolOpDesc.Dilations = Args.GetDilations().GetData();
			FillPoolingDesc(DmlMaxPoolOpDesc);

			return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_MAX_POOLING2, &DmlMaxPoolOpDesc });
		}
		else
		{
			FillPoolingDesc(DmlPoolOpDesc);
		}

		if constexpr (DmlOpType == DML_OPERATOR_AVERAGE_POOLING)
		{
			DmlPoolOpDesc.IncludePadding = static_cast<BOOL>(IncludePadding);
		}

		if constexpr (DmlOpType == DML_OPERATOR_LP_POOLING)
		{
			DmlPoolOpDesc.P = P;
		}
			
		return CreateOperator(Device, DML_OPERATOR_DESC{ DmlOpType, &DmlPoolOpDesc });
	}
};

#define NNE_DML_REGISTER_POOLING_OP(OpName, DmlPrefix, UseGlobalPooling, Version) \
TCHAR const Op##OpName##Version##Name[] = TEXT(#OpName); \
struct FDmlOperator##OpName##Version##Registrator \
{ \
	FDmlOperator##OpName##Version##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd({{TEXT(#OpName), TEXT("Onnx")}, Version}, FOperatorDmlPool<DML_##DmlPrefix##_POOLING_OPERATOR_DESC, DML_OPERATOR_##DmlPrefix##_POOLING, UseGlobalPooling, Op##OpName##Version##Name>::Create, FOperatorDmlPool<DML_##DmlPrefix##_POOLING_OPERATOR_DESC, DML_OPERATOR_##DmlPrefix##_POOLING, UseGlobalPooling, Op##OpName##Version##Name>::Validate); \
	} \
}; \
\
static FDmlOperator##OpName##Version##Registrator RegisterDmlOperator##OpName##Version;

// Register pooling operator on Module startup
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false, 1)
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false, 8)
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false, 10)
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false, 11)
NNE_DML_REGISTER_POOLING_OP(MaxPool, MAX, false, 12)
NNE_DML_REGISTER_POOLING_OP(GlobalMaxPool, MAX, true, 1)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false, 1)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false, 7)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false, 10)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false, 11)
NNE_DML_REGISTER_POOLING_OP(AveragePool, AVERAGE, false, 19)
NNE_DML_REGISTER_POOLING_OP(GlobalAveragePool, AVERAGE, true, 1)
NNE_DML_REGISTER_POOLING_OP(LpPool, LP, false, 2)
NNE_DML_REGISTER_POOLING_OP(LpPool, LP, false, 11)
NNE_DML_REGISTER_POOLING_OP(GlobalLpPool, LP, true, 2)

#undef NNE_DML_REGISTER_POOLING_OP

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
