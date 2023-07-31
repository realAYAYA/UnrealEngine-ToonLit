// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameFramework/Actor.h"
#include "GeometryCollectionISMPoolActor.generated.h"

class UGeometryCollectionISMPoolComponent;

UCLASS(ConversionRoot, ComponentWrapperClass)
class GEOMETRYCOLLECTIONENGINE_API AGeometryCollectionISMPoolActor : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(Category = ISMPoolActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UGeometryCollectionISMPoolComponent> ISMPoolComp;

public:
	/** Returns ISMPoolComp subobject **/
	UGeometryCollectionISMPoolComponent* GetISMPoolComp() const { return ISMPoolComp; }
};



