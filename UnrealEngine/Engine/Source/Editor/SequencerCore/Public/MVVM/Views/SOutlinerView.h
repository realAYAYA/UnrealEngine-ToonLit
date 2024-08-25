// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/BitArray.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Input/Reply.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModels/OutlinerColumns/IOutlinerColumn.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Views/TreeViewTraits.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"

class FDragDropEvent;
class FPaintArgs;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class ITableRow;
class SHeaderRow;
class SScrollBar;
class SWidget;
namespace UE::Sequencer { class FOutlinerViewModel; }
struct FGeometry;
struct FKeyEvent;
struct FPointerEvent;
struct FSlateBrush;

namespace UE::Sequencer
{

struct FSelectionEventSuppressor;
struct FOutlinerHeaderRowWidgetMetaData;

class FViewModel;
class FSequencerCoreSelection;
class SOutlinerViewRow;
class STrackAreaView;
class STrackLane;
class IOutlinerColumn;

enum class EOutlinerColumnGroup : uint8;

enum class ETreeRecursion
{
	Recursive, NonRecursive
};


/** The tree view used in the sequencer */
class SEQUENCERCORE_API SOutlinerView
	: public STreeView<TWeakViewModelPtr<IOutlinerExtension>>
{
public:

	SLATE_BEGIN_ARGS(SOutlinerView){}

		SLATE_ARGUMENT( TSharedPtr<FSequencerCoreSelection>, Selection )
		/** Externally supplied scroll bar */
		SLATE_ARGUMENT( TSharedPtr<SScrollBar>, ExternalScrollbar )

	SLATE_END_ARGS()

	static const FName TrackNameColumn;

	SOutlinerView();
	~SOutlinerView();

	/** Construct this widget */
	void Construct(const FArguments& InArgs, TWeakPtr<FOutlinerViewModel> InWeakOutliner, const TSharedRef<STrackAreaView>& InTrackArea);
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	TSharedPtr<FOutlinerViewModel> GetOutlinerModel() const;

	/** @return the number of root nodes this tree contains */
	int32 GetNumRootNodes() const { return RootNodes.Num(); }

	float GetVirtualTop() const;

	void GetVisibleItems(TArray<TViewModelPtr<IOutlinerExtension>>& OutItems) const;

	void ForceSetSelectedItems(const TSet<TWeakViewModelPtr<IOutlinerExtension>>& InItems);

public:

	/** Get the tree item model at the specified physical vertical position */
	TViewModelPtr<IOutlinerExtension> HitTestNode(float InPhysical) const;

	/** Convert the specified physical vertical position into an absolute virtual position, ignoring expanded states */
	float PhysicalToVirtual(float InPhysical) const;

	/** Convert the specified absolute virtual position into a physical position in the tree.
	 * @note: Will not work reliably for virtual positions that are outside of the physical space
	 */
	float VirtualToPhysical(float InVirtual) const;

	void ReportChildRowGeometry(const TViewModelPtr<IOutlinerExtension>& InNode, const FGeometry& InGeometry);

public:

	/** Refresh this tree as a result of the underlying tree data changing */
	void Refresh();

	/** Expand or collapse nodes */
	void ToggleExpandCollapseNodes(ETreeRecursion Recursion = ETreeRecursion::Recursive, bool bExpandAll = false, bool bCollapseAll = false);

	/** Scroll this tree view by the specified number of slate units */
	void ScrollByDelta(float DeltaInSlateUnits);

	bool IsColumnVisible(const FName& InName) const;

protected:

	/** Set the item's expansion state, including all of its children */
	void ExpandCollapseNode(TViewModelPtr<IOutlinerExtension> InDataModel, bool bExpansionState, ETreeRecursion Recursion);

	/** Generate a row for a particular node */
	virtual TSharedRef<ITableRow> OnGenerateRow(TWeakViewModelPtr<IOutlinerExtension> InDisplayNode, const TSharedRef<STableViewBase>& OwnerTable);

	void CreateTrackLanesForRow(TSharedRef<SOutlinerViewRow> InRow, TViewModelPtr<IOutlinerExtension> InDataModel);
	TSharedPtr<STrackLane> FindOrCreateParentLane(TViewModelPtr<IOutlinerExtension> InDataModel);

	/** Gather the children from the specified node */
	void OnGetChildren(TWeakViewModelPtr<IOutlinerExtension> InParent, TArray<TWeakViewModelPtr<IOutlinerExtension>>& OutChildren) const;

	/** Generate a widget for the specified Node and Column */
	TSharedRef<SWidget> GenerateWidgetForColumn(TViewModelPtr<IOutlinerExtension> InDataModel, const FName& ColumnId, const TSharedRef<SOutlinerViewRow>& Row) const;

	/** Called when a node has been expanded or collapsed */
	void OnExpansionChanged(TWeakViewModelPtr<IOutlinerExtension> InItem, bool bIsExpanded);

	// Tree selection methods which must be overriden to maintain selection consistency with the rest of sequencer.
	virtual void Private_UpdateParentHighlights() override;
	virtual void Private_SetItemSelection( TWeakViewModelPtr<IOutlinerExtension> TheItem, bool bShouldBeSelected, bool bWasUserDirected = false ) override;
	virtual void Private_ClearSelection() override;
	virtual void Private_SelectRangeFromCurrentTo( TWeakViewModelPtr<IOutlinerExtension> InRangeSelectionEnd ) override;
	virtual void Private_SignalSelectionChanged( ESelectInfo::Type SelectInfo ) override;

	virtual void OnRightMouseButtonDown(const FPointerEvent& MouseEvent) override;
	virtual void OnRightMouseButtonUp(const FPointerEvent& MouseEvent) override;

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

public:

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

protected:

	// Private, unimplemented overloaded name for SetItemSelection to prevent external calls - use ForceSetItemSelection instead
	void SetItemSelection();
	// Private, unimplemented overloaded name for SetItemSelection to prevent external calls - use ForceSetItemSelection instead
	void ClearSelection();

	void UpdateViewSelectionFromModel();
	void UpdateModelSelectionFromView();

	void HandleTableViewScrolled(double InScrollOffset);
	void UpdatePhysicalGeometry(bool bIsRefresh);

	/** Handles the context menu opening when right clicking on the tree view. */
	TSharedPtr<SWidget> OnContextMenuOpening();

	void SetItemExpansionRecursive(TWeakViewModelPtr<IOutlinerExtension> InItem, bool bIsExpanded);

public:

	/** Structure used to cache physical geometry for a particular node */
	struct FCachedGeometry
	{
		FCachedGeometry(TWeakViewModelPtr<IOutlinerExtension> InWeakItem,
			float InPhysicalTop, float InPhysicalHeight,
			float InVirtualTop, float InVirtualHeight,
			float InVirtualNestedHeight)
			: WeakItem(MoveTemp(InWeakItem))
			, PhysicalTop(InPhysicalTop), PhysicalHeight(InPhysicalHeight)
			, VirtualTop(InVirtualTop), VirtualHeight(InVirtualHeight), VirtualNestedHeight(InVirtualNestedHeight)
		{}

		TWeakViewModelPtr<IOutlinerExtension> WeakItem;
		float PhysicalTop, PhysicalHeight;
		float VirtualTop, VirtualHeight, VirtualNestedHeight;
	};

	/** Access all the physical nodes currently visible on the sequencer */
	const TArray<FCachedGeometry>& GetAllVisibleNodes() const { return PhysicalNodes; }

	/** Add a SOutlinerView object that should be modified or updated when this Treeview is updated */
	void AddPinnedTreeView(TSharedPtr<SOutlinerView> PinnedTreeView);

	/** Set a SOutlinerView object this Treeview is pinned to, for operations that should happen on the primary */
	void SetPrimaryTreeView(TSharedPtr<SOutlinerView> InPrimaryTreeView) { PrimaryTreeView = InPrimaryTreeView; }

	/** Set whether this TreeView should show only pinned nodes or only non-pinned nodes  */
	void SetShowPinned(bool bShowPinned) { bShowPinnedNodes = bShowPinned; }

	/** Updates the list of visible outliner columns and regenerates columns in the outliner view */
	void SetOutlinerColumns(const TArray<TSharedPtr<IOutlinerColumn>>& InOutlinerColumns);

protected:

	/** Linear, sorted array of nodes that we currently have generated widgets for */
	TArray<FCachedGeometry> PhysicalNodes;

	int32 CreateOutlinerColumnsForGroup(int32 ColumnIndex, EOutlinerColumnGroup Group);

	/** Populate the map of column definitions, and add relevant columns to the header row. Must be called when outliner columns change */
	void UpdateOutlinerColumns();

	/** Insert a separator column at the specified column index, with a unique identifier */
	void InsertSeparatorColumn(int32 InsertIndex, int32 SeparatorID);

	FReply OnDragRow(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent, TSharedRef<SOutlinerViewRow> InRow);

	FString OnItemToString_Debug(TWeakViewModelPtr<IOutlinerExtension> InWeakModel);

protected:

	using FColumnGenerator = TFunction<TSharedPtr<SWidget>(const FCreateOutlinerColumnParams& Params, const TSharedRef<SOutlinerViewRow>&)>;

	/** The tree view's header row (hidden) */
	TSharedPtr<SHeaderRow> HeaderRow;

	/** MetaData pertaining to each column within HeaderRow */
	TSharedPtr<FOutlinerHeaderRowWidgetMetaData> ColumnMetaData;

	/** The outliner view model */
	TWeakPtr<FOutlinerViewModel> WeakOutliner;

	/** Cached copy of the root nodes from the tree data */
	TArray<TWeakViewModelPtr<IOutlinerExtension>> RootNodes;

	/** Column definitions for each of the columns in the tree view */
	TMap<FName, FColumnGenerator> ColumnGenerators;

	TSharedPtr<FSequencerCoreSelection> Selection;
	TUniquePtr<FSelectionEventSuppressor> DelayedEventSuppressor;

	/** Strong pointer to the track area so we can generate track lanes as we need them */
	TSharedPtr<STrackAreaView> TrackArea;

	/** SOutlinerView objects that should be modified or updated when this Treeview is updated */
	TArray<TSharedPtr<SOutlinerView>> PinnedTreeViews;

	/** The SOutlinerView object this SOutlinerView is pinned to, or nullptr if not pinned */
	TWeakPtr<SOutlinerView> PrimaryTreeView;

	/** Visible Outliner columns to display in the outliner view */
	TArray<TSharedPtr<IOutlinerColumn>> OutlinerColumns;

	float VirtualTop;

	/** When true, the tree selection is being updated from a change in the sequencer selection. */
	bool bUpdatingTreeSelection;

	/** Right mouse button is down, don't update sequencer selection. */
	bool bRightMouseButtonDown;

	/** Whether this tree is for pinned nodes or non-pinned nodes */
	bool bShowPinnedNodes;

	/** Whether we have pending selection changes to broadcast */
	bool bSelectionChangesPending;

	/** Whether physical geometry information should be recomputed */
	bool bRefreshPhysicalGeometry;
};

} // namespace UE::Sequencer

