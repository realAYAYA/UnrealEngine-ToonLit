// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/PCGStackContext.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "Misc/TextFilterExpressionEvaluator.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class STableViewBase;
class STextBlock;
template <typename OptionType> class SComboBox;

class FPCGEditor;
class FPCGMetadataAttributeBase;
class SPCGEditorGraphAttributeListView;
class UPCGComponent;
class UPCGPointData;
class UPCGData;
class UPCGEditorGraphNodeBase;
class UPCGMetadata;
class UPCGParamData;
struct FPCGDataCollection;
struct FPCGPoint;
enum class EPCGMetadataTypes : uint8;

class SComboButton;
class SHeaderRow;
class SSearchBox;
struct FSlateBrush;

struct FPCGListViewItem
{
	int32 Index = INDEX_NONE;
	const FPCGPoint* PCGPoint = nullptr;
};

struct FPCGColumnData
{
	TUniquePtr<const IPCGAttributeAccessor> DataAccessor;
	TUniquePtr<const IPCGAttributeAccessorKeys> DataKeys;
};

typedef TSharedPtr<FPCGListViewItem> PCGListviewItemPtr;

template <typename T, typename = void>
struct FTextAsNumberIsValid : std::false_type {};

/** Utility to see if a value type is supported by FText::AsNumber */
template <typename T>
struct FTextAsNumberIsValid<T, std::void_t<decltype(FText::AsNumber(std::declval<T>()))>> : std::true_type {};

class SPCGListViewItemRow : public SMultiColumnTableRow<PCGListviewItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SPCGListViewItemRow) {}
	SLATE_ARGUMENT(TSharedPtr<SPCGEditorGraphAttributeListView>, AttributeListView)
	SLATE_ARGUMENT(PCGListviewItemPtr, ListViewItem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnId) override;
	
private:
	TWeakPtr<SPCGEditorGraphAttributeListView> AttributeListView;
	PCGListviewItemPtr InternalItem;
};

class FPCGPointFilterExpressionContext : public ITextFilterExpressionContext
{
public:
	explicit FPCGPointFilterExpressionContext(const FPCGListViewItem* InRowItem, const TMap<FName, FPCGColumnData>* InPCGColumnData);

	// ~Begin ITextFilterExpressionContext interface
	virtual bool TestBasicStringExpression(const FTextFilterString& InValue, const ETextFilterTextComparisonMode InTextComparisonMode) const override;
	virtual bool TestComplexExpression(const FName& InKey, const FTextFilterString& InValue, const ETextFilterComparisonOperation InComparisonOperation, const ETextFilterTextComparisonMode InTextComparisonMode) const override;
	// ~End ITextFilterExpressionContext interface

private:
	const FPCGListViewItem* RowItem = nullptr;
	const TMap<FName, FPCGColumnData>* PCGColumnData = nullptr;
};

class SPCGEditorGraphAttributeListView : public SCompoundWidget
{
	friend SPCGListViewItemRow;
	
public:
	SLATE_BEGIN_ARGS(SPCGEditorGraphAttributeListView) {}
	SLATE_END_ARGS()

	virtual ~SPCGEditorGraphAttributeListView() override;

	void Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void RequestRefresh() { bNeedsRefresh = true; }
	
private:
	TSharedRef<SHeaderRow> CreateHeaderRowWidget() const;

	void OnInspectedComponentChanged(UPCGComponent* InPCGComponent);
	void OnInspectedStackChanged(const FPCGStack& InPCGStack);
	void OnInspectedNodeChanged(UPCGEditorGraphNodeBase* InPCGEditorGraphNode);

	void OnGenerateUpdated(UPCGComponent* InPCGComponent);

	const FPCGDataCollection* GetInspectionData();
	
	void RefreshAttributeList();
	void RefreshPinComboBox();
	void RefreshDataComboBox();

	void ApplyRowFilter();

	/** Only connected input pins are added to combo box, so keep track of the node pin index for each item. */
	struct FPinComboBoxItem
	{
		explicit FPinComboBoxItem(FName InName, int32 InPinIndex) : Name(InName), PinIndex(InPinIndex) {}

		FName Name;
		int32 PinIndex = INDEX_NONE;
	};

	FText OnGenerateSelectedPinText() const;
	int32 GetSelectedPinIndex() const;
	void OnSelectionChangedPin(TSharedPtr<FPinComboBoxItem> InItem, ESelectInfo::Type InSelectInfo);
	TSharedRef<SWidget> OnGeneratePinWidget(TSharedPtr<FPinComboBoxItem> InItem) const;

	const FSlateBrush* GetFilterBadgeIcon() const;
	TSharedRef<SWidget> OnGenerateFilterMenu();
	TSharedRef<SWidget> OnGenerateDataWidget(TSharedPtr<FName> InItem) const;
	void OnSelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	FText OnGenerateSelectedDataText() const;
	int32 GetSelectedDataIndex() const;
	void GenerateColumnsFromMetadata(const UPCGData* InPCGData, const UPCGMetadata* PCGMetadata);

	void ToggleAllAttributes();
	void ToggleAttribute(FName InAttributeName);
	ECheckBoxState GetAnyAttributeEnabledState() const;
	bool IsAttributeEnabled(FName InAttributeName) const;

	TSharedRef<ITableRow> OnGenerateRow(PCGListviewItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnItemDoubleClicked(PCGListviewItemPtr Item) const;

	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);
	EColumnSortMode::Type GetColumnSortMode(const FName InColumnId) const;
	void RefreshSorting();

	void OnFilterTextChanged(const FText& InFilterText);
	void OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	void AddColumn(const UPCGPointData* InPCGPointData, const FName& InColumnId, const FText& ColumnLabel, EHorizontalAlignment HeaderHAlign = HAlign_Center, EHorizontalAlignment CellHAlign = HAlign_Right);
	void AddIndexColumn();
	void AddPointDataColumns(const UPCGPointData* InPCGPointData);
	void AddMetadataColumn(const UPCGData* InPCGData, const FName& InColumnId, EPCGMetadataTypes InMetadataType, const TCHAR* PostFix = nullptr);

	/** Pointer back to the PCG editor that owns us */
	TWeakPtr<FPCGEditor> PCGEditorPtr;

	/** Cached PCGComponent being viewed */
	TWeakObjectPtr<UPCGComponent> PCGComponent;

	/** Cached PCGGraphNode being viewed */
	TWeakObjectPtr<UPCGEditorGraphNodeBase> PCGEditorGraphNode;

	TSharedPtr<FTextFilterExpressionEvaluator> TextFilter;

	TSharedPtr<SSearchBox> SearchBoxWidget;	
	TSharedPtr<SHeaderRow> ListViewHeader;
	TSharedPtr<SListView<PCGListviewItemPtr>> ListView;
	TArray<PCGListviewItemPtr> ListViewItems;
	TArray<PCGListviewItemPtr> FilteredListViewItems;

	TSharedPtr<SComboBox<TSharedPtr<FPinComboBoxItem>>> PinComboBox;
	TArray<TSharedPtr<FPinComboBoxItem>> PinComboBoxItems;

	TSharedPtr<SComboBox<TSharedPtr<FName>>> DataComboBox;
	TArray<TSharedPtr<FName>> DataComboBoxItems;
	
	TSharedPtr<STextBlock> NodeNameTextBlock;
	TSharedPtr<STextBlock> InfoTextBlock;
	TSharedPtr<SComboButton> FilterButton;

	TArray<FName> HiddenAttributes;

	TMap<FName, FPCGColumnData> PCGColumnData;

	FText ActiveFilterText;

	FName SortingColumn = NAME_None;
	EColumnSortMode::Type SortMode = EColumnSortMode::Type::Ascending;

	bool bNeedsRefresh = false;
};
