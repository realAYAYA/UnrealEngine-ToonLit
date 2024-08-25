// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "SmartObjectDefinition.h"

class USmartObjectDefinition;

/**
 * ModelView for editing Smart Object Asset.
 */
class FSmartObjectViewModel : public FEditorUndoClient, public TSharedFromThis<FSmartObjectViewModel>
{
public:
	explicit FSmartObjectViewModel(USmartObjectDefinition* InDefinition);
	virtual ~FSmartObjectViewModel() override;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, TConstArrayView<FGuid> /*Selection*/);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSlotsChanged, USmartObjectDefinition* /*Definition*/);

	static TSharedPtr<FSmartObjectViewModel> Register(USmartObjectDefinition* InDefinition);
	void Unregister();
	static TSharedPtr<FSmartObjectViewModel> Get(const USmartObjectDefinition* InDefinition);

	void ResetSelection();
	void SetSelection(const TConstArrayView<FGuid> Items);
	void AddToSelection(const FGuid& Item);
	void RemoveFromSelection(const FGuid& Item);
	bool IsSelected(const FGuid& Item) const;
	TConstArrayView<FGuid> GetSelection() const;

	// Called each time the selection changes.
	FOnSelectionChanged& GetOnSelectionChanged() { return OnSelectionChanged; }

	FOnSlotsChanged& GetOnSlotsChanged() { return OnSlotsChanged; }

	FGuid AddSlot(const FGuid InsertAfterSlotID);
	void MoveSlot(const FGuid SourceSlotID, const FGuid TargetSlotID);
	void RemoveSlot(const FGuid SlotID);

	USmartObjectDefinition* GetAsset() const
	{
		return WeakDefinition.Get();
	}
	
protected:
	static TArray<TSharedPtr<FSmartObjectViewModel>> AllViewModels;
	TWeakObjectPtr<USmartObjectDefinition> WeakDefinition;
	TArray<FGuid> Selection;
	FOnSelectionChanged OnSelectionChanged;
	FOnSlotsChanged OnSlotsChanged;
};