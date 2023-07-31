// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"
#include "Tools/Modes.h"
#include "WidgetEditorToolPaletteMode.generated.h"

class SBorder;

UCLASS(Transient)
class UWidgetEditorToolPaletteMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()

public:

	const static FEditorModeID Id;

protected:

	UWidgetEditorToolPaletteMode();

	void Enter() override;

	bool UsesToolkits() const override; 
	void CreateToolkit() override;
	

};
