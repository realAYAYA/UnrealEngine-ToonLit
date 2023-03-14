// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGComponentDetails.h"
#include "PCGComponent.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DetailCategoryBuilder.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "PCGComponentDetails"

TSharedRef<IDetailCustomization> FPCGComponentDetails::MakeInstance()
{
	return MakeShareable(new FPCGComponentDetails());
}

void FPCGComponentDetails::GatherPCGComponentsFromSelection(const TArray<TWeakObjectPtr<UObject>>& InObjectSelected)
{
	for (const TWeakObjectPtr<UObject>& Object : InObjectSelected)
	{
		UPCGComponent* Component = Cast<UPCGComponent>(Object.Get());
		if (ensure(Component))
		{
			SelectedComponents.Add(Component);
		}
	}
}

void FPCGComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	const FName PCGCategoryName("PCG");
	IDetailCategoryBuilder& PCGCategory = DetailBuilder.EditCategory(PCGCategoryName);

	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	GatherPCGComponentsFromSelection(ObjectsBeingCustomized);

	if (AddDefaultProperties())
	{
		TArray<TSharedRef<IPropertyHandle>> AllProperties;
		bool bSimpleProperties = true;
		bool bAdvancedProperties = false;
		// Add all properties in the category in order
		PCGCategory.GetDefaultProperties(AllProperties, bSimpleProperties, bAdvancedProperties);

		for (auto& Property : AllProperties)
		{
			PCGCategory.AddProperty(Property);
		}
	}

	FDetailWidgetRow& NewRow = PCGCategory.AddCustomRow(FText::GetEmpty());

	NewRow.ValueContent()
		.MaxDesiredWidth(120.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f)
			[
				SNew(SButton)
				.OnClicked(this, &FPCGComponentDetails::OnGenerateClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("GenerateButton", "Generate"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f)
			[
				SNew(SButton)
				.OnClicked(this, &FPCGComponentDetails::OnCleanupClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("CleanupButton", "Cleanup"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f)
			[
				SNew(SButton)
				.OnClicked(this, &FPCGComponentDetails::OnClearPCGLinkClicked)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("ClearPCGLinkButton", "Clear PCG Link"))
				]
			]
		];

	// Attach to generated delegate on the selected components
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->OnPCGGraphGeneratedDelegate.AddRaw(this, &FPCGComponentDetails::OnGraphChanged);
			Component.Get()->OnPCGGraphCleanedDelegate.AddRaw(this, &FPCGComponentDetails::OnGraphChanged);
		}
	}
}

void FPCGComponentDetails::PendingDelete()
{
	// Detach from the generated delegate
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->OnPCGGraphGeneratedDelegate.RemoveAll(this);
			Component.Get()->OnPCGGraphCleanedDelegate.RemoveAll(this);
		}
	}
}

FReply FPCGComponentDetails::OnGenerateClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->Generate();
		}
	}

	return FReply::Handled();
}

FReply FPCGComponentDetails::OnClearPCGLinkClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->ClearPCGLink();
		}
	}

	return FReply::Handled();
}

void FPCGComponentDetails::OnGraphChanged(UPCGComponent* InComponent)
{
	if (!InComponent || !InComponent->GetOwner())
	{
		return;
	}

	// Notify editor that some components might have changed
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.BroadcastComponentsEdited();
}

FReply FPCGComponentDetails::OnCleanupClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->Cleanup();
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
