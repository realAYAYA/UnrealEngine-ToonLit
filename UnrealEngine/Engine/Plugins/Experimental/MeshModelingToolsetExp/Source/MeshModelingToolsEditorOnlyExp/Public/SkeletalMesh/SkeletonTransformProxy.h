// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/TransformProxy.h"

#include "SkeletonTransformProxy.generated.h"

class UObject;

/**
 * USkeletonTransformProxy is a derivation of UTransformProxy that manages several bones and update their transform individually.
 */

UCLASS(Transient)
class MESHMODELINGTOOLSEDITORONLYEXP_API USkeletonTransformProxy : public UTransformProxy
{
	GENERATED_BODY()

public:

	void Initialize(const FTransform& InTransform, const EToolContextCoordinateSystem& InTransformMode);
	
	bool IsValid() const;

	// UTransformProxy overrides
	virtual FTransform GetTransform() const override;

	UPROPERTY()
	EToolContextCoordinateSystem TransformMode = EToolContextCoordinateSystem::Local;
	
protected:
	// UTransformProxy overrides
	virtual void UpdateSharedTransform() override;
	virtual void UpdateObjects() override;
};

