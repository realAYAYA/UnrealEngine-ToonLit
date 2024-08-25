// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "AvaShapeFactory.generated.h"

class UAvaShapeDynamicMeshBase;

UCLASS()
class UAvaShapeFactory : public UActorFactory
{
	GENERATED_BODY()

public:
	UAvaShapeFactory();

	void SetMeshClass(TSubclassOf<UAvaShapeDynamicMeshBase> InMeshClass);

protected:
	TSubclassOf<UAvaShapeDynamicMeshBase> MeshClass;

	//~ Begin UActorFactory
	virtual bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	virtual AActor* GetDefaultActor(const FAssetData& AssetData) override;
	virtual AActor* SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams) override;
	virtual void PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions) override;
	//~ End UActorFactory
};
