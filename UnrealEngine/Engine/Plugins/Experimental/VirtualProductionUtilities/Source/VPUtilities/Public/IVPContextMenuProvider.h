// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IVPContextMenuProvider.generated.h"


class UVPContextMenu;


UINTERFACE(BlueprintType)
class UVPContextMenuProvider : public UInterface
{
	GENERATED_BODY()
};


class IVPContextMenuProvider : public IInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, CallInEditor, Category = "Virtual Production")
	void OnCreateContextMenu();
};
