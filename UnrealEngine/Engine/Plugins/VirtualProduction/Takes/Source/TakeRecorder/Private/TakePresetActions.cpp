// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakePresetActions.h"
#include "TakePreset.h"
#include "TakePresetToolkit.h"
#include "ITakeRecorderModule.h"
#include "Widgets/STakeRecorderTabContent.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Toolkits/ToolkitManager.h"
#include "ISequencer.h"
#include "SequencerSettings.h"

#include "LevelSequence.h"

#define LOCTEXT_NAMESPACE "TakePresetActions"




uint32 FTakePresetActions::GetCategories()
{
	return EAssetTypeCategories::Animation;
}

FText FTakePresetActions::GetName() const
{
	return LOCTEXT("TakePreset_Label", "Take Recorder Preset");
}


UClass* FTakePresetActions::GetSupportedClass() const
{
	return UTakePreset::StaticClass(); 
}


FColor FTakePresetActions::GetTypeColor() const
{
	return FColor(226, 155, 72);
}


void FTakePresetActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	// Should always be WorldCentric
	if (!ensure(EditWithinLevelEditor.IsValid()))
	{
		return;
	}

	for (UObject* Object : InObjects)
	{
		UTakePreset* TakePreset = Cast<UTakePreset>(Object);
		if (TakePreset)
		{
			TSharedPtr<FTakePresetToolkit> Toolkit = MakeShared<FTakePresetToolkit>();
			Toolkit->Initialize(EToolkitMode::WorldCentric, EditWithinLevelEditor, TakePreset);

			TSharedPtr<SDockTab> DockTab = EditWithinLevelEditor->GetTabManager()->TryInvokeTab(ITakeRecorderModule::TakeRecorderTabName);
			if (DockTab.IsValid())
			{
				TSharedRef<STakeRecorderTabContent> TabContent = StaticCastSharedRef<STakeRecorderTabContent>(DockTab->GetContent());
				TabContent->SetupForEditing(Toolkit);
			}
		}
	}
}


bool FTakePresetActions::ShouldForceWorldCentric()
{
	return true;
}


#undef LOCTEXT_NAMESPACE
