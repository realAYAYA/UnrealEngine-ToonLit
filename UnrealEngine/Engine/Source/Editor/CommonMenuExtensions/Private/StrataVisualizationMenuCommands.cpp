// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrataVisualizationMenuCommands.h"

#include "Delegates/Delegate.h"
#include "EditorViewportClient.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "StrataVisualizationData.h"
#include "Misc/AssertionMacros.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealNames.h"

#define LOCTEXT_NAMESPACE "SubstrateVisualizationMenuCommands"

FStrataVisualizationMenuCommands::FStrataVisualizationMenuCommands()
	: TCommands<FStrataVisualizationMenuCommands>
	(
		TEXT("SubstrateVisualizationMenu"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "SubstrateVisualizationMenu", "Substrate"), // Localized context name for displaying
		NAME_None, // Parent context name.  
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	),
	CommandMap()
{
}

void FStrataVisualizationMenuCommands::BuildCommandMap()
{
	const FStrataVisualizationData& VisualizationData = GetStrataVisualizationData();
	const FStrataVisualizationData::TModeMap& ModeMap = VisualizationData.GetModeMap();

	CommandMap.Empty();
	for (FStrataVisualizationData::TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FStrataVisualizationData::FModeRecord& Entry = It.Value();
		FStrataVisualizationRecord& Record = CommandMap.Add(Entry.ModeName, FStrataVisualizationRecord());
		Record.Name = Entry.ModeName;
		Record.Command = FUICommandInfoDecl(
			this->AsShared(),
			Entry.ModeName,
			Entry.ModeText,
			Entry.ModeDesc)
			.UserInterfaceType(EUserInterfaceActionType::RadioButton)
			.DefaultChord(FInputChord()
		);
		Record.ViewMode = Entry.ViewMode;
	}
}

void FStrataVisualizationMenuCommands::BuildVisualisationSubMenu(FMenuBuilder& Menu)
{
	const FStrataVisualizationMenuCommands& Commands = FStrataVisualizationMenuCommands::Get();
	if (Commands.IsPopulated())
	{
		// General
		{
			Menu.BeginSection("LevelViewportSubstrateVisualizationModeGeneral", LOCTEXT("SubstrateVisualizationGeneral", "Substrate General View Mode"));
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::MaterialProperties);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::MaterialCount);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::MaterialByteCount);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::StrataInfo);
			Menu.EndSection();
		}

		// Advanced
		{
			Menu.BeginSection("LevelViewportSubstrateVisualizationModeAdvanced", LOCTEXT("SubstrateVisualizationAdvanced", "Substrate Advanced View Mode"));
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::AdvancedMaterialProperties);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::MaterialClassification);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::RoughRefractionClassification);
			Commands.AddCommandTypeToMenu(Menu, FStrataViewMode::DecalClassification);
			Menu.EndSection();
		}
	}
}

bool FStrataVisualizationMenuCommands::AddCommandTypeToMenu(FMenuBuilder& Menu, const FStrataViewMode ViewMode) const
{
	bool bAddedCommands = false;

	const TStrataVisualizationModeCommandMap& Commands = CommandMap;
	for (TCommandConstIterator It = CreateCommandConstIterator(); It; ++It)
	{
		const FStrataVisualizationRecord& Record = It.Value();
		if (Record.ViewMode == ViewMode)
		{
			Menu.AddMenuEntry(Record.Command, NAME_None, Record.Command->GetLabel());
			bAddedCommands = true;
		}
	}

	return bAddedCommands;
}

FStrataVisualizationMenuCommands::TCommandConstIterator FStrataVisualizationMenuCommands::CreateCommandConstIterator() const
{
	return CommandMap.CreateConstIterator();
}

void FStrataVisualizationMenuCommands::RegisterCommands()
{
	BuildCommandMap();
}

void FStrataVisualizationMenuCommands::BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const
{
	// Map Strata visualization mode actions
	for (FStrataVisualizationMenuCommands::TCommandConstIterator It = FStrataVisualizationMenuCommands::Get().CreateCommandConstIterator(); It; ++It)
	{
		const FStrataVisualizationMenuCommands::FStrataVisualizationRecord& Record = It.Value();
		CommandList.MapAction(
			Record.Command,
			FExecuteAction::CreateStatic(&FStrataVisualizationMenuCommands::ChangeStrataVisualizationMode, Client.ToWeakPtr(), Record.Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FStrataVisualizationMenuCommands::IsStrataVisualizationModeSelected, Client.ToWeakPtr(), Record.Name)
		);
	}
}

void FStrataVisualizationMenuCommands::ChangeStrataVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		Client->ChangeStrataVisualizationMode(InName);
	}
}

bool FStrataVisualizationMenuCommands::IsStrataVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		return Client->IsStrataVisualizationModeSelected(InName);
	}
	
	return false;
}

#undef LOCTEXT_NAMESPACE
