// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "CustomizableObjectInstanceFactory.generated.h"

class AActor;
class FText;
class UObject;
class USkeletalMesh;
struct FAssetData;

UCLASS(MinimalAPI, config=Editor)
class UCustomizableObjectInstanceFactory : public UActorFactory
{
	GENERATED_UCLASS_BODY()

protected:
	//~ Begin UActorFactory Interface
	void PostSpawnActor(UObject* Asset, AActor* NewActor) override;
	UObject* GetAssetFromActorInstance(AActor* ActorInstance) override;
	bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	//~ End UActorFactory Interface

	USkeletalMesh* GetSkeletalMeshFromAsset(UObject* Asset, int32 ComponentIndex = 0) const;
	int32 GetNumberOfComponents(class UCustomizableObjectInstance* COInstance);
	FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const;
};
