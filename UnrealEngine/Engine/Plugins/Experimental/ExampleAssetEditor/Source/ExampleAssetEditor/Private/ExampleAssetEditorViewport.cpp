// Copyright Epic Games, Inc.All Rights Reserved.

#include "ExampleAssetEditorViewport.h"

#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "InputRouter.h"
#include "SlateViewportInterfaceWrapper.h"
#include "Slate/SceneViewport.h"

void SExampleAssetEditorViewport::Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs)
{
	// Construct the slate editor viewport
	SAssetEditorViewport::Construct(
		SAssetEditorViewport::FArguments()
			.EditorViewportClient(InArgs._EditorViewportClient),
		InViewportConstructionArgs);

	// Override the viewport interface with our input router wrapper
	SlateInputWrapper = MakeShared<FSlateViewportInterfaceWrapper>(SceneViewport, GetViewportClient()->GetModeTools()->GetInteractiveToolsContext()->InputRouter);
	ViewportWidget->SetViewportInterface(SlateInputWrapper.ToSharedRef());
}
