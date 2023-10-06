// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassObserverProcessor.h"
#include "MassObserverRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassObserverProcessor)

//----------------------------------------------------------------------//
// UMassObserverProcessor
//----------------------------------------------------------------------//
UMassObserverProcessor::UMassObserverProcessor()
{
	bAutoRegisterWithProcessingPhases = false;
#if WITH_EDITORONLY_DATA
	bCanShowUpInSettings = false;
#endif // WITH_EDITORONLY_DATA
}

void UMassObserverProcessor::PostInitProperties()
{
	Super::PostInitProperties();

	UClass* MyClass = GetClass();
	CA_ASSUME(MyClass);

	if (HasAnyFlags(RF_ClassDefaultObject) && MyClass->HasAnyClassFlags(CLASS_Abstract) == false)
	{
		if (ensure(ObservedType != nullptr && Operation != EMassObservedOperation::MAX))
		{
			Register();
		}
		else
		{
			UE_LOG(LogMass, Error, TEXT("%s attempting to register %s while it\'s misconfigured, Type: %s, Operation: %s")
				, ANSI_TO_TCHAR(__FUNCTION__), *MyClass->GetName(), *GetNameSafe(ObservedType), *UEnum::GetValueAsString(Operation));
		}
	}
}

void UMassObserverProcessor::Register()
{
	if (bAutoRegisterWithObserverRegistry)
	{
		check(ObservedType);
		UMassObserverRegistry::GetMutable().RegisterObserver(*ObservedType, Operation, GetClass());
	}
}


