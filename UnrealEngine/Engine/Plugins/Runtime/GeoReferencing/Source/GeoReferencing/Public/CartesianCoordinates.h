// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CartesianCoordinates.generated.h"


USTRUCT(BlueprintType, meta = (Deprecated = "5.0"))

struct GEOREFERENCING_API FCartesianCoordinates
{
	GENERATED_USTRUCT_BODY()

public:
	FCartesianCoordinates();
	FCartesianCoordinates(double InX, double InY, double InZ);
	FCartesianCoordinates(const FVector& Coordinates);

	double X;
	double Y;
	double Z;

	
	FText ToFullText(int32 IntegralDigits = 3);
	FText ToCompactText(int32 IntegralDigits = 3);
	void ToSeparateTexts(FText& OutX, FText& OutY, FText& OutZ, int32 IntegralDigits = 3);
	void ToFloatApproximation(float& OutX, float& OutY, float& OutZ);
	FVector ToVector() const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function

	UE_DEPRECATED(5.0, "Use FCartesianCoordinates::ToVector() instead.")
	FVector3d ToVector3d() const;
	UE_DEPRECATED(5.0, "Use FCartesianCoordinates(const FVector& Coordinates) instead.")
	FCartesianCoordinates(const FVector4d& Coordinates);

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	
};

UCLASS()
class GEOREFERENCING_API UCartesianCoordinatesFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/**
	 * Converts a LargeCoordinates value to localized formatted text, in the form 'X= Y= Z='
	 **/
	UE_DEPRECATED(5.0, "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead.")
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToFullText", AdvancedDisplay = "1", BlueprintAutocast, DeprecatedFunction, DeprecationMessage = "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead."), Category = "GeoReferencing")
	static FORCEINLINE FText ToFullText(UPARAM(ref) FCartesianCoordinates& CartesianCoordinates, int32 IntegralDigits = 3)
	{
		return CartesianCoordinates.ToFullText(IntegralDigits);
	}

	/**
	 * Converts a LargeCoordinates value to formatted text, in the form '(X, Y, Z)'
	 **/
	UE_DEPRECATED(5.0, "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead.")
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToCompactText", AdvancedDisplay = "1", BlueprintAutocast, DeprecatedFunction, DeprecationMessage = "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead."), Category = "GeoReferencing")
	static FORCEINLINE FText ToCompactText(UPARAM(ref) FCartesianCoordinates& CartesianCoordinates, int32 IntegralDigits = 3)
	{
		return CartesianCoordinates.ToCompactText(IntegralDigits);
	}

	/**
	 * Converts a LargeCoordinates value to 3 separate text values
	 **/
	UE_DEPRECATED(5.0, "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead.")
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToSeparateTexts", AdvancedDisplay = "4", BlueprintAutocast, DeprecatedFunction, DeprecationMessage = "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead."), Category = "GeoReferencing")
	static void ToSeparateTexts(UPARAM(ref) FCartesianCoordinates& CartesianCoordinates, FText& OutX, FText& OutY, FText& OutZ, int32 IntegralDigits = 3)
	{
		CartesianCoordinates.ToSeparateTexts(OutX, OutY, OutZ, IntegralDigits);
	}

	/**
	 * Get the Coordinates as a float approximation.
	 * USE WISELY as we can't guarantee there will no be rounding due to IEEE754 float encoding !
	 **/
	UE_DEPRECATED(5.0, "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead.")
	UFUNCTION(BlueprintPure, Category = "GeoReferencing", meta = (DeprecatedFunction, DeprecationMessage = "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead."))
	static void ToFloatApproximation(UPARAM(ref) FCartesianCoordinates& CartesianCoordinates, float& OutX, float& OutY, float& OutZ)
	{
		CartesianCoordinates.ToFloatApproximation(OutX, OutY, OutZ);
	}

	/**
	 * Set the Coordinates from float approximation.
	 * USE WISELY as we can't guarantee there will no be rounding due to IEEE754 float encoding !
	 **/
	UE_DEPRECATED(5.0, "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead.")
	UFUNCTION(BlueprintPure, Category = "GeoReferencing", meta = (DeprecatedFunction, DeprecationMessage = "FCartesianCoordinates is deprecated : Use the version that uses a FVector instead."))
	static FCartesianCoordinates MakeCartesianCoordinatesApproximation(const float& InX, const float& InY, const float& InZ)
	{
		return FCartesianCoordinates(static_cast<float>(InX), static_cast<float>(InY), static_cast<float>(InZ));
	}
};
