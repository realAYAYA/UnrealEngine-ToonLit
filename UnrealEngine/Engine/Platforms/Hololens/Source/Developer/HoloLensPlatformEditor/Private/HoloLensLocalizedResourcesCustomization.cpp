// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensLocalizedResourcesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "Widgets/Input/STextComboBox.h"
#include "Settings/ProjectPackagingSettings.h"

#define LOCTEXT_NAMESPACE "HoloLensLocalizedResourcesCustomization"

FHoloLensLocalizedResourcesNodeBuilder::FHoloLensLocalizedResourcesNodeBuilder(TSharedRef<IPropertyHandle> InLocalizedResourceCollectionProperty, const FString& InPluginName)
	: OptionalPluginName(InPluginName)
	, LocalizedResourceCollectionArray(InLocalizedResourceCollectionProperty->AsArray().ToSharedRef())
{
	// Ensure that we have a default set of fallback resources (empty culture id)
	uint32 NumElements;
	LocalizedResourceCollectionArray->GetNumElements(NumElements);

	TSharedPtr<IPropertyHandle> DefaultResources;
	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> Element = LocalizedResourceCollectionArray->GetElement(i);
		FString AppliesToDlcName;
		TSharedPtr<IPropertyHandle> DlcPluginProperty = Element->GetChildHandle("AppliesToDlcPlugin");
		if (DlcPluginProperty.IsValid())
		{
			DlcPluginProperty->GetValue(AppliesToDlcName);
		}
		FString CultureId;
		Element->GetChildHandle("CultureId")->GetValue(CultureId);

		if (AppliesToDlcName == OptionalPluginName && CultureId.IsEmpty())
		{
			DefaultResources = Element;
		}
	}

	if (!DefaultResources.IsValid())
	{
		AddLocalizedEntry(TEXT(""));
	}

	InLocalizedResourceCollectionProperty->MarkHiddenByCustomization();
	FSimpleDelegate ChangedDelegate = FSimpleDelegate::CreateRaw(this, &FHoloLensLocalizedResourcesNodeBuilder::OnNumElementsChanged);
	LocalizedResourceCollectionArray->SetOnNumElementsChanged(ChangedDelegate);
}

FHoloLensLocalizedResourcesNodeBuilder::~FHoloLensLocalizedResourcesNodeBuilder()
{
	FSimpleDelegate Empty;
	LocalizedResourceCollectionArray->SetOnNumElementsChanged(Empty);
}

void FHoloLensLocalizedResourcesNodeBuilder::AddLocalizedEntryForSelectedCulture()
{
	if (SelectedCulture.IsValid())
	{
		AddLocalizedEntry(*SelectedCulture->GetSelectedItem());
	}
}

void FHoloLensLocalizedResourcesNodeBuilder::AddLocalizedEntry(const FString& CultureId)
{
	// Note: NumElements here wil
	uint32 NumElements;
	LocalizedResourceCollectionArray->GetNumElements(NumElements);

	FPropertyAccess::Result AddResult = LocalizedResourceCollectionArray->AddItem();
	check(AddResult == FPropertyAccess::Success);

	// This little bit of magic forces a refresh of the property handle tree, ensuring
	// that we can safely access element 0 (which we just moved our new entry into)
	LocalizedResourceCollectionArray->MoveElementTo(NumElements, 0);

	TSharedRef<IPropertyHandle> NewlyAddedItem = LocalizedResourceCollectionArray->GetElement(0);
	NewlyAddedItem->NotifyPreChange();
	if (!OptionalPluginName.IsEmpty())
	{
		TSharedPtr<IPropertyHandle> DlcPluginProperty = NewlyAddedItem->GetChildHandle("AppliesToDlcPlugin");
		check(DlcPluginProperty.IsValid());
		DlcPluginProperty->SetValue(OptionalPluginName);
	}
	NewlyAddedItem->GetChildHandle("CultureId")->SetValue(CultureId);
	NewlyAddedItem->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	OnRebuildChildren.ExecuteIfBound();
}

void FHoloLensLocalizedResourcesNodeBuilder::EmptyLocalizedEntries()
{
	uint32 NumElements;
	LocalizedResourceCollectionArray->GetNumElements(NumElements);

	for (int32 i = static_cast<int32>(NumElements) - 1; i >= 0; --i)
	{
		TSharedRef<IPropertyHandle> Element = LocalizedResourceCollectionArray->GetElement(i);
		FString AppliesToDlcName;
		TSharedPtr<IPropertyHandle> DlcPluginProperty = Element->GetChildHandle("AppliesToDlcPlugin");
		if (DlcPluginProperty.IsValid())
		{
			DlcPluginProperty->GetValue(AppliesToDlcName);
		}
		FString CultureId;
		Element->GetChildHandle("CultureId")->GetValue(CultureId);
		if (AppliesToDlcName == OptionalPluginName && !CultureId.IsEmpty())
		{
			LocalizedResourceCollectionArray->DeleteItem(i);
		}
	}
	OnRebuildChildren.ExecuteIfBound();
}

void FHoloLensLocalizedResourcesNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	AvailableUECultureNames.Empty();
	for (const FString& AvailableCulture : GetDefault<UProjectPackagingSettings>()->CulturesToStage)
	{
		AvailableUECultureNames.Add(MakeShared<FString>(AvailableCulture));
	}

	uint32 NumElements;
	LocalizedResourceCollectionArray->GetNumElements(NumElements);
	bool HasLocalizedElements = false;

	TSharedPtr<IPropertyHandle> DefaultResources;
	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> Element = LocalizedResourceCollectionArray->GetElement(i);
		FString AppliesToDlcName;
		TSharedPtr<IPropertyHandle> DlcPluginProperty = Element->GetChildHandle("AppliesToDlcPlugin");
		if (DlcPluginProperty.IsValid())
		{
			DlcPluginProperty->GetValue(AppliesToDlcName);
		}
		FString CultureId;
		Element->GetChildHandle("CultureId")->GetValue(CultureId);

		if (AppliesToDlcName == OptionalPluginName)
		{
			if (CultureId.IsEmpty())
			{
				DefaultResources = Element;
			}
			else
			{
				HasLocalizedElements = true;
				AvailableUECultureNames.RemoveAllSwap(
					[CultureId](TSharedPtr<FString> Entry)
				{
					return *Entry == CultureId;
				});
			}
		}
	}

	check(DefaultResources.IsValid());

	IDetailGroup& NeutralGroup = ChildrenBuilder.AddGroup("NeutralResources", LOCTEXT("NeutralResources", "Neutral Resources"));
	NeutralGroup.AddPropertyRow(DefaultResources.ToSharedRef());

	IDetailGroup& LocalizedGroup = ChildrenBuilder.AddGroup("LocalizedResources", LOCTEXT("LocalizedResources", "Localized Resources"));

	if (AvailableUECultureNames.Num() > 0)
	{
		LocalizedGroup.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AddLocalizationTarget", "Add resource localization target"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(SelectedCulture, STextComboBox)
					.OptionsSource(&AvailableUECultureNames)
					.InitiallySelectedItem(AvailableUECultureNames[0])
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateSP(this, &FHoloLensLocalizedResourcesNodeBuilder::AddLocalizedEntryForSelectedCulture))
				]
			];
	}
	else
	{
		SelectedCulture.Reset();
	}

	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedRef<IPropertyHandle> Element = LocalizedResourceCollectionArray->GetElement(i);
		FString AppliesToDlcName;
		TSharedPtr<IPropertyHandle> DlcPluginProperty = Element->GetChildHandle("AppliesToDlcPlugin");
		if (DlcPluginProperty.IsValid())
		{
			DlcPluginProperty->GetValue(AppliesToDlcName);
		}
		FString CultureId;
		Element->GetChildHandle("CultureId")->GetValue(CultureId);

		if (AppliesToDlcName == OptionalPluginName && !CultureId.IsEmpty())
		{
			// Disable the default array button (duplicate/delete/etc.) since
			// we want tighter control over the allowed operations.
			LocalizedGroup.AddPropertyRow(Element).ShowPropertyButtons(false);
		}
	}
}

TSharedRef<IPropertyTypeCustomization> FHoloLensLocalizedResourcesCustomization::MakeInstance()
{
	return MakeShareable(new FHoloLensLocalizedResourcesCustomization);
}

void FHoloLensLocalizedResourcesCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TSharedRef<IPropertyHandle> UECultureNameProperty = StructPropertyHandle->GetChildHandle(FName("CultureId")).ToSharedRef();

	// Add a button to the header to delete this localization target
	FString CurrentCulture;
	UECultureNameProperty->GetValue(CurrentCulture);
	if (!CurrentCulture.IsEmpty())
	{
		HeaderRow.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString(CurrentCulture))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateLambda(
					[StructPropertyHandle]()
				{
					StructPropertyHandle->GetParentHandle()->AsArray()->DeleteItem(StructPropertyHandle->GetIndexInArray());
				}))
			]
		];
	}
}


void FHoloLensLocalizedResourcesCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Unpack the Strings property
	TSharedPtr<IPropertyHandle> StringsProperty = InStructPropertyHandle->GetChildHandle("Strings");
	if (StringsProperty.IsValid())
	{
		uint32 NumChildren = 0;
		StringsProperty->GetNumChildren(NumChildren);
		for (uint32 i = 0; i < NumChildren; ++i)
		{
			ChildBuilder.AddProperty(StringsProperty->GetChildHandle(i).ToSharedRef());
		}
	}

	ChildBuilder.AddProperty(InStructPropertyHandle->GetChildHandle("Images").ToSharedRef());
}


#undef LOCTEXT_NAMESPACE