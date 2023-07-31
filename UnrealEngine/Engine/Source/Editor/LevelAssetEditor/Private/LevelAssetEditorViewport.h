// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "SLevelViewport.h"

class FLevelEditorViewportInterfaceWrapper;
class FEditorViewportClient;

// Temporary Viewport Client class for a mode manager and sending through the tools context to the SAssetEditorViewport (SEditorViewport) - will probably want to subclass the SAssetEditorViewport here to pull in the input router?
class SLevelAssetEditorViewport
	: public SAssetEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SLevelAssetEditorViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, EditorViewportClient)
		SLATE_ARGUMENT(UInputRouter*, InputRouter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

protected:
	TSharedPtr<FLevelEditorViewportInterfaceWrapper> SlateInputWrapper;
};
