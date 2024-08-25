// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SAssetEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SBaseCharacterFXEditorViewport.h"

namespace UE::Chaos::ClothAsset
{
class FChaosClothPreviewScene;
}
class SClothAnimationScrubPanel;

/**
 * Viewport used for 3D preview in cloth editor. Has a custom toolbar overlay at the top.
 */
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditor3DViewport : public SBaseCharacterFXEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{

public:

	SLATE_BEGIN_ARGS(SChaosClothAssetEditor3DViewport) {}
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);
		SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, EditorViewportClient)
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, ToolkitCommandList)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

	// SAssetEditorViewport
	virtual void BindCommands() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual bool IsVisible() const override;

	virtual void OnFocusViewportToSelection() override;

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override {}

private:

	// Use this command list if we want to enable editor-wide command chords/hotkeys. 
	// Use SEditorViewport::CommandList if we want command hotkeys to only be active when the mouse is in this viewport.
	TSharedPtr<FUICommandList> ToolkitCommandList;

	TWeakPtr<UE::Chaos::ClothAsset::FChaosClothPreviewScene> GetPreviewScene();
	TWeakPtr<const UE::Chaos::ClothAsset::FChaosClothPreviewScene> GetPreviewScene() const;

	float GetViewMinInput() const;
	float GetViewMaxInput() const;
	EVisibility GetAnimControlVisibility() const;
		 

};
