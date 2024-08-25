// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toolkits/BaseToolkit.h"

class UAvaEdMode;

class FAvaEdModeToolkit : public FModeToolkit
{
public:
	FAvaEdModeToolkit(UAvaEdMode* InEdMode);

	//~ Begin IToolkit
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	//~ End IToolkit

	//~ Begin FModeToolkit
	virtual void RequestModeUITabs() override {}
	virtual void InvokeUI() override {}
	virtual void ExtendSecondaryModeToolbar(UToolMenu* InToolbarMenu);
	//~ End FModeToolkit

private:
	TWeakObjectPtr<UAvaEdMode> EdModeWeak;
};
