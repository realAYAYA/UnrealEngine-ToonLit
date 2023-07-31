// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackboardDataFactory.h"

#include "BehaviorTree/BlackboardData.h"
#include "Misc/AssertionMacros.h"
#include "Templates/SubclassOf.h"
#include "UObject/Class.h"

class FFeedbackContext;
class UObject;

#define LOCTEXT_NAMESPACE "BlackboardDataFactory"

UBlackboardDataFactory::UBlackboardDataFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UBlackboardData::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UBlackboardDataFactory::CanCreateNew() const
{
	return true;
}

UObject* UBlackboardDataFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UBlackboardData::StaticClass()));
	return NewObject<UBlackboardData>(InParent, Class, Name, Flags);
}

#undef LOCTEXT_NAMESPACE
