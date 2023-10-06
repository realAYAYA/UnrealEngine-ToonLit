// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGSlice.h"
#include "NNERuntimeRDGHelperSlice.h"
#include "NNETensor.h"
#include "NNETypes.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	/**
	 * Slice operator implementation
	 */
	class FSlice : public FOperatorHlsl
	{
	public:

		FSlice() {}
		virtual ~FSlice() = default;

		TArray<int32> AxesAttr;
		TArray<int32> EndsAttr;
		TArray<int32> StartsAttr;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) const override
		{
			check(InputTensors.Num() == 1);
			check(OutputTensors.Num() == 1);
			
			TConstArrayView<uint32> Dims(InputTensors[0]->GetShape().GetData());
			int32 InputRank = Dims.Num();

			TArray<int32> Axes = AxesAttr;
			TArray<int32> Ends = EndsAttr;
			TArray<int32> Starts = StartsAttr;
			TArray<int32> Start;
			TArray<int32> End;

			//see https://github.com/onnx/onnx/blob/main/docs/Operators.md#slice for algorithm
			Start.SetNum(InputRank);
			End.SetNum(InputRank);
			for (int32 i = 0; i < InputRank; ++i)
			{
				Start[i] = 0;
				End[i] = Dims[i];
			}
			for (int32 i = 0; i < Axes.Num(); ++i)
			{
				if (Axes[i] < 0)
				{
					Axes[i] += InputRank;
				}
			}
			for (int32 i = 0; i < Starts.Num(); ++i)
			{
				if (Starts[i] < 0)
				{
					Starts[i] += Dims[Axes[i]];
				}
			}
			for (int32 i = 0; i < Ends.Num(); ++i)
			{
				if (Ends[i] < 0)
				{
					Ends[i] += Dims[Axes[i]];
				}
			}
			for (int32 i = 0; i < Axes.Num(); ++i)
			{
				Start[Axes[i]] = FMath::Clamp(Starts[i], 0, Dims[Axes[i]]);
				End[Axes[i]] = FMath::Clamp(Ends[i], 0, Dims[Axes[i]]);
			}

			TArray<uint32> OutputShapeData;
			
			OutputShapeData.Reserve(InputRank);
			for (int32 i = 0; i < InputRank; ++i)
			{
				OutputShapeData.Add(End[i] - Start[i]);
			}
			
			NNE::FTensorShape OutputShape = NNE::FTensorShape::Make(OutputShapeData);

			OutputTensors[0]->SetShape(OutputShape);
			
			Internal::CPUHelper::Slice::Apply(*InputTensors[0], *OutputTensors[0], Start);

			if (!OutputTensors[0]->HasPreparedData())
			{
				UE_LOG(LogNNE, Warning, TEXT("Slice: Output could not be computed as a constant tensor, however Slice is not implemented on GPU at the moment."));
				return -1;
			}

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
			check(InputTensorDescs.Num() == 1);
			check(OutputTensorDescs.Num() == 1);

			EndsAttr = Attributes.GetValue<TArray<int32>>(TEXT("ends"));
			StartsAttr = Attributes.GetValue<TArray<int32>>(TEXT("starts"));
			
			TArray<int32> AxesDefault;
			
			for (int i = 0; i < StartsAttr.Num(); ++i)
			{
				AxesDefault.Add(i);
			}
			
			AxesAttr = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("axes"), AxesDefault);

			if (EndsAttr.Num() != StartsAttr.Num() || AxesAttr.Num() != StartsAttr.Num())
			{
				UE_LOG(LogNNE, Warning, TEXT("Unsqueeze: Starts, Ends and Axes must be of the same size."));
				return false;
			}
			
			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			UE_LOG(LogNNE, Warning, TEXT("Slice: Output should be constant and already uploaded to GPU memory. Dispatch should not need to be called."));
		}
	};

	bool ValidateSliceOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputSlices)
	{
		bool bIsValid = true;

		//This match version 1 of the Slice operator, next version are 10, 11 and 13
		//https://github.com/onnx/onnx/blob/main/docs/Changelog.md#Slice-1
		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("axes"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddRequired(TEXT("ends"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddRequired(TEXT("starts"), ENNEAttributeDataType::Int32Array);
		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Int64);
		InputValidator.AddSupportedType(ENNETensorDataType::Int32);
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();

		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	FOperatorHlsl* CreateSliceOperator()
	{
		return new FSlice();
	}

	bool RegisterSliceOperator(FOperatorRegistryHlsl& Registry)
	{
		Registry.OpAdd(TEXT("Slice"), CreateSliceOperator, ValidateSliceOperator);
		return true;
	}
} // UE::NNERuntimeRDG::Private::Hlsl
