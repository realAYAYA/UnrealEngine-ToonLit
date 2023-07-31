// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassCrowdMemberTrait.h"
#include "MassCrowdFragments.h"
#include "MassEntityTemplateRegistry.h"

void UMassCrowdMemberTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FMassCrowdTag>();
	BuildContext.AddFragment<FMassCrowdLaneTrackingFragment>();	
}
