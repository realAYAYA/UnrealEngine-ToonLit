// Copyright Epic Games, Inc. All Rights Reserved.

inline float NNERuntimeBasicCPUTanH(const float X, const float Min = -10.0f, const float Max = 10.0f)
{
	const float Exp2X = exp(2.0f * clamp(X, Min, Max));
	return (Exp2X - 1) / (Exp2X + 1);
}

inline float NNERuntimeBasicCPUSigmoid(const float X)
{
	return 1.0f / (1.0f + exp(-X));
}

export void NNERuntimeBasicCPUOperatorNormalize(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Mean[],
	uniform const float Std[],
	uniform const int BatchNum,
	uniform const int InputOutputNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... InputOutputNum)
	{
		Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] - Mean[Idx]) / Std[Idx];
	}
}

export void NNERuntimeBasicCPUOperatorDenormalize(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Mean[],
	uniform const float Std[],
	uniform const int BatchNum,
	uniform const int InputOutputNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... InputOutputNum)
	{
		Output[BatchIdx * OutputStride + Idx] = (Input[BatchIdx * InputStride + Idx] * Std[Idx]) + Mean[Idx];
	}
}

export void NNERuntimeBasicCPUOperatorClamp(
	uniform float Output[],
	uniform const float Input[],
	uniform const float MinValues[],
	uniform const float MaxValues[],
	uniform const int BatchNum,
	uniform const int InputOutputNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... InputOutputNum)
	{
		Output[BatchIdx * OutputStride + Idx] = clamp(Input[BatchIdx * InputStride + Idx], MinValues[Idx], MaxValues[Idx]);
	}
}

export void NNERuntimeBasicCPUOperatorLinear(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Weights[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int ColNum,
	uniform const int RowNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
		{
			uniform const float Value = Input[BatchIdx * InputStride + RowIdx];

			if (Value != 0.0)
			{
				foreach(ColIdx = 0 ... ColNum)
				{
					Output[BatchIdx * OutputStride + ColIdx] += Value * Weights[RowIdx * ColNum + ColIdx];
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUOperatorCompressedLinear(
	uniform float Output[],
	uniform const float Input[],
	uniform const uint16 Weights[],
	uniform const float WeightOffsets[],
	uniform const float WeightScales[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int ColNum,
	uniform const int RowNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputStride + ColIdx] = Biases[ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
		{
			uniform const float Value = Input[BatchIdx * InputStride + RowIdx];

			if (Value != 0.0)
			{
				uniform const float Offset = WeightOffsets[RowIdx];
				uniform const float Scale = WeightScales[RowIdx];

				foreach(ColIdx = 0 ... ColNum)
				{
					Output[BatchIdx * OutputStride + ColIdx] += Value * ((Scale * ((float)Weights[RowIdx * ColNum + ColIdx])) + Offset);
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUOperatorMultiLinear(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Weights[],
	uniform const float Biases[],
	uniform const int BatchNum,
	uniform const int BlockNum,
	uniform const int ColNum,
	uniform const int RowNum,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, BlockIdx = 0 ... BlockNum, ColIdx = 0 ... ColNum)
	{
		Output[BatchIdx * OutputStride + BlockIdx * ColNum + ColIdx] = Biases[BlockIdx * ColNum + ColIdx];
	}

	for (uniform int BatchIdx = 0; BatchIdx < BatchNum; BatchIdx++)
	{
		for (uniform int BlockIdx = 0; BlockIdx < BlockNum; BlockIdx++)
		{
			for (uniform int RowIdx = 0; RowIdx < RowNum; RowIdx++)
			{
				uniform const float Value = Input[BatchIdx * InputStride + BlockIdx * RowNum + RowIdx];

				if (Value != 0.0)
				{
					foreach (ColIdx = 0 ... ColNum)
					{
						Output[BatchIdx * OutputStride + BlockIdx * ColNum + ColIdx] += Value * Weights[BlockIdx * RowNum * ColNum + RowIdx * ColNum + ColIdx];
					}
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUOperatorCopy(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputStride + Idx] = Input[BatchIdx * InputStride + Idx];
	}
}

export void NNERuntimeBasicCPUOperatorReLU(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputStride + Idx] = max(Input[BatchIdx * InputStride + Idx], 0.0f);
	}
}

export void NNERuntimeBasicCPUOperatorELU(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		const float Value = Input[BatchIdx * InputStride + Idx];
		Output[BatchIdx * OutputStride + Idx] = select(Value > 0.0f, Value, exp(Value) - 1.0f);
	}
}

export void NNERuntimeBasicCPUOperatorTanH(
	uniform float Output[],
	uniform const float Input[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		Output[BatchIdx * OutputStride + Idx] = NNERuntimeBasicCPUTanH(Input[BatchIdx * InputStride + Idx]);
	}
}

export void NNERuntimeBasicCPUOperatorPReLU(
	uniform float Output[],
	uniform const float Input[],
	uniform const float Alpha[],
	uniform const int BatchNum,
	uniform const int Num,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	foreach(BatchIdx = 0 ... BatchNum, Idx = 0 ... Num)
	{
		const float Value = Input[BatchIdx * InputStride + Idx];
		Output[BatchIdx * OutputStride + Idx] = select(Value > 0.0f, Value, Alpha[Idx] * Value);
	}
}

export void NNERuntimeBasicCPUOperatorMemoryCellUpdateMemory(
	uniform float Output[],
	uniform const float RememberGate[],
	uniform const float Memory[],
	uniform const float Update[],
	uniform const int BatchSize,
	uniform const int MemorySize,
	uniform const int OutputStride,
	uniform const int RememberGateStride,
	uniform const int MemoryStride,
	uniform const int UpdateStride)
{
	foreach(BatchIdx = 0 ... BatchSize, Idx = 0 ... MemorySize)
	{
		const float Gate = NNERuntimeBasicCPUSigmoid(RememberGate[BatchIdx * RememberGateStride + Idx]);
		const float Prev = Memory[BatchIdx * MemoryStride + Idx];
		const float Targ = NNERuntimeBasicCPUTanH(Update[BatchIdx * UpdateStride + Idx]);

		Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * Prev + Gate * Targ;
	}
}

export void NNERuntimeBasicCPUOperatorMemoryCellUpdateOutput(
	uniform float Output[],
	uniform const float PassthroughGate[],
	uniform const float MemoryUpdate[],
	uniform const float InputUpdate[],
	uniform const int BatchSize,
	uniform const int OutputSize,
	uniform const int OutputStride,
	uniform const int PassthroughGateStride,
	uniform const int MemoryUpdateStride,
	uniform const int InputUpdateStride)
{
	foreach(BatchIdx = 0 ... BatchSize, Idx = 0 ... OutputSize)
	{
		const float Gate = NNERuntimeBasicCPUSigmoid(PassthroughGate[BatchIdx * PassthroughGateStride + Idx]);
		const float MemTarg = NNERuntimeBasicCPUTanH(MemoryUpdate[BatchIdx * MemoryUpdateStride + Idx]);
		const float InTarg = NNERuntimeBasicCPUTanH(InputUpdate[BatchIdx * InputUpdateStride + Idx]);

		Output[BatchIdx * OutputStride + Idx] = (1.0f - Gate) * MemTarg + Gate * InTarg;
	}
}

export void NNERuntimeBasicCPUOperatorAggregateGatherElements(
	uniform float OutputBuffer[],
	uniform const float InputBuffer[],
	uniform const uint32 ElementNums[],
	uniform const uint32 ElementOffsets[],
	uniform const int BatchSize,
	uniform const int ElementSize,
	uniform const int InputStride)
{
	for (uniform int BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
	{
		uniform const uint32 ElementNum = ElementNums[BatchIdx];
		uniform const uint32 ElementOffset = ElementOffsets[BatchIdx];

		foreach (ElementIdx = 0 ... ElementNum, Idx = 0 ... ElementSize)
		{
			OutputBuffer[(ElementOffset + ElementIdx) * ElementSize + Idx] = InputBuffer[BatchIdx * InputStride + ElementIdx * ElementSize + Idx];
		}
	}
}

export void NNERuntimeBasicCPUOperatorAggregateInsertOneHot(
	uniform float QueryBuffer[],
	uniform const int Index,
	uniform const int BatchSize,
	uniform const int MaskSize,
	uniform const int QueryBufferStride)
{
	for (uniform int BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
	{
		foreach (MaskIdx = 0 ... MaskSize)
		{
			QueryBuffer[BatchIdx * QueryBufferStride + MaskIdx] = 0.0f;
		}

		QueryBuffer[BatchIdx * QueryBufferStride + Index] = 1.0f;
	}
}

export void NNERuntimeBasicCPUOperatorAggregateCountElementNum(
	uniform uint32& TotalElementNum,
	uniform uint32 ElementNums[],
	uniform uint32 ElementOffsets[],
	uniform const float MaskBuffer[],
	uniform const int BatchSize,
	uniform const int MaskSize,
	uniform const int MaskBufferStride)
{
	TotalElementNum = 0;

	for (uniform int BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
	{
		uint32 ElementSumAccum = 0;

		foreach (MaskIdx = 0 ... MaskSize)
		{
			if (MaskBuffer[BatchIdx * MaskBufferStride + MaskIdx]) { ElementSumAccum++; }
		}

		uniform uint32 ElementSum = reduce_add(ElementSumAccum);

		ElementOffsets[BatchIdx] = TotalElementNum;
		ElementNums[BatchIdx] = ElementSum;
		TotalElementNum += ElementSum;
	}
}

export void NNERuntimeBasicCPUOperatorAggregateDotProductAttention(
	uniform float Attention[],
	uniform const float Queries[],
	uniform const float Keys[],
	uniform const int ElementNum,
	uniform const int AttentionEncodingSize,
	uniform const int AttentionHeadNum)
{
	for (uniform int ElementIdx = 0; ElementIdx < ElementNum; ElementIdx++)
	{
		for (uniform int HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
		{
			float AttentionAccum = 0.0f;

			foreach (Idx = 0 ... AttentionEncodingSize)
			{
				AttentionAccum += (
					Keys[ElementIdx * AttentionHeadNum * AttentionEncodingSize + HeadIdx * AttentionEncodingSize + Idx] *
					Queries[ElementIdx * AttentionHeadNum * AttentionEncodingSize + HeadIdx * AttentionEncodingSize + Idx]);
			}

			Attention[ElementIdx * AttentionHeadNum + HeadIdx] = reduce_add(AttentionAccum) / sqrt((uniform float)AttentionEncodingSize);
		}
	}
}

export void NNERuntimeBasicCPUOperatorAggregateSoftmaxPlusOneInplace(
	uniform float AttentionMaxs[],
	uniform float AttentionDenoms[],
	uniform float Attention[],
	uniform const uint32 ElementNums[],
	uniform const uint32 ElementOffsets[],
	uniform const int BatchSize,
	uniform const int AttentionHeadNum)
{
	for (uniform int BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
	{
		uniform const uint32 ElementNum = ElementNums[BatchIdx];
		uniform const uint32 ElementOffset = ElementOffsets[BatchIdx];

		foreach (HeadIdx = 0 ... AttentionHeadNum)
		{
			AttentionMaxs[HeadIdx] = 0.0f;
			AttentionDenoms[HeadIdx] = 0.0f;
		}

		for (uniform uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
		{
			foreach (HeadIdx = 0 ... AttentionHeadNum)
			{
				AttentionMaxs[HeadIdx] = max(AttentionMaxs[HeadIdx], Attention[ElementIdx * AttentionHeadNum + HeadIdx]);
			}
		}

		for (uniform uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
		{
			foreach (HeadIdx = 0 ... AttentionHeadNum)
			{
				AttentionDenoms[HeadIdx] += exp(Attention[ElementIdx * AttentionHeadNum + HeadIdx] - AttentionMaxs[HeadIdx]);
			}
		}

		foreach (ElementIdx = ElementOffset ... ElementOffset + ElementNum, HeadIdx = 0 ... AttentionHeadNum)
		{
			Attention[ElementIdx * AttentionHeadNum + HeadIdx] = exp(Attention[ElementIdx * AttentionHeadNum + HeadIdx] - AttentionMaxs[HeadIdx]) / (AttentionDenoms[HeadIdx] + 1.0f);
		}
	}
}

export void NNERuntimeBasicCPUOperatorAggregateAttentionSum(
	uniform float Output[],
	uniform const float Attention[],
	uniform const float Values[],
	uniform const uint32 ElementNums[],
	uniform const uint32 ElementOffsets[],
	uniform const int BatchSize,
	uniform const int EncodingSize,
	uniform const int AttentionHeadNum,
	uniform const int OutputStride)
{
	for (uniform int BatchIdx = 0; BatchIdx < BatchSize; BatchIdx++)
	{
		foreach (HeadIdx = 0 ... AttentionHeadNum, Idx = 0 ... EncodingSize)
		{
			Output[BatchIdx * OutputStride + HeadIdx * EncodingSize + Idx] = 0.0f;
		}

		uniform const uint32 ElementNum = ElementNums[BatchIdx];
		uniform const uint32 ElementOffset = ElementOffsets[BatchIdx];

		for (uniform uint32 ElementIdx = ElementOffset; ElementIdx < ElementOffset + ElementNum; ElementIdx++)
		{
			for (uniform int HeadIdx = 0; HeadIdx < AttentionHeadNum; HeadIdx++)
			{
				uniform const float Scale = Attention[ElementIdx * AttentionHeadNum + HeadIdx];

				if (Scale != 0.0f)
				{
					foreach(Idx = 0 ... EncodingSize)
					{
						Output[BatchIdx * OutputStride + HeadIdx * EncodingSize + Idx] += Scale * Values[ElementIdx * AttentionHeadNum * EncodingSize + HeadIdx * EncodingSize + Idx];
					}
				}
			}
		}
	}
}

export void NNERuntimeBasicCPUOperatorAggregateGatherQueryValueFromSubLayers(
	uniform float QueryBuffer[],
	uniform float KeyBuffer[],
	uniform float ValueBuffer[],
	uniform uint32 ElementAccum[],
	uniform const uint32 ElementOffsets[],
	uniform const float SubLayerQueryBuffer[],
	uniform const float SubLayerKeyBuffer[],
	uniform const float SubLayerValueBuffer[],
	uniform const uint32 SubLayerBatchIndicesBuffer[],
	uniform const int SubLayerBatchIndexNum,
	uniform const int QuerySize,
	uniform const int KeySize,
	uniform const int ValueSize)
{
	for (uniform int ElementIdx = 0; ElementIdx < SubLayerBatchIndexNum; ElementIdx++)
	{
		uniform const uint32 BatchIdx = SubLayerBatchIndicesBuffer[ElementIdx];
		uniform const uint32 ElementOffset = ElementOffsets[BatchIdx] + ElementAccum[BatchIdx];

		foreach (QueryIdx = 0 ... QuerySize)
		{
			QueryBuffer[ElementOffset * QuerySize + QueryIdx] = SubLayerQueryBuffer[ElementIdx * QuerySize + QueryIdx];
		}

		foreach(KeyIdx = 0 ... KeySize)
		{
			KeyBuffer[ElementOffset * KeySize + KeyIdx] = SubLayerKeyBuffer[ElementIdx * KeySize + KeyIdx];
		}

		foreach(ValueIdx = 0 ... ValueSize)
		{
			ValueBuffer[ElementOffset * ValueSize + ValueIdx] = SubLayerValueBuffer[ElementIdx * ValueSize + ValueIdx];
		}

		ElementAccum[BatchIdx]++;
	}
}

export void NNERuntimeBasicCPUOperatorGather(
	uniform float OutputBuffer[],
	uniform const float InputBuffer[],
	uniform const uint32 BatchIndices[],
	uniform const int BatchIndexNum,
	uniform const int InputOutputSize,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	for (uniform int BatchIndexIdx = 0; BatchIndexIdx < BatchIndexNum; BatchIndexIdx++)
	{
		uniform const uint32 SrcBatchIdx = BatchIndices[BatchIndexIdx];

		foreach (Idx = 0 ... InputOutputSize)
		{
			OutputBuffer[BatchIndexIdx * OutputStride + Idx] = InputBuffer[SrcBatchIdx * InputStride + Idx];
		}
	}
}

export void NNERuntimeBasicCPUOperatorScatter(
	uniform float OutputBuffer[],
	uniform const float InputBuffer[],
	uniform const uint32 BatchIndices[],
	uniform const int BatchIndexNum,
	uniform const int InputOutputSize,
	uniform const int OutputStride,
	uniform const int InputStride)
{
	for (uniform int BatchIndexIdx = 0; BatchIndexIdx < BatchIndexNum; BatchIndexIdx++)
	{
		uniform const uint32 DstBatchIdx = BatchIndices[BatchIndexIdx];

		foreach (Idx = 0 ... InputOutputSize)
		{
			OutputBuffer[DstBatchIdx * OutputStride + Idx] = InputBuffer[BatchIndexIdx * InputStride + Idx];
		}
	}
}