// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BlueprintEditorContext.generated.h"

class UBlueprint;
class FBlueprintEditor;

UCLASS()
class KISMET_API UBlueprintEditorToolMenuContext : public UObject
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category="Tool Menus")
	UBlueprint* GetBlueprintObj() const;

	TWeakPtr<FBlueprintEditor> BlueprintEditor;
};
