// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Metadata/PCGMetadata.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

class FPCGEditor;
class UPCGComponent;
class UPCGNode;
class UPCGParamData;
struct FPCGPoint;

struct FPCGMetadataInfo
{
	FName MetadataId = NAME_None;
	int8 Index = 0;
};

struct FPCGListViewItem
{
	int32 Index = INDEX_NONE;
	const FPCGPoint* PCGPoint = nullptr;
	const UPCGMetadata* PCGMetadata = nullptr;
	const UPCGParamData* PCGParamData = nullptr;
	int64 MetaDataItemKey = -1;
	const TMap<FName, FPCGMetadataInfo>* MetadataInfos = nullptr;
};

typedef TSharedPtr<FPCGListViewItem> PCGListviewItemPtr;

class SPCGListViewItemRow : public SMultiColumnTableRow<PCGListviewItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPCGListViewItemRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, const PCGListviewItemPtr& Item);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;
private:
	FText ConvertPointDataToText(const FPCGPoint* PCGPoint, const FName& ColumnId) const;
	FText ConvertMetadataAttributeToText(const FPCGMetadataAttributeBase* AttributeBase, const FPCGMetadataInfo* MetadataInfo, int64 ItemKey = 0) const;

protected:
	PCGListviewItemPtr InternalItem;
};

class SPCGEditorGraphAttributeListView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphAttributeListView) { }
	SLATE_END_ARGS()

	virtual ~SPCGEditorGraphAttributeListView() override;

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

private:
	TSharedRef<SHeaderRow> CreateHeaderRowWidget() const;

	void OnDebugObjectChanged(UPCGComponent* InPCGComponent);
	void OnInspectedNodeChanged(UPCGNode* InPCGNode);

	void OnGenerateUpdated(UPCGComponent* InPCGComponent);

	void RefreshAttributeList();
	void RefreshDataComboBox();

	TSharedRef<SWidget> OnGenerateDataWidget(TSharedPtr<FName> InItem) const;
	void OnSelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	FText OnGenerateSelectedDataText() const;
	int32 GetSelectedDataIndex() const;
	void GenerateColumnsFromMetadata(const UPCGMetadata* PCGMetadata);

	TSharedRef<ITableRow> OnGenerateRow(PCGListviewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable) const;

	void AddColumn(const FName& InColumnID, const FText& ColumnLabel, float ColumnWidth, EHorizontalAlignment HeaderHAlign = HAlign_Center, EHorizontalAlignment CellHAlign = HAlign_Right);
	void RemoveColumn(const FName& InColumnID);

	void AddIndexColumn();
	void RemoveIndexColumn();

	void AddPointDataColumns();
	void RemovePointDataColumns();

	void AddMetadataColumn(const FName& InColumnId, const FName& InMetadataId, const int8 InValueIndex = INDEX_NONE, const TCHAR* PostFix = nullptr);
	void RemoveMetadataColumns();

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	/** Cached PCGComponent being viewed */
	UPCGComponent* PCGComponent = nullptr;

	TSharedPtr<SHeaderRow> ListViewHeader;
	TSharedPtr<SListView<PCGListviewItemPtr>> ListView;
	TArray<PCGListviewItemPtr> ListViewItems;

	TSharedPtr<SComboBox<TSharedPtr<FName>>> DataComboBox;
	TArray<TSharedPtr<FName>> DataComboBoxItems;

	TMap<FName, FPCGMetadataInfo> MetadataInfos;
};
