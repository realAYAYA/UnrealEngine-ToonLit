// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassAssortedFragmentsTrait.h"
#include "MassEntityTemplateRegistry.h"


void UMassAssortedFragmentsTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	for (const FInstancedStruct& Fragment : Fragments)
	{
		if (Fragment.IsValid())
		{
			const UScriptStruct* Type = Fragment.GetScriptStruct();
			CA_ASSUME(Type);
			if (Type->IsChildOf(FMassFragment::StaticStruct()))
			{
				BuildContext.AddFragment(Fragment);
			}
			else
			{
				UE_LOG(LogMass, Error, TEXT("Struct type %s is not a child of FMassFragment"), *GetPathNameSafe(Type));
			}
		}
	}
	
	for (const FInstancedStruct& Tag : Tags)
	{
		if (Tag.IsValid())
		{
			const UScriptStruct* Type = Tag.GetScriptStruct();
			CA_ASSUME(Type);
			if (Type->IsChildOf(FMassTag::StaticStruct()))
			{
				BuildContext.AddTag(*Type);
			}
			else
			{
				UE_LOG(LogMass, Error, TEXT("Struct type %s is not a child of FMassTag"), *GetPathNameSafe(Type));
			}
		}
	}
}
