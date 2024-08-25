// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

enum class EDMXControlConsoleEditorViewMode : uint8;
struct FOptionalSize;
struct FSlateBrush;
struct FSlateColor;
class SHorizontalBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	class FDMXControlConsoleFaderGroupControllerModel;
	class SDMXControlConsoleEditorExpandArrowButton;
	class SDMXControlConsoleEditorFaderGroupControllerToolbar;

	/** A widget which displays a collection of Element Controllers */
	class SDMXControlConsoleEditorFaderGroupControllerView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupControllerView)
			{}

		SLATE_END_ARGS()

		/** Constructor */
		SDMXControlConsoleEditorFaderGroupControllerView();

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Fader Group Controller this view is based on */
		UDMXControlConsoleFaderGroupController* GetFaderGroupController() const;

		/** Gets current ViewMode */
		EDMXControlConsoleEditorViewMode GetViewMode() const { return ViewMode; }

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

	private:
		/** Generates ElementControllersHorizontalBox widget */
		TSharedRef<SWidget> GenerateElementControllersWidget();

		/** Gets wheter this Fader Group Controller is selected or not */
		bool IsSelected() const;

		/** Gets a reference to the toolbar's ExpandArrow button */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> GetExpandArrowButton() const;

		/** Should be called when an Element Controller was added to the Fader Group Controller this view displays */
		void OnElementControllerAdded();

		/** Adds an Element Controller slot widget */
		void AddElementController(UDMXControlConsoleElementController* ElementController);

		/** Should be called when an Element Controller was deleted from the Fader Group Controller this view displays */
		void OnElementControllerRemoved();

		/** Checks if ElementControllers array contains a reference to the given Element Controller */
		bool ContainsElementController(const UDMXControlConsoleElementController* InElementController) const;

		/** Updates this widget to the last saved expansion state from the model */
		void UpdateExpansionState();

		/** Called when the Expand Arrow button is clicked */
		void OnExpandArrowClicked(bool bExpand);

		/** Called when the Fader Group Controller gets grouped */
		void OnFaderGroupControllerGrouped();

		/** Called when the Fixture Patch of a Fader Group in the Controller has changed */
		void OnFaderGroupControllerFixturePatchChanged();

		/** Notifies this Fader Group Controller to add a new Element Controller */
		FReply OnAddElementControllerClicked();

		/** Called when Fader Groups view mode is changed */
		void OnViewModeChanged();

		/** True if the given View Mode matches the current one */
		bool IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const;

		/** Gets the height of this view according to the current Faders View Mode  */
		FOptionalSize GetFaderGroupControllerViewHeightByFadersViewMode() const;

		/** Gets the border color of this view */
		FSlateColor GetFaderGroupControllerViewBorderColor() const;

		/** Changes the brush when this widget is hovered */
		const FSlateBrush* GetFaderGroupControllerViewBorderImage() const;

		/** Changes the background brush when this widget is hovered */
		const FSlateBrush* GetFaderGroupControllerViewBackgroundBorderImage() const;

		/** Gets the visibility of this view according to the given View Mode */
		EVisibility GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const;

		/** Gets the visibility for each Element Controller widget in this view */
		EVisibility GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel) const;

		/** Gets the visibility of the horizontal Add Button widget */
		EVisibility GetAddButtonVisibility() const;

		/** Gets the visibility of the vertical Add Button widget */
		EVisibility GetAddRowButtonVisibility() const;

		/** Gets the visibility of the ElementControllersHorizontalBox widget */
		EVisibility GetElementControllersHorizontalBoxVisibility() const;

		/** Gets the visibility of the add element controller button */
		EVisibility GetAddElementControllerButtonVisibility() const;

		/** Gets the visibility of the group color tag image */
		EVisibility GetGroupColorTagImageVisibility() const;

		/** Current view mode */
		EDMXControlConsoleEditorViewMode ViewMode;

		/** Reference to the toolbar widget for this view */
		TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerToolbar> FaderGroupControllerToolbar;

		/** Horizontal Box containing the Element Controllers in this Fader Group Controller */
		TSharedPtr<SHorizontalBox> ElementControllersHorizontalBox;

		/** Array of weak references to Element Controller widgets */
		TArray<TWeakPtr<SWidget>> ElementControllerWidgets;

		/** Reference to the Fader Group Controller model this view is based on */
		TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
