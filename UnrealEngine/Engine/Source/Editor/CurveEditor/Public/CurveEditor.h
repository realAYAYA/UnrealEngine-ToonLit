// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "CurveDataAbstraction.h"
#include "CurveEditorHelpers.h"
#include "CurveEditorScreenSpace.h"
#include "CurveEditorSelection.h"
#include "CurveEditorSnapMetrics.h"
#include "CurveEditorTypes.h"
#include "CurveModel.h"
#include "Delegates/Delegate.h"
#include "EditorUndoClient.h"
#include "HAL/PlatformCrt.h"
#include "IBufferedCurveModel.h"
#include "ICurveEditorBounds.h"
#include "ICurveEditorDragOperation.h"
#include "ICurveEditorModule.h"
#include "ICurveEditorToolExtension.h"
#include "ITimeSlider.h"
#include "Internationalization/Text.h"
#include "Math/Axis.h"
#include "Math/Range.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/FrameRate.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Tree/CurveEditorTree.h"

class FCurveEditor;
class FCurveEditor;
class FCurveModel;
class FUICommandList;
class IBufferedCurveModel;
class ICurveEditorExtension;
class ICurveEditorToolExtension;
class ITimeSliderController;
class SCurveEditorPanel;
class SCurveEditorView;
class UCurveEditorCopyBuffer;
class UCurveEditorCopyableCurveKeys;
class UCurveEditorSettings;
struct FCurveDrawParams;
struct FCurveEditorInitParams;
struct FCurveEditorSnapMetrics;
struct FGeometry;

DECLARE_DELEGATE_OneParam(FOnSetBoolean, bool)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveToolChanged, FCurveEditorToolID)
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCurveArrayChanged, FCurveModel*, bool /*displayed*/,const FCurveEditor*);

/** Enums to describe supported tangent types, by default support all but smart auto*/
enum class ECurveEditorTangentTypes : int32
{
	InterpolationConstant = 0x1,
	InterpolationLinear = 0x2,
	InterpolationCubicAuto = 0x4,

	InterpolationCubicUser = 0x8,
	InterpolationCubicBreak = 0x10,
	InterpolationCubicWeighted = 0x20,
	InterpolationCubicSmartAuto = 0x40,

};

class CURVEEDITOR_API FCurveEditor 
	: public FEditorUndoClient
	, public TSharedFromThis<FCurveEditor>
{
public:

	/**
	 * Container holding the current key/tangent selection
	 */
	FCurveEditorSelection Selection;

public:

	/** Attribute used to retrieve the current input snap rate (also used for display) */
	TAttribute<FFrameRate> InputSnapRateAttribute;

	/** Attribute used to retrieve the current value-axis grid line state */
	TAttribute<TOptional<float>> FixedGridSpacingAttribute;

	/** Attribute used to determine if we should snap input values */
	TAttribute<bool> InputSnapEnabledAttribute;

	/** Attribute used to determine if we should snap output values */
	TAttribute<bool> OutputSnapEnabledAttribute;

	/** Delegate that is invoked when the input snapping has been enabled/disabled */
	FOnSetBoolean OnInputSnapEnabledChanged;

	/** Delegate that is invoked when the output snapping has been enabled/disabled */
	FOnSetBoolean OnOutputSnapEnabledChanged;

	/** Attribute used for determining default attributes to apply to a newly create key */
	TAttribute<FKeyAttributes> DefaultKeyAttributes;

	/** Grid line label text format strings for the X and Y axis */
	TAttribute<FText> GridLineLabelFormatXAttribute, GridLineLabelFormatYAttribute;

	/** Padding applied to zoom-to-fit the input */
	TAttribute<double> InputZoomToFitPadding;

	/** Padding applied to zoom-to-fit the output */
	TAttribute<double> OutputZoomToFitPadding;

	/** Delegate that is invoked when a tool becomes active. Also fired when the tool goes inactive. */
	FOnActiveToolChanged OnActiveToolChangedDelegate;
public:

	/**
	 * Constructor
	 */
	FCurveEditor();

	/**
	 * Non-copyable (shared ptr semantics)
	 */
	FCurveEditor(const FCurveEditor&) = delete;
	FCurveEditor& operator=(const FCurveEditor&) = delete;

	virtual ~FCurveEditor();

	void InitCurveEditor(const FCurveEditorInitParams& InInitParams);

	virtual int32 GetSupportedTangentTypes();

public:

	void SetPanel(TSharedPtr<SCurveEditorPanel> InPanel);

	TSharedPtr<SCurveEditorPanel> GetPanel() const;

	void SetView(TSharedPtr<SCurveEditorView> InPanel);

	TSharedPtr<SCurveEditorView> GetView() const;

	FCurveEditorScreenSpaceH GetPanelInputSpace() const;

	void ResetMinMaxes();

public:
	/**
	 * Zoom the curve editor to fit all the selected curves (or all curves if none selected)
	 *
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFit(EAxisList::Type Axes = EAxisList::All);
	/**
	 * Zoom the curve editor to fit all the currently visible curves
	 *
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFitAll(EAxisList::Type Axes = EAxisList::All);
	/**
	 * Zoom the curve editor to fit the requested curves.
	 *
	 * @param CurveModelIDs The curve IDs to zoom to fit.
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFitCurves(TArrayView<const FCurveModelID> CurveModelIDs, EAxisList::Type Axes = EAxisList::All);
	/**
	 * Zoom the curve editor to fit all the current key selection. Zooms to fit all if less than 2 keys are selected.
	 *
	 * @param Axes         (Optional) Axes to lock the zoom to
	 */
	void ZoomToFitSelection(EAxisList::Type Axes = EAxisList::All);
	/**
	* Assign a new bounds container to this curve editor
	*/
	void SetBounds(TUniquePtr<ICurveEditorBounds>&& InBounds);
	/**
	 * Retrieve the current curve editor bounds implementation
	 */
	const ICurveEditorBounds& GetBounds() const { return *Bounds.Get(); }
	/**
	 * Retrieve the current curve editor bounds implementation
	 */
	ICurveEditorBounds& GetBounds() { return *Bounds.Get();	}
	/*
	 * Sets a Time Slider controller for this Curve Editor to be sync'd against. Can be null.
	 */
	void SetTimeSliderController(TSharedPtr<ITimeSliderController> InTimeSliderController) { WeakTimeSliderController = InTimeSliderController; }
	/**
	 * Retrieve the optional Time Slider Controller that this Curve Editor may be sync'd with.
	 */
	TSharedPtr<ITimeSliderController> GetTimeSliderController() const { return WeakTimeSliderController.Pin(); }
	/**
	* Retrieve this curve editor's command list
	*/
	TSharedPtr<FUICommandList> GetCommands() const { return CommandList; }
	/**
	* Returns true of the specified tool is currently active.
	*/
	bool IsToolActive(const FCurveEditorToolID InToolID) const;
	/**
	* Attempts to make the specified tool the active tool. This will cancel the current tool if there is one.
	*/
	void MakeToolActive(const FCurveEditorToolID InToolID);
	/**
	* Attempts to get the currently active tool. Will return nullptr if there is no active tool.
	* Do not store a reference to this returned pointer, instead only store FCurveEditorToolIDs!
	*/
	ICurveEditorToolExtension* GetCurrentTool() const;

	FCurveEditorToolID AddTool(TUniquePtr<ICurveEditorToolExtension>&& InTool);
	
	/** Nudge left or right*/
	void TranslateSelectedKeys(double SecondsToAdd);
	void TranslateSelectedKeysLeft();
	void TranslateSelectedKeysRight();

	/** Snap time to the first selected key */
	void SnapToSelectedKey();

	/** Step to next or previous key from the current time */
	void StepToNextKey();
	void StepToPreviousKey();

	/** Step forward/backward, jump to start/end */
	void StepForward();
	void StepBackward();
	void JumpToStart();
	void JumpToEnd();

	/** Selection range for ie. looping playback */
	void SetSelectionRangeStart();
	void SetSelectionRangeEnd();
	void ClearSelectionRange();

	/** Selection */
	void SelectAllKeys();
	void SelectForward();
	void SelectBackward();
	void SelectNone();

	/** Toggle the expansion state of the selected nodes or all nodes if none selected */
	void ToggleExpandCollapseNodes(bool bRecursive);

	/**
	 * Find a curve by its ID
	 *
	 * @return a ptr to the curve if found, nullptr otherwise
	 */
	FCurveModel* FindCurve(FCurveModelID CurveID) const;
	/**
	* Add a new curve to this editor
	*/
	FCurveModelID AddCurve(TUniquePtr<FCurveModel>&& InCurve);
	FCurveModelID AddCurveForTreeItem(TUniquePtr<FCurveModel>&& InCurve, FCurveEditorTreeItemID TreeItemID);

	/**
	* Remove a curve from this editor.
	*/
	void RemoveCurve(FCurveModelID InCurveID);

	/** Remove all curves from this editor */
	void RemoveAllCurves();

	bool IsCurvePinned(FCurveModelID InCurveID) const;

	void PinCurve(FCurveModelID InCurveID);

	void UnpinCurve(FCurveModelID InCurveID);

	const TSet<FCurveModelID>& GetPinnedCurves() const
	{
		return PinnedCurves;
	}

	const SCurveEditorView* FindFirstInteractiveView(FCurveModelID InCurveID) const;

	/**
	 * Retrieve this curve editor's settings
	 */
	UCurveEditorSettings* GetSettings() const { return Settings; }
	/**
	* Access all the curves currently contained in the Curve Editor regardless of visibility.
	*/
	const TMap<FCurveModelID, TUniquePtr<FCurveModel>>& GetCurves() const;

	FCurveEditorSelection& GetSelection() { return Selection; }
	/**
	 * Generate a utility struct for snapping values
	 */
	FCurveSnapMetrics GetCurveSnapMetrics(FCurveModelID CurveModel) const;

	/**
	 * Returns the value grid line spacing state
	 */
	TOptional<float> GetGridSpacing() const { return FixedGridSpacingAttribute.Get(); }

	/**
	 * Returned the cached struct for snapping editing movement to a specific axis based on user preferences.
	 */
	FCurveEditorAxisSnap GetAxisSnap() const { return AxisSnapMetrics; }
	void SetAxisSnap(const FCurveEditorAxisSnap& InAxisSnap) { AxisSnapMetrics = InAxisSnap; }
	TAttribute<FText> GetGridLineLabelFormatXAttribute() const { return GridLineLabelFormatXAttribute; }
	TAttribute<FText> GetGridLineLabelFormatYAttribute() const { return GridLineLabelFormatYAttribute; }
	TAttribute<FKeyAttributes> GetDefaultKeyAttributes() const { return DefaultKeyAttributes; }

	void SuppressBoundTransformUpdates(bool bSuppress) { bBoundTransformUpdatesSuppressed = bSuppress; }
	bool AreBoundTransformUpdatesSuppressed() const { return bBoundTransformUpdatesSuppressed; }

	/** Return the curve model IDs that are selected in the tree or have selected keys */
	TSet<FCurveModelID> GetSelectionFromTreeAndKeys() const;

	TSet<FCurveModelID> GetEditedCurves() const;
	/** Attribute used for determining default attributes to apply to a newly create key */
	TAttribute<FKeyAttributes> GetDefaultKeyAttribute() const { return DefaultKeyAttributes; }
	/** Create a copy of the specified set of curves. */
	void AddBufferedCurves(const TSet<FCurveModelID>& InCurves);
	/** Attempts to apply the buffered curve to the passed in curve set. Returns true on success. */
	bool ApplyBufferedCurves(const TSet<FCurveModelID>& InCurvesToApplyTo, const bool bSwapBufferCurves);
	/** Return the number of stored Buffered Curves. */
	int32 GetNumBufferedCurves() const { return BufferedCurves.Num(); }
	/** Return the array of buffered curves */
	const TArray<TUniquePtr<IBufferedCurveModel>>& GetBufferedCurves() const { return BufferedCurves; }
	/** Returns whether the buffered curve is to be acted on, ie. selected, in the tree view or with selected keys */
	bool IsActiveBufferedCurve(const TUniquePtr<IBufferedCurveModel>& BufferedCurve) const;

	// ~FCurveEditor

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	// ~FEditorUndoClient

	const TArray<TSharedRef<ICurveEditorExtension>> GetEditorExtensions() const
	{
		return EditorExtensions;
	}

	const TMap<FCurveEditorToolID, TUniquePtr<ICurveEditorToolExtension>>& GetToolExtensions() const
	{
		return ToolExtensions;
	}

public:

	/**
	 * Retrieve a tree item from its ID
	 */
	FCurveEditorTreeItem& GetTreeItem(FCurveEditorTreeItemID ItemID);

	/**
	 * Retrieve a tree item from its ID
	 */
	const FCurveEditorTreeItem& GetTreeItem(FCurveEditorTreeItemID ItemID) const;

	/**
	 * Finds a tree item from its ID
	 */
	FCurveEditorTreeItem* FindTreeItem(FCurveEditorTreeItemID ItemID);

	/**
	 * Finds a tree item from its ID
	 */
	const FCurveEditorTreeItem* FindTreeItem(FCurveEditorTreeItemID ItemID) const;

	/**
	 * Get const access to the entire set of root tree items
	 */
	const TArray<FCurveEditorTreeItemID>& GetRootTreeItems() const;


	/**
	 * Find a tree ID id associated with a CurveModelID
	 */
	FCurveEditorTreeItemID GetTreeIDFromCurveID(FCurveModelID CurveID) const;

	/**
	 * Add a new tree item to this curve editor
	 */
	FCurveEditorTreeItem* AddTreeItem(FCurveEditorTreeItemID ParentID);

	/**
	 * Remove a tree item from the curve editor
	 */
	void RemoveTreeItem(FCurveEditorTreeItemID ItemID);

	/**
	 * Remove all tree items from the curve editor
	 */
	void RemoveAllTreeItems();

	/**
	 * Set the tree selection directly
	 */
	void SetTreeSelection(TArray<FCurveEditorTreeItemID>&& TreeItems);

	/**
	 * Removes items from the current tree selection.
	 */
	void RemoveFromTreeSelection(TArrayView<const FCurveEditorTreeItemID> TreeItems);

	/**
	 * Check whether this tree item is selected
	 */
	ECurveEditorTreeSelectionState GetTreeSelectionState(FCurveEditorTreeItemID TreeItemID) const;

	/**
	 * Retrieve the current tree selection
	 */
	const TMap<FCurveEditorTreeItemID, ECurveEditorTreeSelectionState>& GetTreeSelection() const;

	/**
	 * Access the curve editor tree.
	 */
	FCurveEditorTree* GetTree()
	{
		return &Tree;
	}

	/**
	 * Access the curve editor tree.
	 */
	const FCurveEditorTree* GetTree() const
	{
		return &Tree;
	}

	/**
	Whether or not we are are doign a direct selection, could be used to see why a curve model is being created or destroyed, by direct selection or by sequencer filtering?
	*/
	bool IsDoingDirectSelection() const
	{
		return Tree.IsDoingDirectSelection();
	}


	/**
	 * Retrieve a serial number that is incremented any time a curve is added or removed
	 */
	uint32 GetActiveCurvesSerialNumber() const
	{
		return ActiveCurvesSerialNumber;
	}

public:

	/**
	 * Check whether this curve editor can automatically zoom to the current selection
	 */
	bool ShouldAutoFrame() const;
public:

	/**
	 * Check whether keys should be snapped to the input display rate when dragging around
	 */
	bool IsInputSnappingEnabled() const;
	void ToggleInputSnapping();

	/**
	 * Check whether keys should be snapped to the output snap interval when dragging around
	 */
	bool IsOutputSnappingEnabled() const;
	void ToggleOutputSnapping();

public:

	/**
	 * Cut the currently selected keys
	 */
	void CutSelection();
	
	/**
	 * Copy the currently selected keys
	 */
	void CopySelection() const;

	/**
	 * Returns whether the current clipboard contains objects which CurveEditor can paste
	 */
	bool CanPaste(const FString& TextToImport) const;

protected:
	void ImportCopyBufferFromText(const FString& TextToImport, /*out*/ TArray<UCurveEditorCopyBuffer*>& ImportedCopyBuffers) const;
	TSet<FCurveModelID> GetTargetCurvesForPaste() const;
	bool CopyBufferCurveToCurveID(const UCurveEditorCopyableCurveKeys* InSourceCurve, const FCurveModelID InTargetCurve, TOptional<double> InTimeOffset, const bool bInAddToSelection, const bool bInOverwriteRange);

	void GetChildCurveModelIDs(const FCurveEditorTreeItemID TreeItemID, TSet<FCurveModelID>& CurveModelIDs) const;

public:
	/**
	 * Paste keys
	 */
	void PasteKeys(TSet<FCurveModelID> CurveModelIDs, const bool bInOverwriteRange = false);

	/**
	 * Delete the currently selected keys
	 */
	void DeleteSelection();

	/**
	 * Flatten the tangents on the selected keys
	 */
	void FlattenSelection();

	/**
	 * Straighten the tangents on the selected keys
	 */
	void StraightenSelection();

	/**
	 * Set random curve colors
	 */
	void SetRandomCurveColorsForSelected();

	/**
	 * Pick a curve color and set on selected
	 */	
	void SetCurveColorsForSelected();

	/**
	* Do we currently have keys to flatten or straighten?
	*/
	bool CanFlattenOrStraightenSelection() const;

public:

	/**
	 * Called by SCurveEditorPanel to update the allocated geometry for this curve editor.
	 */
	void UpdateGeometry(const FGeometry& CurrentGeometry);

public:

	/**
	 * Called by SCurveEditorPanel to determine where to draw grid lines along the X-axis. This allows
	 * synchronization with an external data source (such as Sequencer's Timeline ticker). A similar
	 * function for the Y grid lines is not provided due to the Curve Editor's ability to have multiple
	 * views with repeated gridlines and values.
	 */
	virtual void GetGridLinesX(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const
	{
		ConstructXGridLines(MajorGridLines, MinorGridLines, MajorGridLabels);
	}

	/**
	 * Bind UI commands that this curve editor responds to
	 */
	void BindCommands();

public:

	/** Suspend or resume broadcast of curve array changing  */
	void SuspendBroadcast()
	{
		SuspendBroadcastCount++;
	}

	void ResumeBroadcast()
	{
		SuspendBroadcastCount--;
		checkf(SuspendBroadcastCount >= 0, TEXT("Suspend/Resume broadcast mismatch Curve Editor!"));
	}

	bool IsBroadcasting()
	{
		return SuspendBroadcastCount == 0;
	}

	void BroadcastCurveChanged(FCurveModel* InCurve);

protected:

	/**
	 * Construct grid lines along the current display frame rate or time-base
	 */
	void ConstructXGridLines(TArray<float>& MajorGridLines, TArray<float>& MinorGridLines, TArray<FText>* MajorGridLabels) const;

	/**
	 * Internal zoom to fit implementation
	 *
	 * @param Axes        The axes to zoom (only X or Y supported)
	 * @param CurveKeySet Map from curve ID to keys that should be considered for zoom. Empty key sets will cause the entire curve range to be zoomed.
	 */
	void ZoomToFitInternal(EAxisList::Type Axes, const TMap<FCurveModelID, FKeyHandleSet>& CurveKeySet);

	/**
	*	Apply a specific buffered curve to a specific target curve.
	*/
	void ApplyBufferedCurveToTarget(const IBufferedCurveModel* BufferedCurve, FCurveModel* TargetCurve);

	void OnCustomColorsChanged();

protected:

	/** Curve editor bounds implementation */
	TUniquePtr<ICurveEditorBounds> Bounds;

	/** Map from curve model ID to the actual curve model */
	TMap<FCurveModelID, TUniquePtr<FCurveModel>> CurveData;
	/** Map from curve model ID to its originating tree item */
	TMap<FCurveModelID, FCurveEditorTreeItemID> TreeIDByCurveID;

	/** Set of pinned curve models */
	TSet<FCurveModelID> PinnedCurves;

	TWeakPtr<SCurveEditorPanel> WeakPanel;

	TWeakPtr<SCurveEditorView> WeakView;

	/** Hierarchical information pertaining to curve data */
	FCurveEditorTree Tree;

	/** The currently active tool if any. If unset then no tool is currently active and the next selection will default to the first tool. */
	TOptional<FCurveEditorToolID> ActiveTool;

	/** UI command list of actions mapped to this curve editor */
	TSharedPtr<FUICommandList> CommandList;

	/** Curve editor settings object */
	UCurveEditorSettings* Settings;

	/** List of editor extensions we have initialized. */
	TArray<TSharedRef<ICurveEditorExtension>> EditorExtensions;

	/** List of tool extensions we have initialized. */
	TMap<FCurveEditorToolID, TUniquePtr<ICurveEditorToolExtension>> ToolExtensions;

	/** Optional external Time Slider controller to sync with. Enables some additional functionality. */
	TWeakPtr<ITimeSliderController> WeakTimeSliderController;

	/** 
	* Should attempts to update the bounds of each curve be ignored? This allows tools to keep the bounds from being automatically updated each frame 
	* which allows Normalized views to push past their boundaries without the normalization ratio changing per-frame as you drag.
	*/
	bool bBoundTransformUpdatesSuppressed;
	
	/** Track which axis UI movements should be snapped to (where applicable) based on limitations imposed by the UI. */
	FCurveEditorAxisSnap AxisSnapMetrics;

	/** Buffered Curves. When a curve is buffered it is copied and the new copy is uniquely owned by the Curve Editor. */
	TArray<TUniquePtr<IBufferedCurveModel>> BufferedCurves;

	/** A serial number that is incremented any time the currently active set of curves are changed */
	uint32 ActiveCurvesSerialNumber;

	/** Counter to suspend broadcasting of changed delegates*/
	int32 SuspendBroadcastCount;

private:

	/** Cached physical size of the panel representing this editor */
	FVector2D CachedPhysicalSize;

public:
	/**
	* Delegate that's broadcast when the curve display changes.
	*/
	FOnCurveArrayChanged OnCurveArrayChanged;
};
