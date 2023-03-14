// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MVVMViewModelBlueprintToolMenuContext.generated.h"

class FMVVMViewModelBlueprintEditor;

UCLASS()
class MODELVIEWVIEWMODELEDITOR_API UMVVMViewModelBlueprintToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<FMVVMViewModelBlueprintEditor> ViewModelBlueprintEditor;
};
