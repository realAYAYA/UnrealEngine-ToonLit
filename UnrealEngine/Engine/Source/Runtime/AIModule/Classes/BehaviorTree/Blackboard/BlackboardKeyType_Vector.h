// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Vector.generated.h"

class UBlackboardComponent;

UCLASS(EditInlineNew, meta=(DisplayName="Vector"), MinimalAPI)
class UBlackboardKeyType_Vector : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	typedef FVector FDataType; 
	static AIMODULE_API const FDataType InvalidValue;
	
	static AIMODULE_API FVector GetValue(const UBlackboardKeyType_Vector* KeyOb, const uint8* RawData);
	static AIMODULE_API bool SetValue(UBlackboardKeyType_Vector* KeyOb, uint8* RawData, const FVector& Value);

	AIMODULE_API virtual EBlackboardCompare::Type CompareValues(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock,
		const UBlackboardKeyType* OtherKeyOb, const uint8* OtherMemoryBlock) const override;

	// If not set, will default to  FAISystem::InvalidLocation
	UPROPERTY(EditDefaultsOnly, Category = Blackboard, meta = (EditCondition = "bUseDefaultValue"))
	FVector DefaultValue = FVector::ZeroVector;

	UPROPERTY()
	bool bUseDefaultValue = false;

protected:
	AIMODULE_API virtual void InitializeMemory(UBlackboardComponent& OwnerComp, uint8* RawData) override;
	AIMODULE_API virtual FString DescribeValue(const UBlackboardComponent& OwnerComp, const uint8* RawData) const override;
	AIMODULE_API virtual bool GetLocation(const UBlackboardComponent& OwnerComp, const uint8* RawData, FVector& Location) const override;
	AIMODULE_API virtual bool IsEmpty(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock) const override;
	AIMODULE_API virtual void Clear(UBlackboardComponent& OwnerComp, uint8* MemoryBlock) override;
	AIMODULE_API virtual bool TestBasicOperation(const UBlackboardComponent& OwnerComp, const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};
