// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectViewModel.h"
#include "SmartObjectDefinition.h"
#include "Editor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

TArray<TSharedPtr<FSmartObjectViewModel>> FSmartObjectViewModel::AllViewModels;

FSmartObjectViewModel::FSmartObjectViewModel(USmartObjectDefinition* InDefinition)
	: WeakDefinition(InDefinition)
{
	GEditor->RegisterForUndo(this);
}

FSmartObjectViewModel::~FSmartObjectViewModel()
{
	GEditor->UnregisterForUndo(this);
}

TSharedPtr<FSmartObjectViewModel> FSmartObjectViewModel::Register(USmartObjectDefinition* InDefinition)
{
	TSharedPtr<FSmartObjectViewModel> ViewModel = Get(InDefinition);
	if (ViewModel.IsValid())
	{
		ensureAlwaysMsgf(false, TEXT("Smart Object View model for %s already exsits."), *GetNameSafe(InDefinition));
		return ViewModel;
	}

	ViewModel = MakeShared<FSmartObjectViewModel>(InDefinition);
	AllViewModels.Add(ViewModel);

	return ViewModel;
}

void FSmartObjectViewModel::Unregister()
{
	AllViewModels.Remove(AsShared());
}

TSharedPtr<FSmartObjectViewModel> FSmartObjectViewModel::Get(const USmartObjectDefinition* InDefinition)
{
	TSharedPtr<FSmartObjectViewModel>* ViewModel = AllViewModels.FindByPredicate([InDefinition](const TSharedPtr<FSmartObjectViewModel>& ViewModel)
	{
		check(ViewModel.IsValid());
		return ViewModel->WeakDefinition.Get() == InDefinition;
	});
	return ViewModel ? *ViewModel : nullptr;
}

void FSmartObjectViewModel::ResetSelection()
{
	if (Selection.Num() > 0)
	{
		Selection.Reset();
		if (OnSelectionChanged.IsBound())
		{
			OnSelectionChanged.Broadcast(Selection);
		}
	}
}

void FSmartObjectViewModel::SetSelection(const TConstArrayView<FGuid> Items)
{
	Selection.Reset();
	for (const FGuid& Item : Items)
	{
		Selection.AddUnique(Item);
	}
	
	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Broadcast(Selection);
	}
}

void FSmartObjectViewModel::AddToSelection(const FGuid& Item)
{
	Selection.AddUnique(Item);
	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Broadcast(Selection);
	}
}

void FSmartObjectViewModel::RemoveFromSelection(const FGuid& Item)
{
	Selection.Remove(Item);
	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Broadcast(Selection);
	}
}

bool FSmartObjectViewModel::IsSelected(const FGuid& Item) const
{
	return Selection.Contains(Item);
}

TConstArrayView<FGuid> FSmartObjectViewModel::GetSelection() const 
{
	return Selection;
}

FGuid FSmartObjectViewModel::AddSlot(const FGuid InsertAfterSlotID)
{
	FGuid NewSlotID;
	
	if (USmartObjectDefinition* Definition = WeakDefinition.Get())
	{
		{
			FScopedTransaction Transaction(LOCTEXT("SmartObject_AddSlot", "Add Slot"));
			Definition->Modify();

			int32 InsertAtIndex = 0;
			for ( ; InsertAtIndex < Definition->Slots.Num(); InsertAtIndex++)
			{
				if (Definition->Slots[InsertAtIndex].ID == InsertAfterSlotID)
				{
					InsertAtIndex++;
					break;
				}
			}

			FSmartObjectSlotDefinition& Slot = Definition->Slots.InsertDefaulted_GetRef(InsertAtIndex);
			Slot.ID = FGuid::NewGuid();
			NewSlotID = Slot.ID;
		}

		if (OnSlotsChanged.IsBound())
		{
			OnSlotsChanged.Broadcast(Definition);
		}
	}

	return NewSlotID;
}

void FSmartObjectViewModel::MoveSlot(const FGuid SourceSlotID, const FGuid TargetSlotID)
{
	if (USmartObjectDefinition* Definition = WeakDefinition.Get())
	{
		const int32 SourceSlotIndex = Definition->FindSlotByID(SourceSlotID);
		
		int32 TargetSlotIndex = INDEX_NONE;
		int32 TargetDataDefinitionIndex = INDEX_NONE;
		Definition->FindSlotAndDefinitionDataIndexByID(TargetSlotID, TargetSlotIndex, TargetDataDefinitionIndex);

		if (SourceSlotIndex == INDEX_NONE
			|| TargetSlotIndex == INDEX_NONE)
		{
			return;
		}

		{
			FScopedTransaction Transaction(LOCTEXT("SmartObject_MoveSlot", "Move Slot"));
			Definition->Modify();

			const FSmartObjectSlotDefinition SlotCopy = Definition->Slots[SourceSlotIndex];
			Definition->Slots.RemoveAt(SourceSlotIndex);
			Definition->Slots.Insert(SlotCopy, TargetSlotIndex);
		}
		
		if (OnSlotsChanged.IsBound())
		{
			OnSlotsChanged.Broadcast(Definition);
		}
	}
}

void FSmartObjectViewModel::RemoveSlot(const FGuid SlotID)
{
	if (USmartObjectDefinition* Definition = WeakDefinition.Get())
	{
		const int32 SlotIndex = Definition->FindSlotByID(SlotID);

		if (SlotIndex == INDEX_NONE)
		{
			return;
		}

		{
			FScopedTransaction Transaction(LOCTEXT("SmartObject_RemoveSlot", "Remove Slot"));
			Definition->Modify();
			Definition->Slots.RemoveAt(SlotIndex);
		}
		
		if (OnSlotsChanged.IsBound())
		{
			OnSlotsChanged.Broadcast(Definition);
		}
	}
}

#undef LOCTEXT_NAMESPACE