// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldConditions/SmartObjectWorldConditionSchema.h"
#include "SmartObjectSubsystem.h"
#include "WorldConditions/SmartObjectWorldConditionBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectWorldConditionSchema)

USmartObjectWorldConditionSchema::USmartObjectWorldConditionSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UserActorRef = AddContextDataDesc(TEXT("UserActor"), AActor::StaticClass(), EWorldConditionContextDataType::Dynamic);
	SmartObjectActorRef = AddContextDataDesc(TEXT("SmartObjectActor"), AActor::StaticClass(), EWorldConditionContextDataType::Persistent);
	SmartObjectHandleRef = AddContextDataDesc(TEXT("SmartObjectHandle"), FSmartObjectHandle::StaticStruct(), EWorldConditionContextDataType::Persistent);
	SlotHandleRef = AddContextDataDesc(TEXT("SlotHandle"), FSmartObjectSlotHandle::StaticStruct(), EWorldConditionContextDataType::Persistent);
	SubsystemRef = AddContextDataDesc(TEXT("Subsystem"), USmartObjectSubsystem::StaticClass(), EWorldConditionContextDataType::Persistent);
}

bool USmartObjectWorldConditionSchema::IsStructAllowed(const UScriptStruct* InScriptStruct) const
{
	check(InScriptStruct);
	return Super::IsStructAllowed(InScriptStruct)
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FWorldConditionCommonActorBase>::Get())
		|| InScriptStruct->IsChildOf(TBaseStructure<FSmartObjectWorldConditionBase>::Get());
}
