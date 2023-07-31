// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "MassEntityTraitBase.generated.h"

struct FMassEntityTemplateBuildContext;

/**
 * Base class for Mass Entity Traits.
 * An entity trait is a set of fragments that create a logical trait tha makes sense to end use (i.e. replication, visualization).
 * The template building method allows to configure some fragments based on properties or cached values.
 * For example, a fragment can be added based on a referenced asset, or some memory hungry settings can be
 * cached and just and index stored on a fragment.
 */
UCLASS(Abstract, BlueprintType, EditInlineNew, CollapseCategories)
class MASSSPAWNER_API UMassEntityTraitBase : public UObject
{
	GENERATED_BODY()

public:

	/** Appends items into the entity template required for the trait. */
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const PURE_VIRTUAL(UMassEntityTraitBase::BuildTemplate, return; );

	virtual void DestroyTemplate() const {}

	/** Called when all Traits have had BuildTemplate() called. */
	virtual void ValidateTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const {};
};
