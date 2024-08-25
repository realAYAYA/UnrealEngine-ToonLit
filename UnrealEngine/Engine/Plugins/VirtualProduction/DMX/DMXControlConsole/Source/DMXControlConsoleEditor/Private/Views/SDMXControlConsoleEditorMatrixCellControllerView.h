// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FOptionalSize;
struct FSlateColor;
class SHorizontalBox;
class UDMXControlConsoleCellAttributeController;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFixturePatchMatrixCell;
class UDMXControlConsoleMatrixCellController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	class SDMXControlConsoleEditorElementControllerView;
	class SDMXControlConsoleEditorExpandArrowButton;

	/** A widget which displays a collection of Cell Attribute Controllers */
	class SDMXControlConsoleEditorMatrixCellControllerView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorMatrixCellControllerView)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Matrix Cell Controller this widget is based on */
		UDMXControlConsoleMatrixCellController* GetMatrixCellController() const;

		/** Gets a reference to the Matrix Cell showed by this widget */
		UDMXControlConsoleFixturePatchMatrixCell* GetMatrixCell() const;

		/** Gets a reference to this widget's ExpandArrow button */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton>& GetExpandArrowButton() { return ExpandArrowButton; }

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

	private:
		/** Should be called when a Cell Attribute Controller was added to the Matrix Cell Controller this widget displays */
		void OnCellAttributeControllerAdded();

		/** Adds a Cell Attribute Controller slot widget */
		void AddCellAttributeController(UDMXControlConsoleCellAttributeController* CellAttributeController);

		/** Should be called when a Cell Attribute Controller was deleted from the Matrix Cell Controller this widget displays */
		void OnCellAttributeControllerRemoved();

		/** Checks if the Cell Attribute Controllers array contains a reference to the given Cell Attribute Controller */
		bool ContainsCellAttributeController(UDMXControlConsoleCellAttributeController* CellAttributeController);

		/** Returns true if any of the Cell Attribute Controllers in the Matrix Cell Controller is selected */
		bool IsAnyCellAttributeControllerSelected() const;

		/** Called when the editor model is updated */
		void OnEditorModelUpdated();

		/** Gets the height of the Matrix Cell Controller according to the current Faders View Mode  */
		FOptionalSize GetMatrixCellControllerHeightByFadersViewMode() const;

		/** Gets the Matrix Cell ID as text */
		FText GetMatrixCellLabelText() const;

		/** Gets the label background color */
		FSlateColor GetLabelBorderColor() const;

		/** Gets the visibility for each Cell Attribute Controller widget in this view */
		EVisibility GetCellAttributeControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ControllerModel) const;

		/** Gets the visibility of the CellAttributeControllersHorizontalBox widget */
		EVisibility GetCellAttributeControllersHorizontalBoxVisibility() const;

		/** Gets the widget border brush */
		const FSlateBrush* GetBorderImage() const;

		/** Reference to the Cell Attribute Controllers main widget */
		TSharedPtr<SHorizontalBox> CellAttributeControllersHorizontalBox;

		/** Array of Cell Attribute Controllers views */
		TArray<TWeakPtr<SDMXControlConsoleEditorElementControllerView>> CellAttributeControllerViews;

		/** Reference to the ExpandArrow button used to show/hide the Cell Attribute Controllers */
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton;

		/** Reference to the Matrix Cell Controller being displayed */
		TSharedPtr<FDMXControlConsoleElementControllerModel> MatrixCellControllerModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
