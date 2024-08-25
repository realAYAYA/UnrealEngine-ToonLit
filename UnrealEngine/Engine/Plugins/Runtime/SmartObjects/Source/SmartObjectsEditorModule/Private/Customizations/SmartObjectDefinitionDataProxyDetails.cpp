// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinitionDataProxyDetails.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "IPropertyUtilities.h"
#include "DetailWidgetRow.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SmartObjectDefinition.h"
#include "ScopedTransaction.h"
#include "Widgets/Text/STextBlock.h"
#include "InstancedStructDetails.h"
#include "SmartObjectViewModel.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"
#include "Modules/ModuleManager.h"
#include "StructViewerModule.h"
#include "StructViewerFilter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "SmartObjectEditorStyle.h"
#include "SmartObjectBindingExtension.h"

#define LOCTEXT_NAMESPACE "SmartObjectEditor"

////////////////////////////////////
class FSmartObjectDefinitionDataStructDetails : public FInstancedStructDataDetails
{
public:
	FSmartObjectDefinitionDataStructDetails(TSharedPtr<IPropertyHandle> InStructProperty, const FGuid InID)
		: FInstancedStructDataDetails(InStructProperty)
		, ID(InID)
	{
	}

	virtual void OnChildRowAdded(IDetailPropertyRow& ChildRow) override
	{
		if (ID.IsValid())
		{
			TSharedPtr<IPropertyHandle> ChildPropHandle = ChildRow.GetPropertyHandle();
			check(ChildPropHandle.IsValid());
			
			// Pass the node ID to binding extension. Since the properties are added using AddChildStructure(), we break the hierarchy and cannot access parent.
			ChildPropHandle->SetInstanceMetaData(UE::SmartObject::PropertyBinding::DataIDName, LexToString(ID));
		}
	}

private:
	FGuid ID;
};

////////////////////////////////////
class FSmartObjectDefinitionDataStructFilter : public IStructViewerFilter
{
public:
	/** The base struct for the property that classes must be a child-of. */
	const UScriptStruct* BaseStruct = nullptr;

	// A flag controlling whether we allow to select the BaseStruct
	bool bAllowBaseStruct = true;

	// Disallowed structs
	TArray<const UScriptStruct*> DisallowedStructs;
	
	virtual bool IsStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const UScriptStruct* InStruct, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		for (const UScriptStruct* DisallowedStruct : DisallowedStructs)
		{
			if (InStruct->IsChildOf(DisallowedStruct))
			{
				return false;
			}
		}
		
		if (InStruct == BaseStruct)
		{
			return bAllowBaseStruct;
		}

		if (InStruct->HasMetaData(TEXT("Hidden")))
		{
			return false;
		}

		// Query the native struct to see if it has the correct parent type (if any)
		return !BaseStruct || InStruct->IsChildOf(BaseStruct);
	}

	virtual bool IsUnloadedStructAllowed(const FStructViewerInitializationOptions& InInitOptions, const FSoftObjectPath& InStructPath, TSharedRef<FStructViewerFilterFuncs> InFilterFuncs) override
	{
		// Not supporting User Defined Structs
		return false;
	}

};

////////////////////////////////////

TSharedRef<IPropertyTypeCustomization> FSmartObjectDefinitionDataProxyDetails::MakeInstance()
{
	return MakeShareable(new FSmartObjectDefinitionDataProxyDetails);
}

void FSmartObjectDefinitionDataProxyDetails::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructProperty = StructPropertyHandle;
	PropUtils = StructCustomizationUtils.GetPropertyUtilities();

	DataPropertyHandle = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSmartObjectDefinitionDataProxy, Data));
	check(DataPropertyHandle);

	// Get ID and viewmodel from definition data.
	const FGuid ItemID = GetItemID();
	TSharedPtr<FSmartObjectViewModel> ViewModel = GetViewModel();
	
	HeaderRow
		.WholeRowContent()
		.VAlign(VAlign_Center)
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

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(0, 0, 4, 0))
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("SCS.Component"))
					.ColorAndOpacity(FColor(255,255,255,128))
				]
				
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(ComboButton, SComboButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
					.OnGetMenuContent(this, &FSmartObjectDefinitionDataProxyDetails::GenerateStructPicker)
					.ContentPadding(0)
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(this, &FSmartObjectDefinitionDataProxyDetails::GetDefinitionDataName)
							.ToolTipText(LOCTEXT("SelectDefinitionDataType", "Select Definition Data Type"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					StructProperty->CreateDefaultPropertyButtonWidgets()
				]
			]
		]
		.CopyAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectDefinitionDataProxyDetails::OnCopy)))
		.PasteAction(FUIAction(FExecuteAction::CreateSP(this, &FSmartObjectDefinitionDataProxyDetails::OnPaste)));
}

FText FSmartObjectDefinitionDataProxyDetails::GetDefinitionDataName() const
{
	check(StructProperty);
	// Note: We pick the first struct, we assume that multi-selection is not used.
	const UScriptStruct* Struct = nullptr;
	StructProperty->EnumerateConstRawData([&Struct](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			Struct = static_cast<const FSmartObjectDefinitionDataProxy*>(RawData)->Data.GetScriptStruct();
			return false; // stop
		}
		return true;
	});

	if (Struct)
	{
		return Struct->GetDisplayNameText();
	}
	return LOCTEXT("None", "None");
}

FGuid FSmartObjectDefinitionDataProxyDetails::GetItemID() const
{
	// Note: We pick the first ID, we assume that multi-selection is not used.
	FGuid ItemID;
	StructProperty->EnumerateConstRawData([&ItemID](const void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
	{
		if (RawData)
		{
			ItemID = static_cast<const FSmartObjectDefinitionDataProxy*>(RawData)->ID;
			return false; // stop
		}
		return true;
	});
	return ItemID;
}

TSharedPtr<FSmartObjectViewModel> FSmartObjectDefinitionDataProxyDetails::GetViewModel() const
{
	const USmartObjectDefinition* Definition = nullptr;
	
	TArray<UObject*> OuterObjects;
	StructProperty->GetOuterObjects(OuterObjects);
	for (int32 ObjectIdx = 0; ObjectIdx < OuterObjects.Num(); ObjectIdx++)
	{
		if (const USmartObjectDefinition* OuterDefinition = Cast<USmartObjectDefinition>(OuterObjects[ObjectIdx]))
		{
			Definition = OuterDefinition;
			break;
		}
		if (const USmartObjectDefinition* OuterDefinition = OuterObjects[ObjectIdx]->GetTypedOuter<USmartObjectDefinition>())
		{
			Definition = OuterDefinition;
			break;
		}
	}

	return FSmartObjectViewModel::Get(Definition);
}

void FSmartObjectDefinitionDataProxyDetails::OnCopy() const
{
	FString Value;
	if (StructProperty->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}
}

void FSmartObjectDefinitionDataProxyDetails::OnPaste() const
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	FScopedTransaction Transaction(LOCTEXT("PasteDefinitionData", "Paste Definition Data"));

	StructProperty->NotifyPreChange();

	if (StructProperty->SetValueFromFormattedString(PastedText, EPropertyValueSetFlags::InstanceObjects) == FPropertyAccess::Success)
	{
		// Reset GUIDs on paste
		StructProperty->EnumerateRawData([](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (RawData)
			{
				FSmartObjectDefinitionDataProxy& DataItem = *static_cast<FSmartObjectDefinitionDataProxy*>(RawData);
				DataItem.ID = FGuid::NewGuid();
			}
			return true;
		});

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

void FSmartObjectDefinitionDataProxyDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	check(DataPropertyHandle);
	// Add instance directly as child.
	const TSharedRef<FSmartObjectDefinitionDataStructDetails> DataDetails = MakeShared<FSmartObjectDefinitionDataStructDetails>(DataPropertyHandle, GetItemID());
	StructBuilder.AddCustomBuilder(DataDetails);
}

TSharedRef<SWidget> FSmartObjectDefinitionDataProxyDetails::GenerateStructPicker()
{
	static const FName NAME_ExcludeBaseStruct = "ExcludeBaseStruct";
	static const FName NAME_HideViewOptions = "HideViewOptions";
	static const FName NAME_ShowTreeView = "ShowTreeView";
	static const FName NAME_DisallowedStructs = "DisallowedStructs";
	
	const bool bExcludeBaseStruct = DataPropertyHandle->HasMetaData(NAME_ExcludeBaseStruct);
	const bool bAllowNone = !(DataPropertyHandle->GetMetaDataProperty()->PropertyFlags & CPF_NoClear);
	const bool bHideViewOptions = DataPropertyHandle->HasMetaData(NAME_HideViewOptions);
	const bool bShowTreeView = DataPropertyHandle->HasMetaData(NAME_ShowTreeView);

	TSharedRef<FSmartObjectDefinitionDataStructFilter> StructFilter = MakeShared<FSmartObjectDefinitionDataStructFilter>();
	StructFilter->BaseStruct = TBaseStructure<FSmartObjectDefinitionData>::Get();
	StructFilter->bAllowBaseStruct = !bExcludeBaseStruct;

	for (TSharedPtr<IPropertyHandle> Handle = DataPropertyHandle; Handle.IsValid(); Handle = Handle->GetParentHandle())
	{
		const FString& DisallowedStructs = Handle->GetMetaData(NAME_DisallowedStructs);
		if (!DisallowedStructs.IsEmpty())
		{
			TArray<FString> DisallowedStructNames;
			DisallowedStructs.ParseIntoArray(DisallowedStructNames, TEXT(","));

			for (const FString& DisallowedStructName : DisallowedStructNames)
			{
				UScriptStruct* ScriptStruct = FindObject<UScriptStruct>(nullptr, *DisallowedStructName, /*ExactClass*/false);
				if (ScriptStruct == nullptr)
				{
					ScriptStruct = LoadObject<UScriptStruct>(nullptr, *DisallowedStructName);
				}
				if (ScriptStruct)
				{
					StructFilter->DisallowedStructs.Add(ScriptStruct);
				}
			}
			break;
		}
	}
	

	FStructViewerInitializationOptions Options;
	Options.bShowNoneOption = bAllowNone;
	Options.StructFilter = StructFilter;
	Options.NameTypeToDisplay = EStructViewerNameTypeToDisplay::DisplayName;
	Options.DisplayMode = bShowTreeView ? EStructViewerDisplayMode::TreeView : EStructViewerDisplayMode::ListView;
	Options.bAllowViewOptions = !bHideViewOptions;

	FOnStructPicked OnPicked(FOnStructPicked::CreateSP(this, &FSmartObjectDefinitionDataProxyDetails::OnStructPicked));

	return SNew(SBox)
		.WidthOverride(280)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.MaxHeight(500)
			[
				FModuleManager::LoadModuleChecked<FStructViewerModule>("StructViewer").CreateStructViewer(Options, OnPicked)
			]
		];
}

void FSmartObjectDefinitionDataProxyDetails::OnStructPicked(const UScriptStruct* InStruct)
{
	if (DataPropertyHandle && DataPropertyHandle->IsValidHandle())
	{
		FScopedTransaction Transaction(LOCTEXT("OnStructPicked", "Set Struct"));

		DataPropertyHandle->NotifyPreChange();

		StructProperty->EnumerateRawData([InStruct](void* RawData, const int32 /*DataIndex*/, const int32 /*NumDatas*/)
		{
			if (FInstancedStruct* InstancedStruct = static_cast<FInstancedStruct*>(RawData))
			{
				InstancedStruct->InitializeAs(InStruct);
			}
			return true;
		});

		DataPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		DataPropertyHandle->NotifyFinishedChangingProperties();

		// Property tree will be invalid after changing the struct type, force update.
		if (PropUtils.IsValid())
		{
			PropUtils->ForceRefresh();
		}
	}

	ComboButton->SetIsOpen(false);
}

#undef LOCTEXT_NAMESPACE
