// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SBaseCharacterFXEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SClothEditorRestSpaceViewportToolBar.h"

class UChaosClothAssetEditorMode;
namespace UE::Chaos::ClothAsset { class FChaosClothEditorRestSpaceViewportClient; }

class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditorRestSpaceViewport : public SBaseCharacterFXEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:

	SLATE_BEGIN_ARGS(SChaosClothAssetEditorRestSpaceViewport) {}
		SLATE_ATTRIBUTE(FVector2D, ViewportSize);
		SLATE_ARGUMENT(TSharedPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient>, RestSpaceViewportClient)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

	// SEditorViewport
	virtual void BindCommands() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void OnFocusViewportToSelection() override;
	virtual bool IsVisible() const override;

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

private:

	UChaosClothAssetEditorMode* GetEdMode() const;

	TSharedPtr<UE::Chaos::ClothAsset::FChaosClothEditorRestSpaceViewportClient> RestSpaceViewportClient;

};
