// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningNeuralNetworkObject.h"

#include "LearningNeuralNetwork.h"
#include "LearningRandom.h"

#if UE_LEARNING_ISPC
#include "Learning.ispc.generated.h"
#endif

namespace UE::Learning
{
	namespace NeuralNetworkObject
	{
		static inline float ReLU(const float X)
		{
			return FMath::Max(X, 0.0f);
		}

		static inline float ELU(const float X)
		{
			return X > 0.0f ? X : FMath::InvExpApprox(-X) - 1.0f;
		}

		static inline float Sigmoid(const float X)
		{
			return 1.0f / (1.0f + FMath::InvExpApprox(X));
		}

		static inline float TanH(const float X)
		{
			return FMath::Tanh(X);
		}

		static inline void MatMulPlusBias(
			TLearningArrayView<2, float> Output,
			const TLearningArrayView<2, const float> Input,
			const TLearningArrayView<2, const float> Weights,
			const TLearningArrayView<1, const float> Biases,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::MatMulPlusBias);

			const int32 RowNum = Weights.Num<0>();
			const int32 ColNum = Weights.Num<1>();

#if UE_LEARNING_ISPC
			if (Instances.IsSlice())
			{
				ispc::LearningLayerMatMulPlusBias(
					Output.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Input.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Weights.GetData(),
					Biases.GetData(),
					Instances.GetSliceNum(),
					RowNum,
					ColNum);
			}
			else
			{
				for (const int32 InstanceIdx : Instances)
				{
					ispc::LearningLayerMatMulVecPlusBias(
						Output[InstanceIdx].GetData(),
						Input[InstanceIdx].GetData(),
						Weights.GetData(),
						Biases.GetData(),
						RowNum,
						ColNum);
				}
			}
#else
			for (const int32 InstanceIdx : Instances)
			{
				Array::Copy(Output[InstanceIdx], Biases);

				for (int32 RowIdx = 0; RowIdx < RowNum; RowIdx++)
				{
					const float Value = Input[InstanceIdx][RowIdx];

					if (Value != 0.0)
					{
						for (int32 ColIdx = 0; ColIdx < ColNum; ColIdx++)
						{
							Output[InstanceIdx][ColIdx] += Value * Weights[RowIdx][ColIdx];
						}
					}
				}
			}
#endif
		}

		static inline void ActivationReLU(TLearningArrayView<2, float> InputOutput, const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::ActivationReLU);

			const int32 HiddenNum = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			if (Instances.IsSlice())
			{
				ispc::LearningLayerReLU(
					InputOutput.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Instances.GetSliceNum() * HiddenNum);
			}
			else
			{
				for (const int32 InstanceIdx : Instances)
				{
					ispc::LearningLayerReLU(
						InputOutput[InstanceIdx].GetData(),
						HiddenNum);
				}
			}
#else
			for (const int32 InstanceIdx : Instances)
			{
				for (int32 HiddenIdx = 0; HiddenIdx < HiddenNum; HiddenIdx++)
				{
					InputOutput[InstanceIdx][HiddenIdx] = ReLU(InputOutput[InstanceIdx][HiddenIdx]);
				}
			}
#endif
		}

		static inline void ActivationELU(TLearningArrayView<2, float> InputOutput, const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::ActivationELU);

			const int32 HiddenNum = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			if (Instances.IsSlice())
			{
				ispc::LearningLayerELU(
					InputOutput.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Instances.GetSliceNum() * HiddenNum);
			}
			else
			{
				for (const int32 InstanceIdx : Instances)
				{
					ispc::LearningLayerELU(
						InputOutput[InstanceIdx].GetData(),
						HiddenNum);
				}
			}
#else
			for (const int32 InstanceIdx : Instances)
			{
				for (int32 HiddenIdx = 0; HiddenIdx < HiddenNum; HiddenIdx++)
				{
					InputOutput[InstanceIdx][HiddenIdx] = ELU(InputOutput[InstanceIdx][HiddenIdx]);
				}
			}
#endif
		}

		static inline void ActivationTanH(TLearningArrayView<2, float> InputOutput, const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::ActivationTanH);

			const int32 HiddenNum = InputOutput.Num<1>();

#if UE_LEARNING_ISPC
			if (Instances.IsSlice())
			{
				ispc::LearningLayerTanH(
					InputOutput.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Instances.GetSliceNum() * HiddenNum);
			}
			else
			{
				for (const int32 InstanceIdx : Instances)
				{
					ispc::LearningLayerTanH(
						InputOutput[InstanceIdx].GetData(),
						HiddenNum);
				}
			}
#else
			for (const int32 InstanceIdx : Instances)
			{
				for (int32 HiddenIdx = 0; HiddenIdx < HiddenNum; HiddenIdx++)
				{
					InputOutput[InstanceIdx][HiddenIdx] = TanH(InputOutput[InstanceIdx][HiddenIdx]);
				}
			}
#endif
		}

		static inline void ActionNoise(
			TLearningArrayView<2, float> Output,
			TLearningArrayView<2, float> OutputMean,
			TLearningArrayView<2, float> OutputStd,
			const TLearningArrayView<2, float> Input,
			TLearningArrayView<1, uint32> Seed,
			const TLearningArrayView<1, const float> ActionNoiseScale,
			const float LogActionNoiseMin,
			const float LogActionNoiseMax,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::ActionNoise);

			const int32 InputNum = Input.Num<1>();
			const int32 OutputNum = Output.Num<1>();

#if UE_LEARNING_ISPC
			if (Instances.IsSlice())
			{
				ispc::LearningLayerActionNoise(
					Output.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					OutputMean.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					OutputStd.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Input.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Seed.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					ActionNoiseScale.Slice(Instances.GetSliceStart(), Instances.GetSliceNum()).GetData(),
					Instances.GetSliceNum(),
					InputNum,
					OutputNum,
					LogActionNoiseMin,
					LogActionNoiseMax);
			}
			else
			{
				for (const int32 InstanceIdx : Instances)
				{
					ispc::LearningLayerActionNoiseSingleBatch(
						Output[InstanceIdx].GetData(),
						OutputMean[InstanceIdx].GetData(),
						OutputStd[InstanceIdx].GetData(),
						Input[InstanceIdx].GetData(),
						Seed[InstanceIdx],
						ActionNoiseScale[InstanceIdx],
						OutputNum,
						LogActionNoiseMin,
						LogActionNoiseMax);
				}
			}
#else
			for (const int32 InstanceIdx : Instances)
			{
				for (int32 OutputIdx = 0; OutputIdx < OutputNum; OutputIdx++)
				{
					OutputMean[InstanceIdx][OutputIdx] = Input[InstanceIdx][OutputIdx];
					OutputStd[InstanceIdx][OutputIdx] = ActionNoiseScale[InstanceIdx] *
						FMath::Exp(Sigmoid(Input[InstanceIdx][OutputNum + OutputIdx]) * (LogActionNoiseMax - LogActionNoiseMin) + LogActionNoiseMin);

					Output[InstanceIdx][OutputIdx] = Random::Gaussian(
						Seed[InstanceIdx] ^ 0xab744615 ^ Random::Int(OutputIdx ^ 0xf8a88a27),
						OutputMean[InstanceIdx][OutputIdx],
						OutputStd[InstanceIdx][OutputIdx]);
				}
			}
#endif

			Random::ResampleStateArray(Seed, Instances);
		}

		static inline void EvaluatePolicyLayer(
			TLearningArrayView<2, float> Outputs,
			TLearningArrayView<2, float> OutputMeans,
			TLearningArrayView<2, float> OutputStds,
			TArrayView<TLearningArray<2, float, TInlineAllocator<128>>> Activations,
			const TLearningArrayView<2, const float> Inputs,
			TLearningArrayView<1, uint32> Seed,
			const TLearningArrayView<1, const float> ActionNoiseScale,
			const int32 LayerIdx,
			const int32 LayerNum,
			const float LogActionNoiseMin,
			const float LogActionNoiseMax,
			const FNeuralNetwork& NeuralNetwork,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::EvaluatePolicyLayer);

			TLearningArrayView<2, float> LayerOutput = Activations[LayerIdx - 0];
			const TLearningArrayView<2, const float> LayerInput = LayerIdx == 0 ? Inputs : Activations[LayerIdx - 1];

			// Apply Linear Transformation

			MatMulPlusBias(
				LayerOutput,
				LayerInput,
				NeuralNetwork.Weights[LayerIdx],
				NeuralNetwork.Biases[LayerIdx],
				Instances);

			// Apply Noise on final layer otherwise apply activation in-place

			if (LayerIdx == LayerNum - 1)
			{
				ActionNoise(
					Outputs,
					OutputMeans,
					OutputStds,
					LayerOutput,
					Seed,
					ActionNoiseScale,
					LogActionNoiseMin,
					LogActionNoiseMax,
					Instances);
			}
			else
			{
				switch (NeuralNetwork.ActivationFunction)
				{
				case EActivationFunction::ReLU: ActivationReLU(LayerOutput, Instances); break;
				case EActivationFunction::ELU: ActivationELU(LayerOutput, Instances); break;
				case EActivationFunction::TanH: ActivationTanH(LayerOutput, Instances); break;
				default: UE_LEARNING_NOT_IMPLEMENTED();
				}
			}
		}

		static inline void EvaluateCriticLayer(
			TLearningArrayView<1, float> Outputs,
			TArrayView<TLearningArray<2, float, TInlineAllocator<128>>> Activations,
			const TLearningArrayView<2, const float> Inputs,
			const int32 LayerIdx,
			const int32 LayerNum,
			const FNeuralNetwork& NeuralNetwork,
			const FIndexSet Instances)
		{
			UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::NeuralNetworkObject::EvaluateCriticLayer);

			TLearningArrayView<2, float> LayerOutput = LayerIdx == LayerNum - 1 ? TLearningArrayView<2, float>(Outputs.GetData(), { Outputs.Num<0>(), 1}) : Activations[LayerIdx - 0];
			const TLearningArrayView<2, const float> LayerInput = LayerIdx == 0 ? Inputs : Activations[LayerIdx - 1];

			// Apply Linear Transformation

			MatMulPlusBias(
				LayerOutput,
				LayerInput,
				NeuralNetwork.Weights[LayerIdx],
				NeuralNetwork.Biases[LayerIdx],
				Instances);

			// Apply activation in-place

			if (LayerIdx != LayerNum - 1)
			{
				switch (NeuralNetwork.ActivationFunction)
				{
				case EActivationFunction::ReLU: ActivationReLU(LayerOutput, Instances); break;
				case EActivationFunction::ELU: ActivationELU(LayerOutput, Instances); break;
				case EActivationFunction::TanH: ActivationTanH(LayerOutput, Instances); break;
				default: UE_LEARNING_NOT_IMPLEMENTED();
				}
			}
		}
	}

	FNeuralNetworkPolicyFunction::FNeuralNetworkPolicyFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const TSharedRef<FNeuralNetwork>& InNeuralNetwork,
		const uint32 InSeed,
		const FNeuralNetworkPolicyFunctionSettings& InSettings)
		: FFunctionObject(InInstanceData)
		, NeuralNetwork(InNeuralNetwork)
		, Settings(InSettings)
	{
		SeedHandle = InstanceData->Add<1, uint32>({ InIdentifier, TEXT("Seed") }, { InMaxInstanceNum });
		InputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Input") }, { InMaxInstanceNum, NeuralNetwork->GetInputNum() }, 0.0f);
		OutputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Output") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		OutputMeanHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputMean") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		OutputStdHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("OutputStd") }, { InMaxInstanceNum, NeuralNetwork->GetOutputNum() / 2 }, 0.0f);
		ActionNoiseScaleHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("ActionNoiseScale") }, { InMaxInstanceNum }, Settings.ActionNoiseScale);

		Random::IntArray(InstanceData->View(SeedHandle), InSeed);

		// Allocate temporary storage for activations

		const int32 LayerNum = InNeuralNetwork->GetLayerNum();

		Activations.SetNum(LayerNum);
		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			Activations[LayerIdx].SetNumUninitialized({ InMaxInstanceNum, NeuralNetwork->Weights[LayerIdx].Num<1>() });
		}
	}

	void FNeuralNetworkPolicyFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkPolicyFunction::Evaluate);

		const TLearningArrayView<2, const float> Inputs = InstanceData->ConstView(InputHandle);
		const TLearningArrayView<1, const float> ActionNoiseScale = InstanceData->ConstView(ActionNoiseScaleHandle);
		TLearningArrayView<2, float> Outputs = InstanceData->View(OutputHandle);
		TLearningArrayView<2, float> OutputMeans = InstanceData->View(OutputMeanHandle);
		TLearningArrayView<2, float> OutputStds = InstanceData->View(OutputStdHandle);
		TLearningArrayView<1, uint32> Seed = InstanceData->View(SeedHandle);

		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == Inputs.Num<1>());
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 2 * Outputs.Num<1>());

		const int32 LayerNum = NeuralNetwork->GetLayerNum();

		if (!ensureMsgf(LayerNum > 0, TEXT("Empty Neural Network used in Policy")))
		{
			Array::Zero(Outputs, Instances);
			return;
		}

		UE_LEARNING_ARRAY_VALUE_CHECK(Settings.ActionNoiseMin >= 0.0f && Settings.ActionNoiseMax >= 0.0f);
		const float LogActionNoiseMin = FMath::Loge(Settings.ActionNoiseMin + UE_KINDA_SMALL_NUMBER);
		const float LogActionNoiseMax = FMath::Loge(Settings.ActionNoiseMax + UE_KINDA_SMALL_NUMBER);

		// Compute Layers

		auto LayerEvaluationFunction = [&](const int32 SliceStart, const int32 SliceNum)
		{
			for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				NeuralNetworkObject::EvaluatePolicyLayer(
					Outputs,
					OutputMeans,
					OutputStds,
					Activations,
					Inputs,
					Seed,
					ActionNoiseScale,
					LayerIdx,
					LayerNum,
					LogActionNoiseMin,
					LogActionNoiseMax,
					*NeuralNetwork,
					Instances.Slice(SliceStart, SliceNum));
			}
		};

		if (Settings.bParallelEvaluation && Instances.Num() > Settings.MinParallelBatchSize)
		{
			SlicedParallelFor(Instances.Num(), Settings.MinParallelBatchSize, LayerEvaluationFunction);
		}
		else
		{
			LayerEvaluationFunction(0, Instances.Num());
		}

		Array::Check(Outputs, Instances);
	}

	FNeuralNetworkCriticFunction::FNeuralNetworkCriticFunction(
		const FName& InIdentifier,
		const TSharedRef<FArrayMap>& InInstanceData,
		const int32 InMaxInstanceNum,
		const TSharedRef<FNeuralNetwork>& InNeuralNetwork,
		const FNeuralNetworkCriticFunctionSettings& InSettings)
		: FFunctionObject(InInstanceData)
		, NeuralNetwork(InNeuralNetwork)
		, Settings(InSettings)
	{
		InputHandle = InstanceData->Add<2, float>({ InIdentifier, TEXT("Input") }, { InMaxInstanceNum, NeuralNetwork->GetInputNum() }, 0.0f);
		OutputHandle = InstanceData->Add<1, float>({ InIdentifier, TEXT("Output") }, { InMaxInstanceNum }, 0.0f);

		// Allocate temporary storage for activations
		// 
		// We will use the output handle as the final layer directly
		// so we don't need storage for this activation like we do with
		// the policy network.
		const int32 LayerNum = InNeuralNetwork->GetLayerNum() - 1;

		Activations.SetNum(LayerNum);
		for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			Activations[LayerIdx].SetNumUninitialized({ InMaxInstanceNum, NeuralNetwork->Weights[LayerIdx].Num<1>() });
		}
	}

	void FNeuralNetworkCriticFunction::Evaluate(const FIndexSet Instances)
	{
		UE_LEARNING_TRACE_CPUPROFILER_EVENT_SCOPE(Learning::FNeuralNetworkCriticFunction::Evaluate);

		const TLearningArrayView<2, const float> Inputs = InstanceData->ConstView(InputHandle);
		TLearningArrayView<1, float> Outputs = InstanceData->View(OutputHandle);

		UE_LEARNING_CHECK(NeuralNetwork->GetInputNum() == Inputs.Num<1>());
		UE_LEARNING_CHECK(NeuralNetwork->GetOutputNum() == 1);

		const int32 LayerNum = NeuralNetwork->GetLayerNum();

		if (!ensureMsgf(LayerNum > 0, TEXT("Empty Neural Network used in Critic")))
		{
			Array::Zero(Outputs, Instances);
			return;
		}

		// Compute Layers

		auto LayerEvaluationFunction = [&](const int32 SliceStart, const int32 SliceNum)
		{
			for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				NeuralNetworkObject::EvaluateCriticLayer(
					Outputs,
					Activations,
					Inputs,
					LayerIdx,
					LayerNum,
					*NeuralNetwork,
					Instances.Slice(SliceStart, SliceNum));
			}
		};

		if (Settings.bParallelEvaluation && Instances.Num() > Settings.MinParallelBatchSize)
		{
			SlicedParallelFor(Instances.Num(), Settings.MinParallelBatchSize, LayerEvaluationFunction);
		}
		else
		{
			LayerEvaluationFunction(0, Instances.Num());
		}

		Array::Check(Outputs, Instances);
	}
}
