// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "Misc/ConfigUtilities.h"

#include "NNEEditorSettings.generated.h"

UCLASS(config=Editor, defaultconfig, meta = (DisplayName = "Neural Network Engine (NNE)"))
class UNNEEditorSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:

	virtual FName GetContainerName() const override { return FName("Project"); }
	virtual FName GetCategoryName() const override { return FName("Editor"); }
	virtual void PostInitProperties() override;
	
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = Editor, meta = (
		ConsoleVariable = "nne.Editor.UseDDC", DisplayName = "Use DDC for Neural Network optimizations",
		ToolTip = "True if NNE should use the DDC cache to fetch previously optimized Neural Network."))
	bool bUseDDCCache;
};