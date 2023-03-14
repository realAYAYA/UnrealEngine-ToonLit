// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "HAL/Platform.h"
#include "Templates/Tuple.h"
#include "Templates/TypeCompatibleBytes.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneDecompositionQuery.generated.h"

class UObject;

namespace UE
{
namespace MovieScene
{

	/**
	 * Structure used to decompose the blended result of multiple components.
	 *
	 * Defines an object to query, and the entities that should have their pre-blended component values extracted
	 */
	struct FDecompositionQuery
	{
		/** Defines all the entities that should have their pre-component values extracted for recomposition */
		TArrayView<const FMovieSceneEntityID> Entities;

		/** Whether the entities above are source entities or runtime entities */
		bool bConvertFromSourceEntityIDs = true;

		/** The object that is being decomposed */
		UObject* Object = nullptr;
	};


	/** Used for decomposing how a final blended value was blended */
	struct FWeightedValue
	{
		double Value = 0.f;
		float Weight = 0.f;
		
		double BaseValue = 0.f;  // Should only be set when the value has additive-from-base blend type

		double WeightedValue() const
		{
			return Weight != 0.f ? (Value - BaseValue) / Weight : 0.f;
		}

		FWeightedValue Combine(FWeightedValue Other) const
		{
			return FWeightedValue{Value + Other.Value, Weight + Other.Weight, BaseValue + Other.BaseValue};
		}
	};

	struct FDecomposedValue
	{
		struct FResult
		{
			FWeightedValue Absolute;
			double Additive = 0.f;
		};

		FResult Result;

		TArray<TTuple<FMovieSceneEntityID, FWeightedValue>> DecomposedAbsolutes;
		TArray<TTuple<FMovieSceneEntityID, FWeightedValue>> DecomposedAdditives;
		TArray<TTuple<FMovieSceneEntityID, FWeightedValue>> DecomposedAdditivesFromBase;

		MOVIESCENE_API double Recompose(FMovieSceneEntityID EntityID, double CurrentValue, const double* InitialValue) const;

	private:
		enum class EDecomposedValueBlendType
		{
			Absolute,
			Additive,
			AdditiveFromBase
		};

		void Decompose(
				FMovieSceneEntityID EntityID, FWeightedValue& ThisValue, EDecomposedValueBlendType& OutBlendType, 
				FWeightedValue& Absolutes, FWeightedValue& Additives, FWeightedValue& AdditivesFromBase) const;
	};

	// Align results to cache lines so there's no contention between cores
	struct MS_ALIGN(PLATFORM_CACHE_LINE_SIZE) FAlignedDecomposedValue
	{
		FDecomposedValue Value;
	} GCC_ALIGN(PLATFORM_CACHE_LINE_SIZE);

	struct FValueDecompositionParams
	{
		FDecompositionQuery Query;
		uint16 DecomposeBlendChannel;
		FMovieSceneEntityID PropertyEntityID;
		FComponentTypeID ResultComponentType;
		FComponentTypeID PropertyTag;
	};

	template<typename PropertyType>
	struct TRecompositionResult
	{
		TRecompositionResult(const PropertyType& InCurrentValue, int32 Num)
		{
			while (--Num >= 0)
			{
				Values.Add(InCurrentValue);
			}
		}

		TArray<PropertyType, TInlineAllocator<1>> Values;
	};

} // namespace MovieScene
} // namespace UE


UINTERFACE()
class MOVIESCENE_API UMovieSceneValueDecomposer : public UInterface
{
public:
	GENERATED_BODY()
};

class IMovieSceneValueDecomposer
{
public:
	GENERATED_BODY()

	virtual FGraphEventRef DispatchDecomposeTask(const UE::MovieScene::FValueDecompositionParams& Params, UE::MovieScene::FAlignedDecomposedValue* Output) = 0;
};
