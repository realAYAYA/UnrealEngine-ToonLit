// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

// Remove array entries of the given indices (in ascending order), shifting them toward the front.
// There is a special check to avoid removing all the values, since returning a completely
// empty array would frequently causes errors later in many uses (such as with dimensions).
//
// e.g. input values = {2,1,3,1,1,5}
//      ellidable input indices = {1,3,4}
//      output values = {2,3,5}
template< typename TData, typename TAllocator >
void RemoveValuesByIndex(TConstArrayView<uint32> Indices, TArray<TData, TAllocator>& Values, bool bKeepOneValue)
{
	// Keep the last value at least, if all values would otherwise be removed.
	if (bKeepOneValue && !Indices.IsEmpty() && Indices.Num() == Values.Num()) 
	{
		Indices = Indices.RightChop(1);
	}

	for (int32 Idx = Indices.Num() - 1; Idx >= 0; --Idx)
	{
		Values.RemoveAt(Indices[Idx]);
	}
}

// Upsample and Resize operator are implemented as a DML Resample operator
template < bool IsResize>
class FOperatorDmlResample : public FOperatorDml
{
	static DML_INTERPOLATION_MODE ModeFromString(FStringView StringVal)
	{
		if (FCString::Stricmp(StringVal.GetData(), TEXT("NEAREST")) == 0)
		{
			return DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
		}
		else if (FCString::Stricmp(StringVal.GetData(), TEXT("LINEAR")) == 0)
		{
			return DML_INTERPOLATION_MODE_LINEAR;
		}
		else
		{
			UE_LOG(LogNNE, Warning, TEXT("Unsupported interpolation mode:%s, using nearest neighbor instead"), StringVal.GetData());
			return DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
		}
	}

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlResample<IsResize>();
	}

	//
	//
	//
	virtual bool Initialize(IDMLDevice* Device, TArrayView<const NNECore::Internal::FTensor> InputTensors, TArrayView<const NNECore::Internal::FTensor> OutputTensors, const NNECore::FAttributeMap& Attributes) override
	{
		check(InputTensors.Num() >= 1 && InputTensors.Num() < 4);
		check(OutputTensors.Num() == 1);

		const NNECore::Internal::FTensor& InputTensor = InputTensors[0];
		const NNECore::Internal::FTensor& ScaleTensor = (InputTensors.Num() == 2)?InputTensors[1]:InputTensors[2];//Upsample have scale at position 1, while Resize at 2.
		const NNECore::Internal::FTensor& OutputTensor = OutputTensors[0];

		if (InputTensors.Num() == 3 && InputTensors[2].GetDataType() == ENNETensorDataType::Int64)
		{
			UE_LOG(LogNNE, Warning, TEXT("Unsupported input type for 'sizes' of name %s, only 'scales' of type float is supported."), *InputTensors[2].GetName());
			return false;
		}

		if (!ScaleTensor.HasPreparedData())
		{
			UE_LOG(LogNNE, Warning, TEXT("scales should be a constant tensor, it is here a variable tensor of name %s."), *ScaleTensor.GetName());
			return false;
		}

		if (ScaleTensor.GetShape().Volume() != InputTensor.GetShape().Rank())
		{
			UE_LOG(LogNNE, Warning, TEXT("scales tensor should contain N entries, where N is rank of X."));
			return false;
		}
		
		// Read attributes
		DML_INTERPOLATION_MODE Mode = ModeFromString(Attributes.GetValue<FString>(TEXT("mode")));

		DmlUtil::FSmallArray<float> InputPixelOffsets, OutputPixelOffsets;
		InputPixelOffsets.Init(0.5f, InputTensor.GetShape().Rank());
		OutputPixelOffsets.Init(-0.5f, InputTensor.GetShape().Rank());

		DmlUtil::FSmallArray<float> Scales ( ScaleTensor.GetPreparedData<float>() );

		if constexpr (IsResize)
		{

			if (Mode == DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR)
			{
				FString NeareastMode = Attributes.GetValueOrDefault<FString>(TEXT("nearest_mode"), TEXT("round_prefer_floor"));
				if (FCString::Stricmp(*NeareastMode, TEXT("floor")) != 0)
				{
					UE_LOG(LogNNE, Warning, TEXT("Unsupported neareast mode:%s, using floor instead"), *NeareastMode);
				}
			}

			FString CoordinateTransformationMode = Attributes.GetValueOrDefault<FString>(TEXT("coordinate_transformation_mode"), TEXT("half_pixel"));
			for(int Idx = 0; Idx < InputTensor.GetShape().Rank(); ++Idx)
			{
				float LengthResized = (float) OutputTensor.GetShape().GetData()[Idx];
				float LengthOriginal = (float) InputTensor.GetShape().GetData()[Idx];
				
				if(CoordinateTransformationMode == TEXT("align_corners"))
				{
					Scales[Idx] = (LengthResized - 1.0f) / (LengthOriginal - 1.0f);
					InputPixelOffsets[Idx] = 0.f;
					OutputPixelOffsets[Idx] = 0.f;
				}
				else if(CoordinateTransformationMode == TEXT("asymmetric"))
				{
					InputPixelOffsets[Idx] = 0.f;
					OutputPixelOffsets[Idx] = 0.f;
				}
				else if(CoordinateTransformationMode == TEXT("tf_half_pixel_for_nn"))
				{
					InputPixelOffsets[Idx] = 0.0f;
					OutputPixelOffsets[Idx] = -0.5f;
				}
				else if(CoordinateTransformationMode == TEXT("tf_crop_and_resize"))
				{
					// NOTE: no tests for this, ORT erroneously puts all 0.0fs in the output tensor in this case.
					
					const NNECore::Internal::FTensor& RoiTensor = InputTensors[1];
					if (!RoiTensor.HasPreparedData())
					{
						UE_LOG(LogNNE, Warning, TEXT("roi should be a constant tensor, it is here a variable tensor of name %s."), *RoiTensor.GetName());
						return false;
					}
					if (RoiTensor.GetShape().Rank() != 1)
					{
						UE_LOG(LogNNE, Warning, TEXT("roi tensor should be 1-D."));
						return false;
					}
					if (RoiTensor.GetShape().GetData()[0] != 2 * InputTensor.GetShape().Rank())
					{
						UE_LOG(LogNNE, Warning, TEXT("roi tensor should contain 2*N entries, where N is rank of X."));
						return false;
					}
					float Start = RoiTensor.GetPreparedData<float>()[Idx];
					float End = RoiTensor.GetPreparedData<float>()[Idx + InputTensor.GetShape().Rank()];
					if(LengthResized > 1)
					{
						Scales[Idx] = (LengthResized - 1.0f) / FMath::Max( (End - Start) * (LengthOriginal - 1.0f), 1.0f );
						InputPixelOffsets[Idx] = Start * (1.0f - LengthOriginal);
						OutputPixelOffsets[Idx] = 0.0f;
					}
					else
					{
						UE_LOG(LogNNE, Warning, TEXT("Unsupported combination of transformation mode tf_crop_and_resize and length of resized dimension (%d) <= 1"), Idx);
						return false;
					}
					
				}
				else
				{
					if(CoordinateTransformationMode != TEXT("half_pixel"))
					{
						UE_LOG(LogNNE, Warning, TEXT("Unsupported coordinate transformation mode:%s, using half_pixel instead"), *CoordinateTransformationMode);
					}
				}
			}
			
		}

		// Initialize tensor descriptors
		DmlUtil::FTensorDesc	DmlInputTensor{};
		DmlUtil::FTensorDesc	DmlOutputTensor{};

		if (!DmlInputTensor.InitFromTensor(InputTensor, 1))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensor.InitFromTensor(OutputTensor, 1))
		{
			UE_LOG(LogNNE, Warning, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		// Notify DML graph that we have constant CPU tensor inputs
		for (int i = 1; i < InputTensors.Num(); ++i)
		{
			ConstantCPUInputs.Add(i);
		}

		
		
		// Find any useless dimensions of size 1 that occur in both input and output
		DmlUtil::FSmallUIntArray	SqueezeInds;
		TConstArrayView<uint32>		InputShape = InputTensor.GetShape().GetData();
		TConstArrayView<uint32>		OutputShape = OutputTensor.GetShape().GetData();

		for (int32 Idx = 0, Rank = OutputTensor.GetShape().Rank(); Idx < Rank; ++Idx)
		{
			if (InputShape[Idx] == 1 && OutputShape[Idx] == 1)
			{
				SqueezeInds.Emplace(Idx);
			}
		}

		if (!SqueezeInds.IsEmpty())
		{
			DmlUtil::FSmallUIntArray	SqueezedInputShape(InputShape.GetData(), InputShape.Num());
			DmlUtil::FSmallUIntArray	SqueezedOutputShape(OutputShape.GetData(), OutputShape.Num());
			DmlUtil::FSmallArray<float> ScaleValues(Scales.GetData(), Scales.Num());

			RemoveValuesByIndex(SqueezeInds, SqueezedInputShape, true);
			RemoveValuesByIndex(SqueezeInds, SqueezedOutputShape, true);
			RemoveValuesByIndex(SqueezeInds, ScaleValues, true);
			RemoveValuesByIndex(SqueezeInds, InputPixelOffsets, true);
			RemoveValuesByIndex(SqueezeInds, OutputPixelOffsets, true);

			DmlInputTensor.UpdateShapeAndStrides(SqueezedInputShape);
			DmlOutputTensor.UpdateShapeAndStrides(SqueezedOutputShape);
			Scales = ScaleValues;
		}

		DML_RESAMPLE1_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = &DmlInputTensor.Desc;
		OpDesc.OutputTensor = &DmlOutputTensor.Desc;
		OpDesc.InterpolationMode = Mode;
		OpDesc.DimensionCount = Scales.Num();
		OpDesc.Scales = Scales.GetData();
		OpDesc.InputPixelOffsets = InputPixelOffsets.GetData();
		OpDesc.OutputPixelOffsets = OutputPixelOffsets.GetData();

		return CreateOperator(Device, DML_OPERATOR_DESC { DML_OPERATOR_RESAMPLE1, &OpDesc });
	}
};

#define NNE_DML_REGISTER_RESAMPLE_OP(OpName, IsResize) \
struct FDmlOperator##OpName##Registrator \
{ \
	FDmlOperator##OpName##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd(TEXT(#OpName), FOperatorDmlResample<IsResize>::Create); \
	} \
}; \
\
static FDmlOperator##OpName##Registrator RegisterDmlOperator##OpName;

// Register operator on Module startup
NNE_DML_REGISTER_RESAMPLE_OP(Upsample, false)
NNE_DML_REGISTER_RESAMPLE_OP(Resize, true)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
