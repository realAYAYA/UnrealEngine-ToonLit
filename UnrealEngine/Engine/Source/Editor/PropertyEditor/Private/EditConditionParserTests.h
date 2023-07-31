// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/EnumAsByte.h"
#include "HAL/Platform.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "EditConditionParserTests.generated.h"

UENUM()
enum class EditConditionTestEnum
{
	First = 15,
	Second = 31
};

UENUM()
enum EditConditionByteEnum
{
	First = 15,
	Second = 31
};

UCLASS(transient)
class UEditConditionTestObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Test)
	bool BoolProperty;

	UPROPERTY(EditAnywhere, Category=Test)
	EditConditionTestEnum EnumProperty;

	UPROPERTY(EditAnywhere, Category=Test)
	TEnumAsByte<EditConditionByteEnum> ByteEnumProperty;

	UPROPERTY(EditAnywhere, Category=Test)
	double DoubleProperty;

	UPROPERTY(EditAnywhere, Category=Test)
	int32 IntegerProperty;

	UPROPERTY(EditAnywhere, Category=Test)
	uint8 UintBitfieldProperty : 1;

	UPROPERTY(EditAnywhere, Category=Test)
	TObjectPtr<UObject> UObjectPtr;

	UPROPERTY(EditAnywhere, Category=Test)
	TSoftClassPtr<UObject> SoftClassPtr;

	UPROPERTY(EditAnywhere, Category=Test)
	TWeakObjectPtr<UObject> WeakObjectPtr;
};
