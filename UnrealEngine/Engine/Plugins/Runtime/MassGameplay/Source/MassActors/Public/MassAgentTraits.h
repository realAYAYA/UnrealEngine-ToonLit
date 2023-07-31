// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "MassTranslator.h"
#include "MassAgentTraits.generated.h"

class USceneComponent;

UCLASS(Abstract)
class MASSACTORS_API UMassAgentSyncTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	EMassTranslationDirection GetSyncDirection() const { return SyncDirection; }
	void SetSyncDirection(const EMassTranslationDirection NewDirection) { SyncDirection = NewDirection; }

protected:
	UPROPERTY(EditAnywhere, Category = Mass)
	EMassTranslationDirection SyncDirection = EMassTranslationDirection::BothWays;
};

/** The trait initializes the entity with actor capsule component's radius. In addition, if bSyncTransform is true 
 *  the trait keeps actor capsule component's and entity's transforms in sync. */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Capsule Collision Sync"))
class MASSACTORS_API UMassAgentCapsuleCollisionSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	UPROPERTY(EditAnywhere, Category = Mass)
	bool bSyncTransform = true;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Movement Sync"))
class MASSACTORS_API UMassAgentMovementSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Orientation Sync"))
class MASSACTORS_API UMassAgentOrientationSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

	protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Agent Feet Location Sync"))
class MASSACTORS_API UMassAgentFeetLocationSyncTrait : public UMassAgentSyncTrait
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

