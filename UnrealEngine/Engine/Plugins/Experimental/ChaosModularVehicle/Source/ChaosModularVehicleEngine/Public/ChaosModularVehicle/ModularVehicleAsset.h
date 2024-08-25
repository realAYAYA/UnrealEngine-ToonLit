// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "ChaosModularVehicle/ModularSimCollection.h"

#include "ModularVehicleAsset.generated.h"

class UModularVehicleAsset;

/**
*	FModularVehicleAssetEdit
*     Structured RestCollection access where the scope
*     of the object controls serialization back into the
*     dynamic collection
*
*/
class CHAOSMODULARVEHICLEENGINE_API FModularVehicleAssetEdit
{
public:
	/**
	 * @param UModularVehicleAsset	The FAsset to edit
	 */
	FModularVehicleAssetEdit(UModularVehicleAsset* InAsset);
	~FModularVehicleAssetEdit();

	UModularVehicleAsset* GetAsset();

private:
	UModularVehicleAsset* Asset;
};

/**
* UModularVehicleAsset (UObject)
*
* UObject wrapper for the FModularVehicleAsset
*
*/
UCLASS(customconstructor)
class CHAOSMODULARVEHICLEENGINE_API UModularVehicleAsset : public UObject
{
	GENERATED_UCLASS_BODY()
	friend class FModularVehicleAssetEdit;

public:

	UModularVehicleAsset(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	FModularVehicleAssetEdit EditRestCollection() { return FModularVehicleAssetEdit(this); }

	void Serialize(FArchive& Ar);

#if WITH_EDITORONLY_DATA
	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category = GeometryCollection)
	TObjectPtr<class UThumbnailInfo> ThumbnailInfo;
#endif // WITH_EDITORONLY_DATA

private:
	TSharedPtr<FModularSimCollection, ESPMode::ThreadSafe> ModularSimCollection;
};
