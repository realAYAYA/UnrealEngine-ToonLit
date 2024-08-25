// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "PrimitiveComponentBackedTarget.generated.h"

class UPrimitiveComponent;
class AActor;

struct FHitResult;

UINTERFACE(MinimalAPI)
class UPrimitiveComponentBackedTarget : public UInterface
{
	GENERATED_BODY()
};

class IPrimitiveComponentBackedTarget
{
	GENERATED_BODY()

public:

	/** @return the Component this is a Source for */
	virtual UPrimitiveComponent* GetOwnerComponent() const = 0;

	/** @return the Actor that owns this Component */
	virtual AActor* GetOwnerActor() const = 0;

	/**
	 * Set the visibility of the Component associated with this Source (ie to hide during Tool usage)
	 * @param bVisible desired visibility
	 */
	virtual void SetOwnerVisibility(bool bVisible) const = 0;

	/**
	 * @return the transform on this component
	 * @todo Do we need to return a list of transforms here?
	 */
	virtual FTransform GetWorldTransform() const = 0;

	/**
	 * Compute ray intersection with the MeshDescription this Source is providing
	 * @param WorldRay ray in world space
	 * @param OutHit hit test data
	 * @return true if ray intersected Component
	 */
	virtual bool HitTestComponent(const FRay& WorldRay, FHitResult& OutHit) const = 0;
};
