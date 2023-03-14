// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneDecompositionQuery.h"
#include "Algo/Accumulate.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneDecompositionQuery)


namespace UE
{
namespace MovieScene
{

void FDecomposedValue::Decompose(
		FMovieSceneEntityID EntityID, FWeightedValue& ThisValue, EDecomposedValueBlendType& OutBlendType, 
		FWeightedValue& Absolutes, FWeightedValue& Additives, FWeightedValue& AdditivesFromBase) const
{
	for (TTuple<FMovieSceneEntityID, FWeightedValue> Pair : DecomposedAbsolutes)
	{
		if (Pair.Get<0>() == EntityID)
		{
			ThisValue = Pair.Value;
			OutBlendType = EDecomposedValueBlendType::Absolute;
		}
		else
		{
			Absolutes.Value += Pair.Value.Value * Pair.Value.Weight;
			Absolutes.Weight += Pair.Value.Weight;
		}
	}
	for (TTuple<FMovieSceneEntityID, FWeightedValue> Pair : DecomposedAdditives)
	{
		if (Pair.Get<0>() == EntityID)
		{
			ThisValue = Pair.Value;
			OutBlendType = EDecomposedValueBlendType::Additive;
		}
		else
		{
			Additives.Value += Pair.Value.Value * Pair.Value.Weight;
			Additives.Weight += Pair.Value.Weight;
		}
	}
	for (TTuple<FMovieSceneEntityID, FWeightedValue> Pair : DecomposedAdditivesFromBase)
	{
		if (Pair.Get<0>() == EntityID)
		{
			ThisValue = Pair.Value;
			OutBlendType = EDecomposedValueBlendType::AdditiveFromBase;
		}
		else
		{
			AdditivesFromBase.Value += (Pair.Value.Value - Pair.Value.BaseValue) * Pair.Value.Weight;
			AdditivesFromBase.Weight += Pair.Value.Weight;
		}
	}
}

double FDecomposedValue::Recompose(FMovieSceneEntityID RecomposeEntity, double CurrentValue, const double* InitialValue) const
{
	FWeightedValue DecomposedAbsolute;
	FWeightedValue DecomposedAdditive;
	FWeightedValue DecomposedAdditiveFromBase;

	FWeightedValue Channel;
	EDecomposedValueBlendType BlendType(EDecomposedValueBlendType::Absolute);
	Decompose(RecomposeEntity, Channel, BlendType, DecomposedAbsolute, DecomposedAdditive, DecomposedAdditiveFromBase);

	FWeightedValue ResultAbsolute = Result.Absolute;
	float TotalAbsoluteWeight = ResultAbsolute.Weight + DecomposedAbsolute.Weight;
	const bool bIsAdditive = (BlendType != EDecomposedValueBlendType::Absolute);
	if (!bIsAdditive)
	{
		TotalAbsoluteWeight += Channel.Weight;
	}
	if (TotalAbsoluteWeight < 1.f && InitialValue != nullptr)
	{
		const float InitialValueWeight = (1.f - TotalAbsoluteWeight);
		ResultAbsolute.Value = (*InitialValue) * InitialValueWeight + ResultAbsolute.WeightedValue();
		ResultAbsolute.Weight = 1.f;
	}

	// If this channel is the only thing we decomposed, that is simple
	if (DecomposedAbsolute.Weight == 0.f && DecomposedAdditive.Weight == 0.f && DecomposedAdditiveFromBase.Weight == 0.f)
	{
		if (bIsAdditive)
		{
			const double WeightedAdditiveResult = CurrentValue - ResultAbsolute.Combine(DecomposedAbsolute).WeightedValue() - Result.Additive;
			return (Channel.Weight == 0.f ? WeightedAdditiveResult : WeightedAdditiveResult / Channel.Weight) + Channel.BaseValue;
		}
		else
		{
			if (Channel.Weight != 0.f)
			{
				const float TotalWeight = Channel.Weight + ResultAbsolute.Weight;
				const double WeightedAbsoluteResult = CurrentValue - Result.Additive - ResultAbsolute.Value / TotalWeight;
				return WeightedAbsoluteResult * TotalWeight / Channel.Weight;
			}
			else
			{
				return CurrentValue - Result.Additive - ResultAbsolute.WeightedValue();
			}
		}
	}

	// If the channel had no weight, we can't recompose it - everything else will get the full weighting
	if (Channel.Weight == 0.f)
	{
		return Channel.Value;
	}

	if (bIsAdditive)
	{
		CurrentValue -= ResultAbsolute.Combine(DecomposedAbsolute).WeightedValue();

		const double ThisAdditive = Channel.WeightedValue();
		if (ThisAdditive == 0.f && DecomposedAdditive.WeightedValue() == 0.f && DecomposedAdditiveFromBase.WeightedValue() == 0.f)
		{
			const float TotalAdditiveWeight = DecomposedAdditive.Weight + Channel.Weight;
			return (CurrentValue * Channel.Weight / TotalAdditiveWeight) + Channel.BaseValue;
		}

		// Use the fractions of the values for the recomposition if we have non-zero values
		const double DecomposeFactor = ThisAdditive / (DecomposedAdditive.WeightedValue() + ThisAdditive);
		return CurrentValue * DecomposeFactor / Channel.Weight + Channel.BaseValue;
	}
	else if (DecomposedAdditives.Num() != 0)
	{
		// Absolute channel, but we're keying additives, put the full weight to the additives
		return Channel.Value - Channel.BaseValue;
	}
	else
	{
		const float TotalDecomposedWeight = DecomposedAbsolute.Weight + Channel.Weight;
		CurrentValue -= Result.Additive;
		CurrentValue *= ResultAbsolute.Weight + TotalDecomposedWeight;
		CurrentValue -= ResultAbsolute.Value;

		const double AbsValue = Algo::Accumulate(DecomposedAbsolutes, 0.f, [](double Accum, TTuple<FMovieSceneEntityID, FWeightedValue> In) { return Accum + FMath::Abs(In.Value.Value)*In.Value.Weight; });
		if (AbsValue != 0.f)
		{
			return ((CurrentValue * FMath::Abs(Channel.Value) * Channel.Weight / AbsValue) - Channel.Value) / Channel.Weight;
		}
		else if (TotalDecomposedWeight == 0.f)
		{
			return Channel.Value;
		}
		return (CurrentValue * Channel.Weight / TotalDecomposedWeight) / Channel.Weight;
	}

	return Channel.Value;
}

} // namespace MovieScene
} // namespace UE



