// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/PBDLongRangeConstraintsBase.h"

#include "Chaos/Map.h"
#include "Chaos/Vector.h"
#include "Chaos/Framework/Parallel.h"
#include "ChaosStats.h"
#include "HAL/IConsoleManager.h"

DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Compute Geodesic Constraints"), STAT_ChaosClothComputeGeodesicConstraints, STATGROUP_Chaos);

int32 ChaosPBDLongRangeConstraintsMinParallelBatchSize = 500;

FAutoConsoleVariableRef CVarChaosPBDLongRangeConstraintsMinParallelBatchSize(
	TEXT("p.Chaos.PBDLongRangeConstraints.MinParallelBatchSize"),
	ChaosPBDLongRangeConstraintsMinParallelBatchSize,
	TEXT("The minimum number of long range tethers in a batch to process in parallel."),
	ECVF_Cheat);

namespace Chaos::Softs {

int32 FPBDLongRangeConstraintsBase::GetMinParallelBatchSize()
{
	return ChaosPBDLongRangeConstraintsMinParallelBatchSize;
}

FPBDLongRangeConstraintsBase::FPBDLongRangeConstraintsBase(
	const FSolverParticles& Particles,
	const int32 InParticleOffset,
	const int32 InParticleCount,
	const TArray<TConstArrayView<TTuple<int32, int32, FRealSingle>>>& InTethers,
	const TConstArrayView<FRealSingle>& StiffnessMultipliers,
	const TConstArrayView<FRealSingle>& ScaleMultipliers,
	const FSolverVec2& InStiffness,
	const FSolverVec2& InScale)
	: Tethers(InTethers)
	, Stiffness(InStiffness, StiffnessMultipliers, InParticleCount)
	, Scale(InScale)
	, ParticleOffset(InParticleOffset)
	, ParticleCount(InParticleCount)
{
	if (ScaleMultipliers.Num() == ParticleCount && ParticleCount > 0)
	{
		// Convert the weight maps into an array of lookup indices to the scale table
		ScaleIndices.AddUninitialized(ParticleCount);

		const FRealSingle TableScale = (FRealSingle)(TableSize - 1);

		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			ScaleIndices[Index] = (uint8)(FMath::Clamp(ScaleMultipliers[Index], (FRealSingle)0., (FRealSingle)1.) * TableScale);
		}

		// Initialize empty table until ApplyValues is called
		ScaleTable.AddZeroed(TableSize);
	}
	else
	{
		// Initialize with a one element table until ApplyValues is called
		ScaleIndices.AddZeroed(1);
		ScaleTable.AddZeroed(1);
	}

	// Apply default properties
	ApplyProperties((FSolverReal)(1. / FPBDStiffness::ParameterFrequency), 1);
}

void FPBDLongRangeConstraintsBase::ApplyScale()
{
	const FSolverReal Offset = Scale[0];
	const FSolverReal Range = Scale[1] - Scale[0];
	const int32 ScaleTableSize = ScaleTable.Num();
	const FSolverReal WeightIncrement = (ScaleTableSize > 1) ? (FSolverReal)1. / (FSolverReal)(ScaleTableSize - 1) : (FSolverReal)1.; // Must allow full range from 0 to 1 included
	for (int32 Index = 0; Index < ScaleTableSize; ++Index)
	{
		const FSolverReal Weight = (FSolverReal)Index * WeightIncrement;
		ScaleTable[Index] = Offset + Weight * Range;
	}
}

}  // End namespace Chaos::Softs

