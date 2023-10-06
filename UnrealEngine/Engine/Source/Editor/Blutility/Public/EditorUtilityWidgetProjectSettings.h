// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorConfigBase.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/EngineTypes.h"
#include "Misc/NamePermissionList.h"
#include "WidgetEditingProjectSettings.h"
#include "EditorUtilityWidgetProjectSettings.generated.h"

class UWidgetCompilerRule;
class UUserWidget;
class UWidgetBlueprint;
class UPanelWidget;

/**
 * Implements the settings for Editor Utility Widget Editing Project Settings
 */
UCLASS(config=Editor, defaultconfig)
class BLUTILITY_API UEditorUtilityWidgetProjectSettings : public UWidgetEditingProjectSettings
{
	GENERATED_BODY()

public:

	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;

	// Begin UObject Interface
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
	// End UObject Interface

};
