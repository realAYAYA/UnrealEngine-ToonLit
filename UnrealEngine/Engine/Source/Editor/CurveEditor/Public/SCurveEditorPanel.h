// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "CurveDataAbstraction.h"
#include "CurveDrawInfo.h"
#include "CurveEditor.h"
#include "CurveEditorTypes.h"
#include "Curves/RealCurve.h"
#include "Curves/RichCurve.h"
#include "HAL/Platform.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Layout/Geometry.h"
#include "Layout/Visibility.h"
#include "Math/Axis.h"
#include "Math/Color.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/UniquePtr.h"
#include "Textures/SlateIcon.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
class FExtender;
class FTabManager;
class FUICommandList;
class IDetailsView;
class IGraphEditorView;
class ITimeSliderController;
class SCurveEditorToolProperties;
class SCurveEditorView;
class SCurveEditorViewContainer;
class SCurveEditorFilterPanel;
class SCurveKeyDetailPanel;
class SScrollBox;
class SWidget;
class UCurveEditorFilterBase;
struct FCurveEditorDelayedDrag;
struct FCurveEditorEditObjectContainer;
struct FCurveEditorToolID;
struct FKeyEvent;

/**
 * Curve editor widget that reflects the state of an FCurveEditor
 */
class CURVEEDITOR_API SCurveEditorPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SCurveEditorPanel)
		: _GridLineTint(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
		, _MinimumViewPanelHeight(300.0f)
	{}

		/** Color to draw grid lines */
		SLATE_ATTRIBUTE(FLinearColor, GridLineTint)

		/** Tab Manager which owns this panel. */
		SLATE_ARGUMENT(TSharedPtr<FTabManager>, TabManager)

		/** Optional Time Slider Controller which allows us to synchronize with an externally controlled Time Slider */
		SLATE_ARGUMENT(TSharedPtr<ITimeSliderController>, ExternalTimeSliderController)

		/** If specified, causes the time snap adjustment UI controls to be disabled and specifies the tooltip to be used. Can be used to disable time snap controls when externally controlled. */
		SLATE_ATTRIBUTE(FText, DisabledTimeSnapTooltip)

		/** Widget slot for the tree content */
		SLATE_NAMED_SLOT(FArguments, TreeContent)

		/** The minimum height for the panel which contains the curve editor views. */
		SLATE_ARGUMENT(float, MinimumViewPanelHeight)

	SLATE_END_ARGS()

	SCurveEditorPanel();
	~SCurveEditorPanel();

	/**
	 * Construct a new curve editor panel widget
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor);

	/**
	 * Access the combined command list for this curve editor and panel widget
	 */
	TSharedPtr<FUICommandList> GetCommands() const
	{
		return CommandList;
	}

	/**
	 * Access the details view used for editing selected keys
	 */
	TSharedPtr<class SCurveKeyDetailPanel> GetKeyDetailsView() const
	{
		return KeyDetailsView;
	}

	/**
	 * Access the filter panel
	 */
	TSharedPtr<class SCurveEditorFilterPanel> GetFilterPanel() const
	{
		return FilterPanel;
	}

	/**
	 * Access the tool properties panel
	 */
	TSharedPtr<class SCurveEditorToolProperties> GetToolPropertiesPanel() const
	{
		return ToolPropertiesPanel;
	}

	void AddView(TSharedRef<SCurveEditorView> ViewToAdd);

	void RemoveView(TSharedRef<SCurveEditorView> ViewToRemove);

	/** This returns an extender which is pre-configured with the standard set of Toolbar Icons. Implementers of SCurveEditorPanel should use this
	* to generate the icons and then add any additional context-specific icons (such as save buttons in the Asset Editor) to ensure that the Curve
	* Editor has a consistent set (and order) of icons across all usages.
	*/
	TSharedPtr<FExtender> GetToolbarExtender();

	/** Access the cached geometry of the outer scroll panel that contains this panel's views */
	const FGeometry& GetScrollPanelGeometry() const;

	/** Access the cached geometry of container housing all this panel's views */
	const FGeometry& GetViewContainerGeometry() const;

	/** Get all the views stored in this panel. */
	TArrayView<const TSharedPtr<SCurveEditorView>> GetViews() const;

	/** Get the grid line tint to be used for views on panel */
	FLinearColor GetGridLineTint() const { return GridLineTintAttribute.Get(); }

	/** Scroll this panel's view scroll box vertically by the specified amount */
	void ScrollBy(float Amount);

	/**
	 * Find all the views that the specified curve is being displayed on
	 * @note: Returns an in-place iterator to this curve's view mapping. Adding or removing curves from views *will* invalidate this iterator.
	 *
	 * @param InCurveID The identifier of the curve to find views for
	 * @return An iterator to all the views that this cuvrve is displayed within.
	 */
	TMultiMap<FCurveModelID, TSharedRef<SCurveEditorView>>::TConstKeyIterator FindViews(TRetainedRef<FCurveModelID> InCurveID)
	{
		return CurveViews.CreateConstKeyIterator(InCurveID.Get());
	}

	/**
	 * Remove the specified curve from all views it is currently displayed on.
	 */
	void RemoveCurveFromViews(FCurveModelID InCurveID);

	/** Get the last set View Mode for this UI. Utility function for the UI. */
	ECurveEditorViewID GetViewMode() const { return DefaultViewID; }

	/** Undo occurred, invalidate or update internal structures */
	void PostUndo();

	/** Reset Stored Min/Max's*/
	void ResetMinMaxes();

	/** Delegate for when the chosen filter class has changed */
	FSimpleDelegate OnFilterClassChanged;
	void FilterClassChanged();

private:
	// SWidget Interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	// ~SWidget Interface

	/*~ Keyboard interaction */
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	TSharedRef<SWidget> MakeTimeSnapMenu();
	FText GetTimeSnapMenuTooltip() const;

	TSharedRef<SWidget> MakeGridSpacingMenu();
	TSharedRef<SWidget> MakeAxisSnapMenu();
	TSharedRef<SWidget> MakeViewModeMenu();

	bool IsInlineEditPanelEditable() const;
	EVisibility ShouldInstructionOverlayBeVisible() const;

private:

	/**
	 * Get the visibility for the value splitter control
	 */
	EVisibility GetSplitterVisibility() const;

	/*~ Event bindings */
	void UpdateTime();
	void UpdateEditBox();
	void UpdateCommonCurveInfo();

	/** Creates the drop-down list you see when changing Curve View options. */
	TSharedRef<SWidget> MakeCurveEditorCurveViewOptionsMenu();
	TSharedRef<SWidget> MakeCurveExtrapolationMenu(const bool bInPostExtrapolation);
	FSlateIcon GetCurveExtrapolationPreIcon() const;
	FSlateIcon GetCurveExtrapolationPostIcon() const;

	/** Creates the Curve Editor Filter UI and pre-populates it with the specified class. */
	void ShowCurveFilterUI(TSubclassOf<UCurveEditorFilterBase> FilterClass);

private:

	/**
	 * Bind command mappings for this widget
	 */
	void BindCommands();

	/**
	 * Assign new attributes to the currently selected keys
	 */
	void SetKeyAttributes(FKeyAttributes KeyAttributes, FText Description);

	/**
	 * Assign new curve attributes to all visible curves
	 */
	void SetCurveAttributes(FCurveAttributes CurveAttributes, FText Description);

	/** Compare all the currently selected keys' interp modes against the specified interp mode */
	bool CompareCommonInterpolationMode(ERichCurveInterpMode InterpMode) const
	{
		return CachedCommonKeyAttributes.HasInterpMode() && CachedCommonKeyAttributes.GetInterpMode() == InterpMode;
	}

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentMode(ERichCurveInterpMode InterpMode, ERichCurveTangentMode TangentMode) const
	{
		return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentMode() && CachedCommonKeyAttributes.GetTangentMode() == TangentMode;
	}

	/** Compare all the currently selected keys' tangent modes against the specified tangent mode */
	bool CompareCommonTangentWeightMode(ERichCurveInterpMode InterpMode, ERichCurveTangentWeightMode TangentWeightMode) const
	{
		return CompareCommonInterpolationMode(InterpMode) && CachedCommonKeyAttributes.HasTangentWeightMode() && CachedCommonKeyAttributes.GetTangentWeightMode() == TangentWeightMode;
	}

	/** Compare all the visible curves' pre-extrapolation modes against the specified extrapolation mode */
	bool CompareCommonPreExtrapolationMode(ERichCurveExtrapolation PreExtrapolationMode) const
	{
		return CachedCommonCurveAttributes.HasPreExtrapolation()  && CachedCommonCurveAttributes.GetPreExtrapolation() == PreExtrapolationMode;
	}

	/** Compare all the visible curves' post-extrapolation modes against the specified extrapolation mode */
	bool CompareCommonPostExtrapolationMode(ERichCurveExtrapolation PostExtrapolationMode) const
	{
		return CachedCommonCurveAttributes.HasPostExtrapolation() && CachedCommonCurveAttributes.GetPostExtrapolation() == PostExtrapolationMode;
	}

	/**
	 * Toggle weighted tangents on the current selection
	 */
	void ToggleWeightedTangents();

	/**
	 * Check whether we can toggle weighted tangents on the current selection
	 */
	bool CanToggleWeightedTangents() const;

	/**
	 * Check whether or not we can set a key interpolation on the current selection. If no keys are selected, you can't set an interpolation!
	 */
	bool CanSetKeyInterpolation() const;

	/** Sets the axis snapping to the specified value. Only supports X, Y and None. */
	void SetAxisSnapping(EAxisList::Type InAxis);

	/** Get a reference to the curve editor this panel represents. */
	TSharedPtr<FCurveEditor> GetCurveEditor() const { return CurveEditor; }

	float GetColumnFillCoefficient(int32 ColumnIndex) const
	{
		ensure(ColumnIndex == 0 || ColumnIndex == 1);
		return ColumnFillCoefficients[ColumnIndex];
	}

	/** Called when a column fill percentage is changed by a splitter slot. */
	void OnColumnFillCoefficientChanged(float FillCoefficient, int32 ColumnIndex);

	void OnSplitterFinishedResizing();
	
private:

	/**
	 * Create a new view for a model of the specified type, and add the curve to the view
	 *
	 * @param CurveModelID The ID of the curve we're creating a view for
	 * @param ViewTypeID   The ID of the view we'd like to create
	 * @param bPinned      Whether the view should be pinned or not
	 * @return A new view or nullptr if one could not be created
	 */
	TSharedPtr<SCurveEditorView> CreateViewOfType(FCurveModelID CurveModelID, ECurveEditorViewID ViewTypeID, bool bPinned);

private:

	/** The curve editor pointer */
	TSharedPtr<FCurveEditor> CurveEditor;

	/** Map from curve model ID to the views that it is on */
	TMultiMap<FCurveModelID, TSharedRef<SCurveEditorView>> CurveViews;

	/** Set of externally added views */
	TSet<TSharedRef<SCurveEditorView>> ExternalViews;

	/** (Optional) the current drag operation */
	TOptional<FCurveEditorDelayedDrag> DragOperation;

	/** Cached curve attributes that are common to all visible curves */
	FCurveAttributes CachedCommonCurveAttributes;

	/** Cached key attributes that are common to all selected keys */
	FKeyAttributes CachedCommonKeyAttributes;

	/** True if the current selection supports weighted tangents, false otherwise */
	bool bSelectionSupportsWeightedTangents;

	/** Attribute used for retrieving the desired grid line color */
	TAttribute<FLinearColor> GridLineTintAttribute;

	/** Attribute used for retrieving the tooltip for when the Time Snap control is disabled. Specifying this causes the Time Snap Adjustment to be disabled. */
	TAttribute<FText> DisabledTimeSnapTooltipAttribute;

	/** Edit panel */
	TSharedPtr<SCurveKeyDetailPanel> KeyDetailsView;

	/* Filter panel */
	TSharedPtr<SCurveEditorFilterPanel> FilterPanel;

	/** Tool options panel */
	TSharedPtr<SCurveEditorToolProperties> ToolPropertiesPanel;

	/** Map of edit UI widgets for each curve in the current selection set */
	TMap<FCurveModelID, TSharedPtr<SWidget>> CurveToEditUI;

	/** Command list for widget specific command bindings */
	TSharedPtr<FUICommandList> CommandList;

	/** Cached serial number from the curve editor selection. Used to update edit UIs when the selection changes. */
	uint32 CachedSelectionSerialNumber;

private:

	/** Sets the View Mode for the UI to the specified mode. This will destroy and re-create all views, but leave additional pinned views unmodified. */
	void SetViewMode(const ECurveEditorViewID NewViewMode);

	/** Compare if our current view mode matches the specified one. Utility function for the UI. */
	bool CompareViewMode(const ECurveEditorViewID InViewMode) const;

	/** Rebuild the Curve Views layout to match the currently specified View Mode. */
	void RebuildCurveViews();

	/** Reconstructs the properties widget on tool switch */
	void OnCurveEditorToolChanged(FCurveEditorToolID InToolId);

	/** Last Output Min and Max values for the views*/
	double LastOutputMin = DBL_MAX;
	double LastOutputMax = DBL_MIN;

	/** The last set View Mode for this UI. */
	ECurveEditorViewID DefaultViewID;

	TMultiMap<ECurveEditorViewID, TSharedRef<SCurveEditorView>> FreeViewsByType;

	/** The scrool box that all curve views live inside */
	TSharedPtr<SScrollBox> ScrollBox;

	/** The container for all our vertically laid out curve views. */
	TSharedPtr<SCurveEditorViewContainer> CurveViewsContainer;

private:

	/** Whether to explicitly refresh the views for this panel */
	bool bNeedsRefresh;

	/** Serial number cached from FCurveEditor::GetActiveCurvesSerialNumber() on tick */
	uint32 CachedActiveCurvesSerialNumber;

	/** A copy of the View Geometry used to represent the View portion of the Curve Editor. */
	FGeometry CachedViewGeometry;

	/** The fill coefficients of each column in the grid. */
	float ColumnFillCoefficients[2];

	TSharedPtr<class SSplitter> TreeViewSplitter;

	/** Container of objects that are being used to edit keys on the curve editor */
	TUniquePtr<FCurveEditorEditObjectContainer> EditObjects;

	TWeakPtr<FTabManager> WeakTabManager;
};