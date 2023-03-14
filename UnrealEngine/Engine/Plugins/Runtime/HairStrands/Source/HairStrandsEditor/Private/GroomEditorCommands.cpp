// Copyright Epic Games, Inc. All Rights Reserved.

#include "GroomEditorCommands.h"
#include "GroomEditorStyle.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "GroomEditorCommands"

FGroomEditorCommands::FGroomEditorCommands() :
	TCommands<FGroomEditorCommands>(
		"GroomEditorCommands", // Context name for fast lookup
		NSLOCTEXT("Contexts", "GroomEditorMode", "Groom Tools"), // Localized context name for displaying
		NAME_None, // Parent
		FGroomEditorStyle::GetStyleName() // Icon Style Set
		)
{
}

void FGroomEditorCommands::RegisterCommands()
{
	UI_COMMAND(BeginHairPlaceTool,	"HairPlace","Start the Hair Placement Tool",		EUserInterfaceActionType::Button,	FInputChord());
	UI_COMMAND(Simulate,			"Simulate", "Start simulating Hair",				EUserInterfaceActionType::Button,	FInputChord(EKeys::S, EModifierKey::Alt));
	UI_COMMAND(ResetSimulation,		"Reset",	"Reset the running hair simulation",	EUserInterfaceActionType::Button,	FInputChord());
	UI_COMMAND(PauseSimulation,		"Pause",	"Pause the running hair simulation",	EUserInterfaceActionType::Button,	FInputChord());
	UI_COMMAND(PlaySimulation,		"Play",		"Play the hair simulation",				EUserInterfaceActionType::Button,	FInputChord());

	UI_COMMAND(PlayAnimation,		"Play",		"Play animation",						EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(StopAnimation,		"Stop",		"Stop animation",						EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ViewMode_Lit,			"Lit",				"Default",						EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Guide,			"Guide",			"View guide (only available if the groom has simulation enabled)",					EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_GuideInfluence, "GuideInfluence",	"View guide influence (only available if the groom has simulation enabled)",		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_UV,				"UV",				"View strands UVs ",			EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_RootUV,			"RootUV",			"View roots UVs ",				EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_RootUDIM,		"RootUDIM",			"View root UDIM UVs",			EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Seed,			"Seed",				"View root seed",				EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Dimension,		"Dimension",		"View strands dimension",		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_RadiusVariation,"RadiusVariation",	"View strands radius variation",EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Tangent,		"Tangent",			"View tangents direction",		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_BaseColor,		"BaseColor",		"View vertex base color",		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Roughness,		"Roughness",		"View vertex roughness",		EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_ControlPoints,	"Hair CVs",			"View hair strand CVs",			EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_VisCluster,		"Vis. Clusters",	"View clusters (only available if the groom has simulation enabled)",				EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ViewMode_Group,			"Hair Groups",		"View hair groups",				EUserInterfaceActionType::Button, FInputChord());

	UI_COMMAND(ViewMode_CardsGuides,	"CardsGuide",		"Cards Guides (only available if the groom has cards geometry)",					EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define LOCTEXT_NAMESPACE "GroomViewportLODCommands"

FGroomViewportLODCommands::FGroomViewportLODCommands() :
	TCommands<FGroomViewportLODCommands>(
		TEXT("GroomViewportLODCmd"), // Context name for fast lookup
		NSLOCTEXT("Contexts", "GroomViewportLODCmd", "Groom Viewport LOD Command"), // Localized context name for displaying
		NAME_None, // Parent context name. 
		FAppStyle::GetAppStyleSetName() // Icon Style Set
		)
{
}

void FGroomViewportLODCommands::RegisterCommands()
{
	UI_COMMAND(LODAuto, "LOD Auto", "Automatically select LOD", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(LOD0, "LOD 0", "Force select LOD 0", EUserInterfaceActionType::RadioButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
