// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/StateTreeComponentSchema.h"
#include "StateTreeConditionBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeTaskBase.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Subsystems/WorldSubsystem.h"


UStateTreeComponentSchema::UStateTreeComponentSchema()
	: ContextActorClass(AActor::StaticClass())
	, ContextActorDataDesc(FName("Actor"), AActor::StaticClass(), FGuid(0x1D971B00, 0x28884FDE, 0xB5436802, 0x36984FD5))
{
}

bool UStateTreeComponentSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	return InScriptStruct->IsChildOf(FStateTreeConditionCommonBase::StaticStruct())
	|| InScriptStruct->IsChildOf(FStateTreeEvaluatorCommonBase::StaticStruct())
	|| InScriptStruct->IsChildOf(FStateTreeTaskCommonBase::StaticStruct());
}

bool UStateTreeComponentSchema::IsClassAllowed(const UClass* InClass) const
{
	return IsChildOfBlueprintBase(InClass);
}

bool UStateTreeComponentSchema::IsExternalItemAllowed(const UStruct& InStruct) const
{
	return InStruct.IsChildOf(AActor::StaticClass())
			|| InStruct.IsChildOf(UActorComponent::StaticClass())
			|| InStruct.IsChildOf(UWorldSubsystem::StaticClass());
}

TConstArrayView<FStateTreeExternalDataDesc> UStateTreeComponentSchema::GetContextDataDescs() const
{
	return MakeArrayView(&ContextActorDataDesc, 1);
}

void UStateTreeComponentSchema::PostLoad()
{
	Super::PostLoad();
	ContextActorDataDesc.Struct = ContextActorClass.Get();
}

#if WITH_EDITOR
void UStateTreeComponentSchema::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* Property = PropertyChangedEvent.Property;

	if (Property)
	{
		if (Property->GetOwnerClass() == UStateTreeComponentSchema::StaticClass()
			&& Property->GetFName() == GET_MEMBER_NAME_CHECKED(UStateTreeComponentSchema, ContextActorClass))
		{
			ContextActorDataDesc.Struct = ContextActorClass.Get();
		}
	}
}
#endif