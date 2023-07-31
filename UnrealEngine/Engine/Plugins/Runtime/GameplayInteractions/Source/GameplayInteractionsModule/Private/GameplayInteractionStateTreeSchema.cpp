// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayInteractionStateTreeSchema.h"
#include "GameplayInteractionsTypes.h"
#include "SmartObjectRuntime.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameplayInteractionStateTreeSchema)

UGameplayInteractionStateTreeSchema::UGameplayInteractionStateTreeSchema()
	: ContextActorClass(AActor::StaticClass())
	, SmartObjectActorClass(AActor::StaticClass())
	,ContextDataDescs({
		{UE::GameplayInteraction::Names::ContextActor, AActor::StaticClass(), FGuid(0xDFB93B9E, 0xEDBE4906, 0x851C66B2, 0x7585FA21)},
		{UE::GameplayInteraction::Names::SmartObjectActor, AActor::StaticClass(), FGuid(0x870E433F, 0x99314B95, 0x982B78B0, 0x1B63BBD1)},
		{UE::GameplayInteraction::Names::SmartObjectClaimedHandle, FSmartObjectClaimHandle::StaticStruct(), FGuid(0x13BAB427, 0x26DB4A4A, 0xBD5F937E, 0xDB39F841)},
		{UE::GameplayInteraction::Names::AbortContext, FGameplayInteractionAbortContext::StaticStruct(), FGuid(0xEED35411, 0x85E844A0, 0x95BE6DB5, 0xB63F51BC)}
	})
{
}

bool UGameplayInteractionStateTreeSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct())
			|| InScriptStruct->IsChildOf(FGameplayInteractionStateTreeTask::StaticStruct());
}

bool UGameplayInteractionStateTreeSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UGameplayInteractionStateTreeSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(AActor::StaticClass())
			|| InStruct.IsChildOf(UActorComponent::StaticClass())
			|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}

void UGameplayInteractionStateTreeSchema::PostLoad()
{
	Super::PostLoad();
	ContextDataDescs[0].Struct = ContextActorClass.Get();
	ContextDataDescs[1].Struct = SmartObjectActorClass.Get();
}

#if WITH_EDITOR
void UGameplayInteractionStateTreeSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FProperty* Property = PropertyChangedEvent.Property;

	if (Property)
	{
		if (Property->GetOwnerClass() == UGameplayInteractionStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UGameplayInteractionStateTreeSchema, ContextActorClass))
		{
			ContextDataDescs[0].Struct = ContextActorClass.Get();
		}
		if (Property->GetOwnerClass() == UGameplayInteractionStateTreeSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UGameplayInteractionStateTreeSchema, SmartObjectActorClass))
		{
			ContextDataDescs[1].Struct = SmartObjectActorClass.Get();
		}
	}
}
#endif