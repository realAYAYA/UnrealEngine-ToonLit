// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"
#include "WeightedValue.generated.h"

namespace UE::Chaos::ClothAsset
{
	struct FWeightMapTools
	{
		/**
		 * String code used to disable the override field in the UI.
		 * Uses a character that is removed from the names made by MakeWeightMapName to avoid conflicts.
		 */
		inline static const FString NotOverridden = TEXT("#");

		/**
		 * Modifies a string to make it suitable as an attribute name.
		 * Replaces any deemed special characters and spaces by underscores, and remove leading and ending underscores from the name.
		 * Note that leading underscores are reserved for non user/internal attribute names.
		 */
		static void MakeWeightMapName(FString& InOutString)
		{
			InOutString = SlugStringForValidName(InOutString, TEXT("_")).Replace(TEXT("\\"), TEXT("_"));
			bool bCharsWereRemoved;
			do { InOutString.TrimCharInline(TEXT('_'), &bCharsWereRemoved); } while (bCharsWereRemoved);
		}
	};
}

USTRUCT()
struct FChaosClothAssetWeightedValue
{
	GENERATED_BODY()

	/**
	 * Whether the property can ever be updated/animated in real time.
	 * This could make the simulation takes more CPU time, even more so if the weight maps needs updating.
	 */
	UPROPERTY(EditAnywhere, Category = "Weighted Value")
	bool bIsAnimatable = true;

	/**
	 * Property value corresponding to the lower bound of the Weight Map.
	 * A Weight Map stores a series of Weight values assigned to each point, all between 0 and 1.
	 * The weights are used to interpolate the individual values from Low to High assigned to each different point.
	 * A Weight of 0 always corresponds to the Low parameter value, and a Weight of 1 to the High parameter value.
	 * The value for Low can be set to be bigger than for High in order to reverse the effect of the Weight Map.
	 */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DisplayName = "Low Weight", ChaosClothAssetShortName = "Lo"))
	float Low = 0.f;

	/**
	 * Property value corresponding to the upper bound of the Weight Map.
	 * A Weight Map stores a series of Weight values assigned to each point, all between 0 and 1.
	 * The weights are used to interpolate the individual values from Low to High assigned to each different point.
	 * A Weight of 0 always corresponds to the Low parameter value, and a Weight of 1 to the High parameter value.
	 * The value for Low can be set to be bigger than for High in order to reverse the effect of the Weight Map.
	 */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DisplayName = "High Weight", ChaosClothAssetShortName = "Hi"))
	float High = 1.f;

	/** The name of the weight map for this property. */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DataflowInput))
	mutable FString WeightMap = TEXT("WeightMap");  // Mutable so that it can be name checked in the evaluate function

	/** The weight map override value for when the WeightMap has a connection that replaces the provided weight map value. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Weighted Value")
	mutable FString WeightMap_Override = UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
};

USTRUCT()
struct FChaosClothAssetWeightedValueNonAnimatable
{
	GENERATED_BODY()

	static constexpr bool bIsAnimatable = false;

	/**
	 * Property value corresponding to the lower bound of the Weight Map.
	 * A Weight Map stores a series of Weight values assigned to each point, all between 0 and 1.
	 * The weights are used to interpolate the individual values from Low to High assigned to each different point.
	 * A Weight of 0 always corresponds to the Low parameter value, and a Weight of 1 to the High parameter value.
	 * The value for Low can be set to be bigger than for High in order to reverse the effect of the Weight Map.
	 */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DisplayName = "Low Weight", ChaosClothAssetShortName = "Lo"))
	float Low = 0.f;

	/**
	 * Property value corresponding to the upper bound of the Weight Map.
	 * A Weight Map stores a series of Weight values assigned to each point, all between 0 and 1.
	 * The weights are used to interpolate the individual values from Low to High assigned to each different point.
	 * A Weight of 0 always corresponds to the Low parameter value, and a Weight of 1 to the High parameter value.
	 * The value for Low can be set to be bigger than for High in order to reverse the effect of the Weight Map.
	 */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DisplayName = "High Weight", ChaosClothAssetShortName = "Hi"))
	float High = 1.f;

	/** The name of the weight map for this property. */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DataflowInput))
	mutable FString WeightMap = TEXT("WeightMap");  // Mutable so that it can be name checked in the evaluate function

	/** The weight map override value for when the WeightMap has a connection that replaces the provided weight map value. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Weighted Value")
	mutable FString WeightMap_Override = UE::Chaos::ClothAsset::FWeightMapTools::NotOverridden;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
};


USTRUCT()
struct FChaosClothAssetWeightedValueNonAnimatableNoLowHighRange
{
	GENERATED_BODY()

	/** The name of the weight map for this property. */
	UPROPERTY(EditAnywhere, Category = "Weighted Value", Meta = (DataflowInput))
	FString WeightMap = TEXT("WeightMap");

	/** The weight map override value for when the WeightMap has a connection that replaces the provided weight map value. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Weighted Value")
	mutable FString WeightMap_Override;  // _Override has a special meaning to the property customization, mutable because this property is set while getting the original value
};