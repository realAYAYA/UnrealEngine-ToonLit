// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"
#include "Input/DragAndDrop.h"

#include "Misc/Attribute.h"

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectKey.h"

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionEngineRemoval.h"

#include "SGeometryCollectionOutliner.generated.h"

class FGeometryCollection;
class FGeometryCollectionTreeItem;
class FGeometryCollectionTreeItemBone;
class SGeometryCollectionOutliner;

typedef TArray<TSharedPtr<FGeometryCollectionTreeItem>> FGeometryCollectionTreeItemList;
typedef TSharedPtr<FGeometryCollectionTreeItem> FGeometryCollectionTreeItemPtr;


UENUM(BlueprintType)
enum class EOutlinerItemNameEnum : uint8
{
	BoneName = 0					UMETA(DisplayName = "Bone Name"),
	BoneIndex = 1					UMETA(DisplayName = "Bone Index"),
};

UENUM(BlueprintType)
enum class EOutlinerColumnMode : uint8
{
	StateAndSize = 0		UMETA(DisplayName = "State And Size"),
	Damage = 1				UMETA(DisplayName = "Damage"),
	Removal = 2				UMETA(DisplayName = "Removal"),
	Collision = 3			UMETA(DisplayName = "Collision"),
};

/** Settings for Outliner configuration. **/
UCLASS()
class UOutlinerSettings : public UObject
{

	GENERATED_BODY()
public:
	UOutlinerSettings(const FObjectInitializer& ObjInit);

	/** What is displayed in Outliner text */
	UPROPERTY(EditAnywhere, Category = OutlinerSettings, meta = (DisplayName = "Item Text"))
	EOutlinerItemNameEnum ItemText;

	/** whether to use level coloring */
	UPROPERTY(EditAnywhere, Category = OutlinerSettings, meta = (DisplayName = "Color By Level"))
	bool ColorByLevel;

	/** the column to be display in the outliner */
	UPROPERTY(EditAnywhere, Category = OutlinerSettings, meta = (DisplayName = "Column Mode"))
	EOutlinerColumnMode ColumnMode;
};


class FGeometryCollectionTreeItem : public TSharedFromThis<FGeometryCollectionTreeItem>
{
public:
	virtual ~FGeometryCollectionTreeItem() {}
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, bool bIsPinned = false) = 0;
	virtual void GetChildren(FGeometryCollectionTreeItemList& OutChildren) = 0;
	virtual UGeometryCollectionComponent* GetComponent() const = 0;

	virtual int32 GetBoneIndex() const { return INDEX_NONE; }

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) { return FReply::Unhandled(); }
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent) { return FReply::Unhandled(); }
	
	virtual void OnDragEnter(FDragDropEvent const& InDragDropEvent) {};
	virtual void OnDragLeave(FDragDropEvent const& InDragDropEvent);

	//void MakeDynamicStateMenu(UToolMenu* Menu, SGeometryCollectionOutliner& Outliner);
	void GenerateContextMenu(UToolMenu* Menu, SGeometryCollectionOutliner& Outliner);

	static FColor GetColorPerDepth(uint32 Depth);
};

class FGeometryCollectionTreeItemComponent : public FGeometryCollectionTreeItem
{
public:
	FGeometryCollectionTreeItemComponent(UGeometryCollectionComponent* InComponent, TSharedPtr<STreeView<FGeometryCollectionTreeItemPtr>> InTreeView)
		: Component(InComponent)
		, TreeView(InTreeView)
	{
		RegenerateChildren();
	}

	/** FGeometryCollectionTreeItem interface */
	virtual TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, bool bIsPinned = false);
	virtual void GetChildren(FGeometryCollectionTreeItemList& OutChildren) override;
	virtual UGeometryCollectionComponent* GetComponent() const override { return Component.Get(); }


	FGeometryCollectionTreeItemPtr GetItemFromBoneIndex(int32 BoneIndex) const;
	 
	void GetChildrenForBone(FGeometryCollectionTreeItemBone& BoneItem, FGeometryCollectionTreeItemList& OutChildren) const;
	bool HasChildrenForBone(const FGeometryCollectionTreeItemBone& BoneItem) const;
	FText GetDisplayNameForBone(const FGuid& Guid) const;

	void ExpandAll();
	void RegenerateChildren();
	void RequestTreeRefresh();

	void SetHistogramSelection(TArray<int32>& SelectedBones);

private:
	bool FilterBoneIndex(int32 BoneIndex) const;

private:
	TWeakObjectPtr<UGeometryCollectionComponent> Component;

	TSharedPtr<STreeView<FGeometryCollectionTreeItemPtr>> TreeView;

	/** The direct children under this component */
	TArray<FGeometryCollectionTreeItemPtr> MyChildren;

	TMap<FGuid, FGeometryCollectionTreeItemPtr> NodesMap;
	TMap<FGuid, int32> GuidIndexMap;
	FGuid RootGuid;
	int32 RootIndex;

	TArray<int32> HistogramSelection;
};

class FGeometryCollectionTreeItemBone : public FGeometryCollectionTreeItem
{

public:
	FGeometryCollectionTreeItemBone(const FGuid NewGuid, const int32 InBoneIndex, FGeometryCollectionTreeItemComponent* InParentComponentItem)
		: Guid(NewGuid)
		, BoneIndex(InBoneIndex)
		, ParentComponentItem(InParentComponentItem)
		, ItemColor(FSlateColor::UseForeground())
		, RelativeSize(0)
		, InitialState(INDEX_NONE)
		, Anchored(false)
		, Damage(0)
		, DamageThreshold(0)
		, Broken(false)
		, RemoveOnBreakAvailable(false)
		, RemoveOnBreak(FRemoveOnBreakData::DisabledPackedData)
		, ImportedCollisionsAvailable(false)
		, ImportedCollisionsUsed(false)
	{}

	/** FGeometryCollectionTreeItem interface */
	TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, bool bIsPinned = false);
	TSharedRef<SWidget> MakeNameColumnWidget() const;
	TSharedRef<SWidget> MakeRelativeSizeColumnWidget() const;
	TSharedRef<SWidget> MakeDamagesColumnWidget() const;
	TSharedRef<SWidget> MakeDamageThresholdColumnWidget() const;
	TSharedRef<SWidget> MakeBrokenColumnWidget() const;
	TSharedRef<SWidget> MakeInitialStateColumnWidget() const;
	TSharedRef<SWidget> MakeAnchoredColumnWidget() const;
	TSharedRef<SWidget> MakePostBreakTimeColumnWidget() const;
	TSharedRef<SWidget> MakeRemovalTimeColumnWidget() const;
	TSharedRef<SWidget> MakeImportedCollisionsColumnWidget() const;
	TSharedRef<SWidget> MakeEmptyColumnWidget() const;
	virtual void GetChildren(FGeometryCollectionTreeItemList& OutChildren) override;
	virtual int32 GetBoneIndex() const override { return BoneIndex; }
	virtual UGeometryCollectionComponent* GetComponent() const { return ParentComponentItem->GetComponent(); }
	const FGuid& GetGuid() const { return Guid; }
	bool HasChildren() const;
	
protected:
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragEnter(FDragDropEvent const& InDragDropEvent) override;

private:
	void UpdateItemFromCollection();
	
private:
	const FGuid Guid;
	const int32 BoneIndex;
	FGeometryCollectionTreeItemComponent* ParentComponentItem;
	
	FSlateColor ItemColor;
	FText ItemText;
	float RelativeSize;
	int32 InitialState;
	bool Anchored;
	float Damage;
	float DamageThreshold;
	bool Broken;
	bool RemoveOnBreakAvailable;
	bool IsCluster;
	FRemoveOnBreakData RemoveOnBreak;
	bool ImportedCollisionsAvailable;
	bool ImportedCollisionsUsed;
};

typedef TSharedPtr<class FGeometryCollectionTreeItemBone> FGeometryCollectionTreeItemBonePtr;

namespace SGeometryCollectionOutlinerColumnID
{
	const FName Bone("Bone");
	// State and Size column mode
	const FName RelativeSize("Relative Size");
	const FName InitialState("Initial State");
	const FName Anchored("Anchored");
	// Damage Column Mode
	const FName Damage("Damage");
	const FName DamageThreshold("DamageThreshold");
	const FName Broken("Broken");
	// Removal Column Mode
	const FName PostBreakTime("PostBreakTime");
	const FName RemovalTime("RemovalTime");
	// Collision Column Mode
	const FName ImportedCollisions("ImportedCollisions");
}

class SGeometryCollectionOutlinerRow : public SMultiColumnTableRow<FGeometryCollectionTreeItemBonePtr>
{
protected:
	FGeometryCollectionTreeItemBonePtr Item;

public:
	SLATE_BEGIN_ARGS(SGeometryCollectionOutlinerRow) {}
	SLATE_END_ARGS()

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	void Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, FGeometryCollectionTreeItemBonePtr InItemToEdit)
	{
		Item = InItemToEdit;
		SMultiColumnTableRow<FGeometryCollectionTreeItemBonePtr>::Construct(
		FSuperRowType::FArguments()
			.OnDragDetected(Item.Get(), &FGeometryCollectionTreeItem::OnDragDetected)
			.OnDrop(Item.Get(), &FGeometryCollectionTreeItem::OnDrop)
			.OnDragEnter(Item.Get(), &FGeometryCollectionTreeItem::OnDragEnter)
			.OnDragLeave(Item.Get(), &FGeometryCollectionTreeItem::OnDragLeave)
		, OwnerTableView);
	}
};

class SGeometryCollectionOutliner: public SCompoundWidget
{
	DECLARE_DELEGATE_TwoParams(FOnBoneSelectionChanged, UGeometryCollectionComponent*, TArray<int32>&);

	SLATE_BEGIN_ARGS( SGeometryCollectionOutliner ) 
	{}

		SLATE_EVENT(FOnBoneSelectionChanged, OnBoneSelectionChanged)

	SLATE_END_ARGS() 

public:
	void Construct(const FArguments& InArgs);

	void RegenerateItems();
	void RegenerateHeader();

	TSharedRef<ITableRow> MakeTreeRowWidget(FGeometryCollectionTreeItemPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGeneratePinnedRowWidget(FGeometryCollectionTreeItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable, bool bPinned);
	void OnGetChildren(TSharedPtr<FGeometryCollectionTreeItem> InInfo, TArray< TSharedPtr<FGeometryCollectionTreeItem> >& OutChildren);
	TSharedPtr<SWidget> OnOpenContextMenu();

	void UpdateGeometryCollection();
	void SetComponents(const TArray<UGeometryCollectionComponent*>& InNewComponents);
	void SetBoneSelection(UGeometryCollectionComponent* RootComponent, const TArray<int32>& InSelection, bool bClearCurrentSelection, int32 FocusBoneIdx = -1);
	int32 GetBoneSelectionCount() const;

	void SetInitialDynamicState(int32 InDynamicState);
	void SetAnchored(bool bAnchored);

	void ExpandAll();
	void ExpandRecursive(TSharedPtr<FGeometryCollectionTreeItem> TreeItem, bool bInExpansionState) const;

	// Set the histogram filter on the component matching RootComponent.
	void SetHistogramSelection(UGeometryCollectionComponent* RootComponent, TArray<int32>& SelectedBones);

private:
	void OnSelectionChanged(FGeometryCollectionTreeItemPtr Item, ESelectInfo::Type SelectInfo);
	
private:
	TSharedPtr<STreeView<FGeometryCollectionTreeItemPtr>> TreeView;
	TSharedPtr< SHeaderRow > HeaderRowWidget;
	TArray<TSharedPtr<FGeometryCollectionTreeItemComponent>> RootNodes;
	FOnBoneSelectionChanged BoneSelectionChangedDelegate;
	bool bPerformingSelection;
};
