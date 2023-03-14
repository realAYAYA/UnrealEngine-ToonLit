// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SmartObjectRuntime.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_SOClaimHandle.generated.h"

class UBlackboardComponent;

/**
 * Blackboard key type that holds a SmartObject claim handle 
 */
UCLASS(EditInlineNew, meta=(DisplayName="SO Claim Handle"))
class SMARTOBJECTSMODULE_API UBlackboardKeyType_SOClaimHandle : public UBlackboardKeyType
{
	GENERATED_BODY()
public:
	explicit UBlackboardKeyType_SOClaimHandle(const FObjectInitializer& ObjectInitializer);
	
	typedef FSmartObjectClaimHandle FDataType;
	static const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly)
	FSmartObjectClaimHandle Handle;
	
	static FSmartObjectClaimHandle GetValue(const UBlackboardKeyType_SOClaimHandle* KeyOb, const uint8* MemoryBlock);
	static bool SetValue(UBlackboardKeyType_SOClaimHandle* KeyOb, uint8* MemoryBlock, const FSmartObjectClaimHandle& Value);

protected:
	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp,
												   const uint8* MemoryBlock,
												   const UBlackboardKeyType* OtherKeyOb,
												   const uint8* OtherMemoryBlock) const override;

	virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
	virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
