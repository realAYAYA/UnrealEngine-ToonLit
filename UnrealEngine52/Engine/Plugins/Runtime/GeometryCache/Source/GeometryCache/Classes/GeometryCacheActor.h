// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GeometryCacheActor.generated.h"

class UGeometryCacheComponent;

/** GeometryCache actor, serves as a place-able actor for GeometryCache objects*/
UCLASS(ComponentWrapperClass)
class GEOMETRYCACHE_API AGeometryCacheActor : public AActor
{
	GENERATED_UCLASS_BODY()

	// Begin AActor overrides.
#if WITH_EDITOR
	 virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override; 
#endif // WITH_EDITOR
	// End AActor overrides.
private:
	UPROPERTY(Category = GeometryCacheActor, VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|GeometryCache", AllowPrivateAccess = "true"))
	TObjectPtr<UGeometryCacheComponent> GeometryCacheComponent;
public:
	/** Returns GeometryCacheComponent subobject **/
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	UGeometryCacheComponent* GetGeometryCacheComponent() const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
