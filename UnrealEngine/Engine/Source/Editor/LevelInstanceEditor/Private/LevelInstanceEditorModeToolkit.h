// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Toolkits/BaseToolkit.h"
#include "UObject/NameTypes.h"

class FAssetEditorModeUILayer;

class FLevelInstanceEditorModeToolkit : public FModeToolkit
{
public:
	FLevelInstanceEditorModeToolkit();

	// IToolkit interface 
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;

	virtual void SetModeUILayer(const TSharedPtr<FAssetEditorModeUILayer> InLayer) override;
};