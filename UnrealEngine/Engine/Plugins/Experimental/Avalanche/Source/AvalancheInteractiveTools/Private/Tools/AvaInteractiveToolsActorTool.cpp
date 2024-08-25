// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorTool.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsActorToolBuilder.h"

UAvaInteractiveToolsActorTool::UAvaInteractiveToolsActorTool()
{
}

FAvaInteractiveToolsToolParameters UAvaInteractiveToolsActorTool::GetToolParameters() const
{
	// This is never really needed, it should work regardless. OOP consistency!
	return UAvaInteractiveToolsActorToolBuilder::CreateToolParameters(
		Category,
		Command,
		Identifier,
		Priority,
		ActorClass,
		GetClass()
	);
}
