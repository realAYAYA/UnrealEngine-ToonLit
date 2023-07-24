// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MVVMViewModelBlueprintToolMenuContext.generated.h"

class FMVVMViewModelBlueprintEditor;

UCLASS()
class MODELVIEWVIEWMODELEDITOR_API UMVVMViewModelBlueprintToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<FMVVMViewModelBlueprintEditor> ViewModelBlueprintEditor;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
