// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LandscapeEditorDetailCustomization_Base.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;

/**
 * Slate widgets customizer for the smaller tools that need little customization
 */

class FLandscapeEditorDetailCustomization_MiscTools : public FLandscapeEditorDetailCustomization_Base
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

protected:
	// Component Selection Tool
	static EVisibility GetClearComponentSelectionVisibility();
	static FReply OnClearComponentSelectionButtonClicked();

	// Mask Tool
	static EVisibility GetClearRegionSelectionVisibility();
	static FReply OnClearRegionSelectionButtonClicked();

	// Splines Tool
	static FReply OnApplyAllSplinesButtonClicked();
	static FReply OnApplySelectedSplinesButtonClicked();
	static FReply OnSelectAllControlPointsButtonClicked();
	static FReply OnSelectAllSegmentsButtonClicked();
	void OnbUseAutoRotateControlPointChanged(ECheckBoxState NewState);
	ECheckBoxState GetbUseAutoRotateControlPoint() const;

	// Ramp Tool
	static FReply OnApplyRampButtonClicked();
	static bool GetApplyRampButtonIsEnabled();
	static FReply OnResetRampButtonClicked();

	// Mirror Tool
	static FReply OnApplyMirrorButtonClicked();
	static FReply OnResetMirrorPointButtonClicked();

	// Flatten Tool
	void HandleFlattenValueChanged(float NewValue);
	void OnBeginFlattenToolEyeDrop();
	void OnCompletedFlattenToolEyeDrop(bool Canceled);
	TOptional<float> GetFlattenValue() const;

	// Common Error handling
	EVisibility GetMiscLandscapeErrorVisibility() const;
	FText GetMiscLandscapeErrorText() const;
};
