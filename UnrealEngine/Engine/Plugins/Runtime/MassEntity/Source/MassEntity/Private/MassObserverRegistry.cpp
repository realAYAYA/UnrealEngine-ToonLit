// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassObserverRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassObserverRegistry)

//----------------------------------------------------------------------//
// UMassObserverRegistry
//----------------------------------------------------------------------//
UMassObserverRegistry::UMassObserverRegistry()
{
	// there can be only one!
	check(HasAnyFlags(RF_ClassDefaultObject));
}

void UMassObserverRegistry::RegisterObserver(const UScriptStruct& ObservedType, const EMassObservedOperation Operation, TSubclassOf<UMassProcessor> ObserverClass)
{
	check(ObserverClass);
	checkSlow(ObservedType.IsChildOf(FMassFragment::StaticStruct()) || ObservedType.IsChildOf(FMassTag::StaticStruct()));

	if (ObservedType.IsChildOf(FMassFragment::StaticStruct()))
	{
		(*FragmentObservers[(uint8)Operation]).FindOrAdd(&ObservedType).ClassCollection.AddUnique(ObserverClass);
	}
	else
	{
		(*TagObservers[(uint8)Operation]).FindOrAdd(&ObservedType).ClassCollection.AddUnique(ObserverClass);
	}
}


