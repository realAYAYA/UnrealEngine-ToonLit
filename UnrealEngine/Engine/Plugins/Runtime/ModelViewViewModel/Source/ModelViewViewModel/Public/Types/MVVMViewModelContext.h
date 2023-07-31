// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "MVVMViewModelBase.h"

#include "MVVMViewModelContext.generated.h"

class UMVVMViewModelBase;

/** */
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODEL_API FMVVMViewModelContext
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="MVVM")
	TSubclassOf<UMVVMViewModelBase> ContextClass;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="MVVM")
	FName ContextName;

public:
	bool IsValid() const;
	bool operator== (const FMVVMViewModelContext& Other) const;
	bool IsCompatibleWith(const FMVVMViewModelContext& Other) const;
	bool IsCompatibleWith(const UMVVMViewModelBase* Other) const;
};
