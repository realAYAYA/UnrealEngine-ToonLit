// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActorFactories/ActorFactory.h"
#include "Math/Quat.h"
#include "Math/UnrealMathSSE.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

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
	void PostCreateBlueprint(UObject* Asset, AActor* CDO) override;
	FQuat AlignObjectToSurfaceNormal(const FVector& InSurfaceNormal, const FQuat& ActorRotation) const;
	bool CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg) override;
	//~ End UActorFactory Interface

	int32 GetNumberOfComponents(class UCustomizableObjectInstance* COInstance);
	USkeletalMesh* GetSkeletalMeshFromAsset(UObject* Asset, int32 ComponentIndex = 0) const;
};
