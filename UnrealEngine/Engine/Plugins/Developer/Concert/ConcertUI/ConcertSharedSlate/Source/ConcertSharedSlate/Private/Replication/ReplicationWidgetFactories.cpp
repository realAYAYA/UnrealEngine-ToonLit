// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/ReplicationWidgetFactories.h"

#include "Editor/Model/GenericReplicationStreamModel.h"
#include "Editor/View/MultiEditor/SMultiReplicationStreamEditor.h"
#include "Editor/View/ObjectEditor/SBaseReplicationStreamEditor.h"
#include "Editor/View/ObjectViewer/Property/SPropertyTreeView.h"

namespace UE::ConcertSharedSlate
{
	TSharedRef<IEditableReplicationStreamModel> CreateBaseStreamModel(
		TAttribute<FConcertObjectReplicationMap*> ReplicationMapAttribute,
		TSharedPtr<IStreamExtender> Extender
		)
	{
		return MakeShared<FGenericReplicationStreamModel>(MoveTemp(ReplicationMapAttribute), MoveTemp(Extender));
	}
	
	TSharedRef<IReplicationStreamEditor> CreateBaseStreamEditor(FCreateEditorParams EditorParams, FCreateViewerParams ViewerParams)
	{
		return SNew(SBaseReplicationStreamEditor, MoveTemp(EditorParams.DataModel), MoveTemp(EditorParams.ObjectSource), MoveTemp(EditorParams.PropertySource))
			.PropertyTreeView(MoveTemp(ViewerParams.PropertyTreeView))
			.ObjectColumns(MoveTemp(ViewerParams.ObjectColumns))
			.PrimaryObjectSort(ViewerParams.PrimaryObjectSort)
			.SecondaryObjectSort(ViewerParams.SecondaryObjectSort)
			.ObjectHierarchy(MoveTemp(ViewerParams.ObjectHierarchy))
			.NameModel(MoveTemp(ViewerParams.NameModel))
			.OnExtendObjectsContextMenu(MoveTemp(ViewerParams.OnExtendObjectsContextMenu))
			.LeftOfObjectSearchBar() [ MoveTemp(ViewerParams.LeftOfObjectSearchBar.Widget) ]
			.RightOfObjectSearchBar() [ MoveTemp(ViewerParams.RightOfObjectSearchBar.Widget) ]
			.IsEditingEnabled(MoveTemp(EditorParams.IsEditingEnabled))
			.EditingDisabledToolTipText(MoveTemp(EditorParams.EditingDisabledToolTipText));
	}
	
	TSharedRef<IPropertyTreeView> CreateSearchablePropertyTreeView(FCreatePropertyTreeViewParams Params)
	{
		// The label column is always required
		const bool bHasLabel = Params.PropertyColumns.ContainsByPredicate([](const FPropertyColumnEntry& Entry)
		{
			return Entry.ColumnId == ReplicationColumns::Property::LabelColumnId;
		});
		if (!bHasLabel)
		{
			Params.PropertyColumns.Add(ReplicationColumns::Property::LabelColumn());
		}

		SReplicationTreeView<FReplicatedPropertyData>::FCustomFilter FilterDelegate = Params.FilterItem.IsBound()
			? SReplicationTreeView<FReplicatedPropertyData>::FCustomFilter::CreateLambda([Filter = MoveTemp(Params.FilterItem)](const TSharedPtr<FReplicatedPropertyData>& Item)
			{
				return Filter.Execute(*Item.Get()) == EFilterResult::PassesFilter;
			})
			: SReplicationTreeView<FReplicatedPropertyData>::FCustomFilter{};
		
		return SNew(SPropertyTreeView)
			.FilterItem(MoveTemp(FilterDelegate))
			.Columns(MoveTemp(Params.PropertyColumns))
			.ExpandableColumnLabel(ReplicationColumns::Property::LabelColumnId)
			.PrimarySort(Params.PrimaryPropertySort)
			.SecondarySort(Params.SecondaryPropertySort)
			.SelectionMode(ESelectionMode::Multi)
			.LeftOfSearchBar() [ MoveTemp(Params.LeftOfPropertySearchBar.Widget) ]
			.RightOfSearchBar() [ MoveTemp(Params.RightOfPropertySearchBar.Widget) ]
			.RowBelowSearchBar() [ MoveTemp(Params.RowBelowSearchBar.Widget) ]
			.NoItemsContent() [ MoveTemp(Params.NoItemsContent.Widget) ];
	}

	TSharedRef<IMultiReplicationStreamEditor> CreateBaseMultiStreamEditor(FCreateMultiStreamEditorParams EditorParams, FCreateViewerParams ViewerParams)
	{
		return SNew(SMultiReplicationStreamEditor, MoveTemp(EditorParams), MoveTemp(ViewerParams));
	}
}

