// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameFramework/Actor.h"
#include "GeometryCollectionISMPoolActor.generated.h"

class UGeometryCollectionISMPoolComponent;
class UGeometryCollectionISMPoolDebugDrawComponent;

UCLASS(ConversionRoot, ComponentWrapperClass, MinimalAPI)
class AGeometryCollectionISMPoolActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = ISMPoolActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UGeometryCollectionISMPoolComponent> ISMPoolComp;

	UPROPERTY(Category = ISMPoolActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UGeometryCollectionISMPoolDebugDrawComponent> ISMPoolDebugDrawComp;

public:
	/** Returns ISMPoolComp subobject **/
	UGeometryCollectionISMPoolComponent* GetISMPoolComp() const { return ISMPoolComp; }
};



