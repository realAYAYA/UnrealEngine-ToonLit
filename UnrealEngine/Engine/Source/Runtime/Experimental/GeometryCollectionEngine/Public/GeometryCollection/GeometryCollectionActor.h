// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/** This class represents an GeometryCollection Actor. */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"

#include "GeometryCollectionActor.generated.h"


class UGeometryCollectionComponent;
class UGeometryCollectionDebugDrawComponent;
struct FHitResult;

UCLASS(MinimalAPI)
class AGeometryCollectionActor: public AActor
{
	GENERATED_UCLASS_BODY()

public:

	/* Game state callback */
	GEOMETRYCOLLECTIONENGINE_API virtual void Tick(float DeltaSeconds) override;

#if WITH_EDITOR
	GEOMETRYCOLLECTIONENGINE_API virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif

	/* GeometryCollectionComponent */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Destruction, meta = (ExposeFunctionCategories = "Components|GeometryCollection", AllowPrivateAccess = "true"))
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent;
	UGeometryCollectionComponent* GetGeometryCollectionComponent() const { return GeometryCollectionComponent; }

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	TObjectPtr<UGeometryCollectionDebugDrawComponent> GeometryCollectionDebugDrawComponent_DEPRECATED;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	UFUNCTION(BlueprintCallable, Category = "Physics")
	GEOMETRYCOLLECTIONENGINE_API bool RaycastSingle(FVector Start, FVector End, FHitResult& OutHit) const;

};
