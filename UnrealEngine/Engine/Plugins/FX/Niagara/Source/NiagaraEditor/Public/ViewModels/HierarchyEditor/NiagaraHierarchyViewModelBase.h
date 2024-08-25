// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraEditorUtilities.h"
#include "PropertyEditorDelegates.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ToolMenuSection.h"
#include "IPropertyRowGenerator.h"
#include "Misc/TransactionObjectEvent.h"
#include "ScopedTransaction.h"
#include "NiagaraHierarchyViewModelBase.generated.h"

USTRUCT()
struct FNiagaraHierarchyIdentity
{
	GENERATED_BODY()

	FNiagaraHierarchyIdentity() {}
	FNiagaraHierarchyIdentity(TArray<FGuid> InGuids, TArray<FName> InNames) : Guids(InGuids), Names(InNames) {}
	
	/** An array of guids that have to be satisfied in order to match. */
	UPROPERTY()
	TArray<FGuid> Guids;

	/** Optionally, an array of names can be specified in place of guids. If guids & names are present, guids have to be satisfied first, then names. */
	UPROPERTY()
	TArray<FName> Names;

	bool IsValid() const
	{
		return Guids.Num() > 0 || Names.Num() > 0;
	}
	
	bool operator==(const FNiagaraHierarchyIdentity& OtherIdentity) const
	{
		if(Guids.Num() != OtherIdentity.Guids.Num() || Names.Num() != OtherIdentity.Names.Num())
		{
			return false;
		}

		for(int32 GuidIndex = 0; GuidIndex < Guids.Num(); GuidIndex++)
		{
			if(Guids[GuidIndex] != OtherIdentity.Guids[GuidIndex])
			{
				return false;
			}
		}

		for(int32 NameIndex = 0; NameIndex < Names.Num(); NameIndex++)
		{
			if(!Names[NameIndex].IsEqual(OtherIdentity.Names[NameIndex]))
			{
				return false;
			}
		}

		return true;
	}

	bool operator!=(const FNiagaraHierarchyIdentity& OtherIdentity) const
	{
		return !(*this == OtherIdentity);
	}
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraHierarchyIdentity& Identity)
{
	uint32 Hash = 0;
	
	for(const FGuid& Guid : Identity.Guids)
	{
		HashCombine(Hash, GetTypeHash(Guid));
	}
	
	for(const FName& Name : Identity.Names)
	{
		HashCombine(Hash, GetTypeHash(Name));
	}
	
	return Hash;
}

/** A base class that is used to refresh data that represents external data. Inherit from this class if you need more context data. */
UCLASS()
class UNiagaraHierarchyDataRefreshContext : public UObject
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TMap<FNiagaraHierarchyIdentity, TObjectPtr<UObject>> IdentityToObjectMap;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyItemBase : public UObject
{
	GENERATED_BODY()

public:
	UNiagaraHierarchyItemBase() { Identity.Guids.Add(FGuid::NewGuid()); }
	virtual ~UNiagaraHierarchyItemBase() override {}

	TArray<TObjectPtr<UNiagaraHierarchyItemBase>>& GetChildrenMutable() { return Children; }
	NIAGARAEDITOR_API const TArray<UNiagaraHierarchyItemBase*>& GetChildren() const;

	template<class ChildClass>
	ChildClass* AddChild();

	NIAGARAEDITOR_API UNiagaraHierarchyItemBase* FindChildWithIdentity(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively = false);

	NIAGARAEDITOR_API UNiagaraHierarchyItemBase* CopyAndAddItemAsChild(const UNiagaraHierarchyItemBase& ItemToCopy);
	NIAGARAEDITOR_API UNiagaraHierarchyItemBase* CopyAndAddItemUnderParentIdentity(const UNiagaraHierarchyItemBase& ItemToCopy, FNiagaraHierarchyIdentity ParentIdentity);
	
	/** Remove a child with a given identity. Can be searched recursively. This function operates under the assumption there will be only one item with a given identity. */
	NIAGARAEDITOR_API bool RemoveChildWithIdentity(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively = false);
	
	template<class ChildClass>
	bool DoesOneChildExist(bool bRecursive = false) const;

	template<class ChildClass>
	TArray<ChildClass*> GetChildrenOfType(TArray<ChildClass*>& Out, bool bRecursive = false) const;

	template<class PREDICATE_CLASS>
	void SortChildren(const PREDICATE_CLASS& Predicate, bool bRecursive = false);
	
	virtual FString ToString() const { return GetName(); }

	/** An identity can be optionally set to create a mapping from previously existing guids or names to hierarchy items that represent them. */
	void SetIdentity(FNiagaraHierarchyIdentity InIdentity) { Identity = InIdentity; }
	FNiagaraHierarchyIdentity GetPersistentIdentity() const { return Identity; }
	NIAGARAEDITOR_API TArray<FNiagaraHierarchyIdentity> GetParentIdentities() const;
	
	/** Overridden modify method to also mark all children as modified */
	NIAGARAEDITOR_API virtual bool Modify(bool bAlwaysMarkDirty = true) override;

protected:
	NIAGARAEDITOR_API virtual void PostLoad() override;

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraHierarchyItemBase>> Children;

	UPROPERTY()
	FNiagaraHierarchyIdentity Identity;
	
	/** An optional guid; can be used if hierarchy items represent outside items */
	UPROPERTY()
	FGuid Guid_DEPRECATED;
};

template <class ChildClass>
ChildClass* UNiagaraHierarchyItemBase::AddChild()
{
	ChildClass* NewChild = NewObject<ChildClass>(this);
	GetChildrenMutable().Add(NewChild);

	return NewChild;
}

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

template <class PREDICATE_CLASS>
void UNiagaraHierarchyItemBase::SortChildren(const PREDICATE_CLASS& Predicate, bool bRecursive)
{
	Children.Sort(Predicate);

	if(bRecursive)
	{
		for(TObjectPtr<UNiagaraHierarchyItemBase> Child : Children)
		{
			Child->SortChildren(Predicate, bRecursive);
		}
	}
}

class UNiagaraHierarchySection;

UCLASS(MinimalAPI)
class UNiagaraHierarchyRoot : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()
public:
	UNiagaraHierarchyRoot() {}
	virtual ~UNiagaraHierarchyRoot() override {}

	const TArray<UNiagaraHierarchySection*>& GetSectionData() const { return Sections; }
	TArray<TObjectPtr<UNiagaraHierarchySection>>& GetSectionDataMutable() { return Sections; }


	NIAGARAEDITOR_API TSet<FName> GetSections() const;
	NIAGARAEDITOR_API int32 GetSectionIndex(UNiagaraHierarchySection* Section) const;

	NIAGARAEDITOR_API UNiagaraHierarchySection* AddSection(FText InNewSectionName, int32 InsertIndex = INDEX_NONE);
	NIAGARAEDITOR_API UNiagaraHierarchySection* FindSectionByIdentity(FNiagaraHierarchyIdentity SectionIdentity);
	/** This will copy the section element itself */
	NIAGARAEDITOR_API void DuplicateSectionFromOtherRoot(const UNiagaraHierarchySection& SectionToCopy);
	NIAGARAEDITOR_API void RemoveSection(FText SectionName);
	NIAGARAEDITOR_API void RemoveSectionByIdentity(FNiagaraHierarchyIdentity SectionIdentity);
	
	NIAGARAEDITOR_API virtual bool Modify(bool bAlwaysMarkDirty = true) override;
protected:

	UPROPERTY()
	TArray<TObjectPtr<UNiagaraHierarchySection>> Sections;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyItem : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()
public:
	UNiagaraHierarchyItem() {}
	virtual ~UNiagaraHierarchyItem() override {}
};

UCLASS(MinimalAPI)
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

	virtual FString ToString() const override { return Category.ToString(); }

	/** Since the category points to a section object, during merge or copy paste etc. it is possible the section pointer will point at a section from another root.
	 *  We fix this up by looking through our available sections and match up via persistent identity.
	 *  This function expects the correct section with the same identity to exist already at the root level
	 */
	NIAGARAEDITOR_API void FixupSectionLinkage();
private:
	UPROPERTY()
	FName Category;

	/** The tooltip used when the user is hovering this category */
	UPROPERTY(EditAnywhere, Category = "Category", meta = (MultiLine = "true"))
	FText Tooltip;

	UPROPERTY()
	TObjectPtr<UNiagaraHierarchySection> Section = nullptr;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchySection : public UNiagaraHierarchyItemBase
{
	GENERATED_BODY()

public:
	UNiagaraHierarchySection() {}

	void SetSectionName(FName InSectionName) { Section = InSectionName; }
	FName GetSectionName() const { return Section; }
	
	NIAGARAEDITOR_API void SetSectionNameAsText(const FText& Text);
	FText GetSectionNameAsText() const { return FText::FromName(Section); }

	FText GetTooltip() const { return Tooltip; }

	virtual FString ToString() const override { return Section.ToString(); }
private:
	UPROPERTY()
	FName Section;

	/** The tooltip used when the user is hovering this section */
	UPROPERTY(EditAnywhere, Category = "Section", meta = (MultiLine = "true"))
	FText Tooltip;
};

UCLASS(MinimalAPI)
class UNiagaraHierarchyObjectProperty : public UNiagaraHierarchyItem
{
	GENERATED_BODY()
public:
	/** To know what object this ObjectProperty is referring to, a persistent guid that can be mapped back to an object is required. */
	NIAGARAEDITOR_API void Initialize(FGuid ObjectGuid, FString PropertyName);
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

UCLASS(BlueprintType)
class UNiagaraHierarchyMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> Items;

	bool bFromHierarchy = false;
};

UCLASS(Abstract)
class UNiagaraHierarchyViewModelBase : public UObject, public FSelfRegisteringEditorUndoClient
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnHierarchyChanged)
	DECLARE_MULTICAST_DELEGATE(FOnHierarchyPropertiesChanged)
	DECLARE_DELEGATE_OneParam(FOnSectionActivated, TSharedPtr<FNiagaraHierarchySectionViewModel> Section)
	DECLARE_DELEGATE_OneParam(FOnItemAdded, TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddedItem)
	DECLARE_DELEGATE_OneParam(FOnRefreshViewRequested, bool bForceFullRefresh)
	DECLARE_DELEGATE_OneParam(FOnNavigateToItemInHierarchyRequested, FNiagaraHierarchyIdentity Identity)

	GENERATED_BODY()

	UNiagaraHierarchyViewModelBase();
	virtual ~UNiagaraHierarchyViewModelBase() override;

	void Initialize();
	void Finalize();
	
	void AddCategory(TSharedPtr<FNiagaraHierarchyItemViewModelBase> CategoryParent) const;
	void AddSection() const;

	NIAGARAEDITOR_API void DeleteItemWithIdentity(FNiagaraHierarchyIdentity Identity);
	NIAGARAEDITOR_API void DeleteItemsWithIdentities(TArray<FNiagaraHierarchyIdentity> Identities);

	NIAGARAEDITOR_API void NavigateToItemInHierarchy(const FNiagaraHierarchyIdentity& NiagaraHierarchyIdentity);

	/** Refreshes all data and widgets */
	void ForceFullRefresh();
	void ForceFullRefreshOnTimer();
	void RequestFullRefreshNextFrame();

	/** The hierarchy root the widget is editing. This should point to persistent data stored somewhere else as the serialized root of the hierarchy. */
	virtual UNiagaraHierarchyRoot* GetHierarchyRoot() const PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::GetHierarchyRoot, return nullptr;);
	TSharedPtr<struct FNiagaraHierarchyRootViewModel> GetHierarchyRootViewModel() const { return HierarchyRootViewModel; }
	
	/** This function will create the view model for a given item. Override to customize view model behavior by providing custom classes. */
	virtual TSharedPtr<FNiagaraHierarchyItemViewModelBase> CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent);
	
	/** Hierarchy items reflect the already edited hierarchy. This should generally be constructed from persistent serialized data. */
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetHierarchyItems() const;
	
	const UNiagaraHierarchyDataRefreshContext* GetRefreshContext() const { return RefreshContext; }
	void SetRefreshContext(UNiagaraHierarchyDataRefreshContext* InContext) { RefreshContext = InContext; }
	
	/** Prepares the items we want to create a hierarchy for. Primary purpose is to add children to the source root to gather the items to display in the source panel.
	 * The root view model is also given as a way to forcefully sync view models to access additional functionality, if needed */
	virtual void PrepareSourceItems(UNiagaraHierarchyRoot* SourceRoot, TSharedPtr<FNiagaraHierarchyRootViewModel> SourceRootViewModel) PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::PrepareSourceItems,);
	
	/** Additional commands can be specified overriding the Commands function. */
	virtual void SetupCommands() {}
	TSharedRef<FUICommandList> GetCommands() const { return Commands.ToSharedRef(); }

	/** Function to implement for custom drag drop ops. */
	virtual TSharedRef<FNiagaraHierarchyDragDropOp> CreateDragDropOp(TSharedRef<FNiagaraHierarchyItemViewModelBase> Item) PURE_VIRTUAL(UNiagaraHierarchyViewModelBase::CreateDragDropOp, return MakeShared<FNiagaraHierarchyDragDropOp>(nullptr););
	
	void OnGetChildren(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item, TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& OutChildren) const;
	
	virtual bool SupportsDetailsPanel() { return false; }
	// Overriding this will give the details panel instance customizations for specific UClasses
	virtual TArray<TTuple<UClass*, FOnGetDetailCustomizationInstance>> GetInstanceCustomizations() { return {}; }

	void RefreshAllViews(bool bFullRefresh = false) const;
	void RefreshSourceView(bool bFullRefresh = false) const;
	void RefreshHierarchyView(bool bFullRefresh = false) const;
	void RefreshSectionsView() const;
	
	// Delegate that call functions from SNiagaraHierarchy
	FSimpleDelegate& OnRefreshSourceItemsRequested() { return RefreshSourceItemsRequestedDelegate; }
	FOnRefreshViewRequested& OnRefreshSourceView() { return RefreshSourceViewDelegate; }
	FOnRefreshViewRequested& OnRefreshHierarchyView() { return RefreshHierarchyWidgetDelegate; }
	FSimpleDelegate& OnRefreshSectionsView() { return RefreshSectionsViewDelegate; }

	// Delegates for external systems
	FOnHierarchyChanged& OnHierarchyChanged() { return OnHierarchyChangedDelegate; } 
	FOnHierarchyChanged& OnHierarchyPropertiesChanged() { return OnHierarchyPropertiesChangedDelegate; } 
	FOnItemAdded& OnItemAdded() { return OnItemAddedDelegate; }
	FOnRefreshViewRequested& OnRefreshViewRequested() { return RefreshAllViewsRequestedDelegate; }
	FOnNavigateToItemInHierarchyRequested& OnNavigateToItemInHierarchyRequested() { return OnNavigateToItemInHierarchyRequestedDelegate; }
	FSimpleDelegate& OnInitialized() { return OnInitializedDelegate; }
	
	// Sections
	NIAGARAEDITOR_API void SetActiveHierarchySection(TSharedPtr<struct FNiagaraHierarchySectionViewModel>);
	TSharedPtr<FNiagaraHierarchySectionViewModel> GetActiveHierarchySection() const;
	UNiagaraHierarchySection* GetActiveHierarchySectionData() const;
	bool IsHierarchySectionActive(const UNiagaraHierarchySection* Section) const;
	FOnSectionActivated& OnHierarchySectionActivated() { return OnHierarchySectionActivatedDelegate; }

	FString OnItemToStringDebug(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemBaseViewModel) const;

protected:
	virtual void InitializeInternal() {}
	virtual void FinalizeInternal() {}
	
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
private:
	bool FilterForHierarchySection(TSharedPtr<const FNiagaraHierarchyItemViewModelBase> ItemViewModel) const;
	bool FilterForUncategorizedRootItemsInAllSection(TSharedPtr<const FNiagaraHierarchyItemViewModelBase> ItemViewModel) const;
protected:
	UPROPERTY()
	TObjectPtr<UNiagaraHierarchyRoot> HierarchyRoot;
	
	TSharedPtr<struct FNiagaraHierarchyRootViewModel> HierarchyRootViewModel;

	TWeakPtr<struct FNiagaraHierarchySectionViewModel> ActiveHierarchySection;

	TSharedPtr<FUICommandList> Commands;

	TMap<UObject*, TSharedRef<IPropertyRowGenerator>> ObjectToPropertyRowGeneratorMap;
	TMap<UObject*, TArray<TSharedRef<IDetailTreeNode>>> ObjectTreeNodeMap;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraHierarchyDataRefreshContext> RefreshContext = nullptr;
	
protected:
	// delegate collection to call UI functions
	FSimpleDelegate RefreshSourceItemsRequestedDelegate;
	FOnRefreshViewRequested RefreshAllViewsRequestedDelegate;
	FOnRefreshViewRequested RefreshSourceViewDelegate;
	FOnRefreshViewRequested RefreshHierarchyWidgetDelegate;
	FSimpleDelegate RefreshSectionsViewDelegate;
	FOnNavigateToItemInHierarchyRequested OnNavigateToItemInHierarchyRequestedDelegate;
	
	FOnItemAdded OnItemAddedDelegate;
	FOnSectionActivated OnHierarchySectionActivatedDelegate;
	FOnSectionActivated OnSourceSectionActivatedDelegate;
	FOnHierarchyChanged OnHierarchyChangedDelegate;
	FOnHierarchyPropertiesChanged OnHierarchyPropertiesChangedDelegate;
	
	FSimpleDelegate OnInitializedDelegate;

	FTimerHandle FullRefreshNextFrameHandle;
};

struct FNiagaraHierarchyItemViewModelBase : TSharedFromThis<FNiagaraHierarchyItemViewModelBase>, public FTickableEditorObject
{
	DECLARE_DELEGATE(FOnSynced)
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilterChild, const TSharedPtr<const FNiagaraHierarchyItemViewModelBase> Child);
	DECLARE_DELEGATE_OneParam(FOnChildRequestedDeletion, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child)
	
	FNiagaraHierarchyItemViewModelBase(UNiagaraHierarchyItemBase* InItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy)
		: ItemBase(InItemBase)
		, Parent(InParent)
		, HierarchyViewModel(InHierarchyViewModel)
		, bIsForHierarchy(bInIsForHierarchy)
	{		
		
	}

	/** Can be implemented for additional logic that the constructor isn't valid for. */
	virtual void Initialize() {}
	
	NIAGARAEDITOR_API virtual ~FNiagaraHierarchyItemViewModelBase() override;
	
	UNiagaraHierarchyItemBase* GetDataMutable() const { return ItemBase; }
	const UNiagaraHierarchyItemBase* GetData() const { return ItemBase; }
	
	template<class T>
	T* GetDataMutable() const { return Cast<T>(ItemBase); }
	
	template<class T>
	const T* GetData() const { return Cast<T>(ItemBase); }
	
	NIAGARAEDITOR_API virtual void Tick(float DeltaTime) override;
	NIAGARAEDITOR_API virtual TStatId GetStatId() const override;
	
	virtual FString ToString() const { return ItemBase->ToString(); }
	FText ToStringAsText() const { return FText::FromString(ToString()); }
	virtual TArray<FString> GetSearchTerms() const { return {ToString()} ;}

	NIAGARAEDITOR_API void RefreshChildrenData();
	NIAGARAEDITOR_API void SyncViewModelsToData();
	const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetChildren() const { return Children; }
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetChildrenMutable() { return Children; }
	NIAGARAEDITOR_API const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& GetFilteredChildren() const;

	NIAGARAEDITOR_API void AddChildFilter(FOnFilterChild InFilterChild);
	
	template<class DataClass, class ViewModelChildClass>
	void GetChildrenViewModelsForType(TArray<TSharedPtr<ViewModelChildClass>>& OutChildren, bool bRecursive = false);

	/** Returns the hierarchy depth via number of parents above. */
	int32 GetHierarchyDepth() const;
	NIAGARAEDITOR_API bool HasParent(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ParentCandidate, bool bRecursive = false);

	NIAGARAEDITOR_API void AddChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item);
	NIAGARAEDITOR_API TSharedPtr<FNiagaraHierarchyItemViewModelBase> AddNewItem(TSubclassOf<UNiagaraHierarchyItemBase> NewItemClass);
	NIAGARAEDITOR_API TSharedRef<FNiagaraHierarchyItemViewModelBase> DuplicateToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToDuplicate, int32 InsertIndex = INDEX_NONE);
	NIAGARAEDITOR_API TSharedRef<FNiagaraHierarchyItemViewModelBase> ReparentToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToMove, int32 InsertIndex = INDEX_NONE);

	NIAGARAEDITOR_API TSharedPtr<FNiagaraHierarchyItemViewModelBase> FindViewModelForChild(UNiagaraHierarchyItemBase* Child, bool bSearchRecursively = false) const;
	NIAGARAEDITOR_API TSharedPtr<FNiagaraHierarchyItemViewModelBase> FindViewModelForChild(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively = false) const;
	NIAGARAEDITOR_API int32 FindIndexOfChild(UNiagaraHierarchyItemBase* Child) const;
	NIAGARAEDITOR_API int32 FindIndexOfDataChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child) const;
	NIAGARAEDITOR_API int32 FindIndexOfDataChild(UNiagaraHierarchyItemBase* Child) const;

	NIAGARAEDITOR_API UNiagaraHierarchyItemBase* AddChild(TSubclassOf<UNiagaraHierarchyItemBase> NewChildClass, FNiagaraHierarchyIdentity ChildIdentity);
	
	/** Deleting will ask the parent to delete its child */
	NIAGARAEDITOR_API void Delete();
	NIAGARAEDITOR_API void DeleteChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child);

	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> GetHierarchyViewModel() const { return HierarchyViewModel; }

	/** Returns a set result if the item can accept a drop either above/onto/below the item.  */
	NIAGARAEDITOR_API TOptional<EItemDropZone> OnCanRowAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item);
	NIAGARAEDITOR_API virtual FReply OnDroppedOnRow(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item);
	NIAGARAEDITOR_API void OnRowDragLeave(const FDragDropEvent& DragDropEvent);

	struct FCanPerformActionResults
	{
		FCanPerformActionResults(bool bInCanPerform) : bCanPerform(bInCanPerform) {}
		
		bool bCanPerform = false;
		/** A message that is used when bCanPerform is false. Will either be used in tooltips in the hierarchy editor or as popup message. */
		FText CanPerformMessage;

		bool operator==(const bool& bOther) const
		{
			return bCanPerform == bOther;
		}

		bool operator!=(const bool& bOther) const
		{
			return !(*this==bOther);
		}
	};

	/** Should return true if properties are supposed to be editable & needs to be true if typical operations should work on it (renaming, dragging, deleting etc.) */
	virtual FCanPerformActionResults IsEditableByUser() { return FCanPerformActionResults(false); }
	
	/** Needs to be true in order to allow drag & drop operations to parent items to this item */
	virtual bool CanHaveChildren() const { return false; }
	
	/** Should return true if an item should be draggable. An uneditable item can not be dragged even if CanDragInternal returns true. */
	NIAGARAEDITOR_API FCanPerformActionResults CanDrag();
	
	/** Returns true if renamable */
	bool CanRename() { return IsEditableByUser().bCanPerform && CanRenameInternal(); }

	void Rename(FName NewName) { RenameInternal(NewName); HierarchyViewModel->OnHierarchyPropertiesChanged().Broadcast(); }

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
	
	/** Returns true if deletable */
	bool CanDelete() { return IsEditableByUser().bCanPerform && CanDeleteInternal(); }

	/** Returns true if the given item can be dropped on the given target area. */
	FCanPerformActionResults CanDropOn(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone) { return CanDropOnInternal(DraggedItem, ItemDropZone); }

	/** Gets executed when an item was dropped on this. */
	void OnDroppedOn(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) { OnDroppedOnInternal(DroppedItem, ItemDropZone); }

	/** Determines the section this item belongs to. */
	const UNiagaraHierarchySection* GetSection() const { return GetSectionInternal(); }
	
	/** For data cleanup that represents external data, this needs to return true in order for live cleanup to work. */
	virtual bool RepresentsExternalData() const { return false; }
	/** This function determines whether a hierarchy item that represents that external data should be maintained during data refresh
	 * Needs to be implemented if RepresentsExternalData return true.
	 * The context object can be used to add arbitrary data. */
	virtual bool DoesExternalDataStillExist(const UNiagaraHierarchyDataRefreshContext* Context) const { return false; }

	bool IsForHierarchy() const { return bIsForHierarchy; }

	/** Override this to register dynamic context menu entries when right clicking a hierarchy item */
	virtual void PopulateDynamicContextMenuSection(FToolMenuSection& DynamicSection) {}

	/** The UObject we display in the details panel when this item is selected. By default it's the item the view model represents. */
	virtual UObject* GetDataForEditing() { return ItemBase; }
	
	/** Used to create customized drag drop ops. */
	NIAGARAEDITOR_API TSharedRef<class FNiagaraHierarchyDragDropOp> CreateDragDropOp();
	
	NIAGARAEDITOR_API FReply OnDragDetected(const FGeometry& Geometry, const FPointerEvent& PointerEvent, bool bIsSource);

	FSimpleDelegate& GetOnRequestRename() { return OnRequestRenameDelegate; }
	FOnSynced& GetOnSynced() { return OnSyncedDelegate; }
	FOnChildRequestedDeletion& OnChildRequestedDeletion() { return OnChildRequestedDeletionDelegate; }

	TWeakPtr<FNiagaraHierarchyItemViewModelBase> GetParent() { return Parent; }

protected:
	/** Should return true if draggable. An optional message can be provided if false that will show as a slate notification. */
	virtual FCanPerformActionResults CanDragInternal() { return false; }

	/** Should return true if renamable */
	virtual bool CanRenameInternal() { return false; }

	virtual void RenameInternal(FName NewName) {}
	
	/** Should return true if deletable. By default, we can delete items in the hierarchy, not in the source. */
	virtual bool CanDeleteInternal() { return IsForHierarchy(); }

	/** Should return true if the given drag drop operation is allowed to succeed. */
	NIAGARAEDITOR_API virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone);
	
	/** Override this to handle drop-on logic. This is called when an item has been dropped on the item that has implemented this function. */
	virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) { }

	/** Can be overridden to support custom sections.
	 * In the hierarchy only categories can be parented directly to the root, but using this it is possible to add items to custom sections in the source panel.
	 * This will only work for top-level objects, i.e. anything directly under the root. */
	virtual const UNiagaraHierarchySection* GetSectionInternal() const { return nullptr; }
private:
	/** View models can implement this to add or remove data. */
	virtual void RefreshChildrenDataInternal() {}
	/** View models can implement this to further customize the view model sync process.
	 * An example for this is how the root view model handles sections, as sections exist outside the children hierarchy */
	virtual void SyncViewModelsToDataInternal() {}
	virtual void FinalizeInternal() {}

	NIAGARAEDITOR_API void PropagateOnChildSynced();
protected:
	UNiagaraHierarchyItemBase* const ItemBase;
	/** Parent should be valid for all instances of this struct except for root objects */
	TWeakPtr<FNiagaraHierarchyItemViewModelBase> Parent;
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> Children;
	TArray<FOnFilterChild> ChildFilters;
	mutable TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> FilteredChildren;
	TWeakObjectPtr<UNiagaraHierarchyViewModelBase> HierarchyViewModel;
	FSimpleDelegate OnRequestRenameDelegate;
	FOnSynced OnSyncedDelegate;
	FOnChildRequestedDeletion OnChildRequestedDeletionDelegate;
	bool bRenamePending = false;
	bool bIsForHierarchy = false;
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
	DECLARE_DELEGATE(FOnSyncPropagated)
	DECLARE_DELEGATE(FOnSectionsChanged)
	DECLARE_DELEGATE_OneParam(FOnSingleSectionChanged, TSharedPtr<FNiagaraHierarchySectionViewModel> AddedSection)
	
	FNiagaraHierarchyRootViewModel(UNiagaraHierarchyItemBase* InItem, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy) : FNiagaraHierarchyItemViewModelBase(InItem, nullptr, InHierarchyViewModel, bInIsForHierarchy) {}
	virtual ~FNiagaraHierarchyRootViewModel() override;

	virtual void Initialize() override;
	
	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone) override;
	virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) override;

	virtual bool CanHaveChildren() const override { return true; }

	TSharedPtr<struct FNiagaraHierarchySectionViewModel> AddSection();
	void DeleteSection(TSharedPtr<FNiagaraHierarchyItemViewModelBase> SectionViewModel);
	TArray<TSharedPtr<struct FNiagaraHierarchySectionViewModel>>& GetSectionViewModels() { return SectionViewModels; }

	FOnSyncPropagated& OnSyncPropagated() { return OnSyncPropagatedDelegate; }

	/** General purpose delegate for when sections change */
	FOnSectionsChanged& OnSectionsChanged() { return OnSectionsChangedDelegate; }
	/** Delegates for when a section is added or removed */
	FOnSingleSectionChanged& OnSectionAdded() { return OnSectionAddedDelegate; }
	FOnSingleSectionChanged& OnSectionDeleted() { return OnSectionDeletedDelegate; }

private:
	void PropagateOnSynced();
	virtual void SyncViewModelsToDataInternal() override;
	TArray<TSharedPtr<struct FNiagaraHierarchySectionViewModel>> SectionViewModels;

	FOnSyncPropagated OnSyncPropagatedDelegate;
	FOnSingleSectionChanged OnSectionAddedDelegate;
	FOnSingleSectionChanged OnSectionDeletedDelegate;
	FOnSectionsChanged OnSectionsChangedDelegate;
};

struct FNiagaraHierarchySectionViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchySectionViewModel(UNiagaraHierarchySection* InItem, TSharedPtr<FNiagaraHierarchyRootViewModel> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy) : FNiagaraHierarchyItemViewModelBase(InItem, InParent, InHierarchyViewModel, bInIsForHierarchy) {}
	
	virtual ~FNiagaraHierarchySectionViewModel() override {}

	void SetSectionName(FName InSectionName) const { Cast<UNiagaraHierarchySection>(ItemBase)->SetSectionName(InSectionName); }
	FName GetSectionName() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetSectionName(); }
	
	void SetSectionNameAsText(const FText& Text) const { Cast<UNiagaraHierarchySection>(ItemBase)->SetSectionNameAsText(Text); }
	FText GetSectionNameAsText() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetSectionNameAsText(); }
	FText GetSectionTooltip() const { return Cast<UNiagaraHierarchySection>(ItemBase)->GetTooltip(); }
	
	void SetSectionImage(const FSlateBrush* InSectionImage) { SectionImage = InSectionImage; }
	const FSlateBrush* GetSectionImage() const { return SectionImage; }

protected:
	/** Only hierarchy sections are editable */
	virtual FCanPerformActionResults IsEditableByUser() override { return FCanPerformActionResults(IsForHierarchy()); }
	virtual bool CanHaveChildren() const override { return false; }
	virtual FCanPerformActionResults CanDragInternal() override { return bIsForHierarchy; }
	/** We can only rename hierarchy sections */
	virtual bool CanRenameInternal() override { return IsForHierarchy(); }
	virtual void RenameInternal(FName NewName) override { GetDataMutable<UNiagaraHierarchySection>()->SetSectionName(NewName); }

	virtual void PopulateDynamicContextMenuSection(FToolMenuSection& DynamicSection) override;

	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone) override;
	virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) override;

	virtual void FinalizeInternal() override;

private:
	const FSlateBrush* SectionImage = nullptr;
};

struct FNiagaraHierarchyItemViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchyItemViewModel(UNiagaraHierarchyItem* InItem, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy) : FNiagaraHierarchyItemViewModelBase(InItem, InParent, InHierarchyViewModel, bInIsForHierarchy) {}
	
	virtual ~FNiagaraHierarchyItemViewModel() override {}

	virtual FCanPerformActionResults IsEditableByUser() override { return FCanPerformActionResults(true); }
	virtual bool CanHaveChildren() const override { return false; }
	virtual FCanPerformActionResults CanDragInternal() override { return true; }

	NIAGARAEDITOR_API virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) override;
	NIAGARAEDITOR_API virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) override;
};

struct FNiagaraHierarchyCategoryViewModel : FNiagaraHierarchyItemViewModelBase
{
	FNiagaraHierarchyCategoryViewModel(UNiagaraHierarchyCategory* InCategory, TSharedPtr<FNiagaraHierarchyItemViewModelBase> InParent, TWeakObjectPtr<UNiagaraHierarchyViewModelBase> InHierarchyViewModel, bool bInIsForHierarchy) : FNiagaraHierarchyItemViewModelBase(InCategory, InParent, InHierarchyViewModel, bInIsForHierarchy) {}
	virtual ~FNiagaraHierarchyCategoryViewModel() override{}

	FText GetCategoryName() const { return GetData<UNiagaraHierarchyCategory>()->GetCategoryAsText(); }
	
	virtual FCanPerformActionResults IsEditableByUser() override { return FCanPerformActionResults(true); }
	virtual bool CanHaveChildren() const override { return true; }
	virtual FCanPerformActionResults CanDragInternal() override { return true; }
	virtual bool CanRenameInternal() override { return true; }
	virtual void RenameInternal(FName NewName) override { GetDataMutable<UNiagaraHierarchyCategory>()->SetCategoryName(NewName); }
	virtual const UNiagaraHierarchySection* GetSectionInternal() const override { return Cast<UNiagaraHierarchyCategory>(ItemBase)->GetSection(); }

	bool IsTopCategoryActive() const;

	virtual FCanPerformActionResults CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone) override;
	virtual void OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone) override;
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
