// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "FrameNumberDisplayFormat.h"

#include "GameplayCamerasEditorSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings)
class GAMEPLAYCAMERASEDITOR_API UGameplayCamerasEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	UGameplayCamerasEditorSettings(const FObjectInitializer& ObjectInitializer);	
};

