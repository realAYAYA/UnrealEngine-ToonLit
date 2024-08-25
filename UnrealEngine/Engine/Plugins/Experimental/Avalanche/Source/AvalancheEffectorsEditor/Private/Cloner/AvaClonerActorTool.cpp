// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaClonerActorTool.h"
#include "AvaEffectorsEditorCommands.h"
#include "AvaInteractiveToolsSettings.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Cloner/CEClonerActor.h"
#include "Cloner/CEClonerActorFactory.h"

UAvaClonerActorTool::UAvaClonerActorTool()
{
	ActorClass = ACEClonerActor::StaticClass();
}

bool UAvaClonerActorTool::UseIdentityRotation() const
{
	return ConditionalIdentityRotation();
}

FName UAvaClonerActorTool::GetCategoryName()
{
	return IAvalancheInteractiveToolsModule::CategoryNameActor;
}

FAvaInteractiveToolsToolParameters UAvaClonerActorTool::GetToolParameters() const
{
	return {
		FAvaEffectorsEditorCommands::Get().Tool_Actor_Cloner,
		TEXT("Clone Actor Tool"),
		3000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaClonerActorTool>(InEdMode);
			}),
		ActorClass,
		CreateActorFactory<UCEClonerActorFactory>()
	};
}
