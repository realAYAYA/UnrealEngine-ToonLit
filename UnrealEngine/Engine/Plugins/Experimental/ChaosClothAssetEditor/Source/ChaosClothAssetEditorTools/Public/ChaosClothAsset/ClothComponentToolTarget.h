// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ClothAssetBackedTarget.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"
#include "ClothComponentToolTarget.generated.h"

class UChaosClothComponent;

/**
 * A tool target backed by a cloth component
 */
UCLASS(Transient)
class CHAOSCLOTHASSETEDITORTOOLS_API UClothComponentToolTarget : public UPrimitiveComponentToolTarget, public IClothAssetBackedTarget
{
	GENERATED_BODY()

public:

	// UToolTarget
	virtual bool IsValid() const override;

	// IClothAssetBackedTarget
	virtual UChaosClothAsset* GetClothAsset() const override;

	UChaosClothComponent* GetClothComponent() const;

protected:
	friend class UClothComponentToolTargetFactory;
};

/** Factory for UClothComponentToolTarget to be used by the target manager. */
UCLASS(Transient)
class CHAOSCLOTHASSETEDITORTOOLS_API UClothComponentToolTargetFactory : public UToolTargetFactory
{
	GENERATED_BODY()

public:

	virtual bool CanBuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) const override;

	virtual UToolTarget* BuildTarget(UObject* SourceObject, const FToolTargetTypeRequirements& TargetTypeInfo) override;
};
