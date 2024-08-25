// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

//#include "SlateBasics.h"

#include "Model/TextureGraphInsightSession.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

class STextureGraphInsightBatchJobViewRow; /// Declare the concrete type of widget used for the raws of the tree view. Defined in the cpp file
class ITableRow;
class STableViewBase;

class TEXTUREGRAPHINSIGHT_API STextureGraphInsightBatchJobView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextureGraphInsightBatchJobView)
		: _inspectOnSimpleClick(false) {}
	SLATE_ARGUMENT(RecordID, recordID)
	SLATE_ARGUMENT(bool, inspectOnSimpleClick)
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	// TreeView Item Types
	class FItemData;
	using FItem = TSharedPtr<FItemData>;
	using FItemArray = TArray< FItem >;
	class FItemData
	{
	public:
		FItemData(RecordID id, bool isMainPhase = true) : _recordID(id), _isMainPhase(isMainPhase) {}
		TSharedPtr < STextureGraphInsightBatchJobViewRow > _widget;
		RecordID _recordID;
		FItemArray _children;
		bool _isMainPhase = true;
	};
	using SItemTreeView = STreeView< FItem >;

	// Standard delegates for the tree view
	TSharedRef<ITableRow>	OnGenerateRowForTree(FItem item, const TSharedRef<STableViewBase>& OwnerTable);
	FORCEINLINE void		OnGetChildrenForView(FItem item, FItemArray& children) { children = item->_children; };
	void					OnClickItemForTree(FItem item);
	void					OnDoubleClickItemForTree(FItem item);
	TSharedPtr<SWidget>		OnContextMenuOpeningForTree();
	TSharedPtr<SWidget>		OnContextMenuBatch(FItem batchItem);
	TSharedPtr<SWidget>		OnContextMenuJob(FItem jobItem);

	// Inspect on double click always
	// eventually trigger inspect on simple click
	bool _inspectOnSimpleClick = false;

	/// The list of root items
	FItemArray _rootItems;

	// The TreeView widget
	TSharedPtr<SItemTreeView> _treeView;


	void OnReplayBatch(RecordID batchId, bool captureRenderDoc);
	void OnReplayJob(RecordID batchId, bool captureRenderDoc);

	void OnBatchNew(RecordID batch);
	void OnBatchUpdate(RecordID batch);
	void OnEngineReset(int32);
};


class TEXTUREGRAPHINSIGHT_API STextureGraphInsightSessionView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextureGraphInsightSessionView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);

	TSharedPtr<STextureGraphInsightBatchJobView> _batchJobView;
};
