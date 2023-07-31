// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTraitBase.h"
#include "MassCrowdMemberTrait.generated.h"

/**
 * Trait to mark an entity with the crowd tag and add required fragments to track current lane
 */
UCLASS(meta = (DisplayName = "CrowdMember"))
class MASSCROWD_API UMassCrowdMemberTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
