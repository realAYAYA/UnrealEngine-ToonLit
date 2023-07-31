// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DisplayClusterEditorSettings.generated.h"


/**
 * Implements nDisplay settings
 **/
UCLASS(config = Engine, defaultconfig)
class DISPLAYCLUSTEREDITOR_API UDisplayClusterEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UDisplayClusterEditorSettings(const FObjectInitializer& ObjectInitializer);

public:
	static const FName Container;
	static const FName Category;
	static const FName Section;

protected:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	UPROPERTY(config, EditAnywhere, Category = Main, meta = (ToolTip = "When enabled, replaces the original GameEngine to DisplayClusterGameEngine", ConfigRestartRequired = true))
	bool bEnabled;
};
