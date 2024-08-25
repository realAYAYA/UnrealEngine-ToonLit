// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDecorator.h"

#include "RigDecorator_AnimNextCppDecorator.generated.h"

namespace UE::AnimNext { struct FDecorator; }

/**
 * AnimNext RigDecorator for all C++ decorators.
 * The decorator shared data UScriptStruct determines which properties are exposed.
 */
USTRUCT(BlueprintType)
struct ANIMNEXT_API FRigDecorator_AnimNextCppDecorator : public FRigVMDecorator
{
	GENERATED_BODY()

	// The struct the decorator exposes with its shared data. Each one of its properties will be added as a pin.
	UPROPERTY(meta = (Hidden))
	UScriptStruct* DecoratorSharedDataStruct = nullptr;

#if WITH_EDITOR
	virtual void GetProgrammaticPins(URigVMController* InController, int32 InParentPinIndex, const FString& InDefaultValue, struct FRigVMPinInfoArray& OutPinArray) const override;
	
	virtual UScriptStruct* GetDecoratorSharedDataStruct() const { return DecoratorSharedDataStruct; }

	const UE::AnimNext::FDecorator* GetDecorator() const;
#endif
};

USTRUCT(BlueprintType)
struct FAnimNextCppDecoratorWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FRigDecorator_AnimNextCppDecorator CppDecorator;
};
