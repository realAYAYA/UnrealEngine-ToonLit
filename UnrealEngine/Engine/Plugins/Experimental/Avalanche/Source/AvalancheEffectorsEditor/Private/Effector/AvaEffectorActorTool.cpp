// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEffectorActorTool.h"
#include "AvaEffectorsEditorCommands.h"
#include "AvaInteractiveToolsSettings.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Effector/CEEffectorActor.h"
#include "Effector/CEEffectorActorFactory.h"

UAvaEffectorActorTool::UAvaEffectorActorTool()
{
	ActorClass = ACEEffectorActor::StaticClass();
}

bool UAvaEffectorActorTool::UseIdentityRotation() const
{
	return ConditionalIdentityRotation();
}

FName UAvaEffectorActorTool::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryNameActor;
}

FAvaInteractiveToolsToolParameters UAvaEffectorActorTool::GetToolParameters() const
{
	return {
		FAvaEffectorsEditorCommands::Get().Tool_Actor_Effector,
		TEXT("Effector Actor Tool"),
		4000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaEffectorActorTool>(InEdMode);
			}),
		ActorClass,
		CreateActorFactory<UCEEffectorActorFactory>()
	};
}
