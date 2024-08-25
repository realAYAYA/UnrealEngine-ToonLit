// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML
#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"
#include "Algo/Count.h"

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

template<typename TData>
inline TArray<TData, TInlineAllocator<GMaxTensorRank>> MakeFillArray(TData Value, int32 Count)
{
	TArray<TData, TInlineAllocator<GMaxTensorRank>>	Values;

	Values.Init(Value, Count);
	return Values;
}

// Upsample and Resize operator are implemented as a DML Resample operator
template <bool IsResize>
class FOperatorDmlResample : public FOperatorDml
{
	enum ECoordTransformMode : uint8
	{
		None,
		AlignCorners,
		Asymmetric,
		TfHalfPixelForNN,
		TfCropAndResize,
		HalfPixel
	};

	enum ENearestNeighborRoundingMode : uint8
	{
		RoundPreferFloor,	// round halves down
		RoundPreferCeil,	// round halves up
		Floor,				// round always down
		Ceil				// round always up
	};

	DML_INTERPOLATION_MODE			Mode;
	ECoordTransformMode				CoordTransformMode;
	ENearestNeighborRoundingMode	NearestMode;
	bool 							bUseSizesTensor;

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

	static constexpr uint32 ResizeMinAllowedInputTensors = 1, ResizeMaxAllowedInputTensors = 4;
	static constexpr uint32 UpsampleNumAllowedInputTensors = 2;
	static constexpr uint32 NumAllowedOutputTensors = 1;
	static constexpr int32 	MinTensorRank = 0, MaxTensorRank = 4;

public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlResample<IsResize>();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		const FString OpName = IsResize ? TEXT("Resize") : TEXT("Upsample");
		if constexpr (IsResize)
		{
			if (InputShapes.Num() < ResizeMinAllowedInputTensors || InputShapes.Num() > ResizeMaxAllowedInputTensors)
			{
				UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be in [%d, %d]."), 
											*OpName, InputShapes.Num(), ResizeMinAllowedInputTensors, ResizeMaxAllowedInputTensors);
				return false;
			}

			if(InputShapes.Num() > 1)
			{
				TSet<ENNETensorDataType> AllowedDataTypes = { ENNETensorDataType::Double, ENNETensorDataType::Float, ENNETensorDataType::Half };

				// Add support for a scalar tensor which doesn't have a type
				if (InputShapes[1].Rank() == 0 && InputTypes[1] == ENNETensorDataType::None)
				{
					AllowedDataTypes.Add(ENNETensorDataType::None);
				}

				if (!CheckGenericTensor1D(OpName, InputTypes[1], InputShapes[1], AllowedDataTypes))
				{
					return false;
				}
			}

			if(InputShapes.Num() == 3)
			{
				if (!CheckGenericTensor1D(OpName, InputTypes[2], InputShapes[2], 
					{ 	ENNETensorDataType::Float
					}
					))
				{
					return false;
				}
			}

			if(InputShapes.Num() == 4)
			{
				if (!CheckGenericTensor1D(OpName, InputTypes[3], InputShapes[3], 
					{ 	ENNETensorDataType::Int64
					}
					))
				{
					return false;
				}
			}
		}
		else
		{
			if(InputShapes.Num() != UpsampleNumAllowedInputTensors)
			{
				UE_LOG(LogNNE, Warning, TEXT("DML %s: Invalid number of input tensors. %d provided, it should be %d."), *OpName, InputShapes.Num(), UpsampleNumAllowedInputTensors);
				return false;
			}

			if(InputShapes.Num() > 1) //-V547
			{
				if (!CheckGenericTensor1D(OpName, InputTypes[1], InputShapes[1], 
					{ 	ENNETensorDataType::Float
					}
					))
				{
					return false;
				}
			}
		}
		
		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], 
			{ 	
				ENNETensorDataType::Float, ENNETensorDataType::Half
			},
			MinTensorRank, MaxTensorRank + Algo::Count(InputShapes[0].GetData(), (int32) 1) // Allow ones due to squeezing (see Create method)
		  	))
		{
			return false;
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		if constexpr (IsResize)
		{
			check(Inputs.Num() >= ResizeMinAllowedInputTensors && Inputs.Num() <= ResizeMaxAllowedInputTensors);
		}
		else
		{
			check(Inputs.Num() == UpsampleNumAllowedInputTensors);
		}
		check(Outputs.Num() == NumAllowedOutputTensors);

		bUseSizesTensor = Inputs.Num() == 4;

		if (Inputs.Num() == 3 && Inputs[2].GetDataType() != ENNETensorDataType::Float)
		{
			UE_LOG(LogNNE, Warning, TEXT("Unsupported input type for 'scales' of name %s, only 'scales' of type float is supported."), *Inputs[2].GetName());
			return false;
		}
		
		if (bUseSizesTensor && Inputs[3].GetDataType() != ENNETensorDataType::Int64)
		{
			UE_LOG(LogNNE, Warning, TEXT("Unsupported input type for 'sizes' of name %s, only 'sizes' of type int64 is supported."), *Inputs[3].GetName());
			return false;
		}

		if(bUseSizesTensor && Inputs[2].GetName() != TEXT(""))
		{
			UE_LOG(LogNNE, Warning, TEXT("'scales' should be an empty-string tensor when 'sizes' is specified."));
			return false;
		}

		// Read attributes
		Mode = ModeFromString(Attributes.GetValueOrDefault<FString>(TEXT("mode"), TEXT("nearest")));

		if constexpr (IsResize)
		{
			if (Mode == DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR)
			{
				FString NearestModeStr = Attributes.GetValueOrDefault<FString>(TEXT("nearest_mode"), TEXT("round_prefer_floor"));
				
				if (NearestModeStr == TEXT("round_prefer_floor"))
				{
					NearestMode = ENearestNeighborRoundingMode::RoundPreferFloor;
				}
				else if (NearestModeStr == TEXT("round_prefer_ceil"))
				{
					NearestMode = ENearestNeighborRoundingMode::RoundPreferCeil;
				}
				else if (NearestModeStr == TEXT("floor"))
				{
					NearestMode = ENearestNeighborRoundingMode::Floor;
				}
				else if (NearestModeStr == TEXT("ceil"))
				{
					NearestMode = ENearestNeighborRoundingMode::Ceil;
				}
				else
				{
					NearestMode = ENearestNeighborRoundingMode::Floor;
					UE_LOG(LogNNE, Warning, TEXT("Unsupported neareast mode:%s, using floor instead"), *NearestModeStr);
				}
			}

			FString CoordinateTransformationMode = Attributes.GetValueOrDefault<FString>(TEXT("coordinate_transformation_mode"), TEXT("half_pixel"));
			if (CoordinateTransformationMode == TEXT("align_corners"))
			{
				CoordTransformMode = ECoordTransformMode::AlignCorners;
			}
			else if (CoordinateTransformationMode == TEXT("asymmetric"))
			{
				CoordTransformMode = ECoordTransformMode::Asymmetric;
			}
			else if (CoordinateTransformationMode == TEXT("tf_half_pixel_for_nn"))
			{
				CoordTransformMode = ECoordTransformMode::TfHalfPixelForNN;
			}
			else if (CoordinateTransformationMode == TEXT("tf_crop_and_resize"))
			{
				CoordTransformMode = ECoordTransformMode::TfCropAndResize;
			}
			else
			{
				CoordTransformMode = ECoordTransformMode::HalfPixel;

				if (CoordinateTransformationMode != TEXT("half_pixel"))
				{
					UE_LOG(LogNNE, Warning, TEXT("Unsupported coordinate transformation mode:%s, using half_pixel instead"), *CoordinateTransformationMode);
				}
			}
		}

		for (int i = 1; i < Inputs.Num(); ++i)
		{
			ConstantCPUInputs.Add(i);
		}

		return true;
	}
	
	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& ScaleTensor = (InputTensors.Num() == 2) ? *InputTensors[1] : *InputTensors[2]; // Upsample has scale at position 1, while Resize at 2
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		if (!bUseSizesTensor && !ScaleTensor.HasPreparedData())
		{
			UE_LOG(LogNNE, Error, TEXT("scales should be a constant tensor, it is here a variable tensor of name %s."), *ScaleTensor.GetName());
			return -1;
		}

		if (!bUseSizesTensor && ScaleTensor.GetShape().Volume() != InputTensor.GetShape().Rank())
		{
			UE_LOG(LogNNE, Error, TEXT("scales tensor should contain N entries, where N is rank of X."));
			return -1;
		}

		if constexpr (IsResize)
		{
			if (CoordTransformMode == ECoordTransformMode::TfCropAndResize)
			{
				const NNE::Internal::FTensor& RoiTensor = *InputTensors[1];
				
				if (!RoiTensor.IsConstant())
				{
					UE_LOG(LogNNE, Warning, TEXT("roi should be a constant tensor, it is here a variable tensor of name %s."), *RoiTensor.GetName());
					return -1;
				}

				if (RoiTensor.GetShape().Rank() != 1)
				{
					UE_LOG(LogNNE, Warning, TEXT("roi tensor should be 1-D."));
					return -1;
				}

				if (RoiTensor.GetShape().GetData()[0] != 2 * InputTensor.GetShape().Rank())
				{
					UE_LOG(LogNNE, Warning, TEXT("roi tensor should contain 2*N entries, where N is rank of X."));
					return -1;
				}

				for (int Idx = 0; Idx < InputTensor.GetShape().Rank(); ++Idx)
				{
					float LengthResized = (float)OutputTensor.GetShape().GetData()[Idx];

					if (LengthResized <= 1)
					{
						UE_LOG(LogNNE, Warning, TEXT("Unsupported combination of transformation mode tf_crop_and_resize and length of resized dimension (%d) <= 1"), Idx);
						return -1;
					}
				}
			}

			if(bUseSizesTensor)
			{
				const NNE::Internal::FTensor& SizesTensor = *InputTensors[2];
				Util::FSmallArray<int64> Sizes(SizesTensor.GetPreparedData<int64>());
				Util::FSmallArray<uint32> SizesUint32;
				SizesUint32.SetNumUninitialized(Sizes.Num());
				for(int Idx = 0; Idx < Sizes.Num(); ++Idx)
				{
					SizesUint32[Idx] = (uint32) Sizes[Idx];
					if(Sizes[Idx] != (int64) SizesUint32[Idx])
					{
						UE_LOG(LogNNE, Warning, TEXT("Overflow in conversion of 'sizes'"));
						return -1;
					}
				}
				OutputTensors[0]->SetShape(NNE::FTensorShape::Make(TConstArrayView<uint32>(SizesUint32)));
			
			}
		}

		if(!bUseSizesTensor)
		{
			TConstArrayView<float>	ScalesData = ScaleTensor.GetPreparedData<float>();
			TConstArrayView<uint32>	InputShape = InputTensor.GetShape().GetData();
			TArray<uint32>			OutputShape;

			for (int32 i = 0; i < InputShape.Num(); ++i)
			{
				OutputShape.Emplace(FMath::FloorToInt32(InputShape[i] * ScalesData[i]));
			}

			OutputTensors[0]->SetShape(NNE::FTensorShape::Make(OutputShape));
		}
		

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& ScaleTensor = (InputTensors.Num() == 2) ? *InputTensors[1] : *InputTensors[2]; // Upsample has scale at position 1, while Resize at 2
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		Util::FSmallArray<float> InputPixelOffsets, OutputPixelOffsets;

		InputPixelOffsets.Init(0.5f, InputTensor.GetShape().Rank());
		OutputPixelOffsets.Init(-0.5f, InputTensor.GetShape().Rank());

		Util::FSmallArray<float> Scales;
		if(!bUseSizesTensor)
		{
			Scales = ScaleTensor.GetPreparedData<float>();
		}

		if constexpr (IsResize)
		{
			if(bUseSizesTensor)
			{
				const NNE::Internal::FTensor& SizesTensor = *InputTensors[2];
				Util::FSmallArray<int64> Sizes(SizesTensor.GetPreparedData<int64>());

				Scales.SetNum(Sizes.Num());
				for(int Idx = 0; Idx < Sizes.Num(); ++Idx)
				{
					Scales[Idx] = (float) Sizes[Idx] / (float) InputTensor.GetShape().GetData()[Idx];
				}
			}
			for (int Idx = 0; Idx < InputTensor.GetShape().Rank(); ++Idx)
			{
				float LengthResized = (float) OutputTensor.GetShape().GetData()[Idx];
				float LengthOriginal = (float) InputTensor.GetShape().GetData()[Idx];

				if (CoordTransformMode == ECoordTransformMode::AlignCorners)
				{
					Scales[Idx] = (LengthResized - 1.0f) / (LengthOriginal - 1.0f);
					InputPixelOffsets[Idx] = 0.f;
					OutputPixelOffsets[Idx] = 0.f;
				}
				else if (CoordTransformMode == ECoordTransformMode::Asymmetric)
				{
					InputPixelOffsets[Idx] = 0.f;
					OutputPixelOffsets[Idx] = 0.f;
				}
				else if (CoordTransformMode == ECoordTransformMode::TfHalfPixelForNN)
				{
					InputPixelOffsets[Idx] = 0.0f;
					OutputPixelOffsets[Idx] = -0.5f;
				}
				else if (CoordTransformMode == ECoordTransformMode::TfCropAndResize)
				{
					// NOTE: no tests for this, ORT erroneously puts all 0.0fs in the output tensor in this case.
					const NNE::Internal::FTensor& RoiTensor = *InputTensors[1];
					
					float Start = RoiTensor.GetPreparedData<float>()[Idx];
					float End = RoiTensor.GetPreparedData<float>()[Idx + InputTensor.GetShape().Rank()];

					if (LengthResized > 1)
					{
						Scales[Idx] = (LengthResized - 1.0f) / FMath::Max((End - Start) * (LengthOriginal - 1.0f), 1.0f);
						InputPixelOffsets[Idx] = Start * (1.0f - LengthOriginal);
						OutputPixelOffsets[Idx] = 0.0f;
					}
				}
			}
		}

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		DmlInputTensorDesc
			.SetTensorRank(MinTensorRank, MaxTensorRank)
			.SetFromTensor(InputTensor);
		
		DmlOutputTensorDesc
			.SetTensorRank(MinTensorRank, MaxTensorRank)
			.SetFromTensor(OutputTensor);
		
		// Find any useless dimensions of size 1 that occur in both input and output
		Util::FSmallUIntArray		SqueezeInds;
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
			Util::FSmallUIntArray		SqueezedInputShape(InputShape.GetData(), InputShape.Num());
			Util::FSmallUIntArray		SqueezedOutputShape(OutputShape.GetData(), OutputShape.Num());
			Util::FSmallArray<float>	ScaleValues(Scales.GetData(), Scales.Num());

			RemoveValuesByIndex(SqueezeInds, SqueezedInputShape, true);
			RemoveValuesByIndex(SqueezeInds, SqueezedOutputShape, true);
			RemoveValuesByIndex(SqueezeInds, ScaleValues, true);
			RemoveValuesByIndex(SqueezeInds, InputPixelOffsets, true);
			RemoveValuesByIndex(SqueezeInds, OutputPixelOffsets, true);

			if constexpr (IsResize)
			{
				const int32 SqueezedDimCount = SqueezedOutputShape.Num();
				const int32 DmlDimCount = OutputTensor.GetShape().Rank();

				if (DmlDimCount > SqueezedDimCount)
				{
					ScaleValues.Insert(MakeFillArray<float>(1.0f, DmlDimCount - SqueezedDimCount), 0);
					InputPixelOffsets.Insert(MakeFillArray<float>(0.5f, DmlDimCount - SqueezedDimCount), 0);
					OutputPixelOffsets.Insert(MakeFillArray<float>(-0.5f, DmlDimCount - SqueezedDimCount), 0);
				}
				else
				{
					DmlInputTensorDesc.SetShape(SqueezedInputShape);
					DmlOutputTensorDesc.SetShape(SqueezedOutputShape);
				}
			}
			else
			{
				DmlInputTensorDesc.SetShape(SqueezedInputShape);
				DmlOutputTensorDesc.SetShape(SqueezedOutputShape);
			}

			Scales = ScaleValues;
		}

		if (!DmlInputTensorDesc.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlOutputTensorDesc.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_AXIS_DIRECTION RoundingDirection = DML_AXIS_DIRECTION_DECREASING;
		if (Mode == DML_INTERPOLATION_MODE_NEAREST_NEIGHBOR)
		{
			float OffsetAdjustment = 0.5f;

			switch (NearestMode)
			{
				case ENearestNeighborRoundingMode::RoundPreferFloor:
					RoundingDirection = DML_AXIS_DIRECTION_INCREASING;
					OffsetAdjustment = 0.5f;
					break;

				case ENearestNeighborRoundingMode::RoundPreferCeil:
					RoundingDirection = DML_AXIS_DIRECTION_DECREASING;
					OffsetAdjustment = -0.5f;
					break;

				case ENearestNeighborRoundingMode::Floor:
					RoundingDirection = DML_AXIS_DIRECTION_DECREASING;
					OffsetAdjustment = 0.0f;
					break;

				case ENearestNeighborRoundingMode::Ceil:
					RoundingDirection = DML_AXIS_DIRECTION_INCREASING;
					OffsetAdjustment = 0.0f;
					break;

				default:
					UE_LOG(LogNNE, Warning, TEXT("Nearest neighbor rounding mode should have been initialized"));
			}

			if (OffsetAdjustment != 0.0f)
			{
				for (float& Offset : InputPixelOffsets)
				{
					Offset += OffsetAdjustment;
				}
			}
		}

		DML_RESAMPLE2_OPERATOR_DESC	OpDesc{};

		OpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		OpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		OpDesc.InterpolationMode = Mode;
		OpDesc.RoundingDirection = RoundingDirection;
		OpDesc.DimensionCount = Scales.Num();
		OpDesc.Scales = Scales.GetData();
		OpDesc.InputPixelOffsets = InputPixelOffsets.GetData();
		OpDesc.OutputPixelOffsets = OutputPixelOffsets.GetData();

		return CreateOperator(Device, DML_OPERATOR_DESC{ DML_OPERATOR_RESAMPLE2, &OpDesc });
	}
};

#define NNE_DML_REGISTER_RESAMPLE_OP(OpName, IsResize, Version) \
struct FDmlOperator##OpName##Version##Registrator \
{ \
	FDmlOperator##OpName##Version##Registrator() \
	{ \
		FOperatorRegistryDml::Get()->OpAdd({{TEXT(#OpName), TEXT("Onnx")}, Version}, FOperatorDmlResample<IsResize>::Create, FOperatorDmlResample<IsResize>::Validate); \
	} \
}; \
\
static FDmlOperator##OpName##Version##Registrator RegisterDmlOperator##OpName##Version;

// Register operator on Module startup
NNE_DML_REGISTER_RESAMPLE_OP(Upsample, false, 9)
NNE_DML_REGISTER_RESAMPLE_OP(Resize, true, 10)
NNE_DML_REGISTER_RESAMPLE_OP(Resize, true, 11)
NNE_DML_REGISTER_RESAMPLE_OP(Resize, true, 13)

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
