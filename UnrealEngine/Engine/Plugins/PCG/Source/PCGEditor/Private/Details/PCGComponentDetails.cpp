// Copyright Epic Games, Inc. All Rights Reserved.

#include "Details/PCGComponentDetails.h"
#include "PCGComponent.h"
#include "PCGEditorStyle.h"
#include "PCGSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "PCGComponentDetails"

static TAutoConsoleVariable<bool> CVarBroadcastComponentChanged(
		TEXT("pcg.ComponentDetails.BroadcastComponentChanged"),
		false,
		TEXT("Controls whether we should broadcast component changes to the level editor"));

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
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
				.OnClicked(this, &FPCGComponentDetails::OnGenerateClicked)
				.ToolTipText(FText::FromString("Generates graph data. \nCtrl + Click flushes the cache and force generates."))
				.Visibility(this, &FPCGComponentDetails::GenerateButtonVisible)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(16,16))
						.Image_Lambda([]() { return FSlateApplication::Get().GetModifierKeys().IsControlDown() ? FPCGEditorStyle::Get().GetBrush("PCG.Command.ForceRegenClearCache") : FPCGEditorStyle::Get().GetBrush("PCG.Command.ForceRegen"); })
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text_Lambda([]() { return FSlateApplication::Get().GetModifierKeys().IsControlDown() ? LOCTEXT("ForceRegenerateButton", "Force Generate") : LOCTEXT("GenerateButton", "Generate"); })
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			.VAlign(VAlign_Fill)
			[
				SNew(SButton)
				.OnClicked(this, &FPCGComponentDetails::OnCancelClicked)
				.Visibility(this, &FPCGComponentDetails::CancelButtonVisible)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(0.0f, 0.0f, 6.0f, 0.0f)
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.DesiredSizeOverride(FVector2D(16, 16))
						.Image(FPCGEditorStyle::Get().GetBrush("PCG.Command.StopRegen"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.Text(LOCTEXT("CancelButton", "Cancel"))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &FPCGComponentDetails::OnCleanupClicked)
				.Visibility(this, &FPCGComponentDetails::CleanupButtonVisible)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("CleanupButton", "Cleanup"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
				.OnClicked(this, &FPCGComponentDetails::OnRefreshClicked)
				.Visibility(this, &FPCGComponentDetails::RefreshButtonVisible)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("RefreshButton", "Refresh"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Fill)
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.VAlign(VAlign_Center)
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

EVisibility FPCGComponentDetails::GenerateButtonVisible() const
{
	for (const TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		// If component is runtime generated then generate/cleanup is managed by the scheduler.
		if (Component.IsValid() && !Component->IsGenerating() && !Component->IsManagedByRuntimeGenSystem())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FPCGComponentDetails::CancelButtonVisible() const
{
	for (const TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		// If component is runtime generated then generate/cleanup is managed by the scheduler.
		if (Component.IsValid() && Component->IsGenerating() && !Component->IsManagedByRuntimeGenSystem())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FPCGComponentDetails::CleanupButtonVisible() const
{
	for (const TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		// If component is runtime generated then generate/cleanup is managed by the scheduler.
		if (Component.IsValid() && !Component->IsManagedByRuntimeGenSystem())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

EVisibility FPCGComponentDetails::RefreshButtonVisible() const
{
	for (const TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid() && Component->IsManagedByRuntimeGenSystem())
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Collapsed;
}

FReply FPCGComponentDetails::OnGenerateClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			bool bForce = false;
			FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
			if (ModifierKeys.IsControlDown())
			{
				Component->GetSubsystem()->FlushCache();
				bForce = true;
			}

			Component.Get()->Generate(bForce);
		}
	}

	return FReply::Handled();
}

FReply FPCGComponentDetails::OnCancelClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			Component.Get()->CancelGeneration();
		}
	}

	return FReply::Handled();
}

FReply FPCGComponentDetails::OnRefreshClicked()
{
	for (TWeakObjectPtr<UPCGComponent>& Component : SelectedComponents)
	{
		if (Component.IsValid())
		{
			// Trigger the deepest refresh - re-initialize the PAs.
			Component.Get()->Refresh(EPCGChangeType::Structural | EPCGChangeType::GenerationGrid, /*bCancelExistingRefresh=*/true);
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
	if (CVarBroadcastComponentChanged.GetValueOnGameThread())
	{
		FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.BroadcastComponentsEdited();
	}
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
