// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteVisualizationMenuCommands.h"

#include "Delegates/Delegate.h"
#include "EditorViewportClient.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "NaniteVisualizationData.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealNames.h"

int32 GNaniteVisualizeAdvanced = 0;
static FAutoConsoleVariableRef CVarNaniteVisualizeAdvanced(
	TEXT("r.Nanite.Visualize.Advanced"),
	GNaniteVisualizeAdvanced,
	TEXT("")
);

#define LOCTEXT_NAMESPACE "NaniteVisualizationMenuCommands"

FNaniteVisualizationMenuCommands::FNaniteVisualizationMenuCommands()
	: TCommands<FNaniteVisualizationMenuCommands>
	(
		TEXT("NaniteVisualizationMenu"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "NaniteVisualizationMenu", "Nanite Visualization"), // Localized context name for displaying
		NAME_None, // Parent context name.  
		FAppStyle::GetAppStyleSetName() // Icon Style Set
	),
	CommandMap()
{
}

void FNaniteVisualizationMenuCommands::BuildCommandMap()
{
	const FNaniteVisualizationData& VisualizationData = GetNaniteVisualizationData();
	const FNaniteVisualizationData::TModeMap& ModeMap = VisualizationData.GetModeMap();

	CommandMap.Empty();
	for (FNaniteVisualizationData::TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FNaniteVisualizationData::FModeRecord& Entry = It.Value();
		FNaniteVisualizationRecord& Record = CommandMap.Add(Entry.ModeName, FNaniteVisualizationRecord());
		Record.Name = Entry.ModeName;
		Record.Command = FUICommandInfoDecl(
			this->AsShared(),
			Entry.ModeName,
			Entry.ModeText,
			Entry.ModeDesc)
			.UserInterfaceType(EUserInterfaceActionType::RadioButton)
			.DefaultChord(FInputChord()
		);

		switch (Entry.ModeType)
		{
		default:
		case FNaniteVisualizationData::FModeType::Overview:
			Record.Type = FNaniteVisualizationType::Overview;
			break;

		case FNaniteVisualizationData::FModeType::Standard:
			Record.Type = FNaniteVisualizationType::Standard;
			break;

		case FNaniteVisualizationData::FModeType::Advanced:
			Record.Type = FNaniteVisualizationType::Advanced;
			break;
		}
	}
}

void FNaniteVisualizationMenuCommands::BuildVisualisationSubMenu(FMenuBuilder& Menu)
{
	const bool bShowAdvanced = GNaniteVisualizeAdvanced != 0;

	const FNaniteVisualizationMenuCommands& Commands = FNaniteVisualizationMenuCommands::Get();
	if (Commands.IsPopulated())
	{
		Menu.BeginSection("LevelViewportNaniteVisualizationMode", LOCTEXT("NaniteVisualizationHeader", "Nanite Visualization Mode"));

		if (Commands.AddCommandTypeToMenu(Menu, FNaniteVisualizationType::Overview))
		{
			Menu.AddMenuSeparator();
		}

		if (Commands.AddCommandTypeToMenu(Menu, FNaniteVisualizationType::Standard))
		{
			if (bShowAdvanced)
			{
				Menu.AddMenuSeparator();
			}
		}

		if (bShowAdvanced)
		{
			Commands.AddCommandTypeToMenu(Menu, FNaniteVisualizationType::Advanced);
		}

		Menu.EndSection();
	}
}

bool FNaniteVisualizationMenuCommands::AddCommandTypeToMenu(FMenuBuilder& Menu, const FNaniteVisualizationType Type) const
{
	bool bAddedCommands = false;

	const TNaniteVisualizationModeCommandMap& Commands = CommandMap;
	for (TCommandConstIterator It = CreateCommandConstIterator(); It; ++It)
	{
		const FNaniteVisualizationRecord& Record = It.Value();
		if (Record.Type == Type)
		{
			Menu.AddMenuEntry(Record.Command, NAME_None, Record.Command->GetLabel());
			bAddedCommands = true;
		}
	}

	return bAddedCommands;
}

FNaniteVisualizationMenuCommands::TCommandConstIterator FNaniteVisualizationMenuCommands::CreateCommandConstIterator() const
{
	return CommandMap.CreateConstIterator();
}

void FNaniteVisualizationMenuCommands::RegisterCommands()
{
	BuildCommandMap();
}

void FNaniteVisualizationMenuCommands::BindCommands(FUICommandList& CommandList, const TSharedPtr<FEditorViewportClient>& Client) const
{
	// Map Nanite visualization mode actions
	for (FNaniteVisualizationMenuCommands::TCommandConstIterator It = FNaniteVisualizationMenuCommands::Get().CreateCommandConstIterator(); It; ++It)
	{
		const FNaniteVisualizationMenuCommands::FNaniteVisualizationRecord& Record = It.Value();
		CommandList.MapAction(
			Record.Command,
			FExecuteAction::CreateStatic(&FNaniteVisualizationMenuCommands::ChangeNaniteVisualizationMode, Client.ToWeakPtr(), Record.Name),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FNaniteVisualizationMenuCommands::IsNaniteVisualizationModeSelected, Client.ToWeakPtr(), Record.Name)
		);
	}
}

void FNaniteVisualizationMenuCommands::ChangeNaniteVisualizationMode(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		Client->ChangeNaniteVisualizationMode(InName);
	}
}

bool FNaniteVisualizationMenuCommands::IsNaniteVisualizationModeSelected(TWeakPtr<FEditorViewportClient> WeakClient, FName InName)
{
	if (TSharedPtr<FEditorViewportClient> Client = WeakClient.Pin())
	{
		return Client->IsNaniteVisualizationModeSelected(InName);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
