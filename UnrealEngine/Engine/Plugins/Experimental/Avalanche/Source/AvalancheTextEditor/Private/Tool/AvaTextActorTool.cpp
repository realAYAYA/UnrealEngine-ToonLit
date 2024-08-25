// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tool/AvaTextActorTool.h"
#include "AvaInteractiveToolsSettings.h"
#include "AvaTextActor.h"
#include "AvaTextActorFactory.h"
#include "AvaTextEditorCommands.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "IAvalancheComponentVisualizersModule.h"

UAvaTextActorTool::UAvaTextActorTool()
{
	ActorClass = AAvaTextActor::StaticClass();
}

bool UAvaTextActorTool::UseIdentityRotation() const
{
	return ConditionalIdentityRotation();
}

FName UAvaTextActorTool::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryNameActor;
}

FAvaInteractiveToolsToolParameters UAvaTextActorTool::GetToolParameters() const
{
	return {
		FAvaTextEditorCommands::Get().Tool_Actor_Text3D,
		TEXT("Text Actor Tool"),
		2000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaTextActorTool>(InEdMode);
			}),
		ActorClass,
		CreateActorFactory<UAvaTextActorFactory>()
	};
}
