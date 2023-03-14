// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionDetailsCustomization.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "PropertyCustomizationHelpers.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "FWorldPartitionDetails"

TSharedRef<IDetailCustomization> FWorldPartitionDetails::MakeInstance()
{
	return MakeShareable(new FWorldPartitionDetails);
}

void FWorldPartitionDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	InDetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	check(ObjectsBeingCustomized.Num() == 1);

	DetailBuilder = &InDetailBuilder;
	WorldPartition = CastChecked<UWorldPartition>(ObjectsBeingCustomized[0].Get());
	RuntimeHashClass = (WorldPartition.IsValid() && WorldPartition->RuntimeHash) ? WorldPartition->RuntimeHash->GetClass() : nullptr;

	IDetailCategoryBuilder& WorldPartitionCategory = InDetailBuilder.EditCategory("WorldPartitionSetup");

	// Enable streaming checkbox
	WorldPartitionCategory.AddCustomRow(LOCTEXT("EnableStreaming", "Enable Streaming"), false)
		.RowTag(TEXT("EnableStreaming"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorldPartitionEnableStreaming", "Enable Streaming"))
			.ToolTipText(LOCTEXT("WorldPartitionEnableStreaming_ToolTip", "Set the world partition enable streaming state."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(MakeAttributeLambda([this]() { return WorldPartition.IsValid() && WorldPartition->IsStreamingEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }))
			.OnCheckStateChanged(this, &FWorldPartitionDetails::HandleWorldPartitionEnableStreamingChanged)
		]
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]() { return WorldPartition.IsValid() && WorldPartition->SupportsStreaming() ? EVisibility::Visible : EVisibility::Hidden; }));

	// Runtime hash class selector
	WorldPartitionCategory.AddCustomRow(LOCTEXT("RuntimeHashClass", "Runtime Hash Class"), false)
		.RowTag(TEXT("RuntimeHashClass"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("WorldPartitionRuntimeHashClass", "Runtime Hash Class"))
			.ToolTipText(LOCTEXT("WorldPartitionRuntimeHashClass_ToolTip", "Set the world partition runtime hash class."))
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		[
			SNew(SClassPropertyEntryBox)
			.MetaClass(UWorldPartitionRuntimeHash::StaticClass())
			.AllowNone(false)
			.HideViewOptions(true)
			.SelectedClass_Lambda([this]() { return RuntimeHashClass; })
			.OnSetClass_Lambda([this](const UClass* Class) { HandleWorldPartitionRuntimeHashClassChanged(Class); })
		]
		.Visibility(TAttribute<EVisibility>::CreateLambda([this]() { return WorldPartition.IsValid() && WorldPartition->IsStreamingEnabled() ? EVisibility::Visible : EVisibility::Hidden; }));

	// Runtime hash properties
	if (WorldPartition->RuntimeHash)
	{
		FAddPropertyParams Params;
		Params.HideRootObjectNode(true);
		Params.UniqueId(TEXT("RuntimeHash"));

		if (IDetailPropertyRow* RuntimeHashRow = WorldPartitionCategory.AddExternalObjects({ WorldPartition->RuntimeHash }, EPropertyLocation::Default, Params))
		{
			(*RuntimeHashRow)
				.ShouldAutoExpand(true)
				.DisplayName(LOCTEXT("RuntimeHash", "Runtime Hash"))
				.Visibility(TAttribute<EVisibility>::CreateLambda([this]() { return WorldPartition.IsValid() && WorldPartition->IsStreamingEnabled() ? EVisibility::Visible : EVisibility::Hidden; }));
		}

		if (!WorldPartition->RuntimeHash->SupportsHLODs())
		{
			InDetailBuilder.HideProperty(InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UWorldPartition, DefaultHLODLayer), UWorldPartition::StaticClass()));
		}
	}
}

void FWorldPartitionDetails::HandleWorldPartitionEnableStreamingChanged(ECheckBoxState InCheckState)
{
	if (InCheckState == ECheckBoxState::Checked)
	{
		if (WorldPartition->CanBeUsedByLevelInstance())
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("WorldPartitionConfirmEnableStreamingOnLevelReferencedByLevelInstance", "You are about to enable streaming on a level that could be used by level instances. Continuing will invalidate level instances referencing this level. Continue?")) == EAppReturnType::No)
			{
				return;
			}
		}

		if (!WorldPartition->bStreamingWasEnabled)
		{
			if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("WorldPartitionConfirmEnableStreaming", "You are about to enable streaming for the first time, the world will be setup to stream. Continue?")) == EAppReturnType::No)
			{
				return;
			}

			if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("WorldPartitionEnableStreamingDialolg", "Please refer to our documentation for how to set up streaming.\n\nWould you like to open it now? ")) == EAppReturnType::Yes)
			{
				IDocumentation::Get()->Open(TEXT("world-partition-in-unreal-engine"), FDocumentationSourceInfo(TEXT("worldpartition")));
			}

			WorldPartition->bStreamingWasEnabled = true;
		}

		WorldPartition->SetEnableStreaming(true);
	}
	else
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("WorldPartitionConfirmDisableStreaming", "You are about to disable streaming, all actors in the world will be always loaded. Continue?")) == EAppReturnType::Yes)
		{
			WorldPartition->SetEnableStreaming(false);
		}
	}
}

void FWorldPartitionDetails::HandleWorldPartitionRuntimeHashClassChanged(const UClass* InRuntimeHashClass)
{
	check(InRuntimeHashClass);
	check(InRuntimeHashClass->IsChildOf<UWorldPartitionRuntimeHash>());
	check(!(InRuntimeHashClass->GetClassFlags() & CLASS_Abstract));

	RuntimeHashClass = InRuntimeHashClass;

	if (WorldPartition.IsValid() && (!WorldPartition->RuntimeHash || WorldPartition->RuntimeHash->GetClass() != RuntimeHashClass))
	{
		WorldPartition->Modify();
		WorldPartition->RuntimeHash = NewObject<UWorldPartitionRuntimeHash>(WorldPartition.Get(), RuntimeHashClass, NAME_None, RF_Transactional);
		WorldPartition->RuntimeHash->SetDefaultValues();

		DetailBuilder->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE
