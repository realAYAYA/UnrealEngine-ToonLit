// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDGConv.h"
#include "Algo/AnyOf.h"
#include "NNEAttributeMap.h"
#include "NNETensor.h"
#include "NNETypes.h"
#include "NNEHlslShadersConvCS.h"
#include "NNEHlslShadersConvMatmulCS.h"
#include "NNERuntimeRDGHelperTranspose.h"
#include "NNERuntimeRDGHlslHelper.h"

namespace UE::NNERuntimeRDG::Private::Hlsl
{
	DECLARE_GPU_STAT_NAMED(FNNEOperatorConvDefault, TEXT("NNE.Operator.Hlsl.Conv.Default"));
	DECLARE_GPU_STAT_NAMED(FNNEOperatorConvMatmul, TEXT("NNE.Operator.Hlsl.Conv.Matmul"));

	using EConvAutoPad = UE::NNEHlslShaders::Internal::EConvAutoPad;

	/**
	 * Convolution operator implementation
	 */
	class FConv : public FOperatorHlsl
	{
	public:

		static FOperatorHlsl* Create()
		{
			return new FConv();
		}

		virtual ~FConv() = default;

	private:

		FConv() {}

		int32 NumDimensions = 0;

		EConvAutoPad AutoPad = EConvAutoPad::NOTSET;
		TArray<int32> Dilations;
		int32 Group = 1;
		TArray<int32> Pads;
		TArray<int32> Strides;
		bool bAreWeightsTransposed = false;

	public:

		virtual int PrepareOutputs(TConstArrayView<NNE::Internal::FTensorRef> InputTensors, TArrayView<NNE::Internal::FTensorRef> OutputTensors) override
		{
			check(InputTensors.Num() >= 2 && InputTensors.Num() <= 3);
			check(OutputTensors.Num() == 1);

			const NNE::FTensorShape& Input = InputTensors[0]->GetShape();
			const NNE::FTensorShape& Weights = InputTensors[1]->GetShape();

			const TArray<int32> OutputShapeData = NNEHlslShaders::Internal::FConvCS::GetOutputShape(Input.GetData(), Weights.GetData(), AutoPad, Dilations, Strides, Pads);
			const NNE::FSymbolicTensorShape OutputShape = NNE::FSymbolicTensorShape::Make(OutputShapeData);
			
			if (!OutputShape.IsConcrete())
			{
				return -1;
			}
			OutputTensors[0]->SetShape(NNE::FTensorShape::MakeFromSymbolic(OutputShape));

			return 0;
		};

		virtual bool Initialize(TConstArrayView<NNE::FTensorDesc> InputTensorDescs, TConstArrayView<NNE::FTensorDesc> OutputTensorDescs, const NNE::FAttributeMap& Attributes) override
		{
            using namespace UE::NNEHlslShaders::Internal;

			check(InputTensorDescs.Num() >= 2 && InputTensorDescs.Num() <= 3);
			check(OutputTensorDescs.Num() == 1);

            const NNE::FTensorDesc& Input = InputTensorDescs[0];
			const NNE::FTensorDesc& Weights = InputTensorDescs[1];
			const NNE::FTensorDesc& Output = OutputTensorDescs[0];
			
			if (Input.GetShape().Rank() < 2)
			{
				UE_LOG(LogNNE, Warning, TEXT("Conv first input should be at least of rank 2"));
				return false;
			}
			if (Weights.GetShape().Rank() != Input.GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Conv first and second inputs should be of same ranks"));
				return false;
			}
			if (Output.GetShape().Rank() != Input.GetShape().Rank())
			{
				UE_LOG(LogNNE, Warning, TEXT("Conv first and output should be of same ranks"));
				return false;
			}

			NumDimensions = Input.GetShape().Rank() - 2;

			TArray<int32> DilationsOrStridesDefault;
			DilationsOrStridesDefault.Init(1, NumDimensions);

			FConvCS::LexFromString(AutoPad, *Attributes.GetValueOrDefault<FString>(TEXT("auto_pad"), TEXT("NOTSET")));
			Dilations = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("dilations"), DilationsOrStridesDefault);
			Group = Attributes.GetValueOrDefault<int32>(TEXT("group"), 1);
			if (AutoPad == EConvAutoPad::NOTSET)
			{
				TArray<int32> PadsDefault;
				PadsDefault.Init(0, 2 * NumDimensions);

				Pads = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("pads"), PadsDefault);
			}
			Strides = Attributes.GetValueOrDefault<TArray<int32>>(TEXT("strides"), DilationsOrStridesDefault);

			return true;
		}

		void DispatchConvDefault (FRDGBuilder& GraphBuilder, const FTensorRDG& Input, const FTensorRDG& Weights, const FTensorRDG* Bias, const FTensorRDG& Output)
		{
			using namespace UE::NNEHlslShaders::Internal;

			constexpr EConvAlgorithm Algorithm = EConvAlgorithm::SharedMemory;
			constexpr EConvGroupSize GroupSize = EConvGroupSize::Size256;
			bool HasBias = Bias != nullptr;
			TArray<int32> OutputShape = FConvCS::GetOutputShape(Input.GetShape().GetData(), Weights.GetShape().GetData(), AutoPad, Dilations, Strides, Pads);

			// Set parameters
			FConvCS::FParameters* Params = GraphBuilder.AllocParameters<FConvCS::FParameters>();
			FConvCS::FillInParameters(GroupSize, Input.GetShape().GetData(), Weights.GetShape().GetData(), HasBias, AutoPad, Group, Dilations,Strides, Pads, *Params);
			Params->X = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			Params->W = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Weights.GetBuffer(), PF_R32_FLOAT));
			if (HasBias) {
				Params->B = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Bias->GetBuffer(), PF_R32_FLOAT));
			}
			Params->Y = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));

			FConvCS::FPermutationDomain PermutationVector;

			PermutationVector.Set<FConvCS::FConvAlgorithm>(Algorithm);
			PermutationVector.Set<FConvCS::FConvAreWeightsTransposed>(bAreWeightsTransposed);
			PermutationVector.Set<FConvCS::FConvGroupSize>(GroupSize);
			PermutationVector.Set<FConvCS::FConvNumDimensions>(NumDimensions);
			PermutationVector.Set<FConvCS::FConvNumReadsPerThread>(FConvCS::GetNumReadsPerThread(GroupSize, Weights.GetShape().GetData(), Dilations, Strides));
			PermutationVector.Set<FConvCS::FConvHasB>(HasBias);
			TShaderMapRef<FConvCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Conv.Default");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorConvDefault);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Conv.Default.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				FConvCS::GetGroupCount(OutputShape, FConvCS::GetGroupShape(GroupSize, NumDimensions)));
		}

		bool DispatchConvMatmul(FRDGBuilder& GraphBuilder, const FTensorRDG& Input, const FTensorRDG& Weights, const FTensorRDG* Bias, const FTensorRDG& Output)
		{
			using namespace UE::NNEHlslShaders::Internal;

			if (Group != 1)
				return false;
			if (Input.GetShape().Rank() != 4)
				return false;
			if (Output.GetShape().Rank() != 4)
				return false;
			if (Weights.GetShape().Rank() != 4)
				return false;
			
			bool bHasBias = Bias != nullptr;

			const bool bHasDilation = Algo::AnyOf(Dilations, [](auto Dim) {return Dim != 1u; });
			if (bHasDilation)
				return false;

			int Ni = Input.GetShape().GetData()[0];
			int Ci = Input.GetShape().GetData()[1];
			int Hi = Input.GetShape().GetData()[2];
			int Wi = Input.GetShape().GetData()[3];

			check(Ni == Output.GetShape().GetData()[0]);
			int Cw = Output.GetShape().GetData()[1];
			int Ho = Output.GetShape().GetData()[2];
			int Wo = Output.GetShape().GetData()[3];

			check(Cw == Weights.GetShape().GetData()[0]);
			check(Ci == Weights.GetShape().GetData()[1]);
			int Hw = Weights.GetShape().GetData()[2];
			int Ww = Weights.GetShape().GetData()[3];

			if (Ci % 16 != 0)
				return false;
			if (Cw % 32 != 0)
				return false;
			if (Wo % 32 != 0) // Idea : support this by launching more threads and discard some results so a threadgroup still operator on only one value for H.
				return false;

			TArray<int32> Padding = FConvCS::GetPadding(Input.GetShape().GetData(), Weights.GetShape().GetData(), AutoPad, Dilations, Strides, Pads);

			// Set parameters
			FConvMatmulCS::FParameters* Params = GraphBuilder.AllocParameters<FConvMatmulCS::FParameters>();
			Params->Input = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Input.GetBuffer(), PF_R32_FLOAT));
			Params->Weight= GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Weights.GetBuffer(), PF_R32_FLOAT));
			if (bHasBias) {
				Params->Bias = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(Bias->GetBuffer(), PF_R32_FLOAT));
			}
			Params->Output = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Output.GetBuffer(), PF_R32_FLOAT));
			Params->Ci = Ci;
			Params->Hi = Hi;
			Params->Wi = Wi;
			Params->Cw = Cw;
			Params->Hw = Hw;
			Params->Ww = Ww;
			Params->Ho = Ho;
			Params->Wo = Wo;
			Params->StrideH = Strides[0];
			Params->StrideW = Strides[1];
			Params->PadTop = Padding[0];
			Params->PadLeft = Padding[1];

			FConvMatmulCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FConvMatmulCS::FConvMatmulAreWeightsTransposed>(bAreWeightsTransposed);
			PermutationVector.Set<FConvMatmulCS::FConvMatmulHasBias>(bHasBias);
			TShaderMapRef<FConvMatmulCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

			RDG_EVENT_SCOPE(GraphBuilder, "NNE.Operator.Hlsl.Conv.Matmul");
			RDG_GPU_STAT_SCOPE(GraphBuilder, FNNEOperatorConvMatmul);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("NNE.Operator.Hlsl.Conv.Matmul.Dispatch"),
				ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
				ComputeShader,
				Params,
				FConvMatmulCS::GetGroupCount(Output.GetShape().GetData()));

			return true;
		}

		virtual void Dispatch(FRDGBuilder& GraphBuilder, TConstArrayView<FTensorRDGRef> InputTensors, TConstArrayView<FTensorRDGRef> OutputTensors) override
		{
			check(InputTensors.Num() >= 2 && InputTensors.Num() <= 3);
			check(OutputTensors.Num() == 1);
			check(InputTensors[0] != nullptr);
			check(InputTensors[1] != nullptr);
			check(OutputTensors[0] != nullptr);

			const FTensorRDG& Input = *InputTensors[0];
			const FTensorRDG& Weights = *InputTensors[1];
			const FTensorRDG& Output = *OutputTensors[0];
			const FTensorRDG* Bias = nullptr;

			if (InputTensors.Num() == 3) {
				check(InputTensors[2] != nullptr);
				Bias = InputTensors[2];
			}

			check(Input.GetShape().Rank() > 2);
			check(Weights.GetShape().Rank() == Input.GetShape().Rank());
			check(Output.GetShape().Rank() == Input.GetShape().Rank());
			check(NumDimensions == (Input.GetShape().Rank() - 2));

			if (DispatchConvMatmul(GraphBuilder, Input, Weights, Bias, Output))
			{
				return;
			}
			DispatchConvDefault(GraphBuilder, Input, Weights, Bias, Output);
		}

		virtual void OptimizeInputsWeights(TArrayView<FTensorRDGRef> InputWeights) override
		{
			using namespace UE::NNEHlslShaders::Internal;

			check(InputWeights.Num() >= 2);
			FTensorRDGRef Weights = InputWeights[1];
			if (Weights == nullptr)
				return;

			// Heuristics : only matmul implementation benefits from transposed weights
			if (Weights->GetShape().Rank() != 4)
				return;

			uint32 Cw = Weights->GetShape().GetData()[0];
			uint32 Ci = Weights->GetShape().GetData()[1];

			if (Ci % 16 != 0)
				return;
			if (Cw % 32 != 0)
				return;
			if (Group != 1)
				return;

			//Transpose from CwCiHwWw to HwWwCiCw
			if (Internal::CPUHelper::Transpose::TransposePreparedData(*Weights, {2,3,1,0} ))
			{
				bAreWeightsTransposed = true;
			}
		};
	};

	bool ValidateConvOperator(const NNE::FAttributeMap& AttributeMap, TConstArrayView<ENNETensorDataType> InputTypes, TConstArrayView<NNE::FSymbolicTensorShape> InputShapes)
	{
		bool bIsValid = true;

		FAttributeValidator AttributeValidator;
		AttributeValidator.AddOptional(TEXT("auto_pad"), ENNEAttributeDataType::String);
		AttributeValidator.AddOptional(TEXT("dilations"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddOptional(TEXT("group"), ENNEAttributeDataType::Int32);
		AttributeValidator.AddOptional(TEXT("kernel_shape"), ENNEAttributeDataType::Int32Array); // idea: cross check input weight shape with this attribute if present
		AttributeValidator.AddOptional(TEXT("pads"), ENNEAttributeDataType::Int32Array);
		AttributeValidator.AddOptional(TEXT("strides"), ENNEAttributeDataType::Int32Array);

		bIsValid &= AttributeValidator.Validate(AttributeMap);

		FInputValidator InputValidator;
		InputValidator.AddSupportedType(ENNETensorDataType::Float);
		InputValidator.AddRequired();
		InputValidator.AddRequired();
		InputValidator.AddOptional();
		bIsValid &= InputValidator.Validate(InputTypes);

		return bIsValid;
	}

	bool RegisterConvOperator(FOperatorRegistryHlsl& Registry)
	{
		// Note: support of a particular version is partial with respect to tensor data types (only the most typical ones are usually supported).
		Registry.OpAdd({{TEXT("Conv"), TEXT("Onnx")}, 1}, FConv::Create, ValidateConvOperator);
		Registry.OpAdd({{TEXT("Conv"), TEXT("Onnx")}, 11}, FConv::Create, ValidateConvOperator);
		return true;
	}

} // UE::NNERuntimeRDG::Private::Hlsl
