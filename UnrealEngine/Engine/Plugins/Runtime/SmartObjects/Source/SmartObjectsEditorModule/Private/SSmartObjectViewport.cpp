// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSmartObjectViewport.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SSmartObjectViewportToolbar.h"
#include "SmartObjectAssetToolkit.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SmartObjectViewport"

void SSmartObjectViewport::Construct(const FArguments& InArgs)
{
	ViewportClient = InArgs._EditorViewportClient;
	PreviewScene = InArgs._PreviewScene;
	AssetEditorToolkitPtr = InArgs._AssetEditorToolkit;

	SEditorViewport::Construct(
		SEditorViewport::FArguments()
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute())
	);
}

void SSmartObjectViewport::BindCommands()
{
	SEditorViewport::BindCommands();
}

TSharedRef<FEditorViewportClient> SSmartObjectViewport::MakeEditorViewportClient()
{
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SSmartObjectViewport::MakeViewportToolbar()
{
	return SAssignNew(ViewportToolbar, SSmartObjectViewportToolBar, SharedThis(this));
}

TSharedRef<SEditorViewport> SSmartObjectViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SSmartObjectViewport::GetExtenders() const
{
	TSharedPtr<FExtender> Result(MakeShareable(new FExtender));
	return Result;
}

void SSmartObjectViewport::OnFloatingButtonClicked()
{
}

#undef LOCTEXT_NAMESPACE