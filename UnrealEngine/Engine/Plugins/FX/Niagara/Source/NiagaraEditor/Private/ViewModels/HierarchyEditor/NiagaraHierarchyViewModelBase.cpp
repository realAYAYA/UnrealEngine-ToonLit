// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/HierarchyEditor/NiagaraHierarchyViewModelBase.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScriptVariable.h"
#include "SDropTarget.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SNiagaraHierarchy.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Input/SCheckBox.h"
#include "Editor.h"
#include "IPropertyRowGenerator.h"
#include "ScopedTransaction.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "NiagaraHierarchyEditor"

const TArray<UNiagaraHierarchyItemBase*>& UNiagaraHierarchyItemBase::GetChildren() const
{
	return Children;
}

UNiagaraHierarchyItemBase* UNiagaraHierarchyItemBase::FindChildWithIdentity(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively)
{
	TObjectPtr<UNiagaraHierarchyItemBase>* FoundItem = Children.FindByPredicate([ChildIdentity](UNiagaraHierarchyItemBase* Child)
	{
		return Child->GetPersistentIdentity() == ChildIdentity;
	});

	if(FoundItem)
	{
		return *FoundItem;
	}
	
	if(bSearchRecursively)
	{
		for(UNiagaraHierarchyItemBase* Child : Children)
		{
			UNiagaraHierarchyItemBase* FoundChild = Child->FindChildWithIdentity(ChildIdentity, bSearchRecursively);

			if(FoundChild)
			{
				return FoundChild;
			}
		}
	}	

	return nullptr;
}

UNiagaraHierarchyItemBase* UNiagaraHierarchyItemBase::CopyAndAddItemAsChild(const UNiagaraHierarchyItemBase& ItemToCopy)
{
	UNiagaraHierarchyItemBase* NewChild = Cast<UNiagaraHierarchyItemBase>(StaticDuplicateObject(&ItemToCopy, this));
	if(NewChild->GetPersistentIdentity() != ItemToCopy.GetPersistentIdentity())
	{
		check(false);
	}
	GetChildrenMutable().Add(NewChild);

	return NewChild;
}

UNiagaraHierarchyItemBase* UNiagaraHierarchyItemBase::CopyAndAddItemUnderParentIdentity(const UNiagaraHierarchyItemBase& ItemToCopy, FNiagaraHierarchyIdentity ParentIdentity)
{
	UNiagaraHierarchyItemBase* ParentItem = FindChildWithIdentity(ParentIdentity, true);

	if(ParentItem)
	{
		UNiagaraHierarchyItemBase* NewChild = Cast<UNiagaraHierarchyItemBase>(StaticDuplicateObject(&ItemToCopy, ParentItem));
		if(NewChild->GetPersistentIdentity() != ItemToCopy.GetPersistentIdentity())
		{
			check(false);
		}
		ParentItem->GetChildrenMutable().Add(NewChild);
		return NewChild;
	}

	return nullptr;
}

bool UNiagaraHierarchyItemBase::RemoveChildWithIdentity(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively)
{	
	int32 RemovedChildrenCount = Children.RemoveAll([ChildIdentity](UNiagaraHierarchyItemBase* Child)
	{
		return Child->GetPersistentIdentity() == ChildIdentity;
	});

	if(RemovedChildrenCount > 1)
	{
		UE_LOG(LogNiagaraEditor, Warning, TEXT("More than one child with the same identity has been found in parent %s"), *ToString());
	}
	
	bool bChildrenRemoved = RemovedChildrenCount > 0;
	
	if(bSearchRecursively && bChildrenRemoved == false)
	{
		for(UNiagaraHierarchyItemBase* Child : Children)
		{
			bChildrenRemoved |= Child->RemoveChildWithIdentity(ChildIdentity, bSearchRecursively);
		}
	}

	return bChildrenRemoved;
}

TArray<FNiagaraHierarchyIdentity> UNiagaraHierarchyItemBase::GetParentIdentities() const
{
	TArray<FNiagaraHierarchyIdentity> ParentIdentities;

	for(UNiagaraHierarchyItemBase* Parent = Cast<UNiagaraHierarchyItemBase>(GetOuter()); Parent != nullptr; Parent = Cast<UNiagaraHierarchyItemBase>(Parent->GetOuter()))
	{
		ParentIdentities.Add(Parent->GetPersistentIdentity());
	}

	return ParentIdentities;
}

bool UNiagaraHierarchyItemBase::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = true;

	for(UNiagaraHierarchyItemBase* Child : Children)
	{
		bSavedToTransactionBuffer &= Child->Modify(bAlwaysMarkDirty);
	}
	
	bSavedToTransactionBuffer &= UObject::Modify(bAlwaysMarkDirty);

	return bSavedToTransactionBuffer;
}

void UNiagaraHierarchyItemBase::PostLoad()
{
	if(Guid_DEPRECATED.IsValid())
	{
		SetIdentity(FNiagaraHierarchyIdentity({Guid_DEPRECATED}, {}));
	}

	Super::PostLoad();
}

UNiagaraHierarchySection* UNiagaraHierarchyRoot::AddSection(FText InNewSectionName, int32 InsertIndex)
{
	TSet<FName> ExistingSectionNames;
	
	for(FName& SectionName : GetSections())
	{
		ExistingSectionNames.Add(SectionName);
	}
	
	FName NewName = FNiagaraUtilities::GetUniqueName(FName(InNewSectionName.ToString()), ExistingSectionNames);
	UNiagaraHierarchySection* NewSectionItem = NewObject<UNiagaraHierarchySection>(this);
	NewSectionItem->SetSectionName(NewName);
	NewSectionItem->SetFlags(RF_Transactional);
	
	if(InsertIndex == INDEX_NONE)
	{
		Sections.Add(NewSectionItem);
	}
	else
	{
		Sections.Insert(NewSectionItem, InsertIndex);
	}
	
	return NewSectionItem;
}

UNiagaraHierarchySection* UNiagaraHierarchyRoot::FindSectionByIdentity(FNiagaraHierarchyIdentity SectionIdentity)
{
	for(UNiagaraHierarchySection* Section : Sections)
	{
		if(Section->GetPersistentIdentity() == SectionIdentity)
		{
			return Section;
		}
	}

	return nullptr;
}

void UNiagaraHierarchyRoot::DuplicateSectionFromOtherRoot(const UNiagaraHierarchySection& SectionToCopy)
{
	if(FindSectionByIdentity(SectionToCopy.GetPersistentIdentity()) != nullptr || SectionToCopy.GetOuter() == this)
	{
		return;
	}
	
	Sections.Add(Cast<UNiagaraHierarchySection>(StaticDuplicateObject(&SectionToCopy, this)));
}

void UNiagaraHierarchyRoot::RemoveSection(FText SectionName)
{
	if(Sections.ContainsByPredicate([SectionName](UNiagaraHierarchySection* Section)
	{
		return Section->GetSectionNameAsText().EqualTo(SectionName);
	}))
	{
		Sections.RemoveAll([SectionName](UNiagaraHierarchySection* Section)
		{
			return Section->GetSectionNameAsText().EqualTo(SectionName);
		});
	}
}

void UNiagaraHierarchyRoot::RemoveSectionByIdentity(FNiagaraHierarchyIdentity SectionIdentity)
{
	Sections.RemoveAll([SectionIdentity](UNiagaraHierarchySection* Section)
	{
		return Section->GetPersistentIdentity() == SectionIdentity;
	});
}

TSet<FName> UNiagaraHierarchyRoot::GetSections() const
{
	TSet<FName> OutSections;
	for(UNiagaraHierarchySection* Section : Sections)
	{
		OutSections.Add(Section->GetSectionName());
	}

	return OutSections;
}

int32 UNiagaraHierarchyRoot::GetSectionIndex(UNiagaraHierarchySection* Section) const
{
	return Sections.Find(Section);
}

bool UNiagaraHierarchyRoot::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = true;
	
	for(UNiagaraHierarchySection* Section : Sections)
	{
		bSavedToTransactionBuffer &= Section->Modify();
	}
	
	bSavedToTransactionBuffer &= Super::Modify(bAlwaysMarkDirty);	

	return bSavedToTransactionBuffer;
}

bool FNiagaraHierarchyCategoryViewModel::IsTopCategoryActive() const
{
	if(UNiagaraHierarchyCategory* Category = GetDataMutable<UNiagaraHierarchyCategory>())
	{
		const UNiagaraHierarchyCategory* Result = Category;
		const UNiagaraHierarchyCategory* TopLevelCategory = Result;
		
		for (; TopLevelCategory != nullptr; TopLevelCategory = TopLevelCategory->GetTypedOuter<UNiagaraHierarchyCategory>() )
		{
			if(TopLevelCategory != nullptr)
			{
				Result = TopLevelCategory;
			}
		}
		
		return HierarchyViewModel->IsHierarchySectionActive(Result->GetSection());
	}

	return false;	
}

FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchyCategoryViewModel::CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone)
{
	FCanPerformActionResults Results(false);
	
	TArray<TSharedPtr<FNiagaraHierarchyCategoryViewModel>> TargetChildrenCategories;
	GetChildrenViewModelsForType<UNiagaraHierarchyCategory, FNiagaraHierarchyCategoryViewModel>(TargetChildrenCategories);
	
	TArray<TSharedPtr<FNiagaraHierarchyCategoryViewModel>> SiblingCategories;
	Parent.Pin()->GetChildrenViewModelsForType<UNiagaraHierarchyCategory, FNiagaraHierarchyCategoryViewModel>(SiblingCategories);
	
	// we only allow drops if some general conditions are fulfilled
	if(DraggedItem->GetData() != GetData() &&
		(!DraggedItem->HasParent(AsShared(), false) || ItemDropZone != EItemDropZone::OntoItem)  &&
		!HasParent(DraggedItem, true))
	{
		// categories can be dropped on categories, but only if the resulting sibling categories or children categories have different names
		if(DraggedItem->GetData()->IsA<UNiagaraHierarchyCategory>())
		{
			if(ItemDropZone != EItemDropZone::OntoItem)
			{
				bool bContainsSiblingWithSameName = SiblingCategories.ContainsByPredicate([DraggedItem](TSharedPtr<FNiagaraHierarchyCategoryViewModel> HierarchyCategoryViewModel)
					{
						return DraggedItem->ToString() == HierarchyCategoryViewModel->ToString() && DraggedItem != HierarchyCategoryViewModel;
					});

				if(bContainsSiblingWithSameName)
				{
					Results.bCanPerform = false;
					Results.CanPerformMessage = LOCTEXT("CantDropCategorNextToCategorySameSiblingNames", "A category of the same name already exists here, potentially in a different section. Please rename your category first.");
					return Results;
				}

				// if we are making a category a sibling of another at the root level, the section will be set to the currently active section. Let that be known.
				if(Parent.Pin()->GetData()->IsA<UNiagaraHierarchyRoot>())
				{
					UNiagaraHierarchyCategory* DraggedCategory = Cast<UNiagaraHierarchyCategory>(DraggedItem->GetDataMutable());
					if(DraggedCategory->GetSection() != HierarchyViewModel->GetActiveHierarchySectionData())
					{
						FText BaseMessage = LOCTEXT("CategorySectionWillUpdateDueToDrop", "The section of the category will change to {0} after the drop");
						Results.CanPerformMessage = FText::FormatOrdered(BaseMessage, HierarchyViewModel->GetActiveHierarchySectionData() == nullptr ? FText::FromString("All") : HierarchyViewModel->GetActiveHierarchySectionData()->GetSectionNameAsText());
					}
				}
			}
			else
			{
				bool bContainsChildrenCategoriesWithSameName = TargetChildrenCategories.ContainsByPredicate([DraggedItem](TSharedPtr<FNiagaraHierarchyCategoryViewModel> HierarchyCategoryViewModel)
					{
						return DraggedItem->ToString() == HierarchyCategoryViewModel->ToString();
					});

				if(bContainsChildrenCategoriesWithSameName)
				{
					Results.bCanPerform = false;
					Results.CanPerformMessage = LOCTEXT("CantDropCategoryOnCategorySameChildCategoryName", "A sub-category of the same name already exists! Please rename your category first.");
					return Results;
				}
			}

			Results.bCanPerform = true;
			return Results;
		}
		else if(DraggedItem->GetData()->IsA<UNiagaraHierarchyItem>())
		{
			// items can generally be dropped onto categories
			Results.bCanPerform = EItemDropZone::OntoItem == ItemDropZone;

			if(Results.bCanPerform)
			{
				if(DraggedItem->IsForHierarchy() == false)
				{
					FText Message = LOCTEXT("AddItemToCategoryDragMessage", "Add {0} to {1}");
					Results.CanPerformMessage = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()), FText::FromString(ToString()));
				}
				else
				{
					FText Message = LOCTEXT("MoveItemToCategoryDragMessage", "Move {0} to {1}");
					Results.CanPerformMessage = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()), FText::FromString(ToString()));
				}
			}
		}
	}

	return Results;
}

void UNiagaraHierarchyCategory::FixupSectionLinkage()
{
	UNiagaraHierarchyRoot* OwningRoot = GetTypedOuter<UNiagaraHierarchyRoot>();

	if(Section != nullptr && Section->GetTypedOuter<UNiagaraHierarchyRoot>() != OwningRoot)
	{
		UNiagaraHierarchySection* CorrectSection = OwningRoot->FindSectionByIdentity(Section->GetPersistentIdentity());
		ensure(CorrectSection != nullptr);
		Section = CorrectSection;
	}
}

void UNiagaraHierarchySection::SetSectionNameAsText(const FText& Text)
{
	Section = FName(Text.ToString());
}

UNiagaraHierarchyViewModelBase::UNiagaraHierarchyViewModelBase()
{
	Commands = MakeShared<FUICommandList>();
}

UNiagaraHierarchyViewModelBase::~UNiagaraHierarchyViewModelBase()
{
	RefreshSourceViewDelegate.Unbind();
	RefreshHierarchyWidgetDelegate.Unbind();
	RefreshSectionsViewDelegate.Unbind();
}

void UNiagaraHierarchyViewModelBase::Initialize()
{		
	HierarchyRoot = GetHierarchyRoot();
	HierarchyRoot->SetFlags(RF_Transactional);

	TArray<UNiagaraHierarchyItemBase*> AllItems;
	HierarchyRoot->GetChildrenOfType<UNiagaraHierarchyItemBase>(AllItems, true);
	for(UNiagaraHierarchyItemBase* Item : AllItems)
	{
		Item->SetFlags(RF_Transactional);
	}

	for(UNiagaraHierarchySection* Section : HierarchyRoot->GetSectionDataMutable())
	{
		Section->SetFlags(RF_Transactional);
	}

	UToolMenus* ToolMenus = UToolMenus::Get();

	FName MenuName("NiagaraHierarchyMenu");
	if(!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* HierarchyMenu = ToolMenus->RegisterMenu(MenuName, NAME_None, EMultiBoxType::Menu);
		HierarchyMenu->AddSection("Base");
	}
	
	SetupCommands();

	HierarchyRootViewModel = MakeShared<FNiagaraHierarchyRootViewModel>(HierarchyRoot.Get(), this, true);
	HierarchyRootViewModel->Initialize();
	HierarchyRootViewModel->AddChildFilter(FNiagaraHierarchyItemViewModelBase::FOnFilterChild::CreateUObject(this, &UNiagaraHierarchyViewModelBase::FilterForHierarchySection));
	HierarchyRootViewModel->AddChildFilter(FNiagaraHierarchyItemViewModelBase::FOnFilterChild::CreateUObject(this, &UNiagaraHierarchyViewModelBase::FilterForUncategorizedRootItemsInAllSection));
	HierarchyRootViewModel->SyncViewModelsToData();
	
	SetActiveHierarchySection(nullptr);
	
	InitializeInternal();
	
	OnInitializedDelegate.ExecuteIfBound();
}

void UNiagaraHierarchyViewModelBase::Finalize()
{	
	HierarchyRootViewModel.Reset();
	HierarchyRoot = nullptr;
	
	FinalizeInternal();
}

void UNiagaraHierarchyViewModelBase::ForceFullRefresh()
{
	RefreshSourceItemsRequestedDelegate.ExecuteIfBound();
	// todo (me) during merge at startup this can be nullptr for some reason
	if(HierarchyRootViewModel.IsValid())
	{
		HierarchyRootViewModel->SyncViewModelsToData();
	}
	RefreshAllViewsRequestedDelegate.ExecuteIfBound(true);
}

void UNiagaraHierarchyViewModelBase::ForceFullRefreshOnTimer()
{
	ensure(FullRefreshNextFrameHandle.IsValid());
	ForceFullRefresh();
	FullRefreshNextFrameHandle.Invalidate();
}

void UNiagaraHierarchyViewModelBase::RequestFullRefreshNextFrame()
{
	if(!FullRefreshNextFrameHandle.IsValid() && GEditor != nullptr)
	{
		FullRefreshNextFrameHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UNiagaraHierarchyViewModelBase::ForceFullRefreshOnTimer));
	}
}

void UNiagaraHierarchyViewModelBase::RefreshAllViews(bool bFullRefresh) const
{
	RefreshAllViewsRequestedDelegate.ExecuteIfBound(bFullRefresh);
}

void UNiagaraHierarchyViewModelBase::RefreshSourceView(bool bFullRefresh) const
{
	RefreshSourceViewDelegate.ExecuteIfBound(bFullRefresh);
}

void UNiagaraHierarchyViewModelBase::RefreshHierarchyView(bool bFullRefresh) const
{
	RefreshHierarchyWidgetDelegate.ExecuteIfBound(bFullRefresh);
}

void UNiagaraHierarchyViewModelBase::RefreshSectionsView() const
{
	RefreshSectionsViewDelegate.ExecuteIfBound();
}

void UNiagaraHierarchyViewModelBase::PostUndo(bool bSuccess)
{
	ForceFullRefresh();
}

void UNiagaraHierarchyViewModelBase::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}

bool UNiagaraHierarchyViewModelBase::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	for(const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectContext : TransactionObjectContexts)
	{
		if(TransactionObjectContext.Key->IsA<UNiagaraHierarchyItemBase>())
		{
			return true;
		}
	}
	
	return false;
}

bool UNiagaraHierarchyViewModelBase::FilterForHierarchySection(TSharedPtr<const FNiagaraHierarchyItemViewModelBase> ItemViewModel) const
{
	if(ActiveHierarchySection.IsValid())
	{
		return GetActiveHierarchySectionData() == ItemViewModel->GetSection();
	}

	return true;
}

bool UNiagaraHierarchyViewModelBase::FilterForUncategorizedRootItemsInAllSection(TSharedPtr<const FNiagaraHierarchyItemViewModelBase> ItemViewModel) const
{
	// we want to filter out all items that are directly added to the root if we aren't in the 'All' section
	if(ActiveHierarchySection.IsValid())
	{
		return ItemViewModel->GetData<UNiagaraHierarchyCategory>() != nullptr;
	}

	return true;
}

TSharedPtr<FNiagaraHierarchyItemViewModelBase> UNiagaraHierarchyViewModelBase::CreateViewModelForData(UNiagaraHierarchyItemBase* ItemBase, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Parent)
{
	if(UNiagaraHierarchyItem* Item = Cast<UNiagaraHierarchyItem>(ItemBase))
	{
		return MakeShared<FNiagaraHierarchyItemViewModel>(Item, Parent, this, Parent.IsValid() ? Parent->IsForHierarchy() : false);
	}
	else if(UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(ItemBase))
	{
		return MakeShared<FNiagaraHierarchyCategoryViewModel>(Category, Parent, this, Parent.IsValid() ? Parent->IsForHierarchy() : false);
	}

	return nullptr;
}

const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& UNiagaraHierarchyViewModelBase::GetHierarchyItems() const
{
	return HierarchyRootViewModel->GetFilteredChildren();
}

void UNiagaraHierarchyViewModelBase::OnGetChildren(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item, TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& OutChildren) const
{
	OutChildren.Append(Item->GetFilteredChildren());
}

void UNiagaraHierarchyViewModelBase::SetActiveHierarchySection(TSharedPtr<FNiagaraHierarchySectionViewModel> Section)
{
	ActiveHierarchySection = Section;	
	RefreshHierarchyView(true);
	OnHierarchySectionActivatedDelegate.ExecuteIfBound(Section);
}

TSharedPtr<FNiagaraHierarchySectionViewModel> UNiagaraHierarchyViewModelBase::GetActiveHierarchySection() const
{
	return ActiveHierarchySection.IsValid() ? ActiveHierarchySection.Pin() : nullptr;
}

UNiagaraHierarchySection* UNiagaraHierarchyViewModelBase::GetActiveHierarchySectionData() const
{
	return ActiveHierarchySection.IsValid() ? ActiveHierarchySection.Pin()->GetDataMutable<UNiagaraHierarchySection>() : nullptr;
}

bool UNiagaraHierarchyViewModelBase::IsHierarchySectionActive(const UNiagaraHierarchySection* Section) const
{
	return ActiveHierarchySection == nullptr || ActiveHierarchySection.Pin()->GetData() == Section;
}

FString UNiagaraHierarchyViewModelBase::OnItemToStringDebug(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemBaseViewModel) const
{
	return ItemBaseViewModel->ToString();	
}

FNiagaraHierarchyItemViewModelBase::~FNiagaraHierarchyItemViewModelBase()
{
	Children.Empty();
	FilteredChildren.Empty();
}

UNiagaraHierarchyItemBase* FNiagaraHierarchyItemViewModelBase::AddChild(TSubclassOf<UNiagaraHierarchyItemBase> NewChildClass, FNiagaraHierarchyIdentity ChildIdentity)
{
	UNiagaraHierarchyItemBase* NewChild = NewObject<UNiagaraHierarchyItemBase>(GetDataMutable(), NewChildClass);
	NewChild->SetIdentity(ChildIdentity);
	GetDataMutable()->GetChildrenMutable().Add(NewChild);
	
	SyncViewModelsToData();
	HierarchyViewModel->OnHierarchyChanged().Broadcast();
	return NewChild;
}

void FNiagaraHierarchyItemViewModelBase::Tick(float DeltaTime)
{
	if(bRenamePending)
	{
		RequestRename();
	}
}

TStatId FNiagaraHierarchyItemViewModelBase::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraHierarchyItemViewModelBase, STATGROUP_Tickables);
}

void FNiagaraHierarchyItemViewModelBase::RefreshChildrenData()
{
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> TmpChildren = Children;
	for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child : TmpChildren)
	{
		if(Child->RepresentsExternalData() && Child->DoesExternalDataStillExist(HierarchyViewModel->GetRefreshContext()) == false)
		{
			Child->Delete();
		}
	}

	RefreshChildrenDataInternal();

	/** All remaining children are supposed to exist at this point, as internal data won't be removed by refreshing & external data was cleaned up already.
	 * This will not call RefreshChildrenData on data that has just been added as no view models exist for these yet.
	 */
	for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child : Children)
	{
		Child->RefreshChildrenData();
	}
}

void FNiagaraHierarchyItemViewModelBase::SyncViewModelsToData()
{	
	// this will recursively remove all outdated external data as well as give individual view models the chance to add new data
	RefreshChildrenData();
	
	// now that the data is refreshed, we can sync to the data by recycling view models & creating new ones
	// old view models will get deleted automatically
	TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> NewChildren;
	for(UNiagaraHierarchyItemBase* Child : ItemBase->GetChildren())
	{		
		int32 ViewModelIndex = FindIndexOfChild(Child);
		// if we couldn't find a view model for a data child, we create it here
		if(ViewModelIndex == INDEX_NONE)
		{
			TSharedPtr<FNiagaraHierarchyItemViewModelBase> ChildViewModel = HierarchyViewModel->CreateViewModelForData(Child, AsShared());
			if(ensure(ChildViewModel.IsValid()))
			{
				ChildViewModel->Initialize();
				ChildViewModel->SyncViewModelsToData();
				NewChildren.Add(ChildViewModel);
			}
		}
		// if we could find the view model, we refresh its contained view models and readd it
		else
		{
			Children[ViewModelIndex]->SyncViewModelsToData();
			NewChildren.Add(Children[ViewModelIndex]);
		}
	}

	Children.Empty();
	Children.Append(NewChildren);
	
	for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child : Children)
	{
		Child->OnChildRequestedDeletion().BindSP(this, &FNiagaraHierarchyItemViewModelBase::DeleteChild);
		Child->GetOnSynced().BindSP(this, &FNiagaraHierarchyItemViewModelBase::PropagateOnChildSynced);
	}

	/** Give the view models a chance to further customize the children sync process. */
	SyncViewModelsToDataInternal();	

	// first we sort the data. Categories before items.
	GetDataMutable()->GetChildrenMutable().StableSort([](const UNiagaraHierarchyItemBase& ItemA, const UNiagaraHierarchyItemBase& ItemB)
		{
			return ItemA.IsA<UNiagaraHierarchyCategory>() && ItemB.IsA<UNiagaraHierarchyItem>();
		});

	// then we sort the view models according to the data order as this is what will determine widget order created from the view models
	Children.Sort([this](const TSharedPtr<FNiagaraHierarchyItemViewModelBase>& ItemA, const TSharedPtr<FNiagaraHierarchyItemViewModelBase>& ItemB)
		{
			return FindIndexOfDataChild(ItemA) < FindIndexOfDataChild(ItemB);
		});
	
	// we refresh the filtered children here as well
	GetFilteredChildren();

	OnSyncedDelegate.ExecuteIfBound();
}

const TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>>& FNiagaraHierarchyItemViewModelBase::GetFilteredChildren() const
{
	FilteredChildren.Empty();

	if(CanHaveChildren())
	{
		for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child : Children)
		{
			bool bPassesFilter = true;
			for(const FOnFilterChild& OnFilterChild : ChildFilters)
			{
				bPassesFilter &= OnFilterChild.Execute(Child);

				if(!bPassesFilter)
				{
					break;
				}
			}

			if(bPassesFilter)
			{
				FilteredChildren.Add(Child);
			}
		}
	}

	return FilteredChildren;
}

int32 FNiagaraHierarchyItemViewModelBase::GetHierarchyDepth() const
{
	if(Parent.IsValid())
	{
		return 1 + Parent.Pin()->GetHierarchyDepth();
	}

	return 0;
}

void FNiagaraHierarchyItemViewModelBase::AddChildFilter(FOnFilterChild InFilterChild)
{
	if(ensure(InFilterChild.IsBound()))
	{
		ChildFilters.Add(InFilterChild);
	}
}

bool FNiagaraHierarchyItemViewModelBase::HasParent(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ParentCandidate, bool bRecursive)
{
	if(Parent.IsValid())
	{
		if(Parent == ParentCandidate)
		{
			return true;
		}
		else if(bRecursive)
		{
			Parent.Pin()->HasParent(ParentCandidate, bRecursive);
		}
	}

	return false;
}

void FNiagaraHierarchyItemViewModelBase::AddChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
{
	if(Item->Parent.IsValid())
	{
		ensure(Item->Parent.Pin() == AsShared());
	}
	
	Children.Add(Item);
}

TSharedPtr<FNiagaraHierarchyItemViewModelBase> FNiagaraHierarchyItemViewModelBase::AddNewItem(TSubclassOf<UNiagaraHierarchyItemBase> NewItemClass)
{
	FText TransactionText = FText::FormatOrdered(LOCTEXT("Transaction_AddedItem", "Added new {0} to hierarchy"), FText::FromString(NewItemClass->GetName()));
	FScopedTransaction Transaction(TransactionText);
	HierarchyViewModel->GetHierarchyRoot()->Modify();
	
	UNiagaraHierarchyItemBase* NewItem = NewObject<UNiagaraHierarchyItemBase>(GetDataMutable(), NewItemClass, NAME_None, RF_Transactional);
	NewItem->Modify();
	GetDataMutable()->GetChildrenMutable().Add(NewItem);
	SyncViewModelsToData();

	TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = FindViewModelForChild(NewItem);
	ensure(ViewModel.IsValid());
	return ViewModel;
}

TSharedRef<FNiagaraHierarchyItemViewModelBase> FNiagaraHierarchyItemViewModelBase::DuplicateToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToDuplicate, int32 InsertIndex)
{
	UNiagaraHierarchyItemBase* NewItem = Cast<UNiagaraHierarchyItemBase>(StaticDuplicateObject(ItemToDuplicate->GetData(), GetDataMutable()));
	if(InsertIndex == INDEX_NONE)
	{
		GetDataMutable()->GetChildrenMutable().Add(NewItem);
	}
	else
	{
		GetDataMutable()->GetChildrenMutable().Insert(NewItem, InsertIndex);
	}
	
	SyncViewModelsToData();

	HierarchyViewModel->OnHierarchyChanged().Broadcast();
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = FindViewModelForChild(NewItem);
	return ViewModel.ToSharedRef();
}

TSharedRef<FNiagaraHierarchyItemViewModelBase> FNiagaraHierarchyItemViewModelBase::ReparentToThis(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ItemToMove, int32 InsertIndex)
{
	UNiagaraHierarchyItemBase* NewItem = Cast<UNiagaraHierarchyItemBase>(StaticDuplicateObject(ItemToMove->GetData(), GetDataMutable()));
	if(InsertIndex == INDEX_NONE)
	{
		GetDataMutable()->GetChildrenMutable().Add(NewItem);
	}
	else
	{
		GetDataMutable()->GetChildrenMutable().Insert(NewItem, InsertIndex);
	}
	
	ItemToMove->Delete();
	SyncViewModelsToData();
	HierarchyViewModel->OnHierarchyChanged().Broadcast();
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = FindViewModelForChild(NewItem);
	return ViewModel.ToSharedRef();
}

TSharedPtr<FNiagaraHierarchyItemViewModelBase> FNiagaraHierarchyItemViewModelBase::FindViewModelForChild(UNiagaraHierarchyItemBase* Child, bool bSearchRecursively) const
{
	int32 Index = FindIndexOfChild(Child);
	if(Index != INDEX_NONE)
	{
		return Children[Index];
	}

	if(bSearchRecursively)
	{
		for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ChildViewModel : Children)
		{
			TSharedPtr<FNiagaraHierarchyItemViewModelBase> FoundViewModel = ChildViewModel->FindViewModelForChild(Child, bSearchRecursively);

			if(FoundViewModel.IsValid())
			{
				return FoundViewModel;
			}
		}
	}

	return nullptr;
}

TSharedPtr<FNiagaraHierarchyItemViewModelBase> FNiagaraHierarchyItemViewModelBase::FindViewModelForChild(FNiagaraHierarchyIdentity ChildIdentity, bool bSearchRecursively) const
{
	for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child : Children)
	{
		if(Child->GetData()->GetPersistentIdentity() == ChildIdentity)
		{
			return Child;
		}
	}

	if(bSearchRecursively)
	{
		for(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ChildViewModel : Children)
		{
			TSharedPtr<FNiagaraHierarchyItemViewModelBase> FoundViewModel = ChildViewModel->FindViewModelForChild(ChildIdentity, bSearchRecursively);

			if(FoundViewModel.IsValid())
			{
				return FoundViewModel;
			}
		}
	}

	return nullptr;
}

int32 FNiagaraHierarchyItemViewModelBase::FindIndexOfChild(UNiagaraHierarchyItemBase* Child) const
{
	return Children.FindLastByPredicate([Child](TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
	{
		return Item->GetData() == Child;
	});
}

int32 FNiagaraHierarchyItemViewModelBase::FindIndexOfDataChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child) const
{
	return GetData()->GetChildren().Find(Child->GetDataMutable());
}

int32 FNiagaraHierarchyItemViewModelBase::FindIndexOfDataChild(UNiagaraHierarchyItemBase* Child) const
{
	return GetData()->GetChildren().Find(Child);
}

void FNiagaraHierarchyItemViewModelBase::Delete()
{
	OnChildRequestedDeletionDelegate.Execute(AsShared());
}

void FNiagaraHierarchyItemViewModelBase::DeleteChild(TSharedPtr<FNiagaraHierarchyItemViewModelBase> Child)
{	
	ensure(Child->GetParent().Pin() == AsShared());	
	GetDataMutable()->GetChildrenMutable().Remove(Child->GetDataMutable());
	Children.Remove(Child);
}

TOptional<EItemDropZone> FNiagaraHierarchyItemViewModelBase::OnCanRowAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
{
	if(TSharedPtr<FNiagaraHierarchyDragDropOp> DragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>())
	{
		FCanPerformActionResults Results = CanDropOn(DragDropOp->GetDraggedItem().Pin(), ItemDropZone);
		DragDropOp->SetDescription(Results.CanPerformMessage);
		return Results.bCanPerform ? ItemDropZone : TOptional<EItemDropZone>();
	}

	return TOptional<EItemDropZone>();
}

FReply FNiagaraHierarchyItemViewModelBase::OnDroppedOnRow(const FDragDropEvent& DragDropEvent, EItemDropZone ItemDropZone, TSharedPtr<FNiagaraHierarchyItemViewModelBase> Item)
{
	if(TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>())
	{
		OnDroppedOn(HierarchyDragDropOp->GetDraggedItem().Pin(), ItemDropZone);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FNiagaraHierarchyItemViewModelBase::OnRowDragLeave(const FDragDropEvent& DragDropEvent)
{
	if(TSharedPtr<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = DragDropEvent.GetOperationAs<FNiagaraHierarchyDragDropOp>())
	{
		HierarchyDragDropOp->SetDescription(FText::GetEmpty());
	}
}

FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchyItemViewModelBase::CanDrag()
{
	FCanPerformActionResults Results = IsEditableByUser();
	if(Results.bCanPerform == false)
	{
		return Results;
	}

	return CanDragInternal();
}

FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchyItemViewModelBase::CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase>, EItemDropZone ItemDropZone)
{
	return false;
}

void FNiagaraHierarchyItemViewModelBase::PropagateOnChildSynced()
{
	OnSyncedDelegate.ExecuteIfBound();
}

TSharedRef<FNiagaraHierarchyDragDropOp> FNiagaraHierarchyItemViewModelBase::CreateDragDropOp()
{
	check(CanDrag() == true);
		
	return HierarchyViewModel->CreateDragDropOp(AsShared());
}

FReply FNiagaraHierarchyItemViewModelBase::OnDragDetected(const FGeometry& Geometry, const FPointerEvent& PointerEvent,	bool bIsSource)
{
	FCanPerformActionResults CanDragResults = CanDrag();
	if(CanDragResults == true)
	{
		// if the drag is coming from source, we check if any of the hierarchy data already contains that item and we don't start a drag drop in that case
		if(bIsSource)
		{
			TArray<TSharedPtr<FNiagaraHierarchyItemViewModelBase>> AllChildren;
			GetChildrenViewModelsForType<UNiagaraHierarchyItemBase, FNiagaraHierarchyItemViewModelBase>(AllChildren, true);

			bool bCanDrag = GetHierarchyViewModel()->GetHierarchyRootViewModel()->FindViewModelForChild(GetData()->GetPersistentIdentity(), true) == nullptr;			

			if(bCanDrag)
			{
				for(TSharedPtr<FNiagaraHierarchyItemViewModelBase>& ItemViewModel : AllChildren)
				{
					if(GetHierarchyViewModel()->GetHierarchyRootViewModel()->FindViewModelForChild(ItemViewModel->GetData()->GetPersistentIdentity(), true) != nullptr)
					{
						bCanDrag = false;
						break;
					}
				}
			}
			
			if(bCanDrag == false)
			{
				return FReply::Unhandled();
			}
		}
		
		TSharedRef<FNiagaraHierarchyDragDropOp> HierarchyDragDropOp = CreateDragDropOp();
		HierarchyDragDropOp->SetFromSourceList(bIsSource);

		return FReply::Handled().BeginDragDrop(HierarchyDragDropOp);			
	}
	else
	{
		// if we can't drag and have a message, we show it as a slate notification
		if(CanDragResults.CanPerformMessage.IsEmpty() == false)
		{
			FNotificationInfo CantDragInfo(CanDragResults.CanPerformMessage);
			FSlateNotificationManager::Get().AddNotification(CantDragInfo);
		}
	}
		
	return FReply::Unhandled();
}

FNiagaraHierarchyRootViewModel::~FNiagaraHierarchyRootViewModel()
{
	
}

void FNiagaraHierarchyRootViewModel::Initialize()
{
	GetOnSynced().BindSP(this, &FNiagaraHierarchyRootViewModel::PropagateOnSynced);
}

FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchyRootViewModel::CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone)
{
	FCanPerformActionResults Results(false);
	
	// we only allow drops if some general conditions are fulfilled
	if(DraggedItem->GetData() != GetData() &&
		(!DraggedItem->HasParent(AsShared(), false) || ItemDropZone != EItemDropZone::OntoItem)  &&
		!HasParent(DraggedItem, true))
	{
		Results.bCanPerform = 
			// items can be dropped onto the root directly if the section is set to "All"
			(DraggedItem->GetData()->IsA<UNiagaraHierarchyItem>() && HierarchyViewModel->GetActiveHierarchySectionData() == nullptr)
				||
			// categories can be dropped onto the root always
			(DraggedItem->GetData()->IsA<UNiagaraHierarchyCategory>());

		if(Results.bCanPerform)
		{
			if(DraggedItem->IsForHierarchy() == false)
			{
				FText Message = LOCTEXT("CanDropSourceItemOnRootDragMessage", "Add {0} to the hierarchy root.");
				Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()));
				Results.CanPerformMessage = Message;
			}
			else
			{
				FText Message = LOCTEXT("CanDropHierarchyItemOnRootDragMessage", "Move {0} to the hierarchy root.");
				Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()));
				Results.CanPerformMessage = Message;
			}			
		}
		else
		{
			FText Message = LOCTEXT("CantDropHierarchyItemOnRootDragMessage", "Can not add {0} here. Please add it to a category!");
			Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()));
			Results.CanPerformMessage = Message;
		}
	}
	
	return Results;
}

void FNiagaraHierarchyRootViewModel::OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone)
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_OnDropOnRoot", "Dropped item on root"));
	HierarchyViewModel->GetHierarchyRoot()->Modify();

	if(DroppedItem->GetDataMutable()->IsA<UNiagaraHierarchyItem>() || DroppedItem->GetDataMutable()->IsA<UNiagaraHierarchyCategory>())
	{
		TSharedPtr<FNiagaraHierarchyItemViewModelBase> NewViewModel;
		// we duplicate the item if the dragged item is from source
		if(DroppedItem->IsForHierarchy() == false)
		{
			NewViewModel = DuplicateToThis(DroppedItem);
		}
		else
		{
			NewViewModel = ReparentToThis(DroppedItem);
		}

		if(UNiagaraHierarchyCategory* AsCategory = Cast<UNiagaraHierarchyCategory>(NewViewModel->GetDataMutable()))
		{
			AsCategory->SetSection(HierarchyViewModel->GetActiveHierarchySectionData());
		}

		HierarchyViewModel->RefreshHierarchyView();
	}
}

TSharedPtr<FNiagaraHierarchySectionViewModel> FNiagaraHierarchyRootViewModel::AddSection()
{
	FScopedTransaction ScopedTransaction(LOCTEXT("NewSectionAdded","Added Section"));
	HierarchyViewModel->GetHierarchyRoot()->Modify();
	
	UNiagaraHierarchySection* SectionData = Cast<UNiagaraHierarchyRoot>(ItemBase)->AddSection(LOCTEXT("NiagaraHierarchyEditorDefaultNewSectionName", "Section"), 0);
	SectionData->Modify();
	TSharedPtr<FNiagaraHierarchySectionViewModel> NewSectionViewModel = MakeShared<FNiagaraHierarchySectionViewModel>(SectionData, StaticCastSharedRef<FNiagaraHierarchyRootViewModel>(AsShared()), HierarchyViewModel, bIsForHierarchy);
	SectionViewModels.Add(NewSectionViewModel);
	SyncViewModelsToData();
	HierarchyViewModel->SetActiveHierarchySection(NewSectionViewModel);

	OnSectionAddedDelegate.ExecuteIfBound(NewSectionViewModel);
	OnSectionsChangedDelegate.ExecuteIfBound();
	return NewSectionViewModel;
}

void FNiagaraHierarchyRootViewModel::DeleteSection(TSharedPtr<FNiagaraHierarchyItemViewModelBase> InSectionViewModel)
{
	TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel = StaticCastSharedPtr<FNiagaraHierarchySectionViewModel>(InSectionViewModel);
	GetDataMutable<UNiagaraHierarchyRoot>()->GetSectionDataMutable().Remove(SectionViewModel->GetDataMutable<UNiagaraHierarchySection>());
	SectionViewModels.Remove(SectionViewModel);

	OnSectionDeletedDelegate.ExecuteIfBound(SectionViewModel);
	OnSectionsChangedDelegate.ExecuteIfBound();
}

void FNiagaraHierarchyRootViewModel::PropagateOnSynced()
{
	OnSyncPropagatedDelegate.ExecuteIfBound();
}

void FNiagaraHierarchyRootViewModel::SyncViewModelsToDataInternal()
{
	const UNiagaraHierarchyRoot* RootData = GetData<UNiagaraHierarchyRoot>();

	TArray<TSharedPtr<FNiagaraHierarchySectionViewModel>> NewSectionViewModels;
	TArray<TSharedPtr<FNiagaraHierarchySectionViewModel>> SectionViewModelsToDelete;
	
	for(TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel : SectionViewModels)
	{
		if(!RootData->GetSectionData().Contains(SectionViewModel->GetData()))
		{
			SectionViewModelsToDelete.Add(SectionViewModel);
		}
	}

	for (TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel : SectionViewModelsToDelete)
	{
		SectionViewModel->Delete();
	}
	
	for(UNiagaraHierarchySection* Section : RootData->GetSectionData())
	{
		TSharedPtr<FNiagaraHierarchySectionViewModel>* SectionViewModelPtr = SectionViewModels.FindByPredicate([Section](TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel)
		{
			return SectionViewModel->GetData() == Section;
		});

		TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel = nullptr;

		if(SectionViewModelPtr)
		{
			SectionViewModel = *SectionViewModelPtr;
		}

		if(SectionViewModel == nullptr)
		{
			SectionViewModel = MakeShared<FNiagaraHierarchySectionViewModel>(Section, StaticCastSharedRef<FNiagaraHierarchyRootViewModel>(AsShared()), HierarchyViewModel, bIsForHierarchy);
			SectionViewModel->SyncViewModelsToData();;
		}
		
		NewSectionViewModels.Add(SectionViewModel);
	}

	SectionViewModels.Empty();
	SectionViewModels.Append(NewSectionViewModels);

	for(TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel : SectionViewModels)
	{
		SectionViewModel->OnChildRequestedDeletion().BindSP(this, &FNiagaraHierarchyRootViewModel::DeleteSection);
	}

	SectionViewModels.Sort([this](const TSharedPtr<FNiagaraHierarchySectionViewModel>& ItemA, const TSharedPtr<FNiagaraHierarchySectionViewModel>& ItemB)
		{
			return
			GetDataMutable<UNiagaraHierarchyRoot>()->GetSectionData().Find(Cast<UNiagaraHierarchySection>(ItemA->GetDataMutable()))
				<
			GetDataMutable<UNiagaraHierarchyRoot>()->GetSectionData().Find(Cast<UNiagaraHierarchySection>(ItemB->GetDataMutable())); 
		});
}

void FNiagaraHierarchySectionViewModel::PopulateDynamicContextMenuSection(FToolMenuSection& DynamicSection)
{
	//DynamicSection.AddMenuEntry()
}

FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchySectionViewModel::CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone)
{
	FCanPerformActionResults Results(false);
	// we don't allow dropping onto source sections and we don't specify a message as the sections aren't going to light up as valid drop targets
	if(IsForHierarchy() == false)
	{
		return false;
	}
	
	if(const UNiagaraHierarchyCategory* Category = Cast<UNiagaraHierarchyCategory>(DraggedItem->GetData()))
	{
		if(ItemDropZone == EItemDropZone::OntoItem)
		{
			FText Message = LOCTEXT("DropCategoryOnSectionDragMessage", "Add {0} to section {1}");
			Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()), FText::FromString(ToString()));
			
			Results.CanPerformMessage = Message;
			Results.bCanPerform = GetData() != Category->GetSection();
		}
	}
	else if(UNiagaraHierarchySection* Section = Cast<UNiagaraHierarchySection>(DraggedItem->GetDataMutable()))
	{
		const bool bSameSection = GetData() == Section;

		if(ItemDropZone == EItemDropZone::OntoItem && bSameSection == false)
		{
			FText Message = LOCTEXT("CantDropSectionOnSectionDragMessage", "Can't drop section on a section.");
			Results.CanPerformMessage = Message;
			Results.bCanPerform = false;
			return Results;
		}
		
		int32 DraggedItemIndex = GetHierarchyViewModel()->GetHierarchyRoot()->GetSectionIndex(Section);
		int32 InsertionIndex = GetHierarchyViewModel()->GetHierarchyRoot()->GetSectionIndex(GetDataMutable<UNiagaraHierarchySection>());
		// we add 1 to the insertion index if it's below an item because we either want to insert at the current index to place the item above, or at current+1 for below
		InsertionIndex += ItemDropZone == EItemDropZone::AboveItem ? -1 : 1;

		Results.bCanPerform = !bSameSection && DraggedItemIndex != InsertionIndex;

		if(Results.bCanPerform)
		{
			if(ItemDropZone == EItemDropZone::AboveItem)
			{
				FText Message = LOCTEXT("MoveSectionLeftDragMessage", "Move section {0} to the left");
				Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()));
				Results.CanPerformMessage = Message;
			}
			else if(ItemDropZone == EItemDropZone::BelowItem)
			{
				FText Message = LOCTEXT("MoveSectionRightDragMessage", "Move section {0} to the right");
				Message = FText::FormatOrdered(Message, FText::FromString(DraggedItem->ToString()));
				Results.CanPerformMessage = Message;
			}
		}
	}
	else if(UNiagaraHierarchyItem* Item = Cast<UNiagaraHierarchyItem>(DraggedItem->GetDataMutable()))
	{
		FText Message = LOCTEXT("CantDropItemOnSectionDragMessage", "Can't drop items onto sections. Please drag a category onto section {0}");
		Message = FText::FormatOrdered(Message, FText::FromString(ToString()));
		Results.bCanPerform = false;
		Results.CanPerformMessage = Message;
	}

	return Results;
}

void FNiagaraHierarchySectionViewModel::OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone)
{
	if(DroppedItem->GetData()->IsA<UNiagaraHierarchySection>())
	{
		UNiagaraHierarchySection* DraggedSectionData = DroppedItem->GetDataMutable<UNiagaraHierarchySection>();

		int32 IndexOfThis = HierarchyViewModel->GetHierarchyRoot()->GetSectionData().Find(GetDataMutable<UNiagaraHierarchySection>());
		int32 DraggedSectionIndex = HierarchyViewModel->GetHierarchyRoot()->GetSectionData().Find(DraggedSectionData);

		TArray<TObjectPtr<UNiagaraHierarchySection>>& SectionData = HierarchyViewModel->GetHierarchyRoot()->GetSectionDataMutable();
		int32 Count = SectionData.Num();

		bool bDropSucceeded = false;
		// above constitutes to the left here
		if(ItemDropZone == EItemDropZone::AboveItem)
		{
			SectionData.RemoveAt(DraggedSectionIndex);
			SectionData.Insert(DraggedSectionData, FMath::Max(IndexOfThis, 0));

			bDropSucceeded = true;
		}
		else if(ItemDropZone == EItemDropZone::BelowItem)
		{
			SectionData.RemoveAt(DraggedSectionIndex);

			if(IndexOfThis + 1 > SectionData.Num())
			{
				SectionData.Add(DraggedSectionData);
			}
			else
			{
				SectionData.Insert(DraggedSectionData, FMath::Min(IndexOfThis+1, Count));
			}

			bDropSucceeded = true;

		}

		if(bDropSucceeded)
		{
			HierarchyViewModel->ForceFullRefresh();
			HierarchyViewModel->OnHierarchyChanged().Broadcast();
		}
	}
	else
	{		
		if(UNiagaraHierarchyCategory* HierarchyCategory = DroppedItem->GetDataMutable<UNiagaraHierarchyCategory>())
		{
			FScopedTransaction Transaction(LOCTEXT("Transaction_OnSectionDrop", "Moved category to section"));
			HierarchyViewModel->GetHierarchyRoot()->Modify();
			
			HierarchyCategory->SetSection(GetDataMutable<UNiagaraHierarchySection>());

			// we null out any sections for all contained categories
			TArray<UNiagaraHierarchyCategory*> AllChildCategories;
			HierarchyCategory->GetChildrenOfType<UNiagaraHierarchyCategory>(AllChildCategories, true);
			for(UNiagaraHierarchyCategory* ChildCategory : AllChildCategories)
			{
				ChildCategory->SetSection(nullptr);
			}

			// we only need to reparent if the parent isn't already the root. This stops unnecessary reordering
			if(DroppedItem->GetParent() != HierarchyViewModel->GetHierarchyRootViewModel())
			{
				HierarchyViewModel->GetHierarchyRootViewModel()->ReparentToThis(DroppedItem);
			}
			
			HierarchyViewModel->RefreshHierarchyView();
			HierarchyViewModel->OnHierarchyChanged().Broadcast();
		}
	}
}

void FNiagaraHierarchySectionViewModel::FinalizeInternal()
{
	if(HierarchyViewModel->GetActiveHierarchySection() == AsShared())
	{
		HierarchyViewModel->SetActiveHierarchySection(nullptr);
	}

	// we make sure to reset all categories' section entry that were referencing this section
	TArray<UNiagaraHierarchyCategory*> AllCategories;
	HierarchyViewModel->GetHierarchyRoot()->GetChildrenOfType<UNiagaraHierarchyCategory>(AllCategories, true);

	for(UNiagaraHierarchyCategory* Category : AllCategories)
	{
		if(Category->GetSection() == GetData())
		{
			Category->SetSection(nullptr);
		}
	}
}

::FNiagaraHierarchyItemViewModelBase::FCanPerformActionResults FNiagaraHierarchyItemViewModel::CanDropOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DraggedItem, EItemDropZone ItemDropZone)
{
	bool bAllowDrop = false;
	
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> SourceDropItem = DraggedItem;
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> TargetDropItem = AsShared();

	// we only allow drops if some general conditions are fulfilled
	if(SourceDropItem->GetData() != TargetDropItem->GetData() &&
		(!SourceDropItem->HasParent(TargetDropItem, false) || ItemDropZone != EItemDropZone::OntoItem)  &&
		!TargetDropItem->HasParent(SourceDropItem, true))
	{
		// items can be generally be dropped above/below other items
		bAllowDrop = (SourceDropItem->GetData()->IsA<UNiagaraHierarchyItem>() && ItemDropZone != EItemDropZone::OntoItem);
	}

	return bAllowDrop;
}

void FNiagaraHierarchyItemViewModel::OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone)
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_MovedItem", "Moved an item in the hierarchy"));
	HierarchyViewModel->GetHierarchyRoot()->Modify();
	
	bool bDropSucceeded = false;
	if(ItemDropZone == EItemDropZone::AboveItem)
	{
		int32 IndexOfThis = Parent.Pin()->FindIndexOfDataChild(AsShared());

		if(DroppedItem->IsForHierarchy() == false)
		{
			Parent.Pin()->DuplicateToThis(DroppedItem, FMath::Max(IndexOfThis, 0));
		}
		else
		{
			Parent.Pin()->ReparentToThis(DroppedItem, FMath::Max(IndexOfThis, 0));
		}

		bDropSucceeded = true;
	}
	else if(ItemDropZone == EItemDropZone::BelowItem)
	{
		int32 IndexOfThis = Parent.Pin()->FindIndexOfDataChild(AsShared());
		
		if(DroppedItem->IsForHierarchy() == false)
		{
			Parent.Pin()->DuplicateToThis(DroppedItem, FMath::Min(IndexOfThis+1, Parent.Pin()->GetChildren().Num()));
		}
		else
		{
			Parent.Pin()->ReparentToThis(DroppedItem, FMath::Min(IndexOfThis+1, Parent.Pin()->GetChildren().Num()));
		}

		bDropSucceeded = true;
	}

	if(bDropSucceeded)
	{
		HierarchyViewModel->RefreshHierarchyView();
		HierarchyViewModel->RefreshSourceView();
	}
	else
	{
		Transaction.Cancel();
	}
}

void FNiagaraHierarchyCategoryViewModel::OnDroppedOnInternal(TSharedPtr<FNiagaraHierarchyItemViewModelBase> DroppedItem, EItemDropZone ItemDropZone)
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_OnCategoryDrop", "Dropped item on/above/below category"));
	HierarchyViewModel->GetHierarchyRoot()->Modify();
	
	if(UNiagaraHierarchyCategory* Category = DroppedItem->GetDataMutable<UNiagaraHierarchyCategory>())
	{
		// if we are dragging a category above/below another category and the new parent is going to be the root, we update its section to the active section
		if(ItemDropZone != EItemDropZone::OntoItem)
		{
			if(Parent.IsValid() && Parent == HierarchyViewModel->GetHierarchyRootViewModel())
			{
				Category->SetSection(HierarchyViewModel->GetActiveHierarchySectionData());

				// we null out any sections for all contained categories
				TArray<UNiagaraHierarchyCategory*> AllChildCategories;
				Category->GetChildrenOfType<UNiagaraHierarchyCategory>(AllChildCategories, true);
				for(UNiagaraHierarchyCategory* ChildCategory : AllChildCategories)
				{
					ChildCategory->SetSection(nullptr);
				}
			}				
		}
		// if we are dragging a category onto another category, we null out its section instead
		else
		{
			Category->SetSection(nullptr);

			// we null out any sections for all contained categories
			TArray<UNiagaraHierarchyCategory*> AllChildCategories;
			Category->GetChildrenOfType<UNiagaraHierarchyCategory>(AllChildCategories, true);
			for(UNiagaraHierarchyCategory* ChildCategory : AllChildCategories)
			{
				ChildCategory->SetSection(nullptr);
			}
		}			
	}

	// the actual moving of the item happens here
	if(ItemDropZone == EItemDropZone::OntoItem)
	{
		if(DroppedItem->IsForHierarchy() == false)
		{
			DuplicateToThis(DroppedItem);
		}
		else
		{
			ReparentToThis(DroppedItem);
		}
	}
	else if(ItemDropZone == EItemDropZone::AboveItem)
	{
		int32 IndexOfThis = Parent.Pin()->FindIndexOfDataChild(AsShared());
		if(DroppedItem->IsForHierarchy() == false)
		{
			Parent.Pin()->DuplicateToThis(DroppedItem, FMath::Max(IndexOfThis, 0));
		}
		else
		{				
			Parent.Pin()->ReparentToThis(DroppedItem, FMath::Max(IndexOfThis, 0));
		}
	}
	else if(ItemDropZone == EItemDropZone::BelowItem)
	{
		int32 IndexOfThis = Parent.Pin()->FindIndexOfDataChild(AsShared());
		if(DroppedItem->IsForHierarchy() == false)
		{
			Parent.Pin()->DuplicateToThis(DroppedItem, FMath::Min(IndexOfThis+1, Parent.Pin()->GetChildren().Num()));
		}
		else
		{
			Parent.Pin()->ReparentToThis(DroppedItem, FMath::Min(IndexOfThis+1, Parent.Pin()->GetChildren().Num()));
		}
	}
}

void UNiagaraHierarchyViewModelBase::AddCategory(TSharedPtr<FNiagaraHierarchyItemViewModelBase> CategoryParent) const
{
	int32 HierarchyDepth = CategoryParent->GetHierarchyDepth();
	if(HierarchyDepth > 15)
	{
		FNotificationInfo Info(LOCTEXT("TooManyNestedCategoriesToastText", "We currently only allow a hierarchy depth of 15."));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}
	
	TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = CategoryParent->AddNewItem(UNiagaraHierarchyCategory::StaticClass());

	if(ensure(ViewModel.IsValid()))
	{
		UNiagaraHierarchyCategory* Category = CastChecked<UNiagaraHierarchyCategory>(ViewModel->GetDataMutable());
		
		TArray<UNiagaraHierarchyCategory*> SiblingCategories;
		Category->GetTypedOuter<UNiagaraHierarchyItemBase>()->GetChildrenOfType<UNiagaraHierarchyCategory>(SiblingCategories);
		
		TSet<FName> CategoryNames;
		for(const auto& SiblingCategory : SiblingCategories)
		{
			CategoryNames.Add(SiblingCategory->GetCategoryName());
		}

		Category->SetCategoryName(FNiagaraUtilities::GetUniqueName(FName("New Category"), CategoryNames));
		
		// we only set the section property if the current section isn't set to "All"
		Category->SetSection(GetActiveHierarchySectionData());
		
		RefreshHierarchyView();

		OnItemAddedDelegate.ExecuteIfBound(ViewModel);
		OnHierarchyChangedDelegate.Broadcast();
	}
}

void UNiagaraHierarchyViewModelBase::AddSection() const
{
	TSharedPtr<FNiagaraHierarchySectionViewModel> HierarchySectionViewModel = HierarchyRootViewModel->AddSection();
	OnItemAddedDelegate.ExecuteIfBound(HierarchySectionViewModel);
	OnHierarchyChangedDelegate.Broadcast();
}

void UNiagaraHierarchyViewModelBase::DeleteItemWithIdentity(FNiagaraHierarchyIdentity Identity)
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_DeleteItem", "Deleted hierarchy item"));
	HierarchyRoot->Modify();

	bool bItemDeleted = false;
	if(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = HierarchyRootViewModel->FindViewModelForChild(Identity, true))
	{
		if(ViewModel->CanDelete())
		{
			ViewModel->Delete();
			bItemDeleted = true;
		}
	}
	
	TArray<TSharedPtr<FNiagaraHierarchySectionViewModel>> SectionViewModels = HierarchyRootViewModel->GetSectionViewModels();
	for(TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel : SectionViewModels)
	{
		if(SectionViewModel->GetData()->GetPersistentIdentity() == Identity && SectionViewModel->CanDelete())
		{
			SectionViewModel->Delete();
			bItemDeleted = true;
		}
	}

	if(bItemDeleted)
	{
		HierarchyRootViewModel->SyncViewModelsToData();
		OnHierarchyChangedDelegate.Broadcast();
	}
}

void UNiagaraHierarchyViewModelBase::DeleteItemsWithIdentities(TArray<FNiagaraHierarchyIdentity> Identities)
{
	FScopedTransaction Transaction(LOCTEXT("Transaction_DeleteMultipleItems", "Deleted hierarchy items"));
	HierarchyRoot->Modify();

	bool bAnyItemsDeleted = false;
	for(FNiagaraHierarchyIdentity& Identity : Identities)
	{
		if(TSharedPtr<FNiagaraHierarchyItemViewModelBase> ViewModel = HierarchyRootViewModel->FindViewModelForChild(Identity, true))
		{
			if(ViewModel->CanDelete())
			{
				ViewModel->Delete();
				bAnyItemsDeleted = true;
				continue;
			}
		}

		TArray<TSharedPtr<FNiagaraHierarchySectionViewModel>> SectionViewModels = HierarchyRootViewModel->GetSectionViewModels();
		for(TSharedPtr<FNiagaraHierarchySectionViewModel> SectionViewModel : SectionViewModels)
		{
			if(SectionViewModel->GetData()->GetPersistentIdentity() == Identity && SectionViewModel->CanDelete())
			{
				SectionViewModel->Delete();
				bAnyItemsDeleted = true;
				continue;
			}
		}
	}

	if(bAnyItemsDeleted)
	{
		HierarchyRootViewModel->SyncViewModelsToData();
		OnHierarchyChangedDelegate.Broadcast();
	}
}

void UNiagaraHierarchyViewModelBase::NavigateToItemInHierarchy(const FNiagaraHierarchyIdentity& NiagaraHierarchyIdentity)
{
	OnNavigateToItemInHierarchyRequestedDelegate.ExecuteIfBound(NiagaraHierarchyIdentity);
}

TSharedPtr<SWidget> FNiagaraHierarchyDragDropOp::GetDefaultDecorator() const
{
	TSharedRef<SWidget> CustomDecorator = CreateCustomDecorator();

	SVerticalBox::FSlot* CustomSlot;
	TSharedPtr<SWidget> Decorator = SNew(SToolTip)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Expose(CustomSlot).
		AutoHeight()
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(STextBlock)
			.Text(this, &FNiagaraHierarchyDragDropOp::GetAdditionalLabel)
			.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText.Important"))
			.Visibility_Lambda([this]()
			{
				return GetAdditionalLabel().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.f)
		[
			SNew(STextBlock)
			.Text(this, &FNiagaraHierarchyDragDropOp::GetDescription)
			.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
			.Visibility_Lambda([this]()
			{
				return GetDescription().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
			})
		]
	];

	if(CustomDecorator != SNullWidget::NullWidget)
	{
		CustomSlot->AttachWidget(CustomDecorator);
	}

	return Decorator;
}

TSharedRef<SWidget> FNiagaraSectionDragDropOp::CreateCustomDecorator() const
{
	return SNew(SCheckBox)
		.Visibility(EVisibility::HitTestInvisible)
		.Style(FAppStyle::Get(), "DetailsView.SectionButton")
		.IsChecked(ECheckBoxState::Unchecked)
		[
			SNew(SInlineEditableTextBlock)
			.Text(GetDraggedSection().Pin()->GetSectionNameAsText())
		];
}

#undef LOCTEXT_NAMESPACE
