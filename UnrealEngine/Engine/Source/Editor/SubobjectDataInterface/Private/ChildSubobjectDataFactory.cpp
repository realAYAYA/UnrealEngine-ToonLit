// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChildSubobjectDataFactory.h"

#include "ChildActorSubobjectData.h"
#include "Components/ActorComponent.h"
#include "Components/ChildActorComponent.h"
#include "Engine/SCS_Node.h"		// #TODO_BH  We need to remove this when the actual subobject refactor happens
#include "Misc/AssertionMacros.h"
#include "Templates/Casts.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"

TSharedPtr<FSubobjectData> FChildSubobjectDataFactory::CreateSubobjectData(const FCreateSubobjectParams& Params)
{
	ensure(Params.Context);
	return TSharedPtr<FChildActorSubobjectData>(new FChildActorSubobjectData(Params.Context, Params.ParentHandle, Params.bIsInheritedSCS));
}

bool FChildSubobjectDataFactory::ShouldCreateSubobjectData(const FCreateSubobjectParams& Params) const
{
	// Check for a BP added CAC
	if(USCS_Node* SCS = Cast<USCS_Node>(Params.Context))
	{
		if(SCS->ComponentTemplate->IsA<UChildActorComponent>())
		{
			return true;
		}
	}
	
	return Params.Context && Params.Context->IsA<UChildActorComponent>();
}