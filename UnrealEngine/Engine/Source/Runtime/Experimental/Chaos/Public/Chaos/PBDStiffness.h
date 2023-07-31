// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "ChaosStats.h"

DECLARE_CYCLE_STAT(TEXT("Chaos PBD Stiffness Apply Values"), STAT_PBD_StiffnessApplyValues, STATGROUP_Chaos);

namespace Chaos::Softs
{

/**
 * Stiffness class for managing real time update to the weight map and low/high value ranges
 * and to exponentiate the stiffness value depending on the iterations and Dt.
 */
class FPBDStiffness final
{
public:
	static constexpr FSolverReal ParameterFrequency = (FSolverReal)120.;  // 60Hz @ 2 iterations as a root for all stiffness values TODO: Make this a global solver parameter

	/**
	 * Weightmap particle constructor. 
	 */
	inline FPBDStiffness(
		const FSolverVec2& InWeightedValue,
		const TConstArrayView<FRealSingle>& Multipliers = TConstArrayView<FRealSingle>(),
		int32 ParticleCount = 0,  // A value of 0 also disables the map
		int32 TableSize = 16,  // Size of the lookup table, can't be more than 256 values, the larger the table the longer it takes to apply changes to the stiffness values
		FSolverReal InParameterFitBase = (FSolverReal)1.e3);  // Logarithm base to use in the stiffness parameter fit function

	/**
	 * Weightmap constraint constructor. 
	 */
	template<int32 Valence>
	inline FPBDStiffness(
		const FSolverVec2& InWeightedValue,
		const TConstArrayView<FRealSingle>& Multipliers = TConstArrayView<FRealSingle>(),
		const TConstArrayView<TVector<int32, Valence>>& Constraints = TConstArrayView<TVector<int32, Valence>>(),
		int32 ParticleOffset = INDEX_NONE, // Constraints have usually a particle offset added to them compared to the weight maps that always starts at index 0
		int32 ParticleCount = 0,  // A value of 0 also disables the map
		int32 TableSize = 16,  // Size of the lookup table, can't be more than 256 values, the larger the table the longer it takes to apply changes to the stiffness values
		FSolverReal InParameterFitBase = (FSolverReal)1.e3,  // Logarithm base to use in the stiffness parameter fit function
		typename TEnableIf<Valence >= 2 && Valence <= 4>::Type* = nullptr);  // Prevents incorrect valence, the value is actually unused

	~FPBDStiffness() {}

	/** Return the number of values stored in the weight map. */
	int32 Num() const { return Indices.Num(); }

	/** Return whether this object contains weight map values. */
	bool HasWeightMap() const { return Table.Num() > 1 && FMath::Abs(WeightedValue[0] - WeightedValue[1]) > UE_KINDA_SMALL_NUMBER; }

	/**
	 * Set the low and high values of the weight map.
	 * The weight map table only gets updated after ApplyValues is called.
	 * Low and high values are clamped between [0,1]
	 */
	void SetWeightedValue(const FSolverVec2& InWeightedValue) { WeightedValue = InWeightedValue.ClampAxes((FSolverReal)0., (FSolverReal)1.); }

	/**
	 * Set the low and high values of the weight map.
	 * The weight map table only gets updated after ApplyValues is called.
	 * Low and high values are not clamped. Commonly used for XPBD Stiffness values which are not [0,1]
	 */
	void SetWeightedValueUnclamped(const FSolverVec2& InWeightedValue) { WeightedValue = InWeightedValue; }

	/**
	 * Return the low and high values set for this weight map.
	 * Both values will always be between 0 and 1 due to having been clamped in SetWeightedValue.
	 */
	const FSolverVec2& GetWeightedValue() const { return WeightedValue; }

	/** Update the weight map table with the current simulation parameters. */
	inline void ApplyValues(const FSolverReal Dt, const int32 NumIterations);

	/** Update the weight map table with the current simulation parameters. */
	inline void ApplyXPBDValues(const FSolverReal MaxStiffnesss);

	/**
	 * Lookup for the exponential weighted value at the specified weight map index.
	 * This function will assert if it is called with a non zero index on an empty weight map.
	*/
	FSolverReal operator[](int32 Index) const { return Table[Indices[Index]]; }

	/** Return the exponential value at the Low weight. */
	FSolverReal GetLow() const { return Table[0]; }

	/** Return the exponential value at the High weight. */
	FSolverReal GetHigh() const { return Table.Last(); }

	/** Return the exponential stiffness value when the weight map is not used. */
	explicit operator FSolverReal() const { return GetLow(); }

	/** Return the particles/constraints indices to the stiffness table as a read only array. */
	TConstArrayView<uint8> GetIndices() const { return TConstArrayView<uint8>(Indices); }

	/** Return the table of stiffnesses as a read only array. */
	TConstArrayView<FSolverReal> GetTable() const { return TConstArrayView<FSolverReal>(Table); }

	/** Reorder Indices based on Constraint reordering. */
	inline void ReorderIndices(const TArray<int32>& OrigToReorderedIndices);

private:
	TArray<uint8> Indices; // Per particle/constraints array of index to the stiffness table
	TArray<FSolverReal> Table;  // Fixed lookup table of stiffness values, use uint8 indexation
	FSolverVec2 WeightedValue;
	const FSolverReal ParameterFitBase;
	const FSolverReal ParameterFitLogBase;
};

FPBDStiffness::FPBDStiffness(
	const FSolverVec2& InWeightedValue,
	const TConstArrayView<FRealSingle>& Multipliers,
	int32 ParticleCount,
	int32 TableSize,
	FSolverReal InParameterFitBase)
	: WeightedValue(InWeightedValue.ClampAxes((FSolverReal)0., (FSolverReal)1.))
	, ParameterFitBase(InParameterFitBase)
	, ParameterFitLogBase(FMath::Loge(InParameterFitBase))
{
	check(TableSize > 0 && TableSize < 256);  // The Stiffness lookup table is restricted to uint8 sized indices

	if (Multipliers.Num() == ParticleCount && ParticleCount > 0)
	{
		// Convert the weight maps into an array of lookup indices to the stiffness table
		Indices.AddUninitialized(ParticleCount);

		const FRealSingle TableScale = (FRealSingle)(TableSize - 1);

		for (int32 Index = 0; Index < ParticleCount; ++Index)
		{
			Indices[Index] = (uint8)(FMath::Clamp(Multipliers[Index], (FRealSingle)0., (FRealSingle)1.) * TableScale);
		}

		// Initialize empty table until ApplyValues is called
		Table.AddZeroed(TableSize);
	}
	else
	{
		// Initialize with a one element table until ApplyValues is called
		Indices.AddZeroed(1);
		Table.AddZeroed(1);
	}
}

template<int32 Valence>
FPBDStiffness::FPBDStiffness(
	const FSolverVec2& InWeightedValue,
	const TConstArrayView<FRealSingle>& Multipliers,
	const TConstArrayView<TVector<int32, Valence>>& Constraints,
	int32 ParticleOffset,
	int32 ParticleCount,
	int32 TableSize,
	FSolverReal InParameterFitBase,
	typename TEnableIf<Valence >= 2 && Valence <= 4>::Type*)
	: WeightedValue(InWeightedValue.ClampAxes((FSolverReal)0., (FSolverReal)1.))
	, ParameterFitBase(InParameterFitBase)
	, ParameterFitLogBase(FMath::Loge(InParameterFitBase))
{
	check(TableSize > 0 && TableSize < 256);  // The Stiffness lookup table is restricted to uint8 sized indices

	const int32 ConstraintCount = Constraints.Num();

	if (Multipliers.Num() == ParticleCount && ParticleCount > 0 && ConstraintCount > 0)
	{
		// Convert the weight maps into an array of lookup indices to the stiffness table
		Indices.AddUninitialized(ConstraintCount);

		const FRealSingle TableScale = (FRealSingle)(TableSize - 1);

		for (int32 ConstraintIndex = 0; ConstraintIndex < ConstraintCount; ++ConstraintIndex)
		{
			const TVector<int32, Valence>& Constraint = Constraints[ConstraintIndex];

			FRealSingle Weight = 0.f;
			for (int32 Index = 0; Index < Valence; ++Index)
			{
				Weight += FMath::Clamp(Multipliers[Constraint[Index] - ParticleOffset], (FRealSingle)0., (FRealSingle)1.);
			}
			Weight /= (FRealSingle)Valence;

			Indices[ConstraintIndex] = (uint8)(Weight * TableScale);
		}

		// Initialize empty table until ApplyValues is called
		Table.AddZeroed(TableSize);
	}
	else
	{
		// Initialize with a one element table until ApplyValues is called
		Indices.AddZeroed(1);
		Table.AddZeroed(1);
	}
}

static inline FSolverReal CalcExponentialParameterFit(const FSolverReal ParameterFitBase, const FSolverReal ParameterFitLogBase, const FSolverReal InValue)
{
	// Get a very steep exponential curve between the [0, 1] range to make easier to set the parameter
	// The base has been chosen empirically.
	// ParameterFit = (pow(ParameterFitBase , InValue) - 1) / (ParameterFitBase - 1)
	// Note that ParameterFit = 0 when InValue = 0, and 1 when InValue = 1.
	return (FMath::Exp(ParameterFitLogBase * InValue) - (FSolverReal)1.) / (ParameterFitBase - (FSolverReal)1.);
}

void FPBDStiffness::ApplyValues(const FSolverReal Dt, const int32 NumIterations)
{
	SCOPE_CYCLE_COUNTER(STAT_PBD_StiffnessApplyValues);

	// Calculate the simulation exponent
	const FSolverReal Exponent = Dt * ParameterFrequency / (FSolverReal)NumIterations;

	// Define the stiffness mapping function
	auto SimulationValue = [this, Exponent](const FSolverReal InValue)->FSolverReal
	{
		// If InValue is 1 then LogValue = -inf and the output becomes -inf as well,
		// in order for this function to be continuous, we want the output to be 1 when InValue = 1
		// and need the stiffness to be exactly 0 when the input is 0
		if (InValue <= (FSolverReal)UE_DOUBLE_SMALL_NUMBER)
		{
			return (FSolverReal)0.;
		}
		if (InValue >= (FSolverReal)(1. - UE_DOUBLE_SMALL_NUMBER))
		{
			return (FSolverReal)1.;
		}
		const FSolverReal ParameterFit = CalcExponentialParameterFit(ParameterFitBase, ParameterFitLogBase, InValue);

		// Use simulation dependent stiffness exponent to alleviate the variations in effect when Dt and NumIterations change
		// This is based on the Position-Based Simulation Methods paper (page 8),
		// but uses the delta time in addition of the number of iterations in the calculation of the error term.
		const FSolverReal LogValue = FMath::Loge((FSolverReal)1. - ParameterFit);
		return (FSolverReal)1. - FMath::Exp(LogValue * Exponent);
	};

	const FSolverReal Offset = WeightedValue[0];
	const FSolverReal Range = WeightedValue[1] - WeightedValue[0];
	const int32 TableSize = Table.Num();
	const FSolverReal WeightIncrement = (TableSize > 1) ? (FSolverReal)1. / (FSolverReal)(TableSize - 1) : (FSolverReal)1.; // Must allow full range from 0 to 1 included
	for (int32 Index = 0; Index < TableSize; ++Index)
	{
		const FSolverReal Weight = (FSolverReal)Index * WeightIncrement;
		Table[Index] = SimulationValue(Offset + Weight * Range);
	}
}

void FPBDStiffness::ApplyXPBDValues(const FSolverReal MaxStiffness)
{
	SCOPE_CYCLE_COUNTER(STAT_PBD_StiffnessApplyValues);
	
	auto SimulationValue = [this, MaxStiffness](const FSolverReal InValue)->FSolverReal
	{
		// Do not apply exponential to XPBDStiffnesses. They are authored in terms of true stiffness values (e.g., kg/s^2), not exponential compliance
		return FMath::Clamp(InValue, (FSolverReal)0., MaxStiffness);
	};

	const FSolverReal Offset = WeightedValue[0];
	const FSolverReal Range = WeightedValue[1] - WeightedValue[0];
	const int32 TableSize = Table.Num();
	const FSolverReal WeightIncrement = (TableSize > 1) ? (FSolverReal)1. / (FSolverReal)(TableSize - 1) : (FSolverReal)1.; // Must allow full range from 0 to 1 included
	for (int32 Index = 0; Index < TableSize; ++Index)
	{
		const FSolverReal Weight = (FSolverReal)Index * WeightIncrement;
		Table[Index] = SimulationValue(Offset + Weight * Range);
	}
}

void FPBDStiffness::ReorderIndices(const TArray<int32>& OrigToReorderedConstraintIndices)
{
	if (Indices.Num() == OrigToReorderedConstraintIndices.Num())
	{
		TArray<uint8> ReorderedIndices;
		ReorderedIndices.SetNumUninitialized(Indices.Num());
		for (int32 OrigConstraintIndex = 0; OrigConstraintIndex < OrigToReorderedConstraintIndices.Num(); ++OrigConstraintIndex)
		{
			const int32 ReorderedConstraintIndex = OrigToReorderedConstraintIndices[OrigConstraintIndex];
			ReorderedIndices[ReorderedConstraintIndex] = Indices[OrigConstraintIndex];
		}
		Indices = MoveTemp(ReorderedIndices);
	}
	else
	{
		check(Indices.Num() == 1);
	}
}

}  // End namespace Chaos::Softs
