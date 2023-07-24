// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintModes/WidgetBlueprintApplicationMode.h"

class FWidgetDebugApplicationMode : public FWidgetBlueprintApplicationMode
{
public:
	FWidgetDebugApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor);

	static bool IsDebugModeEnabled();

	// FApplicationMode interface
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	virtual void PostActivateMode() override;
	// End of FApplicationMode interface
};
