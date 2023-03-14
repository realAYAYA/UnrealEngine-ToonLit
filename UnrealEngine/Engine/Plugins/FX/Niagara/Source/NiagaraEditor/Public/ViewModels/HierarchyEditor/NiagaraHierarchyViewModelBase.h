// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraEditorUtilities.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "NiagaraHierarchyViewModelBase.generated.h"

UCLASS()
class UNiagaraHierarchyItemBase : public UObject
{
	GENERATED_BODY()

public:
	UNiagaraHierarchyItemBase() { Guid = FGuid::NewGuid(); }
	virtual ~UNiagaraHierarchyItemBase() override {}

	TArray<UNiagaraHierarchyItemBase*>& GetChildrenMutable() { return Children; }
	const TArray<UNiagaraHierarchyItemBase*>& GetChildren() const;

	template<class ChildClass>
	bool DoesOneChildExist(bool bRecursive = false) const;

	template<class ChildClass>
	TArray<ChildClass*> GetChildrenOfType(TArray<ChildClass*>& Out, bool bRecursive = false) const;

	/** Used to clean out unneeded data using the bFinalized flag. */
	void Refresh();

	virtual FString ToString() const { return GetName(); }

	/** Finalize an item to mark it is no longer used and should be deleted. */
	void Finalize();
	bool IsFinalized() const { return bFinalized; }

	/** A guid can be optionally set to create a mapping from previously existing guids to hierarchy items that represent them. */
	void SetGuid(FGuid InGuid) { Guid = InGuid; }
	virtual FGuid GetPersistentIdentity() const { return Guid; }
	
	virtual bool CanHaveChildren() const { return false; }

	/** Overridden modify method to also mark all children as modified */
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;

protected:
	/** Called by the public Refresh function. Can be overridden to further customize the refresh process, i.e. refreshing section data in the root. */
	virtual void RefreshDataInternal() { }

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraHierarchyItemBase>> Children;
	
	/** An optional guid; can be used if hierarchy items represent outside items */
	UPROPERTY()
	FGuid Guid;

	UPROPERTY(Transient)
	bool bFinalized = false;
};

template <class ChildClass>
bool UNiagaraHierarchyItemBase::DoesOneChildExist(bool bRecursive) const
{
	for(UNiagaraHierarchyItemBase* ItemBase : Children)
	{
		if(ItemBase->IsA<ChildClass>())
		{
			return true;
		}
	}

	if(bRecursive)
	{
		for(UNiagaraHierarchyItemBase* ItemBase : Children)
		{
			if(ItemBase->DoesOneChildExist<ChildClass>(bRecursive))
			{
				return true;
			}
		}
	}

	return false;
}

template <class ChildClass>
TArray<ChildClass*> UNiagaraHierarchyItemBase::GetChildrenOfType(TArray<ChildClass*>& Out, bool bRecursive) const
{
	for(UNiagaraHierarchyItemBase* ItemBase : Children)
	{
		if(ItemBase->IsA<ChildClass>())
		{
			Out.Add(Cast<ChildClass>(ItemBase));
		}
	}

	if(bRecursive)
	{
		for(UNiagaraHierarchyItemBase* ItemBase : Children)
		{
			ItemBase->GetChildrenOfType<ChildClass>(Out, bRecursive);
		}
	}

	return Out;
}

class UNiagaraHierarchySection;

UCLASS()
class UNiagaraHierarchyRoot : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()
public:
	UNiagaraHierarchyRoot() {}
	virtual ~UNiagaraHierarchyRoot() override {}
	
	virtual void RefreshDataInternal() override;
	
	virtual bool CanHaveChildren() const override { return true; }
	
	class UNiagaraHierarchySection* AddSection(FText InNewSectionName);
	void RemoveSection(FText SectionName);
	TSet<FName> GetSections() const;
	const TArray<UNiagaraHierarchySection*>& GetSectionData() const { return Sections; }
	TArray<UNiagaraHierarchySection*>& GetSectionDataMutable() { return Sections; }
	int32 GetSectionIndex(UNiagaraHierarchySection* Section) const;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
protected:

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraHierarchySection>> Sections;
};

UCLASS()
class UNiagaraHierarchyItem : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()
public:
	UNiagaraHierarchyItem() {}
	virtual ~UNiagaraHierarchyItem() override {}

	/** An item is assumed to not have any children. */
	virtual bool CanHaveChildren() const override { return false; }
};

UCLASS()
class UNiagaraHierarchyCategory : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()
public:
	UNiagaraHierarchyCategory() {}
	UNiagaraHierarchyCategory(FName InCategory) : Category(InCategory) {}
	
	void SetCategoryName(FName NewCategory) { Category = NewCategory; }
	FName GetCategoryName() const { return Category; }

	FText GetCategoryAsText() const { return FText::FromName(Category); }
	FText GetTooltip() const { return Tooltip; }

	void SetSection(UNiagaraHierarchySection* InSection) { Section = InSection; }
	const UNiagaraHierarchySection* GetSection() const { return Section; }

	virtual bool CanHaveChildren() const override { return true; }

	virtual FString ToString() const override { return Category.ToString(); }

private:
	UPROPERTY(EditAnywhere, Category = "Category")
	FName Category;

	/** The tooltip used when the user is hovering this category */
	UPROPERTY(EditAnywhere, Category = "Category")
	FText Tooltip;

	UPROPERTY()
	TObjectPtr<UNiagaraHierarchySection> Section = nullptr;
};

UCLASS()
class UNiagaraHierarchySection : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()

public:
	UNiagaraHierarchySection() {}

	void SetSectionName(FName InSectionName) { Section = InSectionName; }
	FName GetSectionName() const { return Section; }
	
	void SetSectionNameAsText(const FText& Text);
	FText GetSectionNameAsText() const { return FText::FromName(Section); }

	FText GetTooltip() const { return Tooltip; }

	virtual FString ToString() const override { return Section.ToString(); }
private:
	UPROPERTY(EditAnywhere, Category = "Section")
	FName Section;

	/** The tooltip used when the user is hovering this section */
	UPROPERTY(EditAnywhere, Category = "Section")
	FText Tooltip;
};

struct FNiagaraHierarchyItemViewModelBase;
struct FNiagaraHierarchySectionViewModel;

class FNiagaraHierarchyDragDropOp : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraHierarchyDragDropOp, FDragDropOperation)

	FNiagaraHierarchyDragDropOp(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemBase) : DraggedItem(ItemBase) {}

	virtual void Construct() override { FDragDropOperation::Construct(); }
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override final;
	
	/** Override this custom decorator function to provide custom widget visuals */
	virtual TSharedRef<SWidget> CreateCustomDecorator() const { return SNullWidget::NullWidget; }
	
	TWeakPtr<FNiagaraHierarchyItemViewModelBase> GetDraggedItem() { return DraggedItem; }
	void SetAdditionalLabel(FText InText) { AdditionalLabel = InText; }
	FText GetAdditionalLabel() const { return AdditionalLabel; }

	void SetDescription(FText InText) { Description = InText; }
	FText GetDescription() const { return Description; }

	void SetFromSourceList(bool bInFromSourceList) { bFromSourceList = bInFromSourceList; }
	bool GetIsFromSourceList() const { return bFromSourceList; }
protected:
	TWeakPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem;
	/** Additional label will be displayed below the custom decorator. Useful for runtime tweaking of the tooltip based on what we are hovering. */
	FText AdditionalLabel;
	FText Description;
	/** If the drag drop op is from the source list, we can further customize the actions */
	bool bFromSourceList = false;
};

UCLASS(Abstract)
class UNiagaraHierarchyViewModelBase : public UObject, public FEditorUndoClient
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnHierarchyChanged)
	DECLARE_DELEGATE_OneParam(FRefreshTreeWidget, bool bFullRefresh)
	DECLARE_DELEGATE_RetVal_OneParam(bool, FIsItemSelected, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
	DECLARE_DELEGATE_OneParam(FSelectObjectInDetailsPanel, UObject* Object)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSectionActivated, TSharedPtr<FNiagaraHierarchySectionViewModel> Section)
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnItemAdded, TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddedItem)

	GENERATED_BODY()
	
	UNiagaraHierarchyViewModelBase();
	virtual ~UNiagaraHierarchyViewModelBase() override;

	void Initialize();
	void Finalize();

	void AddCategory() const;
	void AddSection() const;

	/** Refreshes all data and widgets */
	void ForceFullRefresh();

	/** The hierarchy root the widget is editing. This should point to persistent data stored somewhere else as the serialized root of the hierarchy. */
	virtual UNiagaraHierarchyRoot* GetHierarchyDataRoot() const PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::GetHierarchyDataRoot, return nullptr;);
	TSharedPtr<struct FNiagaraHierarchyRootViewModel> GetHierarchyViewModelRoot() const { return HierarchyViewModelRoot; }

	/** This function will create the view model for a given item. Override to customize view model behavior by providing custom classes. */
	virtual TSharedPtr<FNiagaraHierarchyItemViewModelBase> CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent);
	
	/** Source items reflect the base, unedited status of items to edit into a hierarchy */
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetSourceItems() const;
	/** Hierarchy items reflect the already edited hierarchy. This should generally be constructed from persistent serialized data. */
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetHierarchyItems() const;
	
	/** Prepares the items we want to create a hierarchy for. The idea is to Drag & Drop these into the hierarchy or click on them to edit details.  */
	virtual void PrepareSourceItems() PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::PrepareSourceItems,);

	/** Additional commands can be specified overriding the Commands function. */
	virtual void SetupCommands() {}
	TSharedRef<FUICommandList> GetCommands() const { return Commands.ToSharedRef(); }

	/** Function to implement for custom drag drop ops. */
	virtual TSharedRef<FNiagaraHierarchyDragDropOp> CreateDragDropOp(TSharedRef<FNiagaraHierarchyItemViewModelBase> Item) PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::CreateDragDropOp, return MakeShared<FNiagaraHierarchyDragDropOp>(nullptr););
	
	virtual void OnSelectionChanged(TSharedPtr<FNiagaraHierarchyItemViewModelBase> HierarchyItem) {}
	
	void OnGetChildren(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item, TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& OutChildren) const;
	
	virtual bool SupportsDetailsPanel() { return false; }
	// Overriding this will give the details panel instance customizations for specific UClasses
	virtual TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> GetInstanceCustomizations() { return {}; }
	
	void RefreshSourceView(bool bFullRefresh = false) const;
	void RefreshHierarchyView(bool bFullRefresh = false) const;
	void RefreshSectionsWidget() const;
	
	// Delegate that call functions from SNiagaraHierarchy
	FRefreshTreeWidget& OnRefreshSourceView() { return RefreshSourceViewDelegate; }
	FRefreshTreeWidget& OnRefreshHierarchyView() { return RefreshHierarchyWidgetDelegate; }
	FSimpleDelegate& OnRefreshSections() { return RefreshSectionsWidgetDelegate; }

	// Delegates for external systems
	FOnHierarchyChanged& OnHierarchyChanged() { return OnHierarchyChangedDelegate; } 

	FOnItemAdded& OnItemAdded() { return OnItemAddedDelegate; }
	
	// Sections
	void SetActiveSection(TSharedPtr<struct FNiagaraHierarchySectionViewModel>);
	TSharedPtr<FNiagaraHierarchySectionViewModel> GetActiveSection() const;
	UNiagaraHierarchySection* GetActiveSectionData() const;
	bool IsSectionActive(const UNiagaraHierarchySection* Section) const;
	FOnSectionActivated& OnSectionActivated() { return OnSectionActivatedDelegate; }
	
	FString OnItemToStringDebug(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemBaseViewModel) const; 
protected:
	virtual void InitializeInternal() {}
	virtual void FinalizeInternal() {}
	
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
protected:
	UPROPERTY()
	TObjectPtr<UNiagaraHierarchyRoot> SourceRoot;

	UPROPERTY()
	TObjectPtr<UNiagaraHierarchyRoot> HierarchyRoot;

	TSharedPtr<struct FNiagaraHierarchyRootViewModel> SourceViewModelRoot;
	TSharedPtr<struct FNiagaraHierarchyRootViewModel> HierarchyViewModelRoot;

	TWeakPtr<struct FNiagaraHierarchySectionViewModel> ActiveSection;
	TSharedPtr<FUICommandList> Commands;

protected:
	// delegate collection to call UI functions
	FRefreshTreeWidget RefreshSourceViewDelegate;
	FRefreshTreeWidget RefreshHierarchyWidgetDelegate;
	FSimpleDelegate RefreshSectionsWidgetDelegate;

	FOnItemAdded OnItemAddedDelegate;
	FOnSectionActivated OnSectionActivatedDelegate;
	FOnHierarchyChanged OnHierarchyChangedDelegate;
};

struct NIAGARAEDITOR_API FNiagaraHierarchyItemViewModelBase : TSharedFromThis<FNiagaraHierarchyItemViewModelBase>, public FTickableEditorObject
{
	DECLARE_MULTICAST_DELEGATE(FOnSynced)

	FNiagaraHierarchyItemViewModelBase(UNiagaraHierarchyItemBase* InItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel)
		: ItemBase(InItemBase)
		, Parent(InParent)
		, HierarchyViewModel(InHierarchyViewModel)
	{		
		
	}
	
	virtual ~FNiagaraHierarchyItemViewModelBase() {}

	UNiagaraHierarchyItemBase* GetDataMutable() const { return ItemBase; }
	const UNiagaraHierarchyItemBase* GetData() const { return ItemBase; }
	
	template<class T>
	T* GetDataMutable() const { return Cast<T>(ItemBase); }
	
	template<class T>
	const T* GetData() const { return Cast<T>(ItemBase); }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	FString ToString() const { return ItemBase->ToString(); }
	
	void SyncToData();
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetChildren() const { return Children; }
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetFilteredChildren() const;

	template<class DataClass, class ViewModelChildClass>
	void GetChildrenViewModelsForType(TArray<TSharedPtr<ViewModelChildClass>>& OutChildren, bool bRecursive = false);

	bool HasParent(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ParentCandidate, bool bRecursive = false);

	void AddChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item);
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddNewItem(TSubclassOf<UNiagaraHierarchyItemBase> NewItemClass);
	void DuplicateToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToDuplicate, int32 InsertIndex = INDEX_NONE);
	void ReparentToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToMove, int32 InsertIndex = INDEX_NONE);

	TSharedPtr<FNiagaraHierarchyItemViewModelBase> FindViewModelForChild(UNiagaraHierarchyItemBase* Child) const;
	int32 FindIndexOfChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child) const;
	int32 FindIndexOfChild(UNiagaraHierarchyItemBase* Child) const;
	int32 FindIndexOfDataChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child) const;
	int32 FindIndexOfDataChild(UNiagaraHierarchyItemBase* Child) const;

	/** Deleting will finalize as well, but is a user triggered action whereas Finalize is an internal call. */
	void Delete();
	/** Finalizing the view model will finalize the data & the children as well as handle cleanup. */
	void Finalize();

	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> GetHierarchyViewModel() const { return HierarchyViewModel; }
	
	virtual FReply OnDroppedOn(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
	{
		return FReply::Unhandled();
	}

	TOptional<EItemDropZone> OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item);
	virtual TOptional<EItemDropZone> OnCanAcceptDropInternal(TSharedPtr<FDragDropOperation> DragDropOp, EItemDropZone ItemDropZone);

	/** Should return true if draggable */
	virtual bool CanDrag() { return false; }

	/** Should return true if renamable */
	virtual bool CanRename() { return false; }

	virtual bool CanDelete() { return true; }

	void RequestRename()
	{
		if(CanRename() && OnRequestRenameDelegate.IsBound())
		{
			bRenamePending = false;
			OnRequestRenameDelegate.Execute();
		}
	}

	void RequestRenamePending()
	{
		if(CanRename())
		{
			bRenamePending = true;
		}
	}

	/** The UObject we display in the details panel when this item is selected. By default it's the item the view model represents. */
	virtual UObject* GetDataForEditing() { return ItemBase; }
	
	/** Used to create customized drag drop ops. */
	TSharedRef<class FNiagaraHierarchyDragDropOp> CreateDragDropOp();
	
	FReply OnDragDetected(const FGeometry& Geometry, const FPointerEvent& PointerEvent, bool bIsSource);

	FSimpleDelegate& GetOnRequestRename() { return OnRequestRenameDelegate; }
	FOnSynced& GetOnSynced() { return OnSyncedDelegate; }

	TWeakPtr<FNiagaraHierarchyItemViewModelBase> GetParent() { return Parent; }

private:
	virtual void SyncChildrenViewModelsInternal() {}
	virtual void FinalizeInternal() {}
protected:
	UNiagaraHierarchyItemBase* const ItemBase;
	/** Parent should be valid for all instances of this struct except for root objects */
	TWeakPtr<FNiagaraHierarchyItemViewModelBase> Parent;
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> Children;
	mutable TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> FilteredChildren;
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	FSimpleDelegate OnRequestRenameDelegate;
	FOnSynced OnSyncedDelegate;
	bool bRenamePending = false;
};

template <class DataClass, class ViewModelClass>
void FNiagaraHierarchyItemViewModelBase::GetChildrenViewModelsForType(TArray<TSharedPtr<ViewModelClass>>& OutChildren, bool bRecursive)
{
	for(auto& Child : Children)
	{
		if(Child->GetData()->IsA<DataClass>())
		{
			OutChildren.Add(StaticCastSharedPtr<ViewModelClass>(Child));
		}
	}

	if(bRecursive)
	{
		for(auto& Child : Children)
		{
			Child->GetChildrenViewModelsForType<DataClass, ViewModelClass>(OutChildren, bRecursive);
		}
	}
}

struct FNiagaraHierarchyRootViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchyRootViewModel(UNiagaraHierarchyItemBase* InItem, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel) : FNiagaraHierarchyItemViewModelBase(InItem, nullptr, InHierarchyViewModel) {}
	
	virtual ~FNiagaraHierarchyRootViewModel() override {}

	virtual bool CanDrag() override { return false; }

	virtual TOptional<EItemDropZone> OnCanAcceptDropInternal(TSharedPtr<FDragDropOperation> DragDropOp, EItemDropZone ItemDropZone) override;
	virtual FReply OnDroppedOn(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) override;

	TSharedPtr<struct FNiagaraHierarchySectionViewModel> AddNewSection();
	TArray<TSharedPtr<struct FNiagaraHierarchySectionViewModel>>& GetSectionViewModels() { return SectionViewModels; }

private:
	virtual void SyncChildrenViewModelsInternal() override;
	TArray<TSharedPtr<struct FNiagaraHierarchySectionViewModel>> SectionViewModels;
};

struct FNiagaraHierarchySectionViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchySectionViewModel(UNiagaraHierarchySection* InItem, TSharedPtr<FNiagaraHierarchyRootViewModel> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel) : FNiagaraHierarchyItemViewModelBase(InItem, InParent, InHierarchyViewModel) {}
	
	virtual ~FNiagaraHierarchySectionViewModel() override {}

	void SetSectionName(FName InSectionName) const { Cast<UNiagaraHierarchySection>(ItemBase)->SetSectionName(InSectionName); }
	FName GetSectionName() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetSectionName(); }
	
	void SetSectionNameAsText(const FText& Text) const { Cast<UNiagaraHierarchySection>(ItemBase)->SetSectionNameAsText(Text); }
	FText GetSectionNameAsText() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetSectionNameAsText(); }
	FText GetSectionTooltip() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetTooltip(); }
	virtual bool CanDrag() override { return false; }
	virtual bool CanRename() override { return true; }
	virtual bool CanDelete() override { return true; }

	virtual TOptional<EItemDropZone> OnCanAcceptDropInternal(TSharedPtr<FDragDropOperation> DragDropOp, EItemDropZone ItemDropZone) override;
	virtual FReply OnDroppedOn(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) override;

	virtual void FinalizeInternal() override;
};

struct NIAGARAEDITOR_API FNiagaraHierarchyItemViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchyItemViewModel(UNiagaraHierarchyItem* InItem, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel) : FNiagaraHierarchyItemViewModelBase(InItem, InParent, InHierarchyViewModel) {}
	
	virtual ~FNiagaraHierarchyItemViewModel() override {}

	virtual bool CanDrag() override { return true; }

	virtual TOptional<EItemDropZone> OnCanAcceptDropInternal(TSharedPtr<FDragDropOperation> DragDropOp, EItemDropZone ItemDropZone) override;
	virtual FReply OnDroppedOn(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) override;
};

struct FNiagaraHierarchyCategoryViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchyCategoryViewModel(UNiagaraHierarchyCategory* InCategory, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel) : FNiagaraHierarchyItemViewModelBase(InCategory, InParent, InHierarchyViewModel) {}
	virtual ~FNiagaraHierarchyCategoryViewModel() override{}

	virtual bool CanDrag() override { return true; }
	virtual bool CanRename() override { return true; }

	const UNiagaraHierarchySection* GetSection() const { return Cast<UNiagaraHierarchyCategory>(ItemBase)->GetSection(); }

	bool IsTopCategoryActive() const;

	virtual TOptional<EItemDropZone> OnCanAcceptDropInternal(TSharedPtr<FDragDropOperation> DragDropOp, EItemDropZone ItemDropZone) override;
	virtual FReply OnDroppedOn(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item) override;
};

class FNiagaraSectionDragDropOp : public FNiagaraHierarchyDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FNiagaraSectionDragDropOp, FNiagaraHierarchyDragDropOp)

	FNiagaraSectionDragDropOp(TSharedPtr<FNiagaraHierarchySectionViewModel> ItemBase) : FNiagaraHierarchyDragDropOp(ItemBase) {}
	
	TWeakPtr<FNiagaraHierarchySectionViewModel> GetDraggedSection() const { return StaticCastSharedPtr<FNiagaraHierarchySectionViewModel>(DraggedItem.Pin()); }
private:
	virtual TSharedRef<SWidget> CreateCustomDecorator() const override;
};
