// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SClothEditorViewportToolBarBase.h"

class FExtender;
class FUICommandList;
class SChaosClothAssetEditorRestSpaceViewport;
namespace UE::Chaos::ClothAsset { class FChaosClothEditorRestSpaceViewportClient; }

/**
 * Toolbar that shows up at the top of the rest space viewport
 */
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditorRestSpaceViewportToolBar : public SChaosClothAssetEditorViewportToolBarBase
{
public:
	SLATE_BEGIN_ARGS(SChaosClothAssetEditorRestSpaceViewportToolBar) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
		SLATE_ARGUMENT(TSharedPtr<FExtender>, Extenders)
		SLATE_ARGUMENT(TSharedPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient>, RestSpaceViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class SChaosClothAssetEditorRestSpaceViewport> InChaosClothAssetEditorViewport);

private:

	float GetCameraPointLightIntensity() const;
	void CameraPointLightIntensityChanged(float NewValue);
	bool IsRenderModeEnabled() const;

	TSharedRef<SWidget> GenerateLightMenu();
	TSharedRef<SWidget> GenerateClothRestSpaceViewportOptionsMenu();

	TSharedRef<SWidget> MakeOptionsMenu();
	TSharedRef<SWidget> MakeDisplayToolBar(const TSharedPtr<FExtender> InExtenders);
	TSharedRef<SWidget> MakeToolBar(const TSharedPtr<FExtender> InExtenders);

	FText GetDisplayString() const;

	FText GetViewModeMenuLabel() const;
	const FSlateBrush* GetViewModeMenuLabelIcon() const;
	TSharedRef<SWidget> GenerateViewModeMenuContent() const;

	void RegisterViewModeMenuContent();

	/** The viewport that we are in */
	TWeakPtr<class SChaosClothAssetEditorRestSpaceViewport> ChaosClothAssetEditorRestSpaceViewportPtr;

	/** Client associated with the viewport we are in */
	TSharedPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient> RestSpaceViewportClient;

	TSharedPtr<FUICommandList> CommandList;
};
