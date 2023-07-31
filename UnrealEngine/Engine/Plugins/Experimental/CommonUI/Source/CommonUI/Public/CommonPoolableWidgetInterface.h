// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/ScriptMacros.h"
#include "CommonPoolableWidgetInterface.generated.h"

UINTERFACE()
class COMMONUI_API UCommonPoolableWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Widget pool, if implemented WidgetFactory will attempt to reuse implementing widget objects.
 */
class COMMONUI_API ICommonPoolableWidgetInterface
{
	GENERATED_BODY()

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Common Poolable Widget")
	void OnAcquireFromPool();

	UFUNCTION(BlueprintNativeEvent, Category = "Common Poolable Widget")
	void OnReleaseToPool();
};