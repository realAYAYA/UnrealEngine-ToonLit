// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef NNE_USE_DIRECTML

#include "NNEDmlOperator.h"
#include "NNEDmlOperatorUtils.h"

namespace UE::NNERuntimeRDG::Private::Dml
{

/**
* Implements both Conv and ConvTranspose operators 
*/
template <DML_CONVOLUTION_DIRECTION Direction>
class FOperatorDmlConv : public FOperatorDml
{
	class FConvArgs : public FKernelArgs
	{
		int32			Group;

	public:

		FConvArgs() = default;

		bool Init(const NNE::FTensorDesc& Input, const NNE::FTensorDesc& Filter, const NNE::FAttributeMap& Attributes)
		{
			if (!FKernelArgs::Init(Attributes, Input.GetShape().Rank(), /*bIsGlobalKernel*/ false, /*bIsTransposed*/ Direction != DML_CONVOLUTION_DIRECTION_FORWARD))
			{
				return false;
			}

			check(Filter.GetShape().Rank() == Input.GetShape().Rank());
			check(Strides.Num() == 0 || Strides.Num() == Filter.GetShape().Rank() - NonspatialDimensionCount);
			check(Dilations.Num() == 0 || Dilations.Num() == Filter.GetShape().Rank() - NonspatialDimensionCount);

			Group = Attributes.GetValueOrDefault<int32>(TEXT("group"), 1);

			if (Direction != DML_CONVOLUTION_DIRECTION_FORWARD)
			{
				const FNNEAttributeValue* AttrOutShape = Attributes.GetAttributeValue(TEXT("output_shape"));

				if (AttrOutShape)
				{
					TArray<int32> OutShapeVal = AttrOutShape->GetValue<TArray<int32>>();

					for (int32 Value : OutShapeVal)
					{
						OutputShape.Add(uint32(Value));
					}
				}
			}

			return true;
		}

		void Evaluate(TConstArrayView<uint32> InputShape, TConstArrayView<uint32> FilterShape)
		{
			// NOTE: To compute paddings we need WindowSize
			for (int32 Dim = FilterShape.Num() - NumDimensions; Dim < FilterShape.Num(); ++Dim)
			{
				WindowSize.Add(FilterShape[Dim]);
			}

			FKernelArgs::Evaluate(InputShape, ConvolutionPadding(InputShape));

			if (Direction == DML_CONVOLUTION_DIRECTION_FORWARD)
			{
				OutputShape[1] = FilterShape[0];
			}
			else
			{
				if (!bHasOutputShape)
				{
					OutputShape[1] = FilterShape[1] * Group;
				}
			}
		}

		TConstArrayView<uint32> GetOutPadding() const
		{
			check(!OutPadding.IsEmpty());
			return MakeArrayView((const uint32*) OutPadding.GetData(), OutPadding.Num());
		}

		int32 GetGroup() const
		{
			return Group;
		}

	private:

		Util::FSmallUIntArray ConvolutionPadding(TConstArrayView<uint32> InputShape)
		{
			const uint32 DimOffset = NonspatialDimensionCount;

			if (Direction == DML_CONVOLUTION_DIRECTION_FORWARD)
			{
				return KernelPadding(
					InputShape, WindowSize,
					MakeArrayView((uint32*) Dilations.GetData(), Dilations.Num()), MakeArrayView((uint32*) Strides.GetData(), Strides.Num())
				);
			}
			else
			{
				Util::FSmallUIntArray Padding;
				Padding.SetNumUninitialized(NumDimensions);
				
				for (uint32 Dim = 0; Dim < NumDimensions; ++Dim)
				{
					Padding[Dim] = (InputShape[Dim + DimOffset] - 1) * Dilations[Dim] - Strides[Dim] + OutPadding[Dim] + 1;
				}

				return Padding;
			}
		}
	};

	mutable FConvArgs	Args;

	static constexpr int32 MinTensorRank = 3, MaxTensorRank = 5;
	static constexpr uint32 MinAllowedInputTensors = 2, MaxAllowedInputTensors = 3;
public:

	static FOperatorDml* Create()
	{
		return new FOperatorDmlConv();
	}

	static bool Validate(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		FString OpName = Direction == DML_CONVOLUTION_DIRECTION_FORWARD ? TEXT("Conv") : TEXT("ConvTranspose");

		if (InputShapes.Num() < MinAllowedInputTensors || InputShapes.Num() > MaxAllowedInputTensors)
		{
			UE_LOG(LogNNE, Warning, TEXT("DML %s: invalid number of input tensors. %d provided, it should be in [%d, %d]."), 
										*OpName, InputShapes.Num(), MinAllowedInputTensors, MaxAllowedInputTensors);
			return false;
		}

		if (!CheckGenericTensor(OpName, InputTypes[0], InputShapes[0], {ENNETensorDataType::Float, ENNETensorDataType::Half}, MinTensorRank, MaxTensorRank) || 
			!CheckGenericTensor(OpName, InputTypes[1], InputShapes[1], {ENNETensorDataType::Float, ENNETensorDataType::Half}, MinTensorRank, MaxTensorRank))
		{
			return false;
		}

		if(InputShapes.Num() == 3)
		{
			// Bias tensor must be 1D
			if (!CheckGenericTensor1D(OpName, InputTypes[2], InputShapes[2], {ENNETensorDataType::Float, ENNETensorDataType::Half}))
			{
				return false;
			}
		}

		return true;
	}

	virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> Inputs, TConstArrayView<NNE::FTensorDesc> Outputs, const NNE::FAttributeMap& Attributes) override
	{
		check(Inputs.Num() >= MinAllowedInputTensors && Inputs.Num() <= MaxAllowedInputTensors);

		const NNE::FTensorDesc& InputTensor = Inputs[0];
		const NNE::FTensorDesc& FilterTensor = Inputs[1];

		if (!Args.Init(InputTensor, FilterTensor, Attributes))
		{
			return false;
		}

		return true;
	}

	virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		Args.Evaluate(InputTensors[0]->GetShape().GetData(), InputTensors[1]->GetShape().GetData());
		OutputTensor.SetShape(NNE::FTensorShape::Make(Args.GetOutputShape()));

		return 0;
	}

	virtual bool Create(IDMLDevice* Device, TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TConstArrayView<NNE::Internal::FTensorRef> OutputTensors) override
	{
		const NNE::Internal::FTensor& InputTensor = *InputTensors[0];
		const NNE::Internal::FTensor& FilterTensor = *InputTensors[1];
		const NNE::Internal::FTensor& OutputTensor = *OutputTensors[0];

		FTensorDescDml	DmlInputTensorDesc;
		FTensorDescDml	DmlFilterTensorDesc;
		FTensorDescDml	DmlBiasTensorDesc;
		FTensorDescDml	DmlOutputTensorDesc;

		if (!DmlInputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(InputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (!DmlFilterTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(FilterTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		if (InputTensors.Num() > 2)
		{
			const NNE::Internal::FTensor& BiasTensor = *InputTensors[2];

			if (!DmlBiasTensorDesc
					.SetTensorRank(MinTensorRank, MaxTensorRank)
					.SetFromTensor1D(BiasTensor, InputTensor.GetShape().Rank())
					.Validate())
			{
				UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
				return false;
			}
		}

		if (!DmlOutputTensorDesc
				.SetTensorRank(MinTensorRank, MaxTensorRank)
				.SetFromTensor(OutputTensor)
				.Validate())
		{
			UE_LOG(LogNNE, Error, TEXT("Failed to initialize tensor(s) for DML inference"));
			return false;
		}

		DML_CONVOLUTION_OPERATOR_DESC	DmlConvOpDesc{};

		DmlConvOpDesc.InputTensor = DmlInputTensorDesc.GetDmlDesc();
		DmlConvOpDesc.FilterTensor = DmlFilterTensorDesc.GetDmlDesc();
		DmlConvOpDesc.BiasTensor = InputTensors.Num() > 2 ? DmlBiasTensorDesc.GetDmlDesc() : nullptr;
		DmlConvOpDesc.OutputTensor = DmlOutputTensorDesc.GetDmlDesc();
		DmlConvOpDesc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
		DmlConvOpDesc.Direction = Direction;
		DmlConvOpDesc.DimensionCount = Args.GetNumDimensions();
		DmlConvOpDesc.Strides = Args.GetStrides().GetData();
		DmlConvOpDesc.Dilations = Args.GetDilations().GetData();
		DmlConvOpDesc.StartPadding = Args.GetStartPadding().GetData();
		DmlConvOpDesc.EndPadding = Args.GetEndPadding().GetData();
		DmlConvOpDesc.OutputPadding = Args.GetOutPadding().GetData();
		DmlConvOpDesc.GroupCount = Args.GetGroup();

		DML_OPERATOR_DESC DmlOpDesc{};

		DmlOpDesc.Type = DML_OPERATOR_CONVOLUTION;
		DmlOpDesc.Desc = &DmlConvOpDesc;

		return CreateOperator(Device, DmlOpDesc);
	}
};

void RegisterConvOperator()
{
	FOperatorRegistryDml::Get()->OpAdd({{TEXT("Conv"), TEXT("Onnx")}, 1}, FOperatorDmlConv<DML_CONVOLUTION_DIRECTION_FORWARD>::Create);
	FOperatorRegistryDml::Get()->OpAdd({{TEXT("Conv"), TEXT("Onnx")}, 11}, FOperatorDmlConv<DML_CONVOLUTION_DIRECTION_FORWARD>::Create);
}

void RegisterConvTransposeOperator()
{
	FOperatorRegistryDml::Get()->OpAdd({{TEXT("ConvTranspose"), TEXT("Onnx")}, 1}, FOperatorDmlConv<DML_CONVOLUTION_DIRECTION_BACKWARD>::Create);
	FOperatorRegistryDml::Get()->OpAdd({{TEXT("ConvTranspose"), TEXT("Onnx")}, 11}, FOperatorDmlConv<DML_CONVOLUTION_DIRECTION_BACKWARD>::Create);
}

struct FDmlOperatorConvRegistrator
{
	FDmlOperatorConvRegistrator()
	{
		RegisterConvOperator();
		RegisterConvTransposeOperator();
	}
};

static FDmlOperatorConvRegistrator RegisterDmlOperatorConv;

} // namespace UE::NNERuntimeRDG::Private::Dml

#endif // NNE_USE_DIRECTML
