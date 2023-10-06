// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"

#include "AssetEditorToolkitMenuContext.generated.h"

class FAssetEditorToolkit;

UCLASS(MinimalAPI)
class UAssetEditorToolkitMenuContext : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Tool Menus")
	UNREALED_API TArray<UObject*> GetEditingObjects() const;

	TWeakPtr<FAssetEditorToolkit> Toolkit;
};
