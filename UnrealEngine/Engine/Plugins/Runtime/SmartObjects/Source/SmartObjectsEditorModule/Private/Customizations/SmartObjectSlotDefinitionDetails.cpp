// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSlotDefinitionDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SmartObjectDefinition.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SBox.h"
#include "SmartObjectViewModel.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"
#include "SmartObjectEditorStyle.h"
#include "SmartObjectBindingExtension.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

TSharedRef<IPropertyTypeCustomization> FSmartObjectSlotDefinitionDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectSlotDefinitionDetails);
}

void FSmartObjectSlotDefinitionDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities().Get();

	NamePropertyHandle = StructProperty->GetChildHandle(TEXT("Name"));
	check(NamePropertyHandle);

	DefinitionDataPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinition, DefinitionData));
	check(DefinitionDataPropertyHandle);
	
	TSharedPtr<IPropertyHandle> ColorPropertyHandle = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinition, DEBUG_DrawColor));
	check(ColorPropertyHandle);
	
	CacheOuterDefinition();
	TSharedPtr<FSmartObjectViewModel> ViewModel = FSmartObjectViewModel::Get(Definition);

	// Get ID of the slot.
	FGuid ItemID;
	StructProperty->EnumerateConstRawData([&ItemID](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			ItemID = static_cast<const FSmartObjectSlotDefinition*>(RawData)->ID;
			return false; // stop
		}
		return true;
	});
	
	HeaderRow
		.WholeRowContent()
		[
			SNew(SBorder)
			.Padding(FMargin(4,1))
			.BorderImage_Lambda([ViewModel, ItemID]()
			{
				bool bSelected = false;
				if (ViewModel.IsValid())
				{
					bSelected = ViewModel->IsSelected(ItemID);
				}
				return bSelected ? FSmartObjectEditorStyle::Get().GetBrush("ItemSelection") : nullptr;
			})
			.OnMouseButtonDown_Lambda([ViewModel, ItemID](const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
				{
					if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
					{
						const bool bToggleSelection = MouseEvent.IsShiftDown() || MouseEvent.IsControlDown();
						if (ViewModel.IsValid())
						{
							if (bToggleSelection)
							{
								if (ViewModel->IsSelected(ItemID))
								{
									ViewModel->RemoveFromSelection(ItemID);
								}
								else
								{
									ViewModel->AddToSelection(ItemID);
								}
							}
							else
							{
								ViewModel->SetSelection({ ItemID });
							}
						}
					}
					return FReply::Unhandled();
				})
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(0,0,4,0))
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Transform"))
					.ColorAndOpacity_Lambda([ColorPropertyHandle]()
					{
						FColor Color = FColor::White;
						ColorPropertyHandle->EnumerateConstRawData([&Color](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
						{
							if (RawData)
							{
								Color = *static_cast<const FColor*>(RawData);
								return false; // stop
							}
							return true;
						});
						return Color;
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SEditableText)
					.Text(this, &FSmartObjectSlotDefinitionDetails::GetSlotName)
					.OnTextCommitted(this, &FSmartObjectSlotDefinitionDetails::OnSlotNameCommitted)
					.SelectAllTextWhenFocused(true)
					.RevertTextOnEscape(true)
					.Font(FSmartObjectEditorStyle::Get().GetFontStyle("LargerDetailFontBold"))
				]
				
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					StructProperty->CreateDefaultPropertyButtonWidgets()
				]
			]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectSlotDefinitionDetails::OnCopy)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectSlotDefinitionDetails::OnPaste)));
}

FText FSmartObjectSlotDefinitionDetails::GetSlotName() const
{
	check(NamePropertyHandle);

	FName Name;
	NamePropertyHandle->GetValue(Name);

	return FText::FromName(Name);
}

void FSmartObjectSlotDefinitionDetails::OnSlotNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit) const
{
	check(NamePropertyHandle);

	if (InTextCommit == ETextCommit::OnEnter || InTextCommit == ETextCommit::OnUserMovedFocus)
	{
		// Remove excess whitespace and prevent categories with just spaces
		const FName NewName = FName(FText::TrimPrecedingAndTrailing(NewText).ToString());
		NamePropertyHandle->SetValue(NewName);
	}
}

void FSmartObjectSlotDefinitionDetails::CacheOuterDefinition()
{
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < OuterObjects.Num(); ObjectIdx++)
	{
		if (USmartObjectDefinition* OuterDefinition = Cast<USmartObjectDefinition>(OuterObjects[ObjectIdx]))
		{
			Definition = OuterDefinition;
			break;
		}
		if (USmartObjectDefinition* OuterDefinition = OuterObjects[ObjectIdx]->GetTypedOuter<USmartObjectDefinition>())
		{
			Definition = OuterDefinition;
			break;
		}
	}
}

void FSmartObjectSlotDefinitionDetails::OnCopy() const
{
	FString Value;
	if (StructProperty->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FSmartObjectSlotDefinitionDetails::OnPaste() const
{
	if (Definition == nullptr)
	{
		return;
	}
	
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	FScopedTransaction Transaction(LOCTEXT("PasteSlot", "Paste Slot"));

	StructProperty->NotifyPreChange();

	if (StructProperty->SetValueFromFormattedString(PastedText, EPropertyValueSetFlags::InstanceObjects) == FPropertyAccess::Success)
	{
		// Reset GUIDs on paste
		TArray<void*> RawNodeData;
		TArray<UObject*> OuterObjects;
		
		StructProperty->AccessRawData(RawNodeData);
		StructProperty->GetOuterObjects(OuterObjects);
		check(RawNodeData.Num() == OuterObjects.Num());
		
		for (int32 Index = 0; Index < RawNodeData.Num(); Index++)
		{
			if (FSmartObjectSlotDefinition* SlotDefinition = static_cast<FSmartObjectSlotDefinition*>(RawNodeData[Index]))
			{
				SlotDefinition->ID = FGuid::NewGuid();
				SlotDefinition->SelectionPreconditions.Initialize(OuterObjects[Index], Definition->GetWorldConditionSchemaClass(), {});
				
				// Set new IDs to all duplicated data too
				for (FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition->DefinitionData)
				{
					DataProxy.ID = FGuid::NewGuid();
				}
			}
		}
		
		StructProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		StructProperty->NotifyFinishedChangingProperties();

		if (PropUtils)
		{
			PropUtils->ForceRefresh();
		}
	}
	else
	{
		Transaction.Cancel();
	}
}

void FSmartObjectSlotDefinitionDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const FName HiddenName(TEXT("Hidden"));
	check(DefinitionDataPropertyHandle);
	check(NamePropertyHandle)

	// Find Slot ID for binding
	FGuid SlotID;
	StructProperty->EnumerateConstRawData([&SlotID](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			const FSmartObjectSlotDefinition& SlotDefinition = *static_cast<const FSmartObjectSlotDefinition*>(RawData);
			SlotID = SlotDefinition.ID;
			return false; // stop
		}
		return true;
	});

	const FString SlotIDString = LexToString(SlotID);
	
	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
	{
		if (TSharedPtr<IPropertyHandle> ChildPropertyHandle = StructPropertyHandle->GetChildHandle(ChildIndex))
		{
			const bool bIsHidden = ChildPropertyHandle->HasMetaData(HiddenName);
			if (bIsHidden)
			{
				continue;
			}
			
			// Skip properties with custom handling.
			if (ChildPropertyHandle->GetProperty() == DefinitionDataPropertyHandle->GetProperty())
			{
				continue;
			}

			// Set slot ID for binding
			ChildPropertyHandle->SetInstanceMetaData(UE::SmartObject::PropertyBinding::DataIDName, LexToString(SlotIDString));

			StructBuilder.AddProperty(ChildPropertyHandle.ToSharedRef());
		}
	}

	if (DefinitionDataPropertyHandle)
	{
		// Set slot ID for binding
		DefinitionDataPropertyHandle->SetInstanceMetaData(UE::SmartObject::PropertyBinding::DataIDName, LexToString(SlotIDString));

		IDetailPropertyRow& DefinitionDataRow = StructBuilder.AddProperty(DefinitionDataPropertyHandle.ToSharedRef());

		DefinitionDataRow.CustomWidget(true)
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
					.Text(DefinitionDataPropertyHandle->GetPropertyDisplayName())
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					DefinitionDataPropertyHandle->CreateDefaultPropertyButtonWidgets()
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE
