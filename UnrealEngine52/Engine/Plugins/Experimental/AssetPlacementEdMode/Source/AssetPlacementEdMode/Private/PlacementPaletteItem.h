// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISMPartition/ISMPartitionInstanceManager.h"
#include "Factories/AssetFactoryInterface.h"

#include "PlacementPaletteItem.generated.h"

class IAssetFactoryInterface;
class UInstancedPlacemenClientSettings;

UCLASS(NotPlaceable)
class UPlacementPaletteClient : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = Settings)
	FSoftObjectPath AssetPath;

	UPROPERTY()
	FGuid ClientGuid;

	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UInstancedPlacemenClientSettings> SettingsObject = nullptr;

	UPROPERTY()
	TScriptInterface<IAssetFactoryInterface> FactoryInterface;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
};
