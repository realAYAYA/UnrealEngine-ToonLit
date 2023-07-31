// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterColorGradingDataModel.h"
#include "SDisplayClusterColorGradingColorWheel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IDetailTreeNode;
class IPropertyRowGenerator;
class SBox;
class SDisplayClusterColorGradingDetailView;
class SHorizontalBox;
struct FDisplayClusterColorGradingDrawerState;

/** A panel that contains up to five color wheels (for saturation, contrast, gamma, gain, and offset) as well as a details view for extra, non-color properties */
class SDisplayClusterColorGradingColorWheelPanel : public SCompoundWidget
{
private:
	typedef SDisplayClusterColorGradingColorWheel::EColorDisplayMode EColorDisplayMode;

	/** The number of color wheels the color wheel panel displays (for saturation, contrast, gamma, gain, and offset) */
	static const uint32 NumColorWheels = 5;

public:
	virtual ~SDisplayClusterColorGradingColorWheelPanel() override;

	SLATE_BEGIN_ARGS(SDisplayClusterColorGradingColorWheelPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FDisplayClusterColorGradingDataModel>, ColorGradingDataModelSource)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Regenerates the color wheel panel from the current state of the data model source */
	void Refresh();

	/** Adds the state of the color wheel panel to the specified drawer state */
	void GetDrawerState(FDisplayClusterColorGradingDrawerState& OutDrawerState);

	/** Sets the state of the color wheel panel from the specified drawer state */
	void SetDrawerState(const FDisplayClusterColorGradingDrawerState& InDrawerState);

private:
	void BindCommands();

	TSharedRef<SWidget> MakeColorDisplayModeCheckbox();
	TSharedRef<SWidget> MakeSettingsMenu();

	void FillColorGradingGroupProperty(const FDisplayClusterColorGradingDataModel::FColorGradingGroup& ColorGradingGroup);
	void ClearColorGradingGroupProperty();

	void FillColorGradingElementsToolBar(const TArray<FDisplayClusterColorGradingDataModel::FColorGradingElement>& ColorGradingElements);
	void ClearColorGradingElementsToolBar();

	void FillColorWheels(const FDisplayClusterColorGradingDataModel::FColorGradingElement& ColorGradingElement);
	void ClearColorWheels();

	TSharedRef<SWidget> CreateColorWheelHeaderWidget(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle);
	TSharedRef<SWidget> CreateColorPropertyExtensions(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle, const TSharedPtr<IDetailTreeNode>& DetailTreeNode);

	bool FilterDetailTreeNode(const TSharedRef<IDetailTreeNode>& InDetailTreeNode);

	void SetColorWheelOrientation(EOrientation NewOrientation);
	bool IsColorWheelOrientationSelected(EOrientation Orientation) const;

	void ToggleColorWheelVisible(int32 ColorWheelIndex);
	bool IsColorWheelVisible(int32 ColorWheelIndex);

	void OnColorGradingGroupSelectionChanged();
	void OnColorGradingElementSelectionChanged();

	void OnColorGradingElementCheckedChanged(ECheckBoxState State, FText ElementName);

	ECheckBoxState IsColorGradingElementSelected(FText ElementName) const;
	EVisibility GetColorWheelPanelVisibility() const;
	EVisibility GetColorWheelVisibility(int32 ColorWheelIndex) const;

	EColorDisplayMode GetColorDisplayMode() const { return ColorDisplayMode; }
	ECheckBoxState IsColorDisplayModeChecked(EColorDisplayMode InColorDisplayMode) const;
	void OnColorDisplayModeCheckedChanged(ECheckBoxState State, EColorDisplayMode InColorDisplayMode);
	FText GetColorDisplayModeLabel(EColorDisplayMode InColorDisplayMode) const;
	FText GetColorDisplayModeToolTip(EColorDisplayMode InColorDisplayMode) const;

private:
	/** The color grading data model that the panel is displaying */
	TSharedPtr<FDisplayClusterColorGradingDataModel> ColorGradingDataModel;

	TSharedPtr<SBox> ColorGradingGroupPropertyBox;
	TSharedPtr<SHorizontalBox> ColorGradingElementsToolBarBox;

	TArray<TSharedPtr<SDisplayClusterColorGradingColorWheel>> ColorWheels;
	TArray<bool> HiddenColorWheels;

	TSharedPtr<SDisplayClusterColorGradingDetailView> DetailView;

	/** Commands used by the color wheel panel */
	TSharedPtr<FUICommandList> CommandList;

	/** The currently selected color grading group */
	int32 SelectedColorGradingGroup = INDEX_NONE;

	/** The current color display mode for the color wheels */
	EColorDisplayMode ColorDisplayMode = EColorDisplayMode::RGB;

	/** The current orientation for the color wheels */
	EOrientation ColorWheelOrientation = EOrientation::Orient_Vertical;
};