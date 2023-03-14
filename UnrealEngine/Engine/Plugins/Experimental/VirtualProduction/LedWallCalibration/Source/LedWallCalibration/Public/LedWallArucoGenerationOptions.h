// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "LedWallArucoGenerationOptions.generated.h"

/** Enumeration of known Aruco dictionaries */
UENUM(BlueprintType)
enum EArucoDictionary
{
	DICT_4X4_50          UMETA(DisplayName = "DICT_4X4_50"),
	DICT_4X4_100		 UMETA(DisplayName = "DICT_4X4_100"),
	DICT_4X4_250		 UMETA(DisplayName = "DICT_4X4_250"),
	DICT_4X4_1000		 UMETA(DisplayName = "DICT_4X4_1000"),
	DICT_5X5_50			 UMETA(DisplayName = "DICT_5X5_50"),
	DICT_5X5_100		 UMETA(DisplayName = "DICT_5X5_100"),
	DICT_5X5_250		 UMETA(DisplayName = "DICT_5X5_250"),
	DICT_5X5_1000		 UMETA(DisplayName = "DICT_5X5_1000"),
	DICT_6X6_50			 UMETA(DisplayName = "DICT_6X6_50"),
	DICT_6X6_100		 UMETA(DisplayName = "DICT_6X6_100"),
	DICT_6X6_250		 UMETA(DisplayName = "DICT_6X6_250"),
	DICT_6X6_1000		 UMETA(DisplayName = "DICT_6X6_1000"),
	DICT_7X7_50			 UMETA(DisplayName = "DICT_7X7_50"),
	DICT_7X7_100		 UMETA(DisplayName = "DICT_7X7_100"),
	DICT_7X7_250		 UMETA(DisplayName = "DICT_7X7_250"),
	DICT_7X7_1000		 UMETA(DisplayName = "DICT_7X7_1000"),
	DICT_ARUCO_ORIGINAL	 UMETA(DisplayName = "DICT_ARUCO_ORIGINAL")
};

/**
 * Structure that can be passed to the Aruco generation function
 */
USTRUCT(BlueprintType)
struct FLedWallArucoGenerationOptions
{
	GENERATED_BODY()

public:

	/** Width of the texture that will contain the Aruco markers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	int32 TextureWidth = 3840;

	/** Height of the texture that will contain the Aruco markers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	int32 TextureHeight = 2160;

	/** Aruco dictionary to use when generating the markers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	TEnumAsByte<EArucoDictionary> ArucoDictionary = EArucoDictionary::DICT_6X6_1000;

	/** Starting marker Id. Arucos will be generated with consecutive Marker Ids, starting from this one */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	int32 MarkerId = 1;

	/** 
	 * Used to avoid using up the symbols in the dictionary as quickly.
	 * Will place the next marker id when [(row + column) mod PlaceModulus] is zero.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aruco")
	int32 PlaceModulus = 2;

public:

	/**
	 * Returns name of selected Aruco dictionary 
	 * 
	 * @return Name of selected Aruco dictionary
	 */
	FString ArucoDictionaryAsString() const
	{
		const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/LedWallCalibration.EArucoDictionary"), true);
		check(EnumPtr);

#if WITH_EDITOR
		return EnumPtr->GetDisplayNameTextByIndex(static_cast<uint8>(ArucoDictionary.GetValue())).ToString();
#else
		return EnumPtr->GetNameStringByIndex(static_cast<uint8>(ArucoDictionary.GetValue()));
#endif
	}
};
