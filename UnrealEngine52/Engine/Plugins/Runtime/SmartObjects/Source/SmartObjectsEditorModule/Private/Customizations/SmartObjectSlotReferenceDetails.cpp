// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSlotReferenceDetails.h"
#include "DetailWidgetRow.h"
#include "SmartObjectTypes.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailLayoutBuilder.h"
#include "SmartObjectDefinition.h"
#include "ScopedTransaction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

TSharedRef<IPropertyTypeCustomization> FSmartObjectSlotReferenceDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectSlotReferenceDetails);
}

void FSmartObjectSlotReferenceDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
		SNew(SComboButton)
			.OnGetMenuContent(this, &FSmartObjectSlotReferenceDetails::OnGetSlotListContent)
			.ContentPadding(FMargin(6.f, 0.f))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &FSmartObjectSlotReferenceDetails::GetCurrentSlotDesc)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
}

void FSmartObjectSlotReferenceDetails::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Empty
}

const USmartObjectDefinition* FSmartObjectSlotReferenceDetails::GetSmartObjectDefinition() const
{
	// Assumes that the slots are directly on a definition, and that there's only one definition.
	check(StructProperty.IsValid());
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	return OuterObjects.IsEmpty() ? nullptr : Cast<USmartObjectDefinition>(OuterObjects[0]);
}

void FSmartObjectSlotReferenceDetails::OnSlotComboChanged(const FGuid NewID)
{
	check(StructProperty.IsValid());
	const USmartObjectDefinition* Definition = GetSmartObjectDefinition();
	const int32 Index = Definition->FindSlotByID(NewID);

	if (Index != INDEX_NONE)
	{
		FScopedTransaction Transaction(LOCTEXT("SetValue", "Set Value"));

		StructProperty->NotifyPreChange();

		TArray<void*> RawNodeData;
		StructProperty->AccessRawData(RawNodeData);
		for (void* Data : RawNodeData)
		{
			if (FSmartObjectSlotReference* SlotRef = static_cast<FSmartObjectSlotReference*>(Data))
			{
				SlotRef->SlotID = NewID;
				SlotRef->SetIndex(Index);
			}
		}
		
		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();
	}
}

TSharedRef<SWidget> FSmartObjectSlotReferenceDetails::OnGetSlotListContent() const
{
	const USmartObjectDefinition* Definition = GetSmartObjectDefinition();
	
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SlotNone", "None"),
		 FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(const_cast<FSmartObjectSlotReferenceDetails*>(this), &FSmartObjectSlotReferenceDetails::OnSlotComboChanged, FGuid())));

	if (Definition)
	{
		MenuBuilder.AddSeparator();

		for (const FSmartObjectSlotDefinition& Slot : Definition->GetSlots())
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(Slot.Name),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(const_cast<FSmartObjectSlotReferenceDetails*>(this), &FSmartObjectSlotReferenceDetails::OnSlotComboChanged, Slot.ID)));
		}

		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

FText FSmartObjectSlotReferenceDetails::GetCurrentSlotDesc() const
{
	check(StructProperty.IsValid());
	const USmartObjectDefinition* Definition = GetSmartObjectDefinition();
	
	// Get ID from the raw values.
	FGuid CurrentID;
	TArray<void*> RawNodeData;
	StructProperty->AccessRawData(RawNodeData);
	for (void* Data : RawNodeData)
	{
		if (const FSmartObjectSlotReference* SlotRef = static_cast<FSmartObjectSlotReference*>(Data))
		{
			CurrentID = SlotRef->SlotID;
			break;
		}
	}

	int32 SlotIndex = INDEX_NONE;
	
	if (Definition != nullptr && CurrentID.IsValid())
	{
		const TConstArrayView<FSmartObjectSlotDefinition> Slots = Definition->GetSlots();
		for (int32 Index = 0; Index < Slots.Num(); Index++)
		{
			if (Slots[Index].ID == CurrentID)
			{
				SlotIndex = Index;
				break;
			}
		}
	}

	// Empty or valid reference.
	if (SlotIndex != INDEX_NONE)
	{
		return FText::FromName(Definition->GetSlots()[SlotIndex].Name);
	}

	// Show missing.
	return LOCTEXT("InvalidSlot", "None");
}

#undef LOCTEXT_NAMESPACE
