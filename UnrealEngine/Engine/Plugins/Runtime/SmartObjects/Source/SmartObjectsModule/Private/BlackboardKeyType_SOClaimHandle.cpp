// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlackboardKeyType_SOClaimHandle.h"

#include "BehaviorTree/BlackboardComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlackboardKeyType_SOClaimHandle)

const UBlackboardKeyType_SOClaimHandle::FDataType UBlackboardKeyType_SOClaimHandle::InvalidValue;

UBlackboardKeyType_SOClaimHandle::UBlackboardKeyType_SOClaimHandle(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ValueSize = sizeof(FSmartObjectClaimHandle);
	SupportedOp = EBlackboardKeyOperation::Basic;
}

FSmartObjectClaimHandle UBlackboardKeyType_SOClaimHandle::GetValue(const UBlackboardKeyType_SOClaimHandle* KeyOb, const uint8* MemoryBlock)
{
	return GetValueFromMemory<FSmartObjectClaimHandle>(MemoryBlock);
}

bool UBlackboardKeyType_SOClaimHandle::SetValue(UBlackboardKeyType_SOClaimHandle* KeyOb, uint8* MemoryBlock, const FSmartObjectClaimHandle& Value)
{
	return SetValueInMemory<FSmartObjectClaimHandle>(MemoryBlock, Value);
}

EBlackboardCompare::Type UBlackboardKeyType_SOClaimHandle::CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
	const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const
{
	const FSmartObjectClaimHandle MyValue = GetValue(this, MemoryBlock);
	const FSmartObjectClaimHandle OtherValue = GetValue((UBlackboardKeyType_SOClaimHandle*)OtherKeyOb, OtherMemoryBlock);

	return (MyValue == OtherValue) ? EBlackboardCompare::Equal : EBlackboardCompare::NotEqual;
}

void UBlackboardKeyType_SOClaimHandle::InitializeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock)
{
	SetValueInMemory<FSmartObjectClaimHandle>(MemoryBlock, FSmartObjectClaimHandle::InvalidHandle);
}

void UBlackboardKeyType_SOClaimHandle::Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock)
{
	SetValueInMemory<FSmartObjectClaimHandle>(MemoryBlock, FSmartObjectClaimHandle::InvalidHandle);
}

FString UBlackboardKeyType_SOClaimHandle::DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const
{
	return LexToString(GetValue(this, MemoryBlock));
}

bool UBlackboardKeyType_SOClaimHandle::TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, const EBasicKeyOperation::Type Op) const
{
	const FSmartObjectClaimHandle ClaimHandle = GetValue(this, MemoryBlock);
	return (Op == EBasicKeyOperation::Set) ? ClaimHandle.IsValid() : !ClaimHandle.IsValid();
}

