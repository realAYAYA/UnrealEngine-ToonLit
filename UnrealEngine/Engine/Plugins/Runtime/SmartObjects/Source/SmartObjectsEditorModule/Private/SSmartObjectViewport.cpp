// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSmartObjectViewport.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SSmartObjectViewportToolbar.h"
#include "SmartObjectAssetToolkit.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "SmartObjectViewport"

void SSmartObjectViewport::Construct(const FArguments& InArgs, const TSharedRef<FSmartObjectAssetToolkit>& InAssetEditorToolkit,FAdvancedPreviewScene* InPreviewScene)
{
	PreviewScene = InPreviewScene;
	AssetEditorToolkitPtr = InAssetEditorToolkit;

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
	ViewportClient = StaticCastSharedPtr<FSmartObjectAssetEditorViewportClient>(AssetEditorToolkitPtr.Pin()->CreateEditorViewportClient());
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