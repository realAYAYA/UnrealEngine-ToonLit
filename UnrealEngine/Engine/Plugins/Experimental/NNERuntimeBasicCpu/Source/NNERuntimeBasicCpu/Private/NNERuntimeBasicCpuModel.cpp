// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeBasicCpuModel.h"

#include "NNE.h"
#include "NNERuntimeBasicCpu.h"
#include "NNERuntimeBasicCpuBuilder.h"

#define NNE_RUNTIME_BASIC_ENABLE_ISPC INTEL_ISPC
//#define NNE_RUNTIME_BASIC_ENABLE_ISPC 0

//#define NNE_RUNTIME_BASIC_ENABLE_NAN_CHECK (!UE_BUILD_SHIPPING)
#define NNE_RUNTIME_BASIC_ENABLE_NAN_CHECK 0

#define NNE_RUNTIME_BASIC_ENABLE_PROFILE (!UE_BUILD_SHIPPING)
//#define NNE_RUNTIME_BASIC_ENABLE_PROFILE 0

#if NNE_RUNTIME_BASIC_ENABLE_PROFILE
#define NNE_RUNTIME_BASIC_TRACE_SCOPE(...) TRACE_CPUPROFILER_EVENT_SCOPE(__VA_ARGS__)
#else
#define NNE_RUNTIME_BASIC_TRACE_SCOPE(...)
#endif

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
#include "NNERuntimeBasicCpu.ispc.generated.h"
#endif

namespace UE::NNE::RuntimeBasic
{
	namespace Private
	{
		//--------------------------------------------------------------------------
		// Serialization Helpers
		//--------------------------------------------------------------------------

		namespace Serialization
		{
			//--------------------------------------------------------------------------

			static inline void Align(uint64& InOutOffset, const uint32 Alignment)
			{
				InOutOffset = ((InOutOffset + Alignment - 1) / Alignment) * Alignment;
			}

			//--------------------------------------------------------------------------

			static inline void Size(uint64& InOutOffset, const uint32& In)
			{
				Align(InOutOffset, sizeof(uint32));
				InOutOffset += sizeof(uint32);
			}

			static inline void Size(uint64& InOutOffset, const float& In)
			{
				Align(InOutOffset, sizeof(float));
				InOutOffset += sizeof(float);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<float> In)
			{
				Align(InOutOffset, 64);
				InOutOffset += In.Num() * sizeof(float);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<uint16> In)
			{
				Align(InOutOffset, 64);
				InOutOffset += In.Num() * sizeof(uint16);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<uint32> In)
			{
				Align(InOutOffset, 64);
				InOutOffset += In.Num() * sizeof(uint32);
			}

			static inline void Size(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer);
			static inline void Size(uint64& InOutOffset, const TConstArrayView<TSharedPtr<ILayer>> InLayers);

			//--------------------------------------------------------------------------

			static inline void Load(uint64& InOutOffset, uint32& Out, TConstArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(uint32));
				Out = *((uint32*)(Data.GetData() + InOutOffset));
				InOutOffset += sizeof(uint32);
			}

			static inline void Load(uint64& InOutOffset, float& Out, TConstArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(float));
				Out = *((float*)(Data.GetData() + InOutOffset));
				InOutOffset += sizeof(float);
			}

			static inline void Load(uint64& InOutOffset, TConstArrayView<float>& Out, TConstArrayView<uint8> Data, int32 Size)
			{
				Align(InOutOffset, 64);
				Out = MakeArrayView<const float>((const float*)(Data.GetData() + InOutOffset), Size);
				InOutOffset += Size * sizeof(float);
			}

			static inline void Load(uint64& InOutOffset, TConstArrayView<uint16>& Out, TConstArrayView<uint8> Data, int32 Size)
			{
				Align(InOutOffset, 64);
				Out = MakeArrayView<const uint16>((const uint16*)(Data.GetData() + InOutOffset), Size);
				InOutOffset += Size * sizeof(uint16);
			}

			static inline void Load(uint64& InOutOffset, TConstArrayView<uint32>& Out, TConstArrayView<uint8> Data, int32 Size)
			{
				Align(InOutOffset, 64);
				Out = MakeArrayView<const uint32>((const uint32*)(Data.GetData() + InOutOffset), Size);
				InOutOffset += Size * sizeof(uint32);
			}

			static inline void Load(uint64& InOutOffset, TSharedPtr<ILayer>& OutLayer, TConstArrayView<uint8> Data);
			static inline void Load(uint64& InOutOffset, TArrayView<TSharedPtr<ILayer>> OutLayers, TConstArrayView<uint8> Data);

			//--------------------------------------------------------------------------

			static inline void Save(uint64& InOutOffset, const uint32 In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(uint32));
				*((uint32*)(Data.GetData() + InOutOffset)) = In;
				InOutOffset += sizeof(uint32);
			}

			static inline void Save(uint64& InOutOffset, const float In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, sizeof(float));
				*((float*)(Data.GetData() + InOutOffset)) = In;
				InOutOffset += sizeof(float);
			}

			static inline void Save(uint64& InOutOffset, TConstArrayView<float> In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, 64);
				FMemory::Memcpy(Data.GetData() + InOutOffset, In.GetData(), In.Num() * sizeof(float));
				InOutOffset += In.Num() * sizeof(float);
			}

			static inline void Save(uint64& InOutOffset, TConstArrayView<uint16> In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, 64);
				FMemory::Memcpy(Data.GetData() + InOutOffset, In.GetData(), In.Num() * sizeof(uint16));
				InOutOffset += In.Num() * sizeof(uint16);
			}

			static inline void Save(uint64& InOutOffset, TConstArrayView<uint32> In, TArrayView<uint8> Data)
			{
				Align(InOutOffset, 64);
				FMemory::Memcpy(Data.GetData() + InOutOffset, In.GetData(), In.Num() * sizeof(uint32));
				InOutOffset += In.Num() * sizeof(uint32);
			}

			static inline void Save(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer, TArrayView<uint8> Data);
			static inline void Save(uint64& InOutOffset, const TConstArrayView<TSharedPtr<ILayer>> InLayers, TArrayView<uint8> Data);
		}

		//--------------------------------------------------------------------------
		// Basic Mathematical Functions
		//--------------------------------------------------------------------------

		static inline float Sigmoid(const float X)
		{
			return 1.0f / (1.0f + FMath::Exp(-X));
		}

		//--------------------------------------------------------------------------
		// Operators
		//--------------------------------------------------------------------------

		static inline void OperatorNanCheck(
			const float* RESTRICT InputOutput, 
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 InputOutputStride)
		{
#if NNE_RUNTIME_BASIC_ENABLE_NAN_CHECK

			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorNanCheck);

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					const float Value = InputOutput[BatchIdx * InputOutputStride + Idx];
					checkf(FMath::IsFinite(Value) && Value != MAX_flt && Value != -MAX_flt,
						TEXT("Invalid value %f found in Batch %i, Value %i"), Value, BatchIdx, Idx);
				}
			}
#endif
		}

		static inline void OperatorCopy(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorCopy);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorCopy(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = Input[BatchIdx * InputStride + Idx];
				}
			}
#endif
		}

		static inline void OperatorNormalize(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Mean,
			const float* RESTRICT Std,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorNormalize);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorNormalize(
				Output,
				Input,
				Mean,
				Std,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] - Mean[Idx]) / Std[Idx];
				}
			}
#endif
		}

		static inline void OperatorDenormalize(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Mean,
			const float* RESTRICT Std,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorDenormalize);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorDenormalize(
				Output,
				Input,
				Mean,
				Std,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] * Std[Idx]) + Mean[Idx];
				}
			}
#endif
		}

		static inline void OperatorClamp(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT MinValues,
			const float* RESTRICT MaxValues,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorClamp);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorClamp(
				Output,
				Input,
				MinValues,
				MaxValues,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = FMath::Clamp(Input[BatchIdx * InputStride + Idx], MinValues[Idx], MaxValues[Idx]);
				}
			}
#endif
		}

		static inline void OperatorLinear(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Weights,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 OutputSize,
			const uint32 InputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorLinear);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorLinear(
				Output,
				Input,
				Weights,
				Biases,
				BatchSize,
				OutputSize,
				InputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
				{
					Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
				{
					const float Value = Input[BatchIdx * InputStride + RowIdx];

					if (Value != 0.0)
					{
						for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
						{
							Output[BatchIdx * OutputStride + ColIdx] += Value * Weights[RowIdx * OutputSize + ColIdx];
						}
					}
				}
			}
#endif
		}

		static inline void OperatorCompressedLinear(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint16* RESTRICT Weights,
			const float* RESTRICT WeightOffsets,
			const float* RESTRICT WeightScales,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 OutputSize,
			const uint32 InputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorCompressedLinear);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorCompressedLinear(
				Output,
				Input,
				Weights,
				WeightOffsets,
				WeightScales,
				Biases,
				BatchSize,
				OutputSize,
				InputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
				{
					Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
				{
					const float Value = Input[BatchIdx * InputStride + RowIdx];

					if (Value != 0.0)
					{
						const float Offset = WeightOffsets[RowIdx];
						const float Scales = WeightScales[RowIdx];

						for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
						{
							Output[BatchIdx * OutputStride + ColIdx] += Value * ((Scales * ((float)Weights[RowIdx * OutputSize + ColIdx])) + Offset);
						}
					}
				}
			}
#endif
		}

		static inline void OperatorMultiLinear(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Weights,
			const float* RESTRICT Biases,
			const uint32 BatchSize,
			const uint32 BlockNum,
			const uint32 OutputSize,
			const uint32 InputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorMultiLinear);

			// For this function ispc generates slightly less efficient code than the naive C++ implementation so we
			// don't bother calling out to the ispc version even if it is available

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
				{
					for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
					{
						Output[BatchIdx * OutputStride + BlockIdx * OutputSize + ColIdx] = Biases[BlockIdx * OutputSize + ColIdx];
					}
				}
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
				{
					for (uint32 RowIdx = 0; RowIdx < InputSize; RowIdx++)
					{
						const float Value = Input[BatchIdx * InputStride + BlockIdx * InputSize + RowIdx];

						if (Value != 0.0)
						{
							for (uint32 ColIdx = 0; ColIdx < OutputSize; ColIdx++)
							{
								Output[BatchIdx * OutputStride + BlockIdx * OutputSize + ColIdx] += Value * Weights[BlockIdx * InputSize * OutputSize + RowIdx * OutputSize + ColIdx];
							}
						}
					}
				}
			}
		}

		static inline void OperatorReLU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorReLU);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorReLU(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = FMath::Max(Input[BatchIdx * InputStride + Idx], 0.0f);
				}
			}
#endif
		}

		static inline void OperatorELU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorELU);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorELU(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					const float Value = Input[BatchIdx * InputStride + Idx];
					Output[BatchIdx * OutputStride + Idx] = Value > 0.0f ? Value : FMath::InvExpApprox(-Value) - 1.0f;
				}
			}
#endif
		}

		static inline void OperatorTanH(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorTanH);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorTanH(
				Output,
				Input,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					Output[BatchIdx * OutputStride + Idx] = FMath::Tanh(Input[BatchIdx * InputStride + Idx]);
				}
			}
#endif
		}

		static inline void OperatorPReLU(
			float* RESTRICT Output,
			const float* RESTRICT Input,
			const float* RESTRICT Alpha,
			const uint32 BatchSize,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorPReLU);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorPReLU(
				Output,
				Input,
				Alpha,
				BatchSize,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					const float Value = Input[BatchIdx * InputStride + Idx];
					Output[BatchIdx * OutputStride + Idx] = Value > 0.0f ? Value : Alpha[Idx] * Value;
				}
			}
#endif
		}

		static inline void OperatorMemoryCellUpdateMemory(
			float* RESTRICT Output,
			const float* RESTRICT RememberGate,
			const float* RESTRICT Memory,
			const float* RESTRICT Update,
			const uint32 BatchSize,
			const uint32 MemorySize,
			const uint32 OutputStride,
			const uint32 RememberGateStride,
			const uint32 MemoryStride,
			const uint32 UpdateStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorMemoryCellUpdateMemory);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorMemoryCellUpdateMemory(
				Output,
				RememberGate,
				Memory,
				Update,
				BatchSize,
				MemorySize,
				OutputStride,
				RememberGateStride,
				MemoryStride,
				UpdateStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < MemorySize; Idx++)
				{
					const float Gate = Sigmoid(RememberGate[BatchIdx * RememberGateStride + Idx]);
					const float Prev = Memory[BatchIdx * MemoryStride + Idx];
					const float Targ = FMath::Tanh(Update[BatchIdx * UpdateStride + Idx]);

					Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * Prev + Gate * Targ;
				}
			}
#endif
		}

		static inline void OperatorMemoryCellUpdateOutput(
			float* RESTRICT Output,
			const float* RESTRICT PassthroughGate,
			const float* RESTRICT MemoryUpdate,
			const float* RESTRICT InputUpdate,
			const uint32 BatchSize,
			const uint32 OutputSize,
			const uint32 OutputStride,
			const uint32 PassthroughGateStride,
			const uint32 MemoryUpdateStride,
			const uint32 InputUpdateStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorMemoryCellUpdateOutput);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorMemoryCellUpdateOutput(
				Output,
				PassthroughGate,
				MemoryUpdate,
				InputUpdate,
				BatchSize,
				OutputSize,
				OutputStride,
				PassthroughGateStride,
				MemoryUpdateStride,
				InputUpdateStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 Idx = 0; Idx < OutputSize; Idx++)
				{
					const float Gate = Sigmoid(PassthroughGate[BatchIdx * PassthroughGateStride + Idx]);
					const float MemTarg = FMath::Tanh(MemoryUpdate[BatchIdx * MemoryUpdateStride + Idx]);
					const float InTarg = FMath::Tanh(InputUpdate[BatchIdx * InputUpdateStride + Idx]);

					Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * MemTarg + Gate * InTarg;
				}
			}
#endif
		}

		static inline void OperatorAggregateGatherElements(
			float* RESTRICT OutputBuffer,
			const float* RESTRICT InputBuffer,
			const uint32* RESTRICT ElementNums,
			const uint32* RESTRICT ElementOffsets,
			const uint32 BatchSize,
			const uint32 ElementSize,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateGatherElements);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateGatherElements(
				OutputBuffer,
				InputBuffer,
				ElementNums,
				ElementOffsets,
				BatchSize,
				ElementSize,
				InputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				const uint32 ElementNum = ElementNums[BatchIdx];
				const uint32 ElementOffset = ElementOffsets[BatchIdx];

				for (uint32 ElementIdx = 0; ElementIdx < ElementNum; ElementIdx++)
				{
					for (uint32 Idx = 0; Idx < ElementSize; Idx++)
					{
						OutputBuffer[(ElementOffset + ElementIdx) * ElementSize + Idx] = InputBuffer[BatchIdx * InputStride + ElementIdx * ElementSize + Idx];
					}
				}
			}
#endif
		}

		static inline void OperatorAggregateInsertOneHot(
			float* RESTRICT QueryBuffer,
			const uint32 Index,
			const uint32 BatchSize,
			const uint32 MaskSize,
			const uint32 QueryBufferStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateInsertOneHot);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateInsertOneHot(
				QueryBuffer,
				Index,
				BatchSize,
				MaskSize,
				QueryBufferStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 MaskIdx = 0; MaskIdx < MaskSize; MaskIdx++)
				{
					QueryBuffer[BatchIdx * QueryBufferStride + MaskIdx] = 0.0f;
				}

				QueryBuffer[BatchIdx * QueryBufferStride + Index] = 1.0f;
			}
#endif
		}

		static inline void OperatorAggregateCountElementNum(
			uint32& TotalElementNum,
			uint32* RESTRICT ElementNums,
			uint32* RESTRICT ElementOffsets,
			const float* RESTRICT MaskBuffer,
			const uint32 BatchSize,
			const uint32 MaskSize,
			const uint32 MaskBufferStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateCountElementNum);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateCountElementNum(
				TotalElementNum,
				ElementNums,
				ElementOffsets,
				MaskBuffer,
				BatchSize,
				MaskSize,
				MaskBufferStride);
#else
			TotalElementNum = 0;

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				uint32 ElementSum = 0;
				for (uint32 MaskIdx = 0; MaskIdx < MaskSize; MaskIdx++)
				{
					if (MaskBuffer[BatchIdx * MaskBufferStride + MaskIdx]) { ElementSum++; }
				}

				ElementOffsets[BatchIdx] = TotalElementNum;
				ElementNums[BatchIdx] = ElementSum;
				TotalElementNum += ElementSum;
			}
#endif
		}

		static inline void OperatorAggregateGatherFromSubLayers(
			float* RESTRICT QueryBuffer,
			float* RESTRICT KeyBuffer,
			float* RESTRICT ValueBuffer,
			uint32* RESTRICT ElementAccum,
			const uint32* RESTRICT ElementNums,
			const uint32* RESTRICT ElementOffsets,
			const TConstArrayView<TArray<uint32>> SubLayerBatchIndices,
			const TConstArrayView<TArray<float>> SubLayerQueryBuffers,
			const TConstArrayView<TArray<float>> SubLayerKeyBuffers,
			const TConstArrayView<TArray<float>> SubLayerValueBuffers,
			const uint32 BatchSize,
			const uint32 QuerySize,
			const uint32 KeySize,
			const uint32 ValueSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateGatherFromSubLayers);

			const uint32 SubLayerNum = SubLayerBatchIndices.Num();

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				ElementAccum[BatchIdx] = 0;
			}

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
			{
				const float* RESTRICT SubLayerQueryBuffer = SubLayerQueryBuffers[SubLayerIdx].GetData();
				const float* RESTRICT SubLayerKeyBuffer = SubLayerKeyBuffers[SubLayerIdx].GetData();
				const float* RESTRICT SubLayerValueBuffer = SubLayerValueBuffers[SubLayerIdx].GetData();
				const uint32* RESTRICT SubLayerBatchIndicesBuffer = SubLayerBatchIndices[SubLayerIdx].GetData();
				const uint32 SubLayerBatchIndexNum = SubLayerBatchIndices[SubLayerIdx].Num();

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
				ispc::NNERuntimeBasicCPUOperatorAggregateGatherQueryValueFromSubLayers(
					QueryBuffer,
					KeyBuffer,
					ValueBuffer,
					ElementAccum,
					ElementOffsets,
					SubLayerQueryBuffer,
					SubLayerKeyBuffer,
					SubLayerValueBuffer,
					SubLayerBatchIndicesBuffer,
					SubLayerBatchIndexNum,
					QuerySize,
					KeySize,
					ValueSize);
#else
				for (uint32 ElementIdx = 0; ElementIdx < SubLayerBatchIndexNum; ElementIdx++)
				{
					const uint32 BatchIdx = SubLayerBatchIndicesBuffer[ElementIdx];
					const uint32 ElementOffset = ElementOffsets[BatchIdx] + ElementAccum[BatchIdx];

					for (uint32 QueryIdx = 0; QueryIdx < QuerySize; QueryIdx++)
					{
						QueryBuffer[ElementOffset * QuerySize + QueryIdx] = SubLayerQueryBuffer[ElementIdx * QuerySize + QueryIdx];
					}

					for (uint32 KeyIdx = 0; KeyIdx < KeySize; KeyIdx++)
					{
						KeyBuffer[ElementOffset * KeySize + KeyIdx] = SubLayerKeyBuffer[ElementIdx * KeySize + KeyIdx];
					}

					for (uint32 ValueIdx = 0; ValueIdx < ValueSize; ValueIdx++)
					{
						ValueBuffer[ElementOffset * ValueSize + ValueIdx] = SubLayerValueBuffer[ElementIdx * ValueSize + ValueIdx];
					}

					ElementAccum[BatchIdx]++;
				}
#endif
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				check(ElementAccum[BatchIdx] == ElementNums[BatchIdx]);
			}
		}

		static inline void OperatorAggregateDotProductAttention(
			float* RESTRICT Attention,
			const float* RESTRICT Queries,
			const float* RESTRICT Keys,
			const uint32 ElementNum,
			const uint32 AttentionEncodingSize,
			const uint32 AttentionHeadNum)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateDotProductAttention);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateDotProductAttention(
				Attention,
				Queries,
				Keys,
				ElementNum,
				AttentionEncodingSize,
				AttentionHeadNum);
#else
			for (uint32 ElementIdx = 0; ElementIdx < ElementNum; ElementIdx++)
			{
				for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
				{
					Attention[ElementIdx * AttentionHeadNum + HeadIdx] = 0.0f;

					for (uint32 Idx = 0; Idx < AttentionEncodingSize; Idx++)
					{
						Attention[ElementIdx * AttentionHeadNum + HeadIdx] += (
							Keys[ElementIdx * AttentionHeadNum * AttentionEncodingSize + HeadIdx * AttentionEncodingSize + Idx] *
							Queries[ElementIdx * AttentionHeadNum * AttentionEncodingSize + HeadIdx * AttentionEncodingSize + Idx]);
					}

					Attention[ElementIdx * AttentionHeadNum + HeadIdx] /= FMath::Sqrt((float)AttentionEncodingSize);
				}
			}
#endif
		}

		static inline void OperatorEncodeElementNums(
			float* RESTRICT OutputBuffer,
			const uint32* RESTRICT ElementNums,
			const uint32 MaxElementNum,
			const uint32 BatchSize,
			const uint32 OutputStride)
		{
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				OutputBuffer[BatchIdx * OutputStride] = (float)ElementNums[BatchIdx] / (float)MaxElementNum;
			}
		}

		static inline void OperatorAggregateSoftmaxPlusOneInplace(
			float* RESTRICT AttentionMaxs,
			float* RESTRICT AttentionDenoms,
			float* RESTRICT Attention,
			const uint32* RESTRICT ElementNums,
			const uint32* RESTRICT ElementOffsets,
			const uint32 BatchSize,
			const uint32 AttentionHeadNum)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateSoftmaxPlusOneInplace);

			// Numerically stable soft-max computation using subtraction of the (positive) max value
			// 
			// Here the +1 in the denominator allows the attention 
			// to attend to nothing as discussed here:
			// https://www.evanmiller.org/attention-is-off-by-one.html

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateSoftmaxPlusOneInplace(
				AttentionMaxs,
				AttentionDenoms,
				Attention,
				ElementNums,
				ElementOffsets,
				BatchSize,
				AttentionHeadNum);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				const uint32 ElementNum = ElementNums[BatchIdx];
				const uint32 ElementOffset = ElementOffsets[BatchIdx];

				for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
				{
					AttentionMaxs[HeadIdx] = 0.0f;
					AttentionDenoms[HeadIdx] = 0.0f;
				}

				for (uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
				{
					for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
					{
						AttentionMaxs[HeadIdx] = FMath::Max(AttentionMaxs[HeadIdx], Attention[ElementIdx * AttentionHeadNum + HeadIdx]);
					}
				}

				for (uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
				{
					for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
					{
						AttentionDenoms[HeadIdx] += FMath::Exp(Attention[ElementIdx * AttentionHeadNum + HeadIdx] - AttentionMaxs[HeadIdx]);
					}
				}

				for (uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
				{
					for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
					{
						Attention[ElementIdx * AttentionHeadNum + HeadIdx] = FMath::Exp(Attention[ElementIdx * AttentionHeadNum + HeadIdx] - AttentionMaxs[HeadIdx]) / (AttentionDenoms[HeadIdx] + 1.0f);
					}
				}
			}
#endif
		}

		static inline void OperatorAggregateAttentionSum(
			float* RESTRICT Output,
			const float* RESTRICT Attention,
			const float* RESTRICT Values,
			const uint32* RESTRICT ElementNums,
			const uint32* RESTRICT ElementOffsets,
			const uint32 BatchSize,
			const uint32 EncodingSize,
			const uint32 AttentionHeadNum,
			const uint32 OutputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorAggregateAttentionSum);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorAggregateAttentionSum(
				Output,
				Attention,
				Values,
				ElementNums,
				ElementOffsets,
				BatchSize,
				EncodingSize,
				AttentionHeadNum,
				OutputStride);
#else
			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
				{
					for (uint32 Idx = 0; Idx < EncodingSize; Idx++)
					{
						Output[BatchIdx * OutputStride + HeadIdx * EncodingSize + Idx] = 0.0f;
					}
				}

				const uint32 ElementNum = ElementNums[BatchIdx];
				const uint32 ElementOffset = ElementOffsets[BatchIdx];

				for (uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
				{
					for (uint32 HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
					{
						const float Scale = Attention[ElementIdx * AttentionHeadNum + HeadIdx];

						if (Scale != 0.0f)
						{
							for (uint32 Idx = 0; Idx < EncodingSize; Idx++)
							{
								Output[BatchIdx * OutputStride + HeadIdx * EncodingSize + Idx] += Scale * Values[ElementIdx * AttentionHeadNum * EncodingSize + HeadIdx * EncodingSize + Idx];
							}
						}
					}
				}
			}
#endif
		}

		static inline void OperatorGather(
			float* RESTRICT OutputBuffer,
			const float* RESTRICT InputBuffer,
			const uint32* RESTRICT BatchIndices,
			const uint32 BatchIndexNum,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorGather);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorGather(
				OutputBuffer,
				InputBuffer,
				BatchIndices,
				BatchIndexNum,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIndexIdx = 0; BatchIndexIdx < BatchIndexNum; BatchIndexIdx++)
			{
				const uint32 SrcBatchIdx = BatchIndices[BatchIndexIdx];
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					OutputBuffer[BatchIndexIdx * OutputStride + Idx] = InputBuffer[SrcBatchIdx * InputStride + Idx];
				}
			}
#endif
		}

		static inline void OperatorScatter(
			float* RESTRICT OutputBuffer,
			const float* RESTRICT InputBuffer,
			const uint32* RESTRICT BatchIndices,
			const uint32 BatchIndexNum,
			const uint32 InputOutputSize,
			const uint32 OutputStride,
			const uint32 InputStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorScatter);

#if NNE_RUNTIME_BASIC_ENABLE_ISPC
			ispc::NNERuntimeBasicCPUOperatorScatter(
				OutputBuffer,
				InputBuffer,
				BatchIndices,
				BatchIndexNum,
				InputOutputSize,
				OutputStride,
				InputStride);
#else
			for (uint32 BatchIndexIdx = 0; BatchIndexIdx < BatchIndexNum; BatchIndexIdx++)
			{
				const uint32 DstBatchIdx = BatchIndices[BatchIndexIdx];
				for (uint32 Idx = 0; Idx < InputOutputSize; Idx++)
				{
					OutputBuffer[DstBatchIdx * OutputStride + Idx] = InputBuffer[BatchIndexIdx * InputStride + Idx];
				}
			}
#endif
		}

		static inline void OperatorGatherSubLayerBatchIndicesExclusive(
			TArrayView<TArray<uint32>> SubLayerBatchIndices,
			const float* RESTRICT SubLayerMaskBuffer,
			const uint32 BatchSize,
			const uint32 SubLayerMaskSize,
			const uint32 SubLayerMaskStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorGatherSubLayerBatchIndicesExclusive);

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerMaskSize; SubLayerIdx++)
			{
				SubLayerBatchIndices[SubLayerIdx].Reset();
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				bool bFound = false;
				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerMaskSize; SubLayerIdx++)
				{
					if (SubLayerMaskBuffer[BatchIdx * SubLayerMaskStride + SubLayerIdx])
					{
						SubLayerBatchIndices[SubLayerIdx].Add(BatchIdx);
						bFound = true;
						break;
					}
				}

				checkf(bFound, TEXT("SubLayer index not found."));
			}
		}

		static inline void OperatorGatherSubLayerBatchIndicesInclusive(
			TArrayView<TArray<uint32>> SubLayerBatchIndices,
			const float* RESTRICT SubLayerMaskBuffer,
			const uint32 BatchSize,
			const uint32 SubLayerMaskSize,
			const uint32 SubLayerMaskStride)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::OperatorGatherSubLayerBatchIndicesInclusive);

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerMaskSize; SubLayerIdx++)
			{
				SubLayerBatchIndices[SubLayerIdx].Reset();
			}

			for (uint32 BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
			{
				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerMaskSize; SubLayerIdx++)
				{
					if (SubLayerMaskBuffer[BatchIdx * SubLayerMaskStride + SubLayerIdx])
					{
						SubLayerBatchIndices[SubLayerIdx].Add(BatchIdx);
					}
				}
			}
		}

		//--------------------------------------------------------------------------
		// Layer Types
		//--------------------------------------------------------------------------

		/** Layer Type Id - this should match what is given in nne_runtime_basic_cpu.py  */
		enum class ELayerType : uint32
		{
			Invalid = 0,
			
			Sequence = 1,
			
			Normalize = 2,
			Denormalize = 3,
			Linear = 4,
			CompressedLinear = 5,
			MultiLinear = 6,

			ReLU = 7,
			ELU = 8,
			TanH = 9,
			PReLU = 10,

			MemoryCell = 11,

			Copy = 12,
			Concat = 13,
			Array = 14,
			
			AggregateSet = 15,
			AggregateOrExclusive = 16,
			AggregateOrInclusive = 17,

			Clamp = 18,
		};

		//--------------------------------------------------------------------------
		// Layer Type Interfaces
		//--------------------------------------------------------------------------

		/**
		 * Interface for a Layer Instance - the data required for performing inference for a layer.
		 */
		struct ILayerInstance
		{
			/** Virtual destructor */
			virtual ~ILayerInstance() = default;

			/** Indicate to this layer instance what the maximum batchsize is going to be when performing inference. */
			virtual void SetMaxBatchSize(const uint32 MaxBatchSize) = 0;
		};

		/**
		 * Interface for a Layer - the network parameter data required for a layer.
		 */
		struct ILayer
		{
			/** Virtual destructor */
			virtual ~ILayer() = default;

			/** Create the instance data required for this type of layer. */
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return nullptr; };

			/** Get the layer type. */
			virtual ELayerType GetLayerType() const = 0;

			/** Get the size of the input vector. */
			virtual uint32 GetInputSize() const = 0;

			/** Get the size of the output vector. */
			virtual uint32 GetOutputSize() const = 0;

			/** Compute the size required to serialize this layer by growing InOutOffset. */
			virtual void SerializationSize(uint64& InOutOffset) const = 0;

			/** Load this layer from the buffer at the given offset. */
			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) = 0;

			/** Save this layer from the buffer at the given offset. */
			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const = 0;

			/**
			 * Evaluate this layer.
			 *
			 * @param Instance				The instance data for this layer - what was returned by `MakeInstance`.
			 * @param OutputBuffer			The output buffer
			 * @param InputBuffer			The input buffer
			 * @param BatchSize				The number of items in the batch
			 * @param OutputBufferSize		The vector size of the items in the output
			 * @param InputBufferSize		The vector size of the items in the input
			 * @param OutputBufferStride	The stride of the output for each item in the batch
			 * @param InputBufferStride		The stride of the input for each item in the batch
			 */
			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) = 0;
		};

		//--------------------------------------------------------------------------
		// Layers
		//--------------------------------------------------------------------------

		struct FSequenceLayer;

		struct FSequenceLayerInstance : public ILayerInstance
		{
			FSequenceLayerInstance(const FSequenceLayer& InSequenceLayer);

			virtual void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FSequenceLayer& SequenceLayer;
			uint32 ActivationStride = 0;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> Instances;
			TArray<float> ActivationBufferFront;
			TArray<float> ActivationBufferBack;
		};

		struct FSequenceLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FSequenceLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::Sequence; }
			virtual uint32 GetInputSize() const override final { return Layers.Num() > 0 ? Layers[0]->GetInputSize() : 0; }
			virtual uint32 GetOutputSize() const override final { return Layers.Num() > 0 ? Layers.Last()->GetOutputSize() : 0; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, (uint32)Layers.Num());
				Serialization::Size(InOutOffset, Layers);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				uint32 LayerNum = 0;
				Serialization::Load(InOutOffset, LayerNum, Data);
				Layers.Init(nullptr, LayerNum);
				Serialization::Load(InOutOffset, Layers, Data);

				LayerInputSizes.SetNumUninitialized(LayerNum);
				LayerOutputSizes.SetNumUninitialized(LayerNum);
				for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
				{
					LayerInputSizes[LayerIdx] = Layers[LayerIdx]->GetInputSize();
					LayerOutputSizes[LayerIdx] = Layers[LayerIdx]->GetOutputSize();
				}
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, (uint32)Layers.Num(), Data);
				Serialization::Save(InOutOffset, Layers, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FSequenceLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				const uint32 LayerNum = Layers.Num();
				check(LayerNum > 0);

				FSequenceLayerInstance* SequenceInstance = StaticCast<FSequenceLayerInstance*>(Instance);
				check(SequenceInstance);

				// If we just have one layer then evaluate layer directly without using intermediate storage

				if (LayerNum == 1)
				{
					Layers[0]->Evaluate(
						SequenceInstance->Instances[0].Get(),
						OutputBuffer,
						InputBuffer,
						BatchSize,
						OutputBufferSize,
						InputBufferSize,
						OutputBufferStride,
						InputBufferStride);

					return;
				}

				// Otherwise evaluate first layer from input into activation buffer

				Layers[0]->Evaluate(
					SequenceInstance->Instances[0].Get(),
					SequenceInstance->ActivationBufferFront.GetData(),
					InputBuffer,
					BatchSize,
					LayerOutputSizes[0],
					LayerInputSizes[0],
					SequenceInstance->ActivationStride,
					InputBufferStride);

				// Evaluate intermediate layers using front and back buffers

				for (uint32 LayerIdx = 1; LayerIdx < LayerNum - 1; LayerIdx++)
				{
					TConstArrayView<float> LayerInput = LayerIdx % 2 == 0 ? 
						SequenceInstance->ActivationBufferBack : 
						SequenceInstance->ActivationBufferFront;

					TArrayView<float> LayerOutput = LayerIdx % 2 == 0 ? 
						SequenceInstance->ActivationBufferFront : 
						SequenceInstance->ActivationBufferBack;

					Layers[LayerIdx]->Evaluate(
						SequenceInstance->Instances[LayerIdx].Get(),
						LayerOutput.GetData(),
						LayerInput.GetData(),
						BatchSize,
						LayerOutputSizes[LayerIdx],
						LayerInputSizes[LayerIdx],
						SequenceInstance->ActivationStride,
						SequenceInstance->ActivationStride);
				}

				// Evaluate final layer from activation buffer into output

				TConstArrayView<float> FinalLayerInput = LayerNum % 2 == 0 ? 
					SequenceInstance->ActivationBufferFront : 
					SequenceInstance->ActivationBufferBack;

				Layers.Last()->Evaluate(
					SequenceInstance->Instances.Last().Get(),
					OutputBuffer,
					FinalLayerInput.GetData(),
					BatchSize,
					OutputBufferSize,
					LayerInputSizes.Last(),
					OutputBufferStride,
					SequenceInstance->ActivationStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> Layers;
			TArray<uint32, TInlineAllocator<32>> LayerInputSizes;
			TArray<uint32, TInlineAllocator<32>> LayerOutputSizes;
		};

		FSequenceLayerInstance::FSequenceLayerInstance(const FSequenceLayer& InSequenceLayer) 
			: SequenceLayer(InSequenceLayer)
		{
			const uint32 LayerNum = SequenceLayer.Layers.Num();
			Instances.Init(nullptr, LayerNum);

			for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				Instances[LayerIdx] = SequenceLayer.Layers[LayerIdx]->MakeInstance();
			}

			// Compute the largest intermediate size used

			ActivationStride = SequenceLayer.Layers[0]->GetInputSize();
			for (uint32 LayerIdx = 1; LayerIdx < LayerNum; LayerIdx++)
			{
				ActivationStride = FMath::Max(ActivationStride, SequenceLayer.Layers[LayerIdx]->GetOutputSize());
			}
		}

		void FSequenceLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FSequenceLayerInstance::SetMaxBatchSize);

			// Propagate call to sub-layer instances

			for (const TSharedPtr<ILayerInstance>& Instance : Instances)
			{
				// Most layers don't allocate instance data and so we need to check for nullptr
				if (Instance)
				{
					Instance->SetMaxBatchSize(MaxBatchSize);
				}
			}

			// Allocate front and back buffers to maximum size. Don't shrink to avoid re-allocation 
			// when smaller batches are requested.

			ActivationBufferFront.SetNumUninitialized(MaxBatchSize * ActivationStride, EAllowShrinking::No);
			ActivationBufferBack.SetNumUninitialized(MaxBatchSize * ActivationStride, EAllowShrinking::No);
		}

		//--------------------------------------------------------------------------

		struct FNormalizeLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Normalize; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Mean);
				Serialization::Size(InOutOffset, Std);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Mean, Data, InputOutputSize);
				Serialization::Load(InOutOffset, Std, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Mean, Data);
				Serialization::Save(InOutOffset, Std, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FNormalizeLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorNormalize(
					OutputBuffer,
					InputBuffer,
					Mean.GetData(),
					Std.GetData(),
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Mean;
			TConstArrayView<float> Std;
		};

		//--------------------------------------------------------------------------

		struct FDenormalizeLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Denormalize; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Mean);
				Serialization::Size(InOutOffset, Std);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Mean, Data, InputOutputSize);
				Serialization::Load(InOutOffset, Std, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Mean, Data);
				Serialization::Save(InOutOffset, Std, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FDenormalizeLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorDenormalize(
					OutputBuffer,
					InputBuffer,
					Mean.GetData(),
					Std.GetData(),
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Mean;
			TConstArrayView<float> Std;
		};

		//--------------------------------------------------------------------------

		struct FLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Linear; }
			virtual uint32 GetInputSize() const override final { return InputSize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, Biases, Data, OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FLinearLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorLinear(
					OutputBuffer,
					InputBuffer,
					Weights.GetData(),
					Biases.GetData(),
					BatchSize,
					OutputSize,
					InputSize,
					OutputBufferStride,
					InputBufferStride);
				
				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			TConstArrayView<float> Biases;
			TConstArrayView<float> Weights;
		};

		//--------------------------------------------------------------------------

		struct FCompressedLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::CompressedLinear; }
			virtual uint32 GetInputSize() const override final { return InputSize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, WeightOffsets);
				Serialization::Size(InOutOffset, WeightScales);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, WeightOffsets, Data, InputSize);
				Serialization::Load(InOutOffset, WeightScales, Data, InputSize);
				Serialization::Load(InOutOffset, Biases, Data, OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, WeightOffsets, Data);
				Serialization::Save(InOutOffset, WeightScales, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FCompressedLinearLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorCompressedLinear(
					OutputBuffer,
					InputBuffer,
					Weights.GetData(),
					WeightOffsets.GetData(),
					WeightScales.GetData(),
					Biases.GetData(),
					BatchSize,
					OutputSize,
					InputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			TConstArrayView<float> WeightOffsets;
			TConstArrayView<float> WeightScales;
			TConstArrayView<float> Biases;
			TConstArrayView<uint16> Weights;
		};

		//--------------------------------------------------------------------------

		struct FMultiLinearLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::MultiLinear; }
			virtual uint32 GetInputSize() const override final { return BlockNum * InputSize; }
			virtual uint32 GetOutputSize() const override final { return BlockNum * OutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, BlockNum);
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, Biases);
				Serialization::Size(InOutOffset, Weights);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, BlockNum, Data);
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, Biases, Data, BlockNum * OutputSize);
				Serialization::Load(InOutOffset, Weights, Data, BlockNum * InputSize * OutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, BlockNum, Data);
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, Biases, Data);
				Serialization::Save(InOutOffset, Weights, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FMultiLinearLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorMultiLinear(
					OutputBuffer,
					InputBuffer,
					Weights.GetData(),
					Biases.GetData(),
					BatchSize,
					BlockNum,
					OutputSize,
					InputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			uint32 BlockNum = 0;
			TConstArrayView<float> Biases;
			TConstArrayView<float> Weights;
		};

		//--------------------------------------------------------------------------

		struct FReLULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::ReLU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FReLULayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorReLU(
					OutputBuffer,
					InputBuffer,
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FELULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::ELU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FELULayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorELU(
					OutputBuffer,
					InputBuffer,
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FTanHLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::TanH; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FTanHLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorTanH(
					OutputBuffer,
					InputBuffer,
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FPReLULayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::PReLU; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, Alpha);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, Alpha, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, Alpha, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FPReLuLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorPReLU(
					OutputBuffer,
					InputBuffer,
					Alpha.GetData(),
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> Alpha;
		};

		//--------------------------------------------------------------------------

		struct FMemoryCellLayer;

		struct FMemoryCellInstance : public ILayerInstance
		{
			FMemoryCellInstance(const FMemoryCellLayer& InMemoryCellLayer);

			virtual void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FMemoryCellLayer& MemoryCellLayer;
			TSharedPtr<ILayerInstance> RememberInstance;
			TSharedPtr<ILayerInstance> PassthroughInstance;
			TSharedPtr<ILayerInstance> MemoryUpdateInstance;
			TSharedPtr<ILayerInstance> OutputInputUpdateInstance;
			TSharedPtr<ILayerInstance> OutputMemoryUpdateInstance;
			TArray<float> RememberGateBuffer;
			TArray<float> MemoryUpdateBuffer;
			TArray<float> PassthroughGateBuffer;
			TArray<float> OutputMemoryUpdateBuffer;
			TArray<float> OutputInputUpdateBuffer;
		};

		struct FMemoryCellLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FMemoryCellInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::MemoryCell; }
			virtual uint32 GetInputSize() const override final { return InputSize + MemorySize; }
			virtual uint32 GetOutputSize() const override final { return OutputSize + MemorySize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputSize);
				Serialization::Size(InOutOffset, OutputSize);
				Serialization::Size(InOutOffset, MemorySize);
				Serialization::Size(InOutOffset, RememberLayer);
				Serialization::Size(InOutOffset, PassthroughLayer);
				Serialization::Size(InOutOffset, MemoryUpdateLayer);
				Serialization::Size(InOutOffset, OutputInputUpdateLayer);
				Serialization::Size(InOutOffset, OutputMemoryUpdateLayer);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputSize, Data);
				Serialization::Load(InOutOffset, OutputSize, Data);
				Serialization::Load(InOutOffset, MemorySize, Data);
				Serialization::Load(InOutOffset, RememberLayer, Data);
				Serialization::Load(InOutOffset, PassthroughLayer, Data);
				Serialization::Load(InOutOffset, MemoryUpdateLayer, Data);
				Serialization::Load(InOutOffset, OutputInputUpdateLayer, Data);
				Serialization::Load(InOutOffset, OutputMemoryUpdateLayer, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputSize, Data);
				Serialization::Save(InOutOffset, OutputSize, Data);
				Serialization::Save(InOutOffset, MemorySize, Data);
				Serialization::Save(InOutOffset, RememberLayer, Data);
				Serialization::Save(InOutOffset, PassthroughLayer, Data);
				Serialization::Save(InOutOffset, MemoryUpdateLayer, Data);
				Serialization::Save(InOutOffset, OutputInputUpdateLayer, Data);
				Serialization::Save(InOutOffset, OutputMemoryUpdateLayer, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FMemoryCellLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FMemoryCellInstance* MemoryCellInstance = StaticCast<FMemoryCellInstance*>(Instance);
				check(MemoryCellInstance);
				
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				// Remember Gate

				RememberLayer->Evaluate(
					MemoryCellInstance->RememberInstance.Get(),
					MemoryCellInstance->RememberGateBuffer.GetData(),
					InputBuffer,
					BatchSize,
					MemorySize,
					InputSize + MemorySize,
					MemorySize,
					InputBufferStride);

				// Passthrough Gate

				PassthroughLayer->Evaluate(
					MemoryCellInstance->PassthroughInstance.Get(),
					MemoryCellInstance->PassthroughGateBuffer.GetData(),
					InputBuffer,
					BatchSize,
					OutputSize,
					InputSize + MemorySize,
					OutputSize,
					InputBufferStride);

				// Memory Update

				MemoryUpdateLayer->Evaluate(
					MemoryCellInstance->MemoryUpdateInstance.Get(),
					MemoryCellInstance->MemoryUpdateBuffer.GetData(),
					InputBuffer,
					BatchSize,
					MemorySize,
					InputSize + MemorySize,
					MemorySize,
					InputBufferStride);

				// Update Memory State

				OperatorMemoryCellUpdateMemory(
					OutputBuffer + OutputSize,
					MemoryCellInstance->RememberGateBuffer.GetData(),
					InputBuffer + InputSize,
					MemoryCellInstance->MemoryUpdateBuffer.GetData(),
					BatchSize,
					MemorySize,
					OutputBufferStride,
					MemorySize,
					InputBufferStride,
					MemorySize);

				// Output Input Update

				OutputInputUpdateLayer->Evaluate(
					MemoryCellInstance->OutputInputUpdateInstance.Get(),
					MemoryCellInstance->OutputInputUpdateBuffer.GetData(),
					InputBuffer,
					BatchSize,
					OutputSize,
					InputSize + MemorySize,
					OutputSize,
					InputBufferStride);

				// Output Memory Update

				OutputMemoryUpdateLayer->Evaluate(
					MemoryCellInstance->OutputMemoryUpdateInstance.Get(),
					MemoryCellInstance->OutputMemoryUpdateBuffer.GetData(),
					OutputBuffer + OutputSize,
					BatchSize,
					OutputSize,
					MemorySize,
					OutputSize,
					OutputBufferStride);

				// Update Final Output

				OperatorMemoryCellUpdateOutput(
					OutputBuffer,
					MemoryCellInstance->PassthroughGateBuffer.GetData(),
					MemoryCellInstance->OutputMemoryUpdateBuffer.GetData(),
					MemoryCellInstance->OutputInputUpdateBuffer.GetData(),
					BatchSize,
					OutputSize,
					OutputBufferStride,
					OutputSize,
					OutputSize,
					OutputSize);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputSize = 0;
			uint32 OutputSize = 0;
			uint32 MemorySize = 0;
			TSharedPtr<ILayer> RememberLayer;
			TSharedPtr<ILayer> PassthroughLayer;
			TSharedPtr<ILayer> MemoryUpdateLayer;
			TSharedPtr<ILayer> OutputInputUpdateLayer;
			TSharedPtr<ILayer> OutputMemoryUpdateLayer;
		};

		FMemoryCellInstance::FMemoryCellInstance(const FMemoryCellLayer& InMemoryCellLayer) 
			: MemoryCellLayer(InMemoryCellLayer)
		{
			RememberInstance = MemoryCellLayer.RememberLayer->MakeInstance();
			PassthroughInstance = MemoryCellLayer.RememberLayer->MakeInstance();
			MemoryUpdateInstance = MemoryCellLayer.RememberLayer->MakeInstance();
			OutputInputUpdateInstance = MemoryCellLayer.RememberLayer->MakeInstance();
			OutputMemoryUpdateInstance = MemoryCellLayer.RememberLayer->MakeInstance();
		}

		void FMemoryCellInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FMemoryCellInstance::SetMaxBatchSize);

			if (RememberInstance) { RememberInstance->SetMaxBatchSize(MaxBatchSize); }
			if (PassthroughInstance) { PassthroughInstance->SetMaxBatchSize(MaxBatchSize); }
			if (MemoryUpdateInstance) { MemoryUpdateInstance->SetMaxBatchSize(MaxBatchSize); }
			if (OutputInputUpdateInstance) { OutputInputUpdateInstance->SetMaxBatchSize(MaxBatchSize); }
			if (OutputMemoryUpdateInstance) { OutputMemoryUpdateInstance->SetMaxBatchSize(MaxBatchSize); }

			RememberGateBuffer.SetNumUninitialized(MaxBatchSize * MemoryCellLayer.MemorySize, EAllowShrinking::No);
			PassthroughGateBuffer.SetNumUninitialized(MaxBatchSize * MemoryCellLayer.OutputSize, EAllowShrinking::No);
			MemoryUpdateBuffer.SetNumUninitialized(MaxBatchSize * MemoryCellLayer.MemorySize, EAllowShrinking::No);
			OutputInputUpdateBuffer.SetNumUninitialized(MaxBatchSize * MemoryCellLayer.OutputSize, EAllowShrinking::No);
			OutputMemoryUpdateBuffer.SetNumUninitialized(MaxBatchSize * MemoryCellLayer.OutputSize, EAllowShrinking::No);
		}

		//--------------------------------------------------------------------------

		struct FCopyLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Copy; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FCopyLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorCopy(
					OutputBuffer,
					InputBuffer,
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
		};

		//--------------------------------------------------------------------------

		struct FConcatLayer;

		struct FConcatLayerInstance : public ILayerInstance
		{
			FConcatLayerInstance(const FConcatLayer& InConcatLayer);

			void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FConcatLayer& ConcatLayer;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> Instances;
		};

		struct FConcatLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FConcatLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::Concat; }
			virtual uint32 GetInputSize() const override final { return TotalInputSize; }
			virtual uint32 GetOutputSize() const override final { return TotalOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, (uint32)Layers.Num());
				Serialization::Size(InOutOffset, InputSizes);
				Serialization::Size(InOutOffset, OutputSizes);
				Serialization::Size(InOutOffset, Layers);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				uint32 LayerNum = 0;
				Serialization::Load(InOutOffset, LayerNum, Data);
				Serialization::Load(InOutOffset, InputSizes, Data, LayerNum);
				Serialization::Load(InOutOffset, OutputSizes, Data, LayerNum);
				Layers.Init(nullptr, LayerNum);
				Serialization::Load(InOutOffset, Layers, Data);
				PostLoad();
			}

			void PostLoad()
			{
				const uint32 LayerNum = Layers.Num();
				InputOffsets.SetNumUninitialized(LayerNum);
				OutputOffsets.SetNumUninitialized(LayerNum);

				TotalInputSize = 0;
				TotalOutputSize = 0;
				for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
				{
					InputOffsets[LayerIdx] = TotalInputSize;
					OutputOffsets[LayerIdx] = TotalOutputSize;
					TotalInputSize += InputSizes[LayerIdx];
					TotalOutputSize += OutputSizes[LayerIdx];
				}
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, (uint32)Layers.Num(), Data);
				Serialization::Save(InOutOffset, InputSizes, Data);
				Serialization::Save(InOutOffset, OutputSizes, Data);
				Serialization::Save(InOutOffset, Layers, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FConcatLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FConcatLayerInstance* ConcatInstance = StaticCast<FConcatLayerInstance*>(Instance);
				check(ConcatInstance);

				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				const int32 LayerNum = Layers.Num();

				for (int32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
				{
					Layers[LayerIdx]->Evaluate(
						ConcatInstance->Instances[LayerIdx].Get(),
						OutputBuffer + OutputOffsets[LayerIdx],
						InputBuffer + InputOffsets[LayerIdx],
						BatchSize,
						OutputSizes[LayerIdx],
						InputSizes[LayerIdx],
						OutputBufferStride,
						InputBufferStride);
				}

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			TConstArrayView<uint32> InputSizes;
			TConstArrayView<uint32> OutputSizes;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> Layers;

			uint32 TotalInputSize = 0;
			uint32 TotalOutputSize = 0;
			TArray<uint32, TInlineAllocator<32>> InputOffsets;
			TArray<uint32, TInlineAllocator<32>> OutputOffsets;
		};

		FConcatLayerInstance::FConcatLayerInstance(const FConcatLayer& InConcatLayer)
			: ConcatLayer(InConcatLayer)
		{
			const uint32 LayerNum = ConcatLayer.Layers.Num();
			Instances.Init(nullptr, LayerNum);

			for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
			{
				Instances[LayerIdx] = ConcatLayer.Layers[LayerIdx]->MakeInstance();
			}
		}

		void FConcatLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FConcatLayerInstance::SetMaxBatchSize);

			// Propagate call to sub-layer instances

			for (const TSharedPtr<ILayerInstance>& Instance : Instances)
			{
				if (Instance)
				{
					Instance->SetMaxBatchSize(MaxBatchSize);
				}
			}
		}

		//--------------------------------------------------------------------------

		struct FArrayLayer;

		struct FArrayLayerInstance : public ILayerInstance
		{
			FArrayLayerInstance(const FArrayLayer& InArrayLayer);

			void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FArrayLayer& ArrayLayer;
			TSharedPtr<ILayerInstance> Instance;
			TArray<float> ElementInputBuffer;
			TArray<float> ElementOutputBuffer;
		};

		struct FArrayLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FArrayLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::Array; }
			virtual uint32 GetInputSize() const override final { return ElementNum * ElementInputSize; }
			virtual uint32 GetOutputSize() const override final { return ElementNum * ElementOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, ElementNum);
				Serialization::Size(InOutOffset, ElementInputSize);
				Serialization::Size(InOutOffset, ElementOutputSize);
				Serialization::Size(InOutOffset, SubLayer);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, ElementNum, Data);
				Serialization::Load(InOutOffset, ElementInputSize, Data);
				Serialization::Load(InOutOffset, ElementOutputSize, Data);
				Serialization::Load(InOutOffset, SubLayer, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, ElementNum, Data);
				Serialization::Save(InOutOffset, ElementInputSize, Data);
				Serialization::Save(InOutOffset, ElementOutputSize, Data);
				Serialization::Save(InOutOffset, SubLayer, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FArrayLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FArrayLayerInstance* ArrayInstance = StaticCast<FArrayLayerInstance*>(Instance);
				check(ArrayInstance);

				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				// If inputs and outputs are already tightly packed then evaluate directly as large batch

				if (InputBufferStride == ElementNum * ElementInputSize &&
					OutputBufferStride == ElementNum * ElementOutputSize)
				{
					SubLayer->Evaluate(
						ArrayInstance->Instance.Get(),
						OutputBuffer,
						InputBuffer,
						BatchSize * ElementNum,
						ElementOutputSize,
						ElementInputSize,
						ElementOutputSize,
						ElementInputSize);

					return;
				}

				// Otherwise gather all inputs into one large buffer packed together tightly

				OperatorCopy(
					ArrayInstance->ElementInputBuffer.GetData(),
					InputBuffer,
					BatchSize,
					ElementNum * ElementInputSize,
					ElementNum * ElementInputSize,
					InputBufferStride);

				// Evaluate sub-layer on large batch of all elements

				SubLayer->Evaluate(
					ArrayInstance->Instance.Get(),
					ArrayInstance->ElementOutputBuffer.GetData(),
					ArrayInstance->ElementInputBuffer.GetData(),
					BatchSize * ElementNum,
					ElementOutputSize,
					ElementInputSize,
					ElementOutputSize,
					ElementInputSize);

				// And scatter outputs out of tightly packed buffer

				OperatorCopy(
					OutputBuffer,
					ArrayInstance->ElementOutputBuffer.GetData(),
					BatchSize,
					ElementNum * ElementOutputSize,
					OutputBufferStride,
					ElementNum * ElementOutputSize);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 ElementNum = 0;
			uint32 ElementInputSize = 0;
			uint32 ElementOutputSize = 0;
			TSharedPtr<ILayer> SubLayer;
		};

		FArrayLayerInstance::FArrayLayerInstance(const FArrayLayer& InArrayLayer) : ArrayLayer(InArrayLayer)
		{
			Instance = ArrayLayer.SubLayer->MakeInstance();
		}

		void FArrayLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FArrayLayerInstance::SetMaxBatchSize);

			if (Instance)
			{
				// We are going to evaluate the sublayer on one large batch so we use MaxBatchSize * ArrayLayer.ElementNum.
				Instance->SetMaxBatchSize(MaxBatchSize * ArrayLayer.ElementNum);
			}

			ElementInputBuffer.SetNumUninitialized(MaxBatchSize * ArrayLayer.ElementNum * ArrayLayer.ElementInputSize, EAllowShrinking::No);
			ElementOutputBuffer.SetNumUninitialized(MaxBatchSize * ArrayLayer.ElementNum * ArrayLayer.ElementOutputSize, EAllowShrinking::No);
		}

		//--------------------------------------------------------------------------

		struct FAggregateSetLayer;

		struct FAggregateSetLayerInstance : public ILayerInstance
		{
			FAggregateSetLayerInstance(const FAggregateSetLayer& InAggregateSetLayer);

			void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FAggregateSetLayer& AggregateSetLayer;
			TSharedPtr<ILayerInstance> SubLayerInstance;
			TSharedPtr<ILayerInstance> QueryInstance;
			TSharedPtr<ILayerInstance> KeyInstance;
			TSharedPtr<ILayerInstance> ValueInstance;

			uint32 TotalElementNum = 0;
			TArray<uint32, TInlineAllocator<32>> ElementNums;
			TArray<uint32, TInlineAllocator<32>> ElementOffsets;

			TArray<float> InputElementBuffer;
			TArray<float> OutputElementBuffer;
			TArray<float> QueryBuffer;
			TArray<float> KeyBuffer;
			TArray<float> ValueBuffer;
			TArray<float> AttentionMaxsBuffer;
			TArray<float> AttentionDenomsBuffer;
			TArray<float> AttentionBuffer;
		};

		struct FAggregateSetLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FAggregateSetLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::AggregateSet; }
			virtual uint32 GetInputSize() const override final { return MaxElementNum * ElementInputSize + MaxElementNum; }
			virtual uint32 GetOutputSize() const override final { return AttentionHeadNum * OutputEncodingSize + 1; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, MaxElementNum);
				Serialization::Size(InOutOffset, ElementInputSize);
				Serialization::Size(InOutOffset, ElementOutputSize);
				Serialization::Size(InOutOffset, OutputEncodingSize);
				Serialization::Size(InOutOffset, AttentionEncodingSize);
				Serialization::Size(InOutOffset, AttentionHeadNum);
				Serialization::Size(InOutOffset, SubLayer);
				Serialization::Size(InOutOffset, QueryLayer);
				Serialization::Size(InOutOffset, KeyLayer);
				Serialization::Size(InOutOffset, ValueLayer);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, MaxElementNum, Data);
				Serialization::Load(InOutOffset, ElementInputSize, Data);
				Serialization::Load(InOutOffset, ElementOutputSize, Data);
				Serialization::Load(InOutOffset, OutputEncodingSize, Data);
				Serialization::Load(InOutOffset, AttentionEncodingSize, Data);
				Serialization::Load(InOutOffset, AttentionHeadNum, Data);
				Serialization::Load(InOutOffset, SubLayer, Data);
				Serialization::Load(InOutOffset, QueryLayer, Data);
				Serialization::Load(InOutOffset, KeyLayer, Data);
				Serialization::Load(InOutOffset, ValueLayer, Data);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, MaxElementNum, Data);
				Serialization::Save(InOutOffset, ElementInputSize, Data);
				Serialization::Save(InOutOffset, ElementOutputSize, Data);
				Serialization::Save(InOutOffset, OutputEncodingSize, Data);
				Serialization::Save(InOutOffset, AttentionEncodingSize, Data);
				Serialization::Save(InOutOffset, AttentionHeadNum, Data);
				Serialization::Save(InOutOffset, SubLayer, Data);
				Serialization::Save(InOutOffset, QueryLayer, Data);
				Serialization::Save(InOutOffset, KeyLayer, Data);
				Serialization::Save(InOutOffset, ValueLayer, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateSetLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FAggregateSetLayerInstance* AggregateSetInstance = StaticCast<FAggregateSetLayerInstance*>(Instance);
				check(AggregateSetInstance);

				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				// Count the number of elements for each item in the batch
				
				OperatorAggregateCountElementNum(
					AggregateSetInstance->TotalElementNum,
					AggregateSetInstance->ElementNums.GetData(),
					AggregateSetInstance->ElementOffsets.GetData(),
					InputBuffer + MaxElementNum * ElementInputSize,
					BatchSize,
					MaxElementNum,
					InputBufferStride);

				// Gather Elements from all batches into one large tightly packed buffer

				OperatorAggregateGatherElements(
					AggregateSetInstance->InputElementBuffer.GetData(),
					InputBuffer,
					AggregateSetInstance->ElementNums.GetData(),
					AggregateSetInstance->ElementOffsets.GetData(),
					BatchSize,
					ElementInputSize,
					InputBufferStride);

				// Evaluate Sublayer on all elements

				SubLayer->Evaluate(
					AggregateSetInstance->SubLayerInstance.Get(),
					AggregateSetInstance->OutputElementBuffer.GetData(),
					AggregateSetInstance->InputElementBuffer.GetData(),
					AggregateSetInstance->TotalElementNum,
					ElementOutputSize,
					ElementInputSize,
					ElementOutputSize,
					ElementInputSize);

				// Compute Query on all elements

				QueryLayer->Evaluate(
					AggregateSetInstance->QueryInstance.Get(),
					AggregateSetInstance->QueryBuffer.GetData(),
					AggregateSetInstance->OutputElementBuffer.GetData(),
					AggregateSetInstance->TotalElementNum,
					AttentionHeadNum * AttentionEncodingSize,
					ElementOutputSize,
					AttentionHeadNum * AttentionEncodingSize,
					ElementOutputSize);

				// Compute Keys on all elements

				KeyLayer->Evaluate(
					AggregateSetInstance->KeyInstance.Get(),
					AggregateSetInstance->KeyBuffer.GetData(),
					AggregateSetInstance->OutputElementBuffer.GetData(),
					AggregateSetInstance->TotalElementNum,
					AttentionHeadNum * AttentionEncodingSize,
					ElementOutputSize,
					AttentionHeadNum * AttentionEncodingSize,
					ElementOutputSize);

				// Compute Values on all elements

				ValueLayer->Evaluate(
					AggregateSetInstance->ValueInstance.Get(),
					AggregateSetInstance->ValueBuffer.GetData(),
					AggregateSetInstance->OutputElementBuffer.GetData(),
					AggregateSetInstance->TotalElementNum,
					AttentionHeadNum * OutputEncodingSize,
					ElementOutputSize,
					AttentionHeadNum * OutputEncodingSize,
					ElementOutputSize);

				// Compute Attention

				OperatorAggregateDotProductAttention(
					AggregateSetInstance->AttentionBuffer.GetData(),
					AggregateSetInstance->QueryBuffer.GetData(),
					AggregateSetInstance->KeyBuffer.GetData(),
					AggregateSetInstance->TotalElementNum,
					AttentionEncodingSize,
					AttentionHeadNum);

				OperatorAggregateSoftmaxPlusOneInplace(
					AggregateSetInstance->AttentionMaxsBuffer.GetData(),
					AggregateSetInstance->AttentionDenomsBuffer.GetData(),
					AggregateSetInstance->AttentionBuffer.GetData(),
					AggregateSetInstance->ElementNums.GetData(),
					AggregateSetInstance->ElementOffsets.GetData(),
					BatchSize,
					AttentionHeadNum);

				OperatorAggregateAttentionSum(
					OutputBuffer,
					AggregateSetInstance->AttentionBuffer.GetData(),
					AggregateSetInstance->ValueBuffer.GetData(),
					AggregateSetInstance->ElementNums.GetData(),
					AggregateSetInstance->ElementOffsets.GetData(),
					BatchSize,
					OutputEncodingSize,
					AttentionHeadNum,
					OutputBufferStride);

				// Append Element Nums

				OperatorEncodeElementNums(
					OutputBuffer + AttentionHeadNum * OutputEncodingSize,
					AggregateSetInstance->ElementNums.GetData(),
					MaxElementNum,
					BatchSize,
					OutputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 MaxElementNum = 0;
			uint32 OutputEncodingSize = 0;
			uint32 AttentionEncodingSize = 0;
			uint32 AttentionHeadNum = 0;

			TSharedPtr<ILayer> SubLayer;
			TSharedPtr<ILayer> QueryLayer;
			TSharedPtr<ILayer> KeyLayer;
			TSharedPtr<ILayer> ValueLayer;

			uint32 ElementInputSize = 0;
			uint32 ElementOutputSize = 0;
		};

		FAggregateSetLayerInstance::FAggregateSetLayerInstance(const FAggregateSetLayer& InAggregateSetLayer)
			: AggregateSetLayer(InAggregateSetLayer)
		{
			SubLayerInstance = AggregateSetLayer.SubLayer->MakeInstance();
			QueryInstance = AggregateSetLayer.QueryLayer->MakeInstance();
			KeyInstance = AggregateSetLayer.KeyLayer->MakeInstance();
			ValueInstance = AggregateSetLayer.ValueLayer->MakeInstance();
		}

		void FAggregateSetLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateSetLayerInstance::SetMaxBatchSize);

			if (SubLayerInstance) { SubLayerInstance->SetMaxBatchSize(MaxBatchSize * AggregateSetLayer.MaxElementNum); }
			if (QueryInstance) { QueryInstance->SetMaxBatchSize(MaxBatchSize * AggregateSetLayer.MaxElementNum); }
			if (KeyInstance) { KeyInstance->SetMaxBatchSize(MaxBatchSize * AggregateSetLayer.MaxElementNum); }
			if (ValueInstance) { ValueInstance->SetMaxBatchSize(MaxBatchSize * AggregateSetLayer.MaxElementNum); }

			ElementNums.SetNumUninitialized(MaxBatchSize, EAllowShrinking::No);
			ElementOffsets.SetNumUninitialized(MaxBatchSize, EAllowShrinking::No);
			
			InputElementBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.ElementInputSize, EAllowShrinking::No);
			OutputElementBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.ElementOutputSize, EAllowShrinking::No);
			QueryBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.AttentionHeadNum * AggregateSetLayer.AttentionEncodingSize, EAllowShrinking::No);
			KeyBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.AttentionHeadNum * AggregateSetLayer.AttentionEncodingSize, EAllowShrinking::No);
			ValueBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.AttentionHeadNum * AggregateSetLayer.OutputEncodingSize, EAllowShrinking::No);
			AttentionMaxsBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.AttentionHeadNum, EAllowShrinking::No);
			AttentionDenomsBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.AttentionHeadNum, EAllowShrinking::No);
			AttentionBuffer.SetNumUninitialized(MaxBatchSize * AggregateSetLayer.MaxElementNum * AggregateSetLayer.AttentionHeadNum, EAllowShrinking::No);
		}

		//--------------------------------------------------------------------------

		struct FAggregateOrExclusiveLayer;

		struct FAggregateOrExclusiveLayerInstance : public ILayerInstance
		{
			FAggregateOrExclusiveLayerInstance(const FAggregateOrExclusiveLayer& InAggregateOrExclusiveLayer);

			void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FAggregateOrExclusiveLayer& AggregateOrExclusiveLayer;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> SubLayerInstances;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> EncoderInstances;

			TArray<TArray<uint32>, TInlineAllocator<32>> SubLayerBatchIndices;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerInputBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerOutputBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerEncodingBuffers;
		};

		struct FAggregateOrExclusiveLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FAggregateOrExclusiveLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::AggregateOrExclusive; }
			virtual uint32 GetInputSize() const override final { return MaxSubLayerInputSize + SubLayers.Num(); }
			virtual uint32 GetOutputSize() const override final { return OutputEncodingSize + SubLayers.Num(); }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, (uint32)SubLayers.Num());
				Serialization::Size(InOutOffset, OutputEncodingSize);
				Serialization::Size(InOutOffset, SubLayerInputSizes);
				Serialization::Size(InOutOffset, SubLayerOutputSizes);
				Serialization::Size(InOutOffset, SubLayers);
				Serialization::Size(InOutOffset, Encoders);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				uint32 SubLayerNum = 0;
				Serialization::Load(InOutOffset, SubLayerNum, Data);
				Serialization::Load(InOutOffset, OutputEncodingSize, Data);
				Serialization::Load(InOutOffset, SubLayerInputSizes, Data, SubLayerNum);
				Serialization::Load(InOutOffset, SubLayerOutputSizes, Data, SubLayerNum);
				SubLayers.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, SubLayers, Data);
				Encoders.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, Encoders, Data);
				PostLoad();
			}

			void PostLoad()
			{
				const uint32 SubLayerNum = SubLayers.Num();
				MaxSubLayerInputSize = 0;
				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
				{
					MaxSubLayerInputSize = FMath::Max(MaxSubLayerInputSize, SubLayerInputSizes[SubLayerIdx]);
				}
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, (uint32)SubLayers.Num(), Data);
				Serialization::Save(InOutOffset, OutputEncodingSize, Data);
				Serialization::Save(InOutOffset, SubLayerInputSizes, Data);
				Serialization::Save(InOutOffset, SubLayerOutputSizes, Data);
				Serialization::Save(InOutOffset, SubLayers, Data);
				Serialization::Save(InOutOffset, Encoders, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateOrExclusiveLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FAggregateOrExclusiveLayerInstance* AggregateOrExclusiveInstance = StaticCast<FAggregateOrExclusiveLayerInstance*>(Instance);
				check(AggregateOrExclusiveInstance);

				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				const uint32 SubLayerNum = SubLayers.Num();

				// Gather the batch indices used by each sub-layer

				OperatorGatherSubLayerBatchIndicesExclusive(
					AggregateOrExclusiveInstance->SubLayerBatchIndices,
					InputBuffer + MaxSubLayerInputSize,
					BatchSize,
					SubLayerNum,
					InputBufferStride);

				// Evaluate Sublayers

				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
				{
					const uint32 SubLayerBatchSize = AggregateOrExclusiveInstance->SubLayerBatchIndices[SubLayerIdx].Num();

					if (SubLayerBatchSize == 0) { continue; }

					OperatorGather(
						AggregateOrExclusiveInstance->SubLayerInputBuffers[SubLayerIdx].GetData(),
						InputBuffer,
						AggregateOrExclusiveInstance->SubLayerBatchIndices[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						SubLayerInputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx],
						InputBufferStride);

					SubLayers[SubLayerIdx]->Evaluate(
						AggregateOrExclusiveInstance->SubLayerInstances[SubLayerIdx].Get(),
						AggregateOrExclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						AggregateOrExclusiveInstance->SubLayerInputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						SubLayerOutputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx],
						SubLayerOutputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx]);

					Encoders[SubLayerIdx]->Evaluate(
						AggregateOrExclusiveInstance->EncoderInstances[SubLayerIdx].Get(),
						AggregateOrExclusiveInstance->SubLayerEncodingBuffers[SubLayerIdx].GetData(),
						AggregateOrExclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						OutputEncodingSize,
						SubLayerOutputSizes[SubLayerIdx],
						OutputEncodingSize,
						SubLayerOutputSizes[SubLayerIdx]);

					OperatorScatter(
						OutputBuffer,
						AggregateOrExclusiveInstance->SubLayerEncodingBuffers[SubLayerIdx].GetData(),
						AggregateOrExclusiveInstance->SubLayerBatchIndices[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						OutputEncodingSize,
						OutputBufferStride,
						OutputEncodingSize);
				}

				// Append SubLayer Mask

				OperatorCopy(
					OutputBuffer + OutputEncodingSize,
					InputBuffer + MaxSubLayerInputSize,
					BatchSize,
					SubLayerNum,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 OutputEncodingSize = 0;
			TConstArrayView<uint32> SubLayerInputSizes;
			TConstArrayView<uint32> SubLayerOutputSizes;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> SubLayers;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> Encoders;

			uint32 MaxSubLayerInputSize = 0;
		};

		FAggregateOrExclusiveLayerInstance::FAggregateOrExclusiveLayerInstance(const FAggregateOrExclusiveLayer& InAggregateOrExclusiveLayer)
			: AggregateOrExclusiveLayer(InAggregateOrExclusiveLayer)
		{
			const uint32 SubLayerNum = AggregateOrExclusiveLayer.SubLayers.Num();

			SubLayerInstances.Init(nullptr, SubLayerNum);
			EncoderInstances.Init(nullptr, SubLayerNum);

			SubLayerBatchIndices.SetNum(SubLayerNum);
			SubLayerInputBuffers.SetNum(SubLayerNum);
			SubLayerOutputBuffers.SetNum(SubLayerNum);
			SubLayerEncodingBuffers.SetNum(SubLayerNum);

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
			{
				SubLayerInstances[SubLayerIdx] = AggregateOrExclusiveLayer.SubLayers[SubLayerIdx]->MakeInstance();
				EncoderInstances[SubLayerIdx] = AggregateOrExclusiveLayer.Encoders[SubLayerIdx]->MakeInstance();
			}
		}

		void FAggregateOrExclusiveLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateOrExclusiveLayerInstance::SetMaxBatchSize);

			const uint32 SubLayerNum = AggregateOrExclusiveLayer.SubLayers.Num();

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
			{
				if (SubLayerInstances[SubLayerIdx]) { SubLayerInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }
				if (EncoderInstances[SubLayerIdx]) { EncoderInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }

				SubLayerBatchIndices[SubLayerIdx].Empty(MaxBatchSize);
				SubLayerInputBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrExclusiveLayer.SubLayerInputSizes[SubLayerIdx], EAllowShrinking::No);
				SubLayerOutputBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrExclusiveLayer.SubLayerOutputSizes[SubLayerIdx], EAllowShrinking::No);
				SubLayerEncodingBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrExclusiveLayer.OutputEncodingSize, EAllowShrinking::No);
			}
		}

		//--------------------------------------------------------------------------

		struct FAggregateOrInclusiveLayer;

		struct FAggregateOrInclusiveLayerInstance : public ILayerInstance
		{
			FAggregateOrInclusiveLayerInstance(const FAggregateOrInclusiveLayer& InAggregateOrInclusiveLayer);

			void SetMaxBatchSize(const uint32 MaxBatchSize) override final;

			const FAggregateOrInclusiveLayer& AggregateOrInclusiveLayer;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> SubLayerInstances;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> QueryInstances;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> KeyInstances;
			TArray<TSharedPtr<ILayerInstance>, TInlineAllocator<32>> ValueInstances;

			TArray<TArray<uint32>, TInlineAllocator<32>> SubLayerBatchIndices;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerInputBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerOutputBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerQueryBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerKeyBuffers;
			TArray<TArray<float>, TInlineAllocator<32>> SubLayerValueBuffers;

			uint32 TotalElementNum = 0;
			TArray<uint32, TInlineAllocator<32>> ElementAccum;
			TArray<uint32, TInlineAllocator<32>> ElementNums;
			TArray<uint32, TInlineAllocator<32>> ElementOffsets;

			TArray<float> AttentionMaxsBuffer;
			TArray<float> AttentionDenomsBuffer;
			TArray<float> AttentionBuffer;
			TArray<float> QueryBuffer;
			TArray<float> KeyBuffer;
			TArray<float> ValueBuffer;
		};

		struct FAggregateOrInclusiveLayer : public ILayer
		{
			virtual TSharedPtr<ILayerInstance> MakeInstance() const { return MakeShared<FAggregateOrInclusiveLayerInstance>(*this); };
			virtual ELayerType GetLayerType() const override final { return ELayerType::AggregateOrInclusive; }
			virtual uint32 GetInputSize() const override final { return TotalSubLayerInputSize + SubLayers.Num(); }
			virtual uint32 GetOutputSize() const override final { return AttentionHeadNum * OutputEncodingSize + SubLayers.Num(); }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, (uint32)SubLayers.Num());
				Serialization::Size(InOutOffset, OutputEncodingSize);
				Serialization::Size(InOutOffset, AttentionEncodingSize);
				Serialization::Size(InOutOffset, AttentionHeadNum);
				Serialization::Size(InOutOffset, SubLayerInputSizes);
				Serialization::Size(InOutOffset, SubLayerOutputSizes);
				Serialization::Size(InOutOffset, SubLayers);
				Serialization::Size(InOutOffset, QueryLayers);
				Serialization::Size(InOutOffset, KeyLayers);
				Serialization::Size(InOutOffset, ValueLayers);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				uint32 SubLayerNum = 0;
				Serialization::Load(InOutOffset, SubLayerNum, Data);
				Serialization::Load(InOutOffset, OutputEncodingSize, Data);
				Serialization::Load(InOutOffset, AttentionEncodingSize, Data);
				Serialization::Load(InOutOffset, AttentionHeadNum, Data);
				Serialization::Load(InOutOffset, SubLayerInputSizes, Data, SubLayerNum);
				Serialization::Load(InOutOffset, SubLayerOutputSizes, Data, SubLayerNum);
				SubLayers.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, SubLayers, Data);
				QueryLayers.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, QueryLayers, Data);
				KeyLayers.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, KeyLayers, Data);
				ValueLayers.Init(nullptr, SubLayerNum);
				Serialization::Load(InOutOffset, ValueLayers, Data);
				PostLoad();
			}

			void PostLoad()
			{
				const uint32 SubLayerNum = SubLayers.Num();
				TotalSubLayerInputSize = 0;
				SubLayerInputOffsets.SetNumUninitialized(SubLayerNum);
				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
				{
					SubLayerInputOffsets[SubLayerIdx] = TotalSubLayerInputSize;
					TotalSubLayerInputSize += SubLayerInputSizes[SubLayerIdx];
				}
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, (uint32)SubLayers.Num(), Data);
				Serialization::Save(InOutOffset, OutputEncodingSize, Data);
				Serialization::Save(InOutOffset, AttentionEncodingSize, Data);
				Serialization::Save(InOutOffset, AttentionHeadNum, Data);
				Serialization::Save(InOutOffset, SubLayerInputSizes, Data);
				Serialization::Save(InOutOffset, SubLayerOutputSizes, Data);
				Serialization::Save(InOutOffset, SubLayers, Data);
				Serialization::Save(InOutOffset, QueryLayers, Data);
				Serialization::Save(InOutOffset, KeyLayers, Data);
				Serialization::Save(InOutOffset, ValueLayers, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateOrInclusiveLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());

				FAggregateOrInclusiveLayerInstance* AggregateOrInclusiveInstance = StaticCast<FAggregateOrInclusiveLayerInstance*>(Instance);
				check(AggregateOrInclusiveInstance);

				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				const uint32 SubLayerNum = SubLayers.Num();

				// Count the number of sub-layer used by each item in the batch

				OperatorAggregateCountElementNum(
					AggregateOrInclusiveInstance->TotalElementNum,
					AggregateOrInclusiveInstance->ElementNums.GetData(),
					AggregateOrInclusiveInstance->ElementOffsets.GetData(),
					InputBuffer + TotalSubLayerInputSize,
					BatchSize,
					SubLayerNum,
					InputBufferStride);

				// Gather the batch indices used by each sub-layer

				OperatorGatherSubLayerBatchIndicesInclusive(
					AggregateOrInclusiveInstance->SubLayerBatchIndices,
					InputBuffer + TotalSubLayerInputSize,
					BatchSize,
					SubLayerNum,
					InputBufferStride);

				// Evaluate Each Sublayer on the associated batch items

				for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
				{
					const uint32 SubLayerBatchSize = AggregateOrInclusiveInstance->SubLayerBatchIndices[SubLayerIdx].Num();

					if (SubLayerBatchSize == 0) { continue; }

					OperatorGather(
						AggregateOrInclusiveInstance->SubLayerInputBuffers[SubLayerIdx].GetData(),
						InputBuffer + SubLayerInputOffsets[SubLayerIdx],
						AggregateOrInclusiveInstance->SubLayerBatchIndices[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						SubLayerInputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx],
						InputBufferStride);

					SubLayers[SubLayerIdx]->Evaluate(
						AggregateOrInclusiveInstance->SubLayerInstances[SubLayerIdx].Get(),
						AggregateOrInclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						AggregateOrInclusiveInstance->SubLayerInputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						SubLayerOutputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx],
						SubLayerOutputSizes[SubLayerIdx],
						SubLayerInputSizes[SubLayerIdx]);

					QueryLayers[SubLayerIdx]->Evaluate(
						AggregateOrInclusiveInstance->QueryInstances[SubLayerIdx].Get(),
						AggregateOrInclusiveInstance->SubLayerQueryBuffers[SubLayerIdx].GetData(),
						AggregateOrInclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						AttentionHeadNum * AttentionEncodingSize,
						SubLayerOutputSizes[SubLayerIdx],
						AttentionHeadNum * AttentionEncodingSize,
						SubLayerOutputSizes[SubLayerIdx]);

					KeyLayers[SubLayerIdx]->Evaluate(
						AggregateOrInclusiveInstance->KeyInstances[SubLayerIdx].Get(),
						AggregateOrInclusiveInstance->SubLayerKeyBuffers[SubLayerIdx].GetData(),
						AggregateOrInclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						AttentionHeadNum * AttentionEncodingSize,
						SubLayerOutputSizes[SubLayerIdx],
						AttentionHeadNum * AttentionEncodingSize,
						SubLayerOutputSizes[SubLayerIdx]);

					ValueLayers[SubLayerIdx]->Evaluate(
						AggregateOrInclusiveInstance->ValueInstances[SubLayerIdx].Get(),
						AggregateOrInclusiveInstance->SubLayerValueBuffers[SubLayerIdx].GetData(),
						AggregateOrInclusiveInstance->SubLayerOutputBuffers[SubLayerIdx].GetData(),
						SubLayerBatchSize,
						AttentionHeadNum * OutputEncodingSize,
						SubLayerOutputSizes[SubLayerIdx],
						AttentionHeadNum * OutputEncodingSize,
						SubLayerOutputSizes[SubLayerIdx]);
				}

				// Gather queries, keys, and values from sub-layers into tightly packed element lists
				// which we can attend over using the ElementNums and ElementOffsets arrays

				OperatorAggregateGatherFromSubLayers(
					AggregateOrInclusiveInstance->QueryBuffer.GetData(),
					AggregateOrInclusiveInstance->KeyBuffer.GetData(),
					AggregateOrInclusiveInstance->ValueBuffer.GetData(),
					AggregateOrInclusiveInstance->ElementAccum.GetData(),
					AggregateOrInclusiveInstance->ElementNums.GetData(),
					AggregateOrInclusiveInstance->ElementOffsets.GetData(),
					AggregateOrInclusiveInstance->SubLayerBatchIndices,
					AggregateOrInclusiveInstance->SubLayerQueryBuffers,
					AggregateOrInclusiveInstance->SubLayerKeyBuffers,
					AggregateOrInclusiveInstance->SubLayerValueBuffers,
					BatchSize,
					AttentionHeadNum * AttentionEncodingSize,
					AttentionHeadNum * AttentionEncodingSize,
					AttentionHeadNum * OutputEncodingSize);

				// Compute Attention

				OperatorAggregateDotProductAttention(
					AggregateOrInclusiveInstance->AttentionBuffer.GetData(),
					AggregateOrInclusiveInstance->QueryBuffer.GetData(),
					AggregateOrInclusiveInstance->KeyBuffer.GetData(),
					AggregateOrInclusiveInstance->TotalElementNum,
					AttentionEncodingSize,
					AttentionHeadNum);

				OperatorAggregateSoftmaxPlusOneInplace(
					AggregateOrInclusiveInstance->AttentionMaxsBuffer.GetData(),
					AggregateOrInclusiveInstance->AttentionDenomsBuffer.GetData(),
					AggregateOrInclusiveInstance->AttentionBuffer.GetData(),
					AggregateOrInclusiveInstance->ElementNums.GetData(),
					AggregateOrInclusiveInstance->ElementOffsets.GetData(),
					BatchSize,
					AttentionHeadNum);

				OperatorAggregateAttentionSum(
					OutputBuffer,
					AggregateOrInclusiveInstance->AttentionBuffer.GetData(),
					AggregateOrInclusiveInstance->ValueBuffer.GetData(),
					AggregateOrInclusiveInstance->ElementNums.GetData(),
					AggregateOrInclusiveInstance->ElementOffsets.GetData(),
					BatchSize,
					OutputEncodingSize,
					AttentionHeadNum,
					OutputBufferStride);

				// Append Element Mask

				OperatorCopy(
					OutputBuffer + AttentionHeadNum * OutputEncodingSize,
					InputBuffer + TotalSubLayerInputSize,
					BatchSize,
					SubLayerNum,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 OutputEncodingSize = 0;
			uint32 AttentionEncodingSize = 0;
			uint32 AttentionHeadNum = 0;
			TConstArrayView<uint32> SubLayerInputSizes;
			TConstArrayView<uint32> SubLayerOutputSizes;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> SubLayers;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> QueryLayers;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> KeyLayers;
			TArray<TSharedPtr<ILayer>, TInlineAllocator<32>> ValueLayers;

			uint32 TotalSubLayerInputSize = 0;
			TArray<uint32, TInlineAllocator<32>> SubLayerInputOffsets;
		};

		FAggregateOrInclusiveLayerInstance::FAggregateOrInclusiveLayerInstance(const FAggregateOrInclusiveLayer& InAggregateOrInclusiveLayer)
			: AggregateOrInclusiveLayer(InAggregateOrInclusiveLayer)
		{
			const uint32 SubLayerNum = AggregateOrInclusiveLayer.SubLayers.Num();

			SubLayerInstances.Init(nullptr, SubLayerNum);
			QueryInstances.Init(nullptr, SubLayerNum);
			KeyInstances.Init(nullptr, SubLayerNum);
			ValueInstances.Init(nullptr, SubLayerNum);

			SubLayerBatchIndices.SetNum(SubLayerNum);
			SubLayerInputBuffers.SetNum(SubLayerNum);
			SubLayerOutputBuffers.SetNum(SubLayerNum);
			SubLayerQueryBuffers.SetNum(SubLayerNum);
			SubLayerKeyBuffers.SetNum(SubLayerNum);
			SubLayerValueBuffers.SetNum(SubLayerNum);

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
			{
				SubLayerInstances[SubLayerIdx] = AggregateOrInclusiveLayer.SubLayers[SubLayerIdx]->MakeInstance();
				QueryInstances[SubLayerIdx] = AggregateOrInclusiveLayer.QueryLayers[SubLayerIdx]->MakeInstance();
				KeyInstances[SubLayerIdx] = AggregateOrInclusiveLayer.KeyLayers[SubLayerIdx]->MakeInstance();
				ValueInstances[SubLayerIdx] = AggregateOrInclusiveLayer.ValueLayers[SubLayerIdx]->MakeInstance();
			}
		}

		void FAggregateOrInclusiveLayerInstance::SetMaxBatchSize(const uint32 MaxBatchSize)
		{
			NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FAggregateOrInclusiveLayerInstance::SetMaxBatchSize);

			const uint32 SubLayerNum = AggregateOrInclusiveLayer.SubLayers.Num();

			for (uint32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
			{
				if (SubLayerInstances[SubLayerIdx]) { SubLayerInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }
				if (QueryInstances[SubLayerIdx]) { QueryInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }
				if (KeyInstances[SubLayerIdx]) { KeyInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }
				if (ValueInstances[SubLayerIdx]) { ValueInstances[SubLayerIdx]->SetMaxBatchSize(MaxBatchSize); }

				SubLayerBatchIndices[SubLayerIdx].Empty(MaxBatchSize);
				SubLayerInputBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.SubLayerInputSizes[SubLayerIdx], EAllowShrinking::No);
				SubLayerOutputBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.SubLayerOutputSizes[SubLayerIdx], EAllowShrinking::No);
				SubLayerQueryBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.AttentionEncodingSize, EAllowShrinking::No);
				SubLayerKeyBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.AttentionEncodingSize, EAllowShrinking::No);
				SubLayerValueBuffers[SubLayerIdx].SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.OutputEncodingSize, EAllowShrinking::No);
			}

			TotalElementNum = 0;
			ElementAccum.SetNumUninitialized(MaxBatchSize);
			ElementNums.SetNumUninitialized(MaxBatchSize);
			ElementOffsets.SetNumUninitialized(MaxBatchSize);

			AttentionMaxsBuffer.SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.AttentionHeadNum, EAllowShrinking::No);
			AttentionDenomsBuffer.SetNumUninitialized(MaxBatchSize * AggregateOrInclusiveLayer.AttentionHeadNum, EAllowShrinking::No);
			AttentionBuffer.SetNumUninitialized(MaxBatchSize * SubLayerNum * AggregateOrInclusiveLayer.AttentionHeadNum, EAllowShrinking::No);
			QueryBuffer.SetNumUninitialized(MaxBatchSize * SubLayerNum * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.AttentionEncodingSize);
			KeyBuffer.SetNumUninitialized(MaxBatchSize * SubLayerNum * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.AttentionEncodingSize);
			ValueBuffer.SetNumUninitialized(MaxBatchSize * SubLayerNum * AggregateOrInclusiveLayer.AttentionHeadNum * AggregateOrInclusiveLayer.OutputEncodingSize);
		}


		//--------------------------------------------------------------------------

		struct FClampLayer : public ILayer
		{
			virtual ELayerType GetLayerType() const override final { return ELayerType::Clamp; }
			virtual uint32 GetInputSize() const override final { return InputOutputSize; }
			virtual uint32 GetOutputSize() const override final { return InputOutputSize; }

			virtual void SerializationSize(uint64& InOutOffset) const override final
			{
				Serialization::Size(InOutOffset, InputOutputSize);
				Serialization::Size(InOutOffset, MinValues);
				Serialization::Size(InOutOffset, MaxValues);
			}

			virtual void SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data) override final
			{
				Serialization::Load(InOutOffset, InputOutputSize, Data);
				Serialization::Load(InOutOffset, MinValues, Data, InputOutputSize);
				Serialization::Load(InOutOffset, MaxValues, Data, InputOutputSize);
			}

			virtual void SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const override final
			{
				Serialization::Save(InOutOffset, InputOutputSize, Data);
				Serialization::Save(InOutOffset, MinValues, Data);
				Serialization::Save(InOutOffset, MaxValues, Data);
			}

			virtual void Evaluate(
				ILayerInstance* Instance,
				float* OutputBuffer,
				const float* InputBuffer,
				const uint32 BatchSize,
				const uint32 OutputBufferSize,
				const uint32 InputBufferSize,
				const uint32 OutputBufferStride,
				const uint32 InputBufferStride) override final
			{
				NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::Private::FClampLayer::Evaluate);
				check(OutputBufferSize == GetOutputSize() && InputBufferSize == GetInputSize());
				check(Instance == nullptr);
				OperatorNanCheck(InputBuffer, BatchSize, InputBufferSize, InputBufferStride);

				OperatorClamp(
					OutputBuffer,
					InputBuffer,
					MinValues.GetData(),
					MaxValues.GetData(),
					BatchSize,
					InputOutputSize,
					OutputBufferStride,
					InputBufferStride);

				OperatorNanCheck(OutputBuffer, BatchSize, OutputBufferSize, OutputBufferStride);
			}

			uint32 InputOutputSize = 0;
			TConstArrayView<float> MinValues;
			TConstArrayView<float> MaxValues;
		};

		//--------------------------------------------------------------------------
		// Layer Serialization
		//--------------------------------------------------------------------------

		namespace Serialization
		{
			static inline void Size(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer)
			{
				Serialization::Size(InOutOffset, (uint32)InLayer->GetLayerType());
				InLayer->SerializationSize(InOutOffset);
			}

			static inline void Size(uint64& InOutOffset, const TConstArrayView<TSharedPtr<ILayer>> InLayers)
			{
				for (const TSharedPtr<ILayer>& Layer : InLayers) { Serialization::Size(InOutOffset, Layer); }
			}

			static inline void Load(uint64& InOutOffset, TSharedPtr<ILayer>& OutLayer, TConstArrayView<uint8> Data)
			{
				uint32 LayerTypeId = (uint32)ELayerType::Invalid;
				Serialization::Load(InOutOffset, LayerTypeId, Data);

				checkf((ELayerType)LayerTypeId != ELayerType::Invalid, TEXT("Invalid Layer"));

				if (OutLayer == nullptr || OutLayer->GetLayerType() != (ELayerType)LayerTypeId)
				{
					switch ((ELayerType)LayerTypeId)
					{
					case ELayerType::Sequence: OutLayer = MakeShared<FSequenceLayer>(); break;
					case ELayerType::Normalize: OutLayer = MakeShared<FNormalizeLayer>(); break;
					case ELayerType::Denormalize: OutLayer = MakeShared<FDenormalizeLayer>(); break;
					case ELayerType::Linear: OutLayer = MakeShared<FLinearLayer>(); break;
					case ELayerType::CompressedLinear: OutLayer = MakeShared<FCompressedLinearLayer>(); break;
					case ELayerType::MultiLinear: OutLayer = MakeShared<FMultiLinearLayer>(); break;
					case ELayerType::ReLU: OutLayer = MakeShared<FReLULayer>(); break;
					case ELayerType::ELU: OutLayer = MakeShared<FELULayer>(); break;
					case ELayerType::TanH: OutLayer = MakeShared<FTanHLayer>(); break;
					case ELayerType::PReLU: OutLayer = MakeShared<FPReLULayer>(); break;
					case ELayerType::MemoryCell: OutLayer = MakeShared<FMemoryCellLayer>(); break;
					case ELayerType::Copy: OutLayer = MakeShared<FCopyLayer>(); break;
					case ELayerType::Concat: OutLayer = MakeShared<FConcatLayer>(); break;
					case ELayerType::Array: OutLayer = MakeShared<FArrayLayer>(); break;
					case ELayerType::AggregateSet: OutLayer = MakeShared<FAggregateSetLayer>(); break;
					case ELayerType::AggregateOrExclusive: OutLayer = MakeShared<FAggregateOrExclusiveLayer>(); break;
					case ELayerType::AggregateOrInclusive: OutLayer = MakeShared<FAggregateOrInclusiveLayer>(); break;
					case ELayerType::Clamp: OutLayer = MakeShared<FClampLayer>(); break;
					default: checkf(false, TEXT("Unknown Layer Id %i"), LayerTypeId);
					}
				}

				OutLayer->SerializationLoad(InOutOffset, Data);
			}

			static inline void Load(uint64& InOutOffset, TArrayView<TSharedPtr<ILayer>> OutLayers, TConstArrayView<uint8> Data)
			{
				for (TSharedPtr<ILayer>& Layer : OutLayers) { Serialization::Load(InOutOffset, Layer, Data); }
			}

			static inline void Save(uint64& InOutOffset, const TSharedPtr<ILayer>& InLayer, TArrayView<uint8> Data)
			{
				Serialization::Save(InOutOffset, (uint32)InLayer->GetLayerType(), Data);
				InLayer->SerializationSave(InOutOffset, Data);
			}

			static inline void Save(uint64& InOutOffset, const TConstArrayView<TSharedPtr<ILayer>> InLayers, TArrayView<uint8> Data)
			{
				for (const TSharedPtr<ILayer>& Layer : InLayers) { Serialization::Save(InOutOffset, Layer, Data); }
			}
		}
	}

	//--------------------------------------------------------------------------
	// NNE Interface Implementation
	//--------------------------------------------------------------------------

	FModelInstanceCPU::FModelInstanceCPU(const TSharedPtr<FModelCPU>& InModel)
		: Model(InModel)
		, InputTensorDesc(FTensorDesc::Make(TEXT("Input"), FSymbolicTensorShape::Make({ -1, -1 }), ENNETensorDataType::Float))
		, OutputTensorDesc(FTensorDesc::Make(TEXT("Output"), FSymbolicTensorShape::Make({ -1, -1 }), ENNETensorDataType::Float))
		, Instance(Model->Layer->MakeInstance())
	{}

	FModelInstanceCPU::ESetInputTensorShapesStatus FModelInstanceCPU::SetInputTensorShapes(TConstArrayView<FTensorShape> InInputShapes)
	{
		NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::FModelInstanceCPU::SetInputTensorShapes);

		if (!ensureMsgf(InInputShapes.Num() == 1, TEXT("Basic CPU Inference only supports single input tensor.")))
		{
			return ESetInputTensorShapesStatus::Fail;
		}

		const FTensorShape& InputShape = InInputShapes[0];

		if (!ensureMsgf(InputShape.Rank() == 2, TEXT("Basic CPU Inference only supports rank 2 input tensors.")))
		{
			return ESetInputTensorShapesStatus::Fail;
		}

		const uint32 InputBatchSize = InputShape.GetData()[0];
		const uint32 InputInputSize = InputShape.GetData()[1];
		const uint32 ModelInputSize = Model->Layer->GetInputSize();
		const uint32 ModelOutputSize = Model->Layer->GetOutputSize();

		if (!ensureMsgf(InputInputSize == ModelInputSize, TEXT("Input tensor shape does not match model input size. Got %i, expected %i."), InputInputSize, ModelInputSize))
		{
			return ESetInputTensorShapesStatus::Fail;
		}

		BatchSize = InputBatchSize;
		InputSize = ModelInputSize;
		OutputSize = ModelOutputSize;

		InputTensorShape = FTensorShape::Make({ BatchSize, InputSize });
		OutputTensorShape = FTensorShape::Make({ BatchSize, OutputSize });

		if (Instance)
		{
			Instance->SetMaxBatchSize(BatchSize);
		}

		return ESetInputTensorShapesStatus::Ok;
	}

	FModelInstanceCPU::ERunSyncStatus FModelInstanceCPU::RunSync(TConstArrayView<FTensorBindingCPU> InInputBindings, TConstArrayView<FTensorBindingCPU> InOutputBindings)
	{
		NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::FModelInstanceCPU::RunSync);

		if (!ensureMsgf(BatchSize > 0, TEXT("SetInputTensorShapes must be run before RunSync")))
		{
			return ERunSyncStatus::Fail;
		}

		if (!ensureMsgf(InInputBindings.Num() == 1, TEXT("Basic CPU Inference only supports single input tensor.")))
		{
			return ERunSyncStatus::Fail;
		}

		if (!ensureMsgf(InOutputBindings.Num() == 1, TEXT("Basic CPU Inference only supports single output tensor.")))
		{
			return ERunSyncStatus::Fail;
		}

		if (!ensureMsgf(InInputBindings[0].SizeInBytes == BatchSize * InputSize * sizeof(float), TEXT("Incorrect Input Tensor Size")))
		{
			return ERunSyncStatus::Fail;
		}

		if (!ensureMsgf(InOutputBindings[0].SizeInBytes == BatchSize * OutputSize * sizeof(float), TEXT("Incorrect Output Tensor Size")))
		{
			return ERunSyncStatus::Fail;
		}

		Model->Layer->Evaluate(
			Instance.Get(),
			(float*)InOutputBindings[0].Data,
			(const float*)InInputBindings[0].Data,
			BatchSize,
			OutputSize,
			InputSize,
			OutputSize,
			InputSize);

		return ERunSyncStatus::Ok;
	}

	uint32 FModelCPU::ModelMagicNumber = 0x0BA51C01;
	uint32 FModelCPU::ModelVersionNumber = 1;

	TSharedPtr<IModelInstanceCPU> FModelCPU::CreateModelInstanceCPU()
	{
		return MakeShared<FModelInstanceCPU>(WeakThis.Pin());
	}

	void FModelCPU::SerializationSize(uint64& InOutOffset) const
	{
		checkf(InOutOffset % 64 == 0,
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		Private::Serialization::Size(InOutOffset, ModelMagicNumber);
		Private::Serialization::Size(InOutOffset, ModelVersionNumber);
		Private::Serialization::Size(InOutOffset, Layer);
	}

	bool FModelCPU::SerializationLoad(uint64& InOutOffset, TConstArrayView<uint8> Data)
	{
		NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::FModelCPU::SerializationLoad);

		checkf(InOutOffset % 64 == 0, 
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		uint32 Magic = INDEX_NONE;
		Private::Serialization::Load(InOutOffset, Magic, Data);
		if (Magic != ModelMagicNumber)
		{
			UE_LOG(LogNNE, Error, TEXT("Invalid Magic Number %i"), Magic);
			return false;
		}

		uint32 Version = INDEX_NONE;
		Private::Serialization::Load(InOutOffset, Version, Data);
		if (Version != ModelVersionNumber)
		{
			UE_LOG(LogNNE, Error, TEXT("Unsupported Version Number %i"), Version);
			return false;
		}

		Private::Serialization::Load(InOutOffset, Layer, Data);

		return true;
	}

	void FModelCPU::SerializationSave(uint64& InOutOffset, TArrayView<uint8> Data) const
	{
		NNE_RUNTIME_BASIC_TRACE_SCOPE(NNE::RuntimeBasic::FModelCPU::SerializationSave);

		checkf(InOutOffset % 64 == 0,
			TEXT("Model must be aligned to 64 bytes because there must be no padding before magic number."));

		Private::Serialization::Save(InOutOffset, ModelMagicNumber, Data);
		Private::Serialization::Save(InOutOffset, ModelVersionNumber, Data);
		Private::Serialization::Save(InOutOffset, Layer, Data);
	}

	//--------------------------------------------------------------------------
	// Builder
	//--------------------------------------------------------------------------

	namespace Private
	{
		static inline float UniformToGaussian(const float R1, const float R2)
		{
			return FMath::Sqrt(-2.0f * FMath::Loge(FMath::Max(R1, UE_SMALL_NUMBER))) * FMath::Cos(R2 * UE_TWO_PI);
		}
	}

	FModelBuilder::FModelBuilder(int32 Seed) : Rng(Seed) {};

	FModelBuilderElement::FModelBuilderElement() = default;
	FModelBuilderElement::FModelBuilderElement(const TSharedPtr<Private::ILayer>& Ptr) : Layer(Ptr) {}
	FModelBuilderElement::~FModelBuilderElement() = default;

	int32 FModelBuilderElement::GetInputSize() const
	{
		check(Layer);
		return Layer->GetInputSize();
	}

	int32 FModelBuilderElement::GetOutputSize() const
	{
		check(Layer);
		return Layer->GetOutputSize();
	}

	FModelBuilderElement FModelBuilder::MakeLinear(
		const uint32 InputSize,
		const uint32 OutputSize,
		const TConstArrayView<float> Weights,
		const TConstArrayView<float> Biases)
	{
		check(Biases.Num() == OutputSize);
		check(Weights.Num() == InputSize * OutputSize);

		const TSharedPtr<Private::FLinearLayer> LinearLayer = MakeShared<Private::FLinearLayer>();
		LinearLayer->InputSize = InputSize;
		LinearLayer->OutputSize = OutputSize;
		LinearLayer->Biases = Biases;
		LinearLayer->Weights = Weights;

		return StaticCastSharedPtr<Private::ILayer>(LinearLayer);
	}

	FModelBuilderElement FModelBuilder::MakeLinearWithRandomKaimingWeights(
		const uint32 InputSize,
		const uint32 OutputSize,
		const float WeightScale)
	{
		return MakeLinear(
			InputSize,
			OutputSize,
			MakeWeightsRandomKaiming(InputSize, OutputSize, WeightScale),
			MakeWeightsZero(OutputSize));
	}

	FModelBuilderElement FModelBuilder::MakeMultiLinear(
		const uint32 InputSize, 
		const uint32 OutputSize, 
		const uint32 BlockNum, 
		const TConstArrayView<float> Weights, 
		const TConstArrayView<float> Biases)
	{
		check(Biases.Num() == OutputSize * BlockNum);
		check(Weights.Num() == InputSize * OutputSize * BlockNum);

		const TSharedPtr<Private::FMultiLinearLayer> MultiLinearLayer = MakeShared<Private::FMultiLinearLayer>();
		MultiLinearLayer->InputSize = InputSize;
		MultiLinearLayer->OutputSize = OutputSize;
		MultiLinearLayer->BlockNum = BlockNum;
		MultiLinearLayer->Biases = Biases;
		MultiLinearLayer->Weights = Weights;

		return StaticCastSharedPtr<Private::ILayer>(MultiLinearLayer);
	}

	FModelBuilderElement FModelBuilder::MakeNormalize(
		const uint32 InputOutputSize,
		const TConstArrayView<float> Mean,
		const TConstArrayView<float> Std)
	{
		check(Mean.Num() == InputOutputSize);
		check(Std.Num() == InputOutputSize);

		const TSharedPtr<Private::FNormalizeLayer> NormalizeLayer = MakeShared<Private::FNormalizeLayer>();
		NormalizeLayer->InputOutputSize = InputOutputSize;
		NormalizeLayer->Mean = Mean;
		NormalizeLayer->Std = Std;

		return StaticCastSharedPtr<Private::ILayer>(NormalizeLayer);
	}

	FModelBuilderElement FModelBuilder::MakeDenormalize(
		const uint32 InputOutputSize,
		const TConstArrayView<float> Mean,
		const TConstArrayView<float> Std)
	{
		check(Mean.Num() == InputOutputSize);
		check(Std.Num() == InputOutputSize);

		const TSharedPtr<Private::FDenormalizeLayer> DenormalizeLayer = MakeShared<Private::FDenormalizeLayer>();
		DenormalizeLayer->InputOutputSize = InputOutputSize;
		DenormalizeLayer->Mean = Mean;
		DenormalizeLayer->Std = Std;

		return StaticCastSharedPtr<Private::ILayer>(DenormalizeLayer);
	}

	FModelBuilderElement FModelBuilder::MakeReLU(const uint32 InputOutputSize)
	{
		const TSharedPtr<Private::FReLULayer> Layer = MakeShared<Private::FReLULayer>();
		Layer->InputOutputSize = InputOutputSize;
		return StaticCastSharedPtr<Private::ILayer>(Layer);
	}

	FModelBuilderElement FModelBuilder::MakeELU(const uint32 InputOutputSize)
	{
		const TSharedPtr<Private::FELULayer> Layer = MakeShared<Private::FELULayer>();
		Layer->InputOutputSize = InputOutputSize;
		return StaticCastSharedPtr<Private::ILayer>(Layer);
	}

	FModelBuilderElement FModelBuilder::MakeTanH(const uint32 InputOutputSize)
	{
		const TSharedPtr<Private::FTanHLayer> Layer = MakeShared<Private::FTanHLayer>();
		Layer->InputOutputSize = InputOutputSize;
		return StaticCastSharedPtr<Private::ILayer>(Layer);
	}

	FModelBuilderElement FModelBuilder::MakeCopy(const uint32 InputOutputSize)
	{
		const TSharedPtr<Private::FCopyLayer> Layer = MakeShared<Private::FCopyLayer>();
		Layer->InputOutputSize = InputOutputSize;
		return StaticCastSharedPtr<Private::ILayer>(Layer);
	}

	FModelBuilderElement FModelBuilder::MakeClamp(const uint32 InputOutputSize, const TConstArrayView<float> MinValues, const TConstArrayView<float> MaxValues)
	{
		check(MinValues.Num() == InputOutputSize);
		check(MaxValues.Num() == InputOutputSize);

		const TSharedPtr<Private::FClampLayer> Layer = MakeShared<Private::FClampLayer>();
		Layer->InputOutputSize = InputOutputSize;
		Layer->MinValues = MinValues;
		Layer->MaxValues = MaxValues;
		return StaticCastSharedPtr<Private::ILayer>(Layer);
	}

	FModelBuilderElement FModelBuilder::MakeActivation(const uint32 InputOutputSize, const EActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case EActivationFunction::ReLU: return MakeReLU(InputOutputSize);
		case EActivationFunction::ELU: return MakeELU(InputOutputSize);
		case EActivationFunction::TanH: return MakeTanH(InputOutputSize);
		default:
			checkf(false, TEXT("Unknown Activation Function"));
			return MakeReLU(InputOutputSize);
		}
	}

	FModelBuilderElement FModelBuilder::MakePReLU(const uint32 InputOutputSize, TConstArrayView<float> Alpha)
	{
		check(Alpha.Num() == InputOutputSize);

		const TSharedPtr<Private::FPReLULayer> ActivationLayer = MakeShared<Private::FPReLULayer>();
		ActivationLayer->InputOutputSize = InputOutputSize;
		ActivationLayer->Alpha = Alpha;

		return StaticCastSharedPtr<Private::ILayer>(ActivationLayer);
	}

	FModelBuilderElement FModelBuilder::MakeSequence(const TConstArrayView<FModelBuilderElement> Elements)
	{
		const TSharedPtr<Private::FSequenceLayer> SequenceLayer = MakeShared<Private::FSequenceLayer>();
		SequenceLayer->Layers.Reserve(Elements.Num());

		for (const FModelBuilderElement& Element : Elements)
		{
			SequenceLayer->Layers.Emplace(Element.Layer);
		}

		for (int32 LayerIdx = 1; LayerIdx < Elements.Num(); LayerIdx++)
		{
			const int32 PrevLayerOutputSize = SequenceLayer->Layers[LayerIdx - 1]->GetOutputSize();
			const int32 NextLayerInputSize = SequenceLayer->Layers[LayerIdx - 0]->GetInputSize();
			checkf(PrevLayerOutputSize == NextLayerInputSize, TEXT("Sequence Layer Dimensions don't match. Output %i vs Input %i."), PrevLayerOutputSize, NextLayerInputSize);
		}

		return StaticCastSharedPtr<Private::ILayer>(SequenceLayer);
	}

	FModelBuilderElement FModelBuilder::MakeMLPWithRandomKaimingWeights(
		const uint32 InputSize,
		const uint32 OutputSize,
		const uint32 HiddenSize,
		const uint32 LayerNum,
		const EActivationFunction ActivationFunction,
		const bool bActivationOnFinalLayer)
	{
		check(LayerNum >= 2);

		TArray<FModelBuilderElement, TInlineAllocator<32>> Layers;
		Layers.Reserve(2 * LayerNum - (bActivationOnFinalLayer ? 0 : 1));

		for (uint32 LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
		{
			const uint32 LayerInputSize = LayerIdx == 0 ? InputSize : HiddenSize;
			const uint32 LayerOuputSize = LayerIdx == LayerNum - 1 ? OutputSize : HiddenSize;

			Layers.Emplace(MakeLinearWithRandomKaimingWeights(LayerInputSize, LayerOuputSize));
			
			if (bActivationOnFinalLayer || LayerIdx != LayerNum - 1)
			{
				Layers.Emplace(MakeActivation(LayerOuputSize, ActivationFunction));
			}
		}
		
		return MakeSequence(Layers);
	}

	FModelBuilderElement FModelBuilder::MakeMemoryCell(
		const uint32 InputNum,
		const uint32 OutputNum,
		const uint32 MemoryNum,
		const FModelBuilderElement& RememberLayer,
		const FModelBuilderElement& PassthroughLayer,
		const FModelBuilderElement& MemoryUpdateLayer,
		const FModelBuilderElement& OutputInputUpdateLayer,
		const FModelBuilderElement& OutputMemoryUpdateLayer)
	{
		check(RememberLayer.GetInputSize() == InputNum + MemoryNum);
		check(RememberLayer.GetOutputSize() == MemoryNum);
		check(PassthroughLayer.GetInputSize() == InputNum + MemoryNum);
		check(PassthroughLayer.GetOutputSize() == OutputNum);
		check(MemoryUpdateLayer.GetInputSize() == InputNum + MemoryNum);
		check(MemoryUpdateLayer.GetOutputSize() == MemoryNum);
		check(OutputInputUpdateLayer.GetInputSize() == InputNum + MemoryNum);
		check(OutputInputUpdateLayer.GetOutputSize() == OutputNum);
		check(OutputMemoryUpdateLayer.GetInputSize() == MemoryNum);
		check(OutputMemoryUpdateLayer.GetOutputSize() == OutputNum);

		TSharedPtr<Private::FMemoryCellLayer> CellLayer = MakeShared<Private::FMemoryCellLayer>();
		CellLayer->InputSize = InputNum;
		CellLayer->OutputSize = OutputNum;
		CellLayer->MemorySize = MemoryNum;
		CellLayer->RememberLayer = RememberLayer.Layer;
		CellLayer->PassthroughLayer = PassthroughLayer.Layer;
		CellLayer->MemoryUpdateLayer = MemoryUpdateLayer.Layer;
		CellLayer->OutputInputUpdateLayer = OutputInputUpdateLayer.Layer;
		CellLayer->OutputMemoryUpdateLayer = OutputMemoryUpdateLayer.Layer;
		
		return StaticCastSharedPtr<Private::ILayer>(CellLayer);
	}

	FModelBuilderElement FModelBuilder::MakeMemoryCellWithLinearRandomKaimingWeights(
		const uint32 InputNum,
		const uint32 OutputNum,
		const uint32 MemoryNum,
		const float WeightScale)
	{
		return MakeMemoryCell(
			InputNum,
			OutputNum,
			MemoryNum,
			MakeLinearWithRandomKaimingWeights(InputNum + MemoryNum, MemoryNum, WeightScale),
			MakeLinearWithRandomKaimingWeights(InputNum + MemoryNum, OutputNum, WeightScale),
			MakeLinearWithRandomKaimingWeights(InputNum + MemoryNum, MemoryNum, WeightScale),
			MakeLinearWithRandomKaimingWeights(InputNum + MemoryNum, OutputNum, WeightScale),
			MakeLinearWithRandomKaimingWeights(MemoryNum, OutputNum, WeightScale));
	}

	FModelBuilderElement FModelBuilder::MakeMemoryBackbone(
		const uint32 MemoryNum,
		const FModelBuilderElement& Prefix,
		const FModelBuilderElement& Cell,
		const FModelBuilderElement& Postfix)
	{
		check(Prefix.GetOutputSize() == Cell.GetInputSize() - MemoryNum);
		check(Postfix.GetInputSize() == Cell.GetOutputSize() - MemoryNum);

		return MakeSequence({
			MakeConcat({
				Prefix,
				MakeCopy(MemoryNum)
				}),
			Cell,
			MakeConcat({
				Postfix,
				MakeCopy(MemoryNum)
			})
		});
	}

	FModelBuilderElement FModelBuilder::MakeConcat(const TConstArrayView<FModelBuilderElement> Elements)
	{
		const int32 LayerNum = Elements.Num();

		const TSharedPtr<Private::FConcatLayer> ConcatLayer = MakeShared<Private::FConcatLayer>();
		ConcatLayer->InputSizes = MakeSizesLayerInputs(Elements);
		ConcatLayer->OutputSizes = MakeSizesLayerOutputs(Elements);
		ConcatLayer->Layers.Reserve(LayerNum);

		for (const FModelBuilderElement& Element : Elements)
		{
			ConcatLayer->Layers.Emplace(Element.Layer);
		}

		ConcatLayer->PostLoad();

		return StaticCastSharedPtr<Private::ILayer>(ConcatLayer);
	}

	FModelBuilderElement FModelBuilder::MakeArray(const uint32 ElementNum, const FModelBuilderElement& SubLayer)
	{
		const TSharedPtr<Private::FArrayLayer> ArrayLayer = MakeShared<Private::FArrayLayer>();
		ArrayLayer->ElementNum = ElementNum;
		ArrayLayer->ElementInputSize = SubLayer.GetInputSize();
		ArrayLayer->ElementOutputSize = SubLayer.GetOutputSize();
		ArrayLayer->SubLayer = SubLayer.Layer;

		return StaticCastSharedPtr<Private::ILayer>(ArrayLayer);
	}

	FModelBuilderElement FModelBuilder::MakeAggregateSet(
		const uint32 MaxElementNum,
		const uint32 OutputEncodingSize,
		const uint32 AttentionEncodingSize,
		const uint32 AttentionHeadNum,
		const FModelBuilderElement& SubLayer,
		const FModelBuilderElement& QueryLayer,
		const FModelBuilderElement& KeyLayer,
		const FModelBuilderElement& ValueLayer)
	{
		check(SubLayer.GetOutputSize() == QueryLayer.GetInputSize());
		check(SubLayer.GetOutputSize() == KeyLayer.GetInputSize());
		check(SubLayer.GetOutputSize() == ValueLayer.GetInputSize());
		check(QueryLayer.GetOutputSize() == AttentionHeadNum * AttentionEncodingSize);
		check(KeyLayer.GetOutputSize() == AttentionHeadNum * AttentionEncodingSize);
		check(ValueLayer.GetOutputSize() == AttentionHeadNum * OutputEncodingSize);

		const TSharedPtr<Private::FAggregateSetLayer> SetLayer = MakeShared<Private::FAggregateSetLayer>();
		SetLayer->MaxElementNum = MaxElementNum;
		SetLayer->ElementInputSize = SubLayer.GetInputSize();
		SetLayer->ElementOutputSize = SubLayer.GetOutputSize();
		SetLayer->OutputEncodingSize = OutputEncodingSize;
		SetLayer->AttentionEncodingSize = AttentionEncodingSize;
		SetLayer->AttentionHeadNum = AttentionHeadNum;
		SetLayer->SubLayer = SubLayer.Layer;
		SetLayer->QueryLayer = QueryLayer.Layer;
		SetLayer->KeyLayer = KeyLayer.Layer;
		SetLayer->ValueLayer = ValueLayer.Layer;

		return StaticCastSharedPtr<Private::ILayer>(SetLayer);
	}

	FModelBuilderElement FModelBuilder::MakeAggregateOrExclusive(
		const uint32 OutputEncodingSize,
		const TConstArrayView<FModelBuilderElement> SubLayers,
		const TConstArrayView<FModelBuilderElement> Encoders)
	{
		check(SubLayers.Num() == Encoders.Num());

		const int32 SubLayerNum = SubLayers.Num();
		for (int32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
		{
			check(SubLayers[SubLayerIdx].GetOutputSize() == Encoders[SubLayerIdx].GetInputSize());
			check(Encoders[SubLayerIdx].GetOutputSize() == OutputEncodingSize);
		}

		const TSharedPtr<Private::FAggregateOrExclusiveLayer> OrExclusiveLayer = MakeShared<Private::FAggregateOrExclusiveLayer>();
		OrExclusiveLayer->OutputEncodingSize = OutputEncodingSize;
		OrExclusiveLayer->SubLayerInputSizes = MakeSizesLayerInputs(SubLayers);
		OrExclusiveLayer->SubLayerOutputSizes = MakeSizesLayerOutputs(SubLayers);
		OrExclusiveLayer->SubLayers.Reserve(SubLayerNum);
		OrExclusiveLayer->Encoders.Reserve(SubLayerNum);

		for (int32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
		{
			OrExclusiveLayer->SubLayers.Add(SubLayers[SubLayerIdx].Layer);
			OrExclusiveLayer->Encoders.Add(Encoders[SubLayerIdx].Layer);
		}

		OrExclusiveLayer->PostLoad();

		return StaticCastSharedPtr<Private::ILayer>(OrExclusiveLayer);
	}

	FModelBuilderElement FModelBuilder::MakeAggregateOrInclusive(
		const uint32 OutputEncodingSize,
		const uint32 AttentionEncodingSize,
		const uint32 AttentionHeadNum,
		const TConstArrayView<FModelBuilderElement> SubLayers,
		const TConstArrayView<FModelBuilderElement> QueryLayers,
		const TConstArrayView<FModelBuilderElement> KeyLayers,
		const TConstArrayView<FModelBuilderElement> ValueLayers)
	{
		check(SubLayers.Num() == QueryLayers.Num());
		check(SubLayers.Num() == KeyLayers.Num());
		check(SubLayers.Num() == ValueLayers.Num());

		const int32 SubLayerNum = SubLayers.Num();
		for (int32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
		{
			check(SubLayers[SubLayerIdx].GetOutputSize() == QueryLayers[SubLayerIdx].GetInputSize());
			check(SubLayers[SubLayerIdx].GetOutputSize() == KeyLayers[SubLayerIdx].GetInputSize());
			check(SubLayers[SubLayerIdx].GetOutputSize() == ValueLayers[SubLayerIdx].GetInputSize());
			check(QueryLayers[SubLayerIdx].GetOutputSize() == AttentionHeadNum * AttentionEncodingSize);
			check(KeyLayers[SubLayerIdx].GetOutputSize() == AttentionHeadNum * AttentionEncodingSize);
			check(ValueLayers[SubLayerIdx].GetOutputSize() == AttentionHeadNum * OutputEncodingSize);
		}

		const TSharedPtr<Private::FAggregateOrInclusiveLayer> OrInclusiveLayer = MakeShared<Private::FAggregateOrInclusiveLayer>();
		OrInclusiveLayer->OutputEncodingSize = OutputEncodingSize;
		OrInclusiveLayer->AttentionEncodingSize = AttentionEncodingSize;
		OrInclusiveLayer->AttentionHeadNum = AttentionHeadNum;
		OrInclusiveLayer->SubLayerInputSizes = MakeSizesLayerInputs(SubLayers);
		OrInclusiveLayer->SubLayerOutputSizes = MakeSizesLayerOutputs(SubLayers);
		OrInclusiveLayer->SubLayers.Reserve(SubLayerNum);
		OrInclusiveLayer->QueryLayers.Reserve(SubLayerNum);
		OrInclusiveLayer->KeyLayers.Reserve(SubLayerNum);
		OrInclusiveLayer->ValueLayers.Reserve(SubLayerNum);

		for (int32 SubLayerIdx = 0; SubLayerIdx < SubLayerNum; SubLayerIdx++)
		{
			OrInclusiveLayer->SubLayers.Add(SubLayers[SubLayerIdx].Layer);
			OrInclusiveLayer->QueryLayers.Add(QueryLayers[SubLayerIdx].Layer);
			OrInclusiveLayer->KeyLayers.Add(KeyLayers[SubLayerIdx].Layer);
			OrInclusiveLayer->ValueLayers.Add(ValueLayers[SubLayerIdx].Layer);
		}

		OrInclusiveLayer->PostLoad();

		return StaticCastSharedPtr<Private::ILayer>(OrInclusiveLayer);
	}

	void FModelBuilder::Reset()
	{
		Rng.Reset();
		WeightsPool.Empty();
		SizesPool.Empty();
	}

	uint64 FModelBuilder::GetWriteByteNum(const FModelBuilderElement& Element) const
	{
		FModelCPU Model;
		Model.Layer = Element.Layer;

		uint64 Offset = 0;
		Model.SerializationSize(Offset);
		return Offset;
	}

	void FModelBuilder::WriteFileData(TArrayView<uint8> OutBytes, uint32& OutInputSize, uint32& OutOutputSize, const FModelBuilderElement& Element) const
	{
		check((uint64)OutBytes.Num() == GetWriteByteNum(Element));

		OutInputSize = Element.GetInputSize();
		OutOutputSize = Element.GetOutputSize();

		// Zero to ensure any padding due to alignment is always zero
		FMemory::Memzero(OutBytes.GetData(), OutBytes.Num());

		FModelCPU Model;
		Model.Layer = Element.Layer;

		uint64 Offset = 0;
		Model.SerializationSave(Offset, OutBytes);
		check(Offset == OutBytes.Num());
	}

	void FModelBuilder::WriteFileData(TArray<uint8>& FileData, uint32& OutInputSize, uint32& OutOutputSize, const FModelBuilderElement& Element) const
	{
		FileData.SetNumUninitialized(GetWriteByteNum(Element));
		WriteFileData(MakeArrayView(FileData), OutInputSize, OutOutputSize, Element);
	}

	void FModelBuilder::WriteFileDataAndReset(TArrayView<uint8> FileData, uint32& OutInputSize, uint32& OutOutputSize, const FModelBuilderElement& Element)
	{
		WriteFileData(FileData, OutInputSize, OutOutputSize, Element);
		Reset();
	}

	void FModelBuilder::WriteFileDataAndReset(TArray<uint8>& FileData, uint32& OutInputSize, uint32& OutOutputSize, const FModelBuilderElement& Element)
	{
		FileData.SetNumUninitialized(GetWriteByteNum(Element));
		WriteFileDataAndReset(MakeArrayView(FileData), OutInputSize, OutOutputSize, Element);
	}

	TArrayView<float> FModelBuilder::MakeWeightsCopy(const TConstArrayView<float> Weights)
	{
		TArray<float>& Values = WeightsPool.AddDefaulted_GetRef();
		Values = Weights;
		return Values;
	}

	TArrayView<float> FModelBuilder::MakeWeightsZero(const uint32 Size)
	{
		TArray<float>& Values = WeightsPool.AddDefaulted_GetRef();
		Values.Init(0.0f, Size);
		return Values;
	}

	TArrayView<float> FModelBuilder::MakeWeightsConstant(const uint32 Size, const float Value)
	{
		TArray<float>& Values = WeightsPool.AddDefaulted_GetRef();
		Values.Init(Value, Size);
		return Values;
	}

	TArrayView<float> FModelBuilder::MakeWeightsRandomKaiming(const uint32 InputSize, const uint32 OutputSize, const float Scale)
	{
		TArray<float>& Values = WeightsPool.AddDefaulted_GetRef();
		Values.SetNumUninitialized(InputSize * OutputSize);
		
		const float Std = Scale * FMath::Sqrt(2.0f / InputSize);

		for (uint32 Idx = 0; Idx < InputSize * OutputSize; Idx++)
		{
			Values[Idx] = Std * Private::UniformToGaussian(Rng.FRand(), Rng.FRand());
		}
		
		return Values;
	}

	TArrayView<uint32> FModelBuilder::MakeSizesZero(const uint32 Size)
	{
		TArray<uint32>& Values = SizesPool.AddDefaulted_GetRef();
		Values.Init(0, Size);
		return Values;
	}

	TArrayView<uint32> FModelBuilder::MakeSizesLayerInputs(const TConstArrayView<FModelBuilderElement> Elements)
	{
		const uint32 SizeNum = Elements.Num();
		TArrayView<uint32> Sizes = MakeSizesZero(SizeNum);
		for (uint32 SizeIdx = 0; SizeIdx < SizeNum; SizeIdx++)
		{
			Sizes[SizeIdx] = Elements[SizeIdx].GetInputSize();
		}

		return Sizes;
	}

	TArrayView<uint32> FModelBuilder::MakeSizesLayerOutputs(const TConstArrayView<FModelBuilderElement> Elements)
	{
		const uint32 SizeNum = Elements.Num();
		TArrayView<uint32> Sizes = MakeSizesZero(SizeNum);
		for (uint32 SizeIdx = 0; SizeIdx < SizeNum; SizeIdx++)
		{
			Sizes[SizeIdx] = Elements[SizeIdx].GetOutputSize();
		}

		return Sizes;
	}

	//--------------------------------------------------------------------------

} // namespace UE::NNE::RuntimeBasic

#undef NNE_RUNTIME_BASIC_TRACE_SCOPE
#undef NNE_RUNTIME_BASIC_ENABLE_PROFILE
#undef NNE_RUNTIME_BASIC_ENABLE_NAN_CHECK
#undef NNE_RUNTIME_BASIC_ENABLE_ISPC