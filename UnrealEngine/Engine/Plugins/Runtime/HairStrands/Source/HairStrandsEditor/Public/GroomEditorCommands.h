// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "Templates/SharedPointer.h"
#include "Framework/Commands/Commands.h"

class FGroomEditorCommands : public TCommands<FGroomEditorCommands>
{
public:

	FGroomEditorCommands();

	TSharedPtr<FUICommandInfo> BeginHairPlaceTool;
	
	TSharedPtr<FUICommandInfo> ResetSimulation;	
	TSharedPtr<FUICommandInfo> PauseSimulation;
	TSharedPtr<FUICommandInfo> PlaySimulation;

	TSharedPtr<FUICommandInfo> PlayAnimation;
	TSharedPtr<FUICommandInfo> StopAnimation;

	TSharedPtr<FUICommandInfo> Simulate;

	TSharedPtr<FUICommandInfo> ViewMode_Lit;
	TSharedPtr<FUICommandInfo> ViewMode_Guide;
	TSharedPtr<FUICommandInfo> ViewMode_GuideInfluence;
	TSharedPtr<FUICommandInfo> ViewMode_UV;
	TSharedPtr<FUICommandInfo> ViewMode_RootUV;
	TSharedPtr<FUICommandInfo> ViewMode_RootUDIM;
	TSharedPtr<FUICommandInfo> ViewMode_Seed;
	TSharedPtr<FUICommandInfo> ViewMode_Dimension;
	TSharedPtr<FUICommandInfo> ViewMode_RadiusVariation;
	TSharedPtr<FUICommandInfo> ViewMode_Tangent;
	TSharedPtr<FUICommandInfo> ViewMode_BaseColor;
	TSharedPtr<FUICommandInfo> ViewMode_Roughness;
	TSharedPtr<FUICommandInfo> ViewMode_ControlPoints;
	TSharedPtr<FUICommandInfo> ViewMode_VisCluster;
	TSharedPtr<FUICommandInfo> ViewMode_Group;

	TSharedPtr<FUICommandInfo> ViewMode_CardsGuides;

	virtual void RegisterCommands() override;
};


class FGroomViewportLODCommands : public TCommands<FGroomViewportLODCommands>
{
public:
	FGroomViewportLODCommands();

	TSharedPtr< FUICommandInfo > LODAuto;
	TSharedPtr< FUICommandInfo > LOD0;

	virtual void RegisterCommands() override;
};
