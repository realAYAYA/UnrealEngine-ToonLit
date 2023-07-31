// Copyright Epic Games, Inc. All Rights Reserved.

#include "InheritedSubobjectDataFactory.h"

#include "ComponentInstanceDataCache.h"
#include "Components/ActorComponent.h"
#include "Engine/SCS_Node.h"		// #TODO_BH  We need to remove this when the actual subobject refactor happens
#include "InheritedSubobjectData.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"

TSharedPtr<FSubobjectData> FInheritedSubobjectDataFactory::CreateSubobjectData(const FCreateSubobjectParams& Params)
{
	return TSharedPtr<FInheritedSubobjectData>(new FInheritedSubobjectData(Params.Context, Params.ParentHandle, Params.bIsInheritedSCS));
}

bool FInheritedSubobjectDataFactory::ShouldCreateSubobjectData(const FCreateSubobjectParams& Params) const
{
	if(UActorComponent* Component = Cast<UActorComponent>(Params.Context))
	{
		// Create an inherited subobject data
		if(Params.bIsInheritedSCS || Component->CreationMethod != EComponentCreationMethod::Instance)
		{
			return true;
		}
	}
	else if (USCS_Node* SCS = Cast<USCS_Node>(Params.Context))
	{
		if (UActorComponent* Comp = SCS->ComponentTemplate)
		{
			return Comp->CreationMethod != EComponentCreationMethod::Instance;
		}
	}

	return false;
}