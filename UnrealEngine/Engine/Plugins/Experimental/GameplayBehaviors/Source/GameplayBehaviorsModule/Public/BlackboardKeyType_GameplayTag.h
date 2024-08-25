// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BehaviorTree/Blackboard/BlackboardKeyEnums.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "GameplayTagContainer.h"
#include "BlackboardKeyType_GameplayTag.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta = (DisplayName = "Gameplay Tag"))
class GAMEPLAYBEHAVIORSMODULE_API UBlackboardKeyType_GameplayTag : public UBlackboardKeyType
{
	GENERATED_BODY()
public:

	UBlackboardKeyType_GameplayTag(const FObjectInitializer& ObjectInitializer);

	typedef FGameplayTagContainer FDataType;
	static const FDataType InvalidValue;

	static FGameplayTagContainer GetValue(const UBlackboardKeyType_GameplayTag* KeyOb, const uint8* RawData);
	static bool SetValue(UBlackboardKeyType_GameplayTag* KeyOb, uint8* RawData, const FGameplayTagContainer& Value);

	virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	UPROPERTY(EditDefaultsOnly, Category = Blackboard)
	FGameplayTagContainer DefaultValue = InvalidValue;

protected:
	virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	virtual bool TestTextOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, ETextKeyOperation::Type Op, const FString& OtherString) const override;
	virtual void CopyValues(UBlackboardComponent& OwnerComp, uint8* MemoryBlock, const UBlackboardKeyType* SourceKeyOb, const uint8* SourceBlock) override;
	virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#endif
