// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "AudioPanelWidgetInterface.generated.h"


UINTERFACE()
class ENGINE_API UAudioPanelWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

class ENGINE_API IAudioPanelWidgetInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	FText GetEditorName();

	UFUNCTION(BlueprintImplementableEvent)
	FName GetIconBrushName();
};