// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Class.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Class"), MinimalAPI)
class UBlackboardKeyType_Class : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef UClass* FDataType;
	static AIMODULE_API const FDataType InvalidValue;

	UPROPERTY(Category=Blackboard, EditDefaultsOnly, meta=(AllowAbstract="1"))
	TObjectPtr<UClass> BaseClass;
	
	static AIMODULE_API UClass* GetValue(const UBlackboardKeyType_Class* KeyOb, const uint8* RawData);
	static AIMODULE_API bool SetValue(UBlackboardKeyType_Class* KeyOb, uint8* RawData, UClass* Value);

	AIMODULE_API virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	AIMODULE_API virtual FString DescribeSelf() const override;
	AIMODULE_API virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;

protected:
	AIMODULE_API virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	AIMODULE_API virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
