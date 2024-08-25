// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef MAX_flt
#define MAX_flt			(3.402823466e+38F)
#endif

#ifndef INDEX_NONE
#define INDEX_NONE		(-1)
#endif

#ifndef UE_SMALL_NUMBER
#define UE_SMALL_NUMBER (1.e-8f)
#endif

inline float LearningSigmoid(const float X)
{
	return 1.0f / (1.0f + exp(-X));
}

/////////////////////////////////////////////////////////
// 
// Functions matching those in LearningRandom.cpp
// 
/////////////////////////////////////////////////////////

inline float LearningRandomUniformToGaussian(const float R1, const float R2)
{
	return sqrt(-2.0f * log(max(R1, 1e-8f))) * cos(R2 * 6.28318530717f);
}

inline uint32 LearningRandomInt(const uint32 State)
{
	uint32 X = State ^ 0xb74eaecf;
	X = ((X >> 16) ^ X) * 0x45d9f3b;
	X = ((X >> 16) ^ X) * 0x45d9f3b;
	return (X >> 16) ^ X;
}

inline uniform uint32 LearningRandomInt(uniform const uint32 State)
{
	uniform uint32 X = State ^ 0xb74eaecf;
	X = ((X >> 16) ^ X) * 0x45d9f3b;
	X = ((X >> 16) ^ X) * 0x45d9f3b;
	return (X >> 16) ^ X;
}

inline float LearningRandomFloat(const uint32 State)
{
	return floatbits(0x3F800000U | (LearningRandomInt(State ^ 0x1c89a74a) >> 9)) - 1.0f;
}

inline uniform float LearningRandomFloat(uniform const uint32 State)
{
	return floatbits(0x3F800000U | (LearningRandomInt(State ^ 0x1c89a74a) >> 9)) - 1.0f;
}

inline int32 LearningRandomIntInRange(const uint32 State, const int32 Min, const int32 Max)
{
	const int32 Range = (Max - Min) + 1;
	return Min + ((Range > 0) ? (int32)(trunc(LearningRandomFloat(State ^ 0x7d3b208a) * (float)(Range))) : 0);
}

inline uniform int32 LearningRandomIntInRange(uniform const uint32 State, uniform const int32 Min, uniform const int32 Max)
{
	uniform const int32 Range = (Max - Min) + 1;
	return Min + ((Range > 0) ? (uniform int32)(LearningRandomFloat(State ^ 0x7d3b208a) * (uniform float)(Range)) : 0);
}

inline float LearningRandomUniform(
	const uint32 State,
	const float Min = 0.0f,
	const float Max = 1.0f)
{
	return (Max - Min) * LearningRandomFloat(State ^ 0x72404541) + Min;
}

inline float LearningRandomGaussian(
	const uint32 State,
	const float Mean = 0.0f,
	const float Std = 1.0f)
{
	return Std * LearningRandomUniformToGaussian(
		LearningRandomFloat(State ^ 0x4855e88f),
		LearningRandomFloat(State ^ 0x0eedb850)) + Mean;
}

export void LearningRandomIntArray(
	uniform uint32 Output[],
	uniform const int DimNum,
	uniform const uint32 State)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		Output[DimIdx] = LearningRandomInt(State ^ 0xbed25b30 ^ LearningRandomInt(DimIdx ^ 0xb521a8d9));
	}
}

export void LearningRandomFloatArray(
	uniform float Output[],
	uniform const int DimNum,
	uniform const uint32 State)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		Output[DimIdx] = LearningRandomFloat(State ^ 0xf955fac7 ^ LearningRandomInt(DimIdx ^ 0xcd989d6f));
	}
}

export void LearningRandomUniformArray(
	uniform float Output[],
	uniform const int DimNum,
	uniform const uint32 State,
	uniform const float Min,
	uniform const float Max)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		Output[DimIdx] = LearningRandomUniform(State ^ 0x5f15554c ^ LearningRandomInt(DimIdx ^ 0x242735e0), Min, Max);
	}
}

export void LearningRandomGaussianArray(
	uniform float Output[],
	uniform const int DimNum,
	uniform const uint32 State,
	uniform const float Mean,
	uniform const float Std)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		Output[DimIdx] = LearningRandomGaussian(State ^ 0x7b5d5f62 ^ LearningRandomInt(DimIdx ^ 0x546ab965), Mean, Std);
	}
}

export void LearningRandomDistributionIndependantNormal(
	uniform float Output[],
	uniform const float Mean[],
	uniform const float LogStd[],
	uniform const int DimNum,
	uniform const uint32 State,
	uniform const float Scale)
{
	if (Scale < UE_SMALL_NUMBER)
	{
		foreach(DimIdx = 0 ... DimNum)
		{
			Output[DimIdx] = Mean[DimIdx];
		}
	}
	else
	{
		foreach(DimIdx = 0 ... DimNum)
		{
			Output[DimIdx] = LearningRandomGaussian(State ^ 0x7db0e4e9 ^ LearningRandomInt(DimIdx ^ 0xf4976a00), Mean[DimIdx], Scale * exp(min(LogStd[DimIdx], 10.0f)));
		}
	}
}

export void LearningRandomDistributionBernoulli(
	uniform float Output[],
	uniform const float Logits[],
	uniform const int DimNum,
	uniform const uint32 State,
	uniform const float Scale)
{
	if (Scale < UE_SMALL_NUMBER)
	{
		foreach(DimIdx = 0 ... DimNum)
		{
			Output[DimIdx] = select(Logits[DimIdx] > 0.0f, 1.0f, 0.0f);
		}
	}
	else
	{
		foreach(DimIdx = 0 ... DimNum)
		{
			Output[DimIdx] = select(LearningSigmoid(Logits[DimIdx] / Scale) > LearningRandomUniform(State ^ 0xf4021a46 ^ LearningRandomInt(DimIdx ^ 0x7a8cc64e)), 1.0f, 0.0f);
		}
	}
}

/////////////////////////////////////////////////////////
// 
// Functions used by various different Optimizers
// 
/////////////////////////////////////////////////////////

export void LearningAdjustCMAMean(
	uniform float InOutMean[],
	uniform float InOutOldMean[],
	uniform const float Weights[],
	uniform const int LossRanking[],
	uniform const float Samples[],
	uniform const int Mu,
	uniform const int DimNum)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		InOutOldMean[DimIdx] = InOutMean[DimIdx];
		InOutMean[DimIdx] = 0.0f;
	}

	for (uniform int LossIdx = 0; LossIdx < Mu; LossIdx++)
	{
		const float LossWeight = Weights[LossIdx];
		const int ControlIdx = LossRanking[LossIdx];

		foreach (DimIdx = 0 ... DimNum)
		{
			InOutMean[DimIdx] += LossWeight * Samples[ControlIdx * DimNum + DimIdx];
		}
	}
}

export void LearningUpdateCMACovariance(
	uniform float InOutCovariance[],
	uniform const float PathCovariance[],
	uniform const float Scale,
	uniform const float C1,
	uniform const int DimNum)
{
	foreach (DimX = 0 ... DimNum, DimY = 0 ... DimNum)
	{
		InOutCovariance[DimX * DimNum + DimY] = Scale * InOutCovariance[DimX * DimNum + DimY] + C1 * PathCovariance[DimX] * PathCovariance[DimY];
	}
}

export void LearningComputeCMAUpdateDirection(
	uniform float OutUpdateDirection[],
	uniform const float Mean[],
	uniform const float OldMean[],
	uniform const float CovarianceInverseSqrt[],
	uniform const int DimNum)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		OutUpdateDirection[DimIdx] = 0.0f;
	}
	
	foreach (RowIdx = 0 ... DimNum, ColIdx = 0 ... DimNum)
	{
		OutUpdateDirection[ColIdx] += (Mean[RowIdx] - OldMean[RowIdx]) * CovarianceInverseSqrt[RowIdx * DimNum + ColIdx];
	}
}

export void LearningTransformCMASamples(
	uniform float Output[],
	const uniform float Samples[],
	const uniform float Mean[],
	const uniform float Covariance[],
	const uniform int SampleNum,
	const uniform int DimNum,
	const uniform float Sigma)
{
	foreach (SampleIdx = 0 ... SampleNum, ColIdx = 0 ... DimNum)
	{
		Output[SampleIdx * DimNum + ColIdx] = Mean[ColIdx];
	}
	
	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		for (uniform int RowIdx = 0; RowIdx < DimNum; RowIdx++)
		{
			uniform const float Value = Sigma * Samples[SampleIdx * DimNum + RowIdx];

			foreach (ColIdx = 0 ... DimNum)
			{
				Output[SampleIdx * DimNum + ColIdx] += Value * Covariance[RowIdx * DimNum + ColIdx];
			}
		}
	}
}

export void LearningUpdateAdamEstimate(
	uniform int32 InOutIterations[],
	uniform float InOutEstimate[],
	uniform float InOutM0[],
	uniform float InOutM1[],
	uniform float InOutM1HatMax[],
	uniform const float Gradient[],
	uniform const float LearningRate,
	uniform const float Beta1,
	uniform const float Beta2,
	uniform const int DimNum)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		InOutIterations[DimIdx]++;

		InOutM0[DimIdx] = Beta1 * InOutM0[DimIdx] + (1.0f - Beta1) * Gradient[DimIdx];
		InOutM1[DimIdx] = Beta2 * InOutM1[DimIdx] + (1.0f - Beta2) * (Gradient[DimIdx] * Gradient[DimIdx]);

		const float M0Hat = InOutM0[DimIdx] / (1.0f - pow(Beta1, InOutIterations[DimIdx]));
		const float M1Hat = InOutM1[DimIdx] / (1.0f - pow(Beta2, InOutIterations[DimIdx]));

		InOutM1HatMax[DimIdx] = max(InOutM1HatMax[DimIdx], M1Hat);
		InOutEstimate[DimIdx] -= (LearningRate * M0Hat) / (sqrt(InOutM1HatMax[DimIdx]) + 1e-8f);
	}
}

export void LearningEstimateGradient(
	uniform float OutGradient[],
	uniform const float Samples[],
	uniform const float Losses[],
	uniform const int SampleNum,
	uniform const int DimNum)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		OutGradient[DimIdx] = 0.0f;
	}

	for (uniform int SampleIdx = 1; SampleIdx < SampleNum; SampleIdx++)
	{
		float Length = 0.0f;
		foreach (DimIdx = 0 ... DimNum)
		{
			const float Difference = Samples[SampleIdx * DimNum + DimIdx] - Samples[0 * DimNum + DimIdx];
			Length += Difference * Difference;
		}
		uniform const float SampleLength = max(sqrt(reduce_add(Length)), 1e-8f);

		uniform const float LossGradient = (Losses[SampleIdx] - Losses[0]) / SampleLength;

		foreach (DimIdx = 0 ... DimNum)
		{
			const float ControlGradient = (Samples[SampleIdx * DimNum + DimIdx] - Samples[0 * DimNum + DimIdx]) / SampleLength;

			OutGradient[DimIdx] += (DimNum * LossGradient * ControlGradient) / (SampleNum - 1);
		}
	}
}

export void LearningSampleAdamOptimizer(
	uniform float OutSamples[],
	uniform const float GaussianSamples[],
	uniform const float Estimate[],
	uniform const float FiniteDifferenceStd,
	uniform const int SampleNum,
	uniform const int DimNum)
{
	foreach (DimIdx = 0 ... DimNum)
	{
		OutSamples[0 * DimNum + DimIdx] = Estimate[DimIdx];
	}

	foreach (SampleIdx = 1 ... SampleNum, DimIdx = 0 ... DimNum)
	{
		OutSamples[SampleIdx * DimNum + DimIdx] = Estimate[DimIdx] + FiniteDifferenceStd * GaussianSamples[SampleIdx * DimNum + DimIdx];
	}
}

export void LearningUpdatePSOVelocitiesSamples(
	uniform float InOutVelocities[],
	uniform float InOutSamples[],
	uniform const float UniformSamplesLocal[],
	uniform const float UniformSamplesGlobal[],
	uniform const float LocalBestPositions[],
	uniform const float GlobalBestPosition[],
	uniform const int SampleNum,
	uniform const int DimNum,
	uniform const float Momentum,
	uniform const float LocalGain,
	uniform const float GlobalGain)
{
	foreach (SampleIdx = 0 ... SampleNum, DimIdx = 0 ... DimNum)
	{
		InOutVelocities[SampleIdx * DimNum + DimIdx] = (
			Momentum * InOutVelocities[SampleIdx * DimNum + DimIdx] +
			LocalGain * UniformSamplesLocal[SampleIdx * DimNum + DimIdx] * (LocalBestPositions[SampleIdx * DimNum + DimIdx] - InOutSamples[SampleIdx * DimNum + DimIdx]) +
			GlobalGain * UniformSamplesGlobal[SampleIdx * DimNum + DimIdx] * (GlobalBestPosition[DimIdx] - InOutSamples[SampleIdx * DimNum + DimIdx]));

		InOutSamples[SampleIdx * DimNum + DimIdx] += InOutVelocities[SampleIdx * DimNum + DimIdx];
	}
}

export void LearningUpdatePSOSamples(
	uniform float InOutSamples[],
	uniform const float Velocities[],
	uniform const int SampleNum,
	uniform const int DimNum)
{
	foreach (SampleIdx = 0 ... SampleNum, DimIdx = 0 ... DimNum)
	{
		InOutSamples[SampleIdx * DimNum + DimIdx] += Velocities[SampleIdx * DimNum + DimIdx];
	}
}

export void LearningUpdatePSOBest(
	uniform float& InOutBestGlobalLoss,
	uniform float InOutGlobalBestPosition[],
	uniform float InOutLocalBestLoss[],
	uniform float InOutLocalBestPositions[],
	uniform const float Losses[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int DimNum)
{
	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		if (Losses[SampleIdx] < InOutLocalBestLoss[SampleIdx])
		{
			InOutLocalBestLoss[SampleIdx] = Losses[SampleIdx];
			
			foreach (DimIdx = 0 ... DimNum)
			{
				InOutLocalBestPositions[SampleIdx * DimNum + DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
			}

			if (Losses[SampleIdx] < InOutBestGlobalLoss)
			{
				InOutBestGlobalLoss = Losses[SampleIdx];

				foreach (DimIdx = 0 ... DimNum)
				{
					InOutGlobalBestPosition[DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////
// 
// Functions related to K-Means Clustering
// 
/////////////////////////////////////////////////////////

export void LearningKMeansInitCenters(
	uniform float OutCenters[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum,
	uniform const uint32 Seed)
{
	for (uniform int ClusterIdx = 0; ClusterIdx < ClusterNum; ClusterIdx++)
	{
		uniform const int32 SampleIdx = LearningRandomIntInRange(LearningRandomInt(Seed ^ ClusterIdx), 0, SampleNum - 1);

		foreach (DimIdx = 0 ... DimNum)
		{
			OutCenters[ClusterIdx * DimNum + DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
		}
	}
}

export void LearningKMeansInitBounds(
	uniform float OutMins[],
	uniform float OutMaxs[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum,
	uniform const uint32 Seed)
{
	for (uniform int ClusterIdx = 0; ClusterIdx < ClusterNum; ClusterIdx++)
	{
		uniform const int32 SampleIdx = LearningRandomIntInRange(LearningRandomInt(Seed ^ ClusterIdx), 0, SampleNum - 1);

		foreach (DimIdx = 0 ... DimNum)
		{
			OutMins[ClusterIdx * DimNum + DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
			OutMaxs[ClusterIdx * DimNum + DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
		}
	}
}

export void LearningKMeansUpdateAssignmentsFromCenters(
	uniform int32 OutAssignments[],
	uniform const float Centers[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum)
{
	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		uniform float BestDistance = MAX_flt;
		uniform int32 BestIndex = INDEX_NONE;

		for (uniform int32 ClusterIdx = 0; ClusterIdx < ClusterNum; ClusterIdx++)
		{
			float Distance = 0.0f;
			foreach (DimIdx = 0 ... DimNum)
			{
				const float Difference = Centers[ClusterIdx * DimNum + DimIdx] - Samples[SampleIdx * DimNum + DimIdx];
				Distance += Difference * Difference;
			}
			uniform const float CurrDistance = reduce_add(Distance);

			if (CurrDistance < BestDistance)
			{
				BestDistance = CurrDistance;
				BestIndex = ClusterIdx;
			}
		}

		OutAssignments[SampleIdx] = BestIndex;
	}
}

export void LearningKMeansUpdateAssignmentsFromBounds(
	uniform int32 OutAssignments[],
	uniform const float Mins[],
	uniform const float Maxs[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum)
{
	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		uniform float BestDistance = MAX_flt;
		uniform int32 BestIndex = INDEX_NONE;

		for (uniform int32 ClusterIdx = 0; ClusterIdx < ClusterNum; ClusterIdx++)
		{
			float Distance = 0.0f;
			foreach (DimIdx = 0 ... DimNum)
			{
				const float Center = (Maxs[ClusterIdx * DimNum + DimIdx] - Mins[ClusterIdx * DimNum + DimIdx]) / 2.0f + Mins[ClusterIdx * DimNum + DimIdx];
				Distance += abs(Center - Samples[SampleIdx * DimNum + DimIdx]);
			}
			uniform const float CurrDistance = reduce_add(Distance);

			if (CurrDistance < BestDistance)
			{
				BestDistance = CurrDistance;
				BestIndex = ClusterIdx;
			}
		}

		OutAssignments[SampleIdx] = BestIndex;
	}
}

export void LearningKMeansUpdateCenters(
	uniform float OutCenters[],
	uniform const int32 Assignments[],
	uniform const int32 AssignmentCounts[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum)
{
	foreach (ClusterIdx = 0 ... ClusterNum, DimIdx = 0 ... DimNum)
	{
		OutCenters[ClusterIdx * DimNum + DimIdx] = 0.0f;
	}

	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		uniform const int32 CenterIdx = Assignments[SampleIdx];
		uniform int32 AssignmentCount = AssignmentCounts[CenterIdx];

		foreach (DimIdx = 0 ... DimNum)
		{
			OutCenters[CenterIdx * DimNum + DimIdx] += Samples[SampleIdx * DimNum + DimIdx] / AssignmentCount;
		}
	}
}

export void LearningKMeansUpdateBounds(
	uniform float OutMins[],
	uniform float OutMaxs[],
	uniform const int32 Assignments[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum)
{
	foreach(ClusterIdx = 0 ... ClusterNum, DimIdx = 0 ... DimNum)
	{
		OutMins[ClusterIdx * DimNum + DimIdx] = +MAX_flt;
		OutMaxs[ClusterIdx * DimNum + DimIdx] = -MAX_flt;
	}

	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		uniform const int32 CenterIdx = Assignments[SampleIdx];

		foreach(DimIdx = 0 ... DimNum)
		{
			OutMins[CenterIdx * DimNum + DimIdx] = min(OutMins[CenterIdx * DimNum + DimIdx], Samples[SampleIdx * DimNum + DimIdx]);
			OutMaxs[CenterIdx * DimNum + DimIdx] = max(OutMaxs[CenterIdx * DimNum + DimIdx], Samples[SampleIdx * DimNum + DimIdx]);
		}
	}
}

export void LearningKMeansComputeClusteredIndex(
	uniform float OutClusteredSamples[],
	uniform int32 OutClusterStarts[],
	uniform int32 OutClusterLengths[],
	uniform int32 OutSampleMapping[],
	uniform int32 OutInverseSampleMapping[],
	uniform const int32 Assignments[],
	uniform const int32 AssignmentCounts[],
	uniform const float Samples[],
	uniform const int SampleNum,
	uniform const int ClusterNum,
	uniform const int DimNum)
{
	uniform int32 ClusterOffset = 0;
	for (uniform int ClusterIdx = 0; ClusterIdx < ClusterNum; ClusterIdx++)
	{
		OutClusterStarts[ClusterIdx] = ClusterOffset;
		ClusterOffset += AssignmentCounts[ClusterIdx];
	}

	// Keep running total of assignments here
	foreach (ClusterIdx = 0 ... ClusterNum)
	{
		OutClusterLengths[ClusterIdx] = 0;
	}

	for (uniform int SampleIdx = 0; SampleIdx < SampleNum; SampleIdx++)
	{
		uniform const int32 ClusterIdx = Assignments[SampleIdx];
		uniform const int32 SortedIdx = OutClusterStarts[ClusterIdx] + OutClusterLengths[ClusterIdx];

		foreach (DimIdx = 0 ... DimNum)
		{
			OutClusteredSamples[SortedIdx * DimNum + DimIdx] = Samples[SampleIdx * DimNum + DimIdx];
		}
		OutClusterLengths[ClusterIdx]++;

		OutSampleMapping[SampleIdx] = SortedIdx;
		OutInverseSampleMapping[SortedIdx] = SampleIdx;
	}
}