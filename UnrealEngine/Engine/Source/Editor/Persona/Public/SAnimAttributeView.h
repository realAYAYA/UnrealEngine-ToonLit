// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IStructureDetailsView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/AnimData/AttributeIdentifier.h"

class ITableRow;
class STableViewBase;
class IStructureDetailsView;
class FStructOnScope;
class SHeaderRow;
class SScrollBox;

namespace EColumnSortMode
{
	enum Type;
}


class FAnimAttributeEntry : public TSharedFromThis<FAnimAttributeEntry>
{
public:
	static TSharedRef<FAnimAttributeEntry> MakeEntry(const FAnimationAttributeIdentifier& InIdentifier, const FName& InSnapshotDisplayName);

	FAnimAttributeEntry() = default;
	FAnimAttributeEntry(const FAnimationAttributeIdentifier& InIdentifier, const FName& InSnapshotDisplayName);
	
	TSharedRef<ITableRow> MakeTableRowWidget(const TSharedRef<STableViewBase>& InOwnerTable);

	FName GetName() const { return Identifier.GetName(); }
	FName GetBoneName() const { return Identifier.GetBoneName(); }
	int32 GetBoneIndex() const { return Identifier.GetBoneIndex(); }
	FName GetTypeName() const { return CachedTypeName; }
	UScriptStruct* GetScriptStruct() const { return Identifier.GetType(); }
	FName GetSnapshotDisplayName() const { return SnapshotDisplayName; }
	
	FName GetDisplayName() const;
	
	UE::Anim::FAttributeId GetAttributeId() const
	{
		return UE::Anim::FAttributeId(GetName(), FCompactPoseBoneIndex(GetBoneIndex()));
	};

	const FAnimationAttributeIdentifier& GetAnimationAttributeIdentifier() const
	{
		return Identifier;
	};

	bool operator==(const FAnimAttributeEntry& InOther) const
	{
		return Identifier == InOther.Identifier && SnapshotDisplayName == InOther.SnapshotDisplayName;
	}

	bool operator!=(const FAnimAttributeEntry& InOther) const
	{
		return !(*this == InOther);
	}
	
	
private:
	FAnimationAttributeIdentifier Identifier;
	
	FName SnapshotDisplayName;

	FName CachedTypeName;
};

class SAnimAttributeEntry : public SMultiColumnTableRow<TSharedPtr<FAnimAttributeEntry>>
{
	SLATE_BEGIN_ARGS( SAnimAttributeEntry )
	{}
	
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FAnimAttributeEntry> InEntry);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;
	
	FText GetEntryName()const ;
	FText GetEntryBoneName() const;
	FText GetEntryTypeName() const;
	FText GetEntrySnapshotDisplayName() const;
private:
	TWeakPtr<FAnimAttributeEntry> Entry;
};

class PERSONA_API SAnimAttributeView : public SCompoundWidget
{
private:	
	static TSharedRef<IStructureDetailsView> CreateValueViewWidget();
	
	static TSharedRef<ITableRow> MakeTableRowWidget(
		TSharedPtr<FAnimAttributeEntry> InItem,
		const TSharedRef<STableViewBase>& InOwnerTable);

	static FName GetSnapshotColumnDisplayName(const TArray<FName>& InSnapshotNames);

public:
	DECLARE_DELEGATE_RetVal_OneParam(FName, FOnGetAttributeSnapshotColumnDisplayName, const TArray<FName>& /** Snapshot Names */)
	
	SLATE_BEGIN_ARGS( SAnimAttributeView )
		: _SnapshotColumnLabelOverride(FText::FromString(TEXT("Direction")))
		{}
		// override what is displayed in the snapshot column, given a set of snapshots that contains the attribute
		SLATE_EVENT(FOnGetAttributeSnapshotColumnDisplayName, OnGetAttributeSnapshotColumnDisplayName)
	
		// override the label on the snapshot column, a typical choice is "Direction"
		SLATE_ATTRIBUTE(FText, SnapshotColumnLabelOverride)
	SLATE_END_ARGS()
	
	SAnimAttributeView();
	
	void Construct(const FArguments& InArgs);

	void DisplayNewAttributeContainerSnapshots(
		const TArray<TTuple<FName, const UE::Anim::FHeapAttributeContainer&>>& InSnapshots, 
		const USkeletalMeshComponent* InOwningComponent);

	void ClearListView();
	
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
private:

	bool ShouldInvalidateListViewCache(
		const TArray<TTuple<FName, const UE::Anim::FHeapAttributeContainer&>>& InSnapshots,
		const USkeletalMeshComponent* InOwningComponent);
	
	void OnSelectionChanged(TSharedPtr<FAnimAttributeEntry> InEntry, ESelectInfo::Type InSelectType);

	void OnFilterTextChanged(const FText& InText);

	EColumnSortMode::Type GetSortModeForColumn(FName InColumnId) const;
	
	void OnSortAttributeEntries(
		EColumnSortPriority::Type InPriority,
		const FName& InColumnId,
		EColumnSortMode::Type InSortMode);

	void ExecuteSort();
	
	void RefreshFilteredAttributeEntries();

	void RefreshValueView();

private:
	/** list view */
	TSharedPtr<SListView<TSharedPtr<FAnimAttributeEntry>>> AttributeListView;
	bool bShouldRefreshListView;

	TSharedPtr<SHeaderRow> HeaderRow;
	
	FName ColumnIdToSort;
	EColumnSortMode::Type ActiveSortMode;
	FOnGetAttributeSnapshotColumnDisplayName OnGetAttributeSnapshotColumnDisplayName;
	TAttribute<FText> SnapshotColumnLabelOverride;

	int32 CachedNumSnapshots;
	// cache all attributes in the attribute container that the list view is observing
	// such that we can use it to detect if a change to the attribute container occured
	// and refresh the list accordingly
	TArray<TTuple<FName, TArray<FAnimationAttributeIdentifier>>> CachedAttributeIdentifierLists;

	// for each attribute, save the name of the attribute container snapshot that contains it
	TMap<FAnimationAttributeIdentifier, TArray<FName>> CachedAttributeSnapshotMap;

	TMap<FName, int32> CachedSnapshotNameIndexMap;

	// attributes to be displayed
	TArray<TSharedPtr<FAnimAttributeEntry>> FilteredAttributeEntries;

	FString FilterText;

	
	/** value view */
	TSharedPtr<SScrollBox> ValueViewBox;
	bool bShouldRefreshValueView;
	TOptional<FAnimAttributeEntry> SelectedAttribute;

	struct FAttributeValueView
	{
		FAttributeValueView(FName InSnapshotName, const FAnimAttributeEntry& InSelectedAttribute);

		void UpdateValue(const UE::Anim::FHeapAttributeContainer& InAttributeContainer) const;

		FAnimAttributeEntry SubjectAttribute;
		FName SnapshotName;
		TSharedPtr<FStructOnScope> StructData;
		TSharedPtr<IStructureDetailsView> ViewWidget;
	};
	
	TArray<FAttributeValueView> SelectedAttributeSnapshotValueViews;
};


inline void SAnimAttributeView::ClearListView()
{
	CachedNumSnapshots = 0;
	CachedAttributeIdentifierLists.Reset();
	CachedAttributeSnapshotMap.Reset();
	CachedSnapshotNameIndexMap.Reset();
	FilteredAttributeEntries.Reset();

	bShouldRefreshListView = true;
}
