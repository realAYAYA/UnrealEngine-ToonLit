// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintFunctionReference.h"
#include "MVVMPropertyPath.h"
#include "Templates/SubclassOf.h"

#include "MVVMLinkedPinValue.generated.h"

class UFunction;
class UK2Node;

/** */
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODELEDITOR_API FMVVMLinkedPinValue
{
public:
	GENERATED_BODY()

	FMVVMLinkedPinValue() = default;
	explicit FMVVMLinkedPinValue(FMVVMBlueprintPropertyPath InPath);
	explicit FMVVMLinkedPinValue(const UBlueprint* InBlueprint, FMVVMBlueprintFunctionReference InConversion);
	explicit FMVVMLinkedPinValue(const UFunction* Function);
	explicit FMVVMLinkedPinValue(TSubclassOf<UK2Node> Node);

public:
	bool IsPropertyPath() const;
	const FMVVMBlueprintPropertyPath& GetPropertyPath() const;

	bool IsConversionFunction() const;
	const UFunction* GetConversionFunction() const;

	bool IsConversionNode() const;
	TSubclassOf<UK2Node> GetConversionNode() const;

public:
	bool IsValid() const;
	bool operator== (const FMVVMLinkedPinValue& Other) const
	{
		return ConversionNode == Other.ConversionNode && ConversionFunction == Other.ConversionFunction && PropertyPath == Other.PropertyPath;
	}

protected:
	UPROPERTY(BlueprintReadWrite, Category = "MVVM")
	FMVVMBlueprintPropertyPath PropertyPath;

	UPROPERTY(BlueprintReadWrite, Category = "MVVM")
	const UFunction* ConversionFunction = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "MVVM")
	TSubclassOf<UK2Node> ConversionNode;
};
