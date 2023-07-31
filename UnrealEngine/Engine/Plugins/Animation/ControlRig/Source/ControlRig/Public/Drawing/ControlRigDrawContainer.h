// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigDrawInstruction.h"
#include "ControlRigDrawContainer.generated.h"

USTRUCT()
struct CONTROLRIG_API FControlRigDrawContainer
{
	GENERATED_BODY()
	virtual ~FControlRigDrawContainer() {}

	FORCEINLINE int32 Num() const { return Instructions.Num(); }
	int32 GetIndex(const FName& InName) const;
	FORCEINLINE const FControlRigDrawInstruction& operator[](int32 InIndex) const { return Instructions[InIndex]; }
	FORCEINLINE FControlRigDrawInstruction& operator[](int32 InIndex) { return Instructions[InIndex]; }
	FORCEINLINE const FControlRigDrawInstruction& operator[](const FName& InName) const { return Instructions[GetIndex(InName)]; }
	FORCEINLINE FControlRigDrawInstruction& operator[](const FName& InName) { return Instructions[GetIndex(InName)]; }

	FORCEINLINE TArray<FControlRigDrawInstruction>::RangedForIteratorType      begin() { return Instructions.begin(); }
	FORCEINLINE TArray<FControlRigDrawInstruction>::RangedForConstIteratorType begin() const { return Instructions.begin(); }
	FORCEINLINE TArray<FControlRigDrawInstruction>::RangedForIteratorType      end() { return Instructions.end(); }
	FORCEINLINE TArray<FControlRigDrawInstruction>::RangedForConstIteratorType end() const { return Instructions.end(); }

	FORCEINLINE uint32 GetAllocatedSize(void) const { return Instructions.GetAllocatedSize(); }

	void Reset();

	UPROPERTY(EditAnywhere, Category = "DrawContainer")
	TArray<FControlRigDrawInstruction> Instructions;
};
