// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinitionFactory.h"
#include "SmartObjectDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinitionFactory)

USmartObjectDefinitionFactory::USmartObjectDefinitionFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = USmartObjectDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* USmartObjectDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, const EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<USmartObjectDefinition>(InParent, Class, Name, Flags);
}

