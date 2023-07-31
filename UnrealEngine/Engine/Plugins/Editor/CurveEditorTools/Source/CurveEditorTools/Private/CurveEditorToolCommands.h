// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Styling/AppStyle.h"
#include "Framework/Commands/Commands.h"

/**
 * Defines commands for the Curve Editor Tools plugin which enables most functionality of the Curve Editor.
 */
class CURVEEDITORTOOLS_API FCurveEditorToolCommands : public TCommands<FCurveEditorToolCommands>
{
public:
	FCurveEditorToolCommands()
		: TCommands<FCurveEditorToolCommands>
		(
			TEXT("CurveEditorTools"),
			NSLOCTEXT("Contexts", "CurveEditorTools", "Curve Editor Tools"),
			NAME_None,
			FAppStyle::GetAppStyleSetName()
		)
	{
	}

	TSharedPtr<FUICommandInfo> SetFocusPlaybackTime;
	TSharedPtr<FUICommandInfo> SetFocusPlaybackRange;
	TSharedPtr<FUICommandInfo> ActivateTransformTool;
	TSharedPtr<FUICommandInfo> ActivateRetimeTool;
	TSharedPtr<FUICommandInfo> ActivateMultiScaleTool;

public:
	virtual void RegisterCommands() override;
};
