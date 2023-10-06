// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"


#include "MVVMViewModelContext.generated.h"

class UMVVMViewModelBase;

/** */
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODEL_API FMVVMViewModelContext
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Viewmodel")
	TSubclassOf<UMVVMViewModelBase> ContextClass;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category="Viewmodel")
	FName ContextName;

public:
	bool IsValid() const;
	bool operator== (const FMVVMViewModelContext& Other) const;
	bool IsCompatibleWith(const FMVVMViewModelContext& Other) const;
	bool IsCompatibleWith(const TSubclassOf<UMVVMViewModelBase>& OtherClass) const;
	bool IsCompatibleWith(const UMVVMViewModelBase* Other) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#endif
