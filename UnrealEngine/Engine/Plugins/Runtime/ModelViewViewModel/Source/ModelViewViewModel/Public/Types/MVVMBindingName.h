// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "MVVMBindingName.generated.h"

USTRUCT(BlueprintType)
struct FMVVMBindingName
{
	GENERATED_BODY()

public:
	FMVVMBindingName() = default;

	explicit FMVVMBindingName(FName InBindingName)
		: BindingName(InBindingName)
	{

	}

public:
	bool IsValid() const
	{
		return !BindingName.IsNone();
	}

	FName ToName() const
	{
		return BindingName;
	}

	FString ToString() const
	{
		return BindingName.ToString();
	}

	bool operator== (const FMVVMBindingName& Other) const
	{
		return BindingName == Other.BindingName;
	}

	bool operator!= (const FMVVMBindingName& Other) const
	{
		return BindingName != Other.BindingName;
	}


	friend int32 GetTypeHash(const FMVVMBindingName& Value)
	{
		return GetTypeHash(Value.BindingName);
	}

private:
	//todo this should be and make helper function to build the name in BP with a picker
	UPROPERTY(EditAnywhere, Category="MVVM")
	FName BindingName;
};
