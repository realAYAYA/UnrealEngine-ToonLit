// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "ZoneShapeActor.generated.h"

class UZoneShapeComponent;

/** Zone Shape actor for standalone zone markup. */
UCLASS(hidecategories = (Input))
class ZONEGRAPH_API AZoneShape : public AActor
{
	GENERATED_BODY()
public:
	AZoneShape(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsEditorOnly() const override { return true; }

	const UZoneShapeComponent* GetShape() const { return ShapeComponent; }

#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif

protected:

	UPROPERTY(Category = Zone, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UZoneShapeComponent> ShapeComponent;
};
