// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorToolNull.h"
#include "AvaInteractiveToolsCommands.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Framework/AvaNullActor.h"

UAvaInteractiveToolsActorToolNull::UAvaInteractiveToolsActorToolNull()
{
	ActorClass = AAvaNullActor::StaticClass();
}

FName UAvaInteractiveToolsActorToolNull::GetCategoryName()
{
	return FAvalancheInteractiveToolsModule::CategoryNameActor;
}

FAvaInteractiveToolsToolParameters UAvaInteractiveToolsActorToolNull::GetToolParameters() const
{
	return {
		FAvaInteractiveToolsCommands::Get().Tool_Actor_Null,
		TEXT("Null Actor Tool"),
		1000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaInteractiveToolsActorToolNull>(InEdMode);
			}),
		ActorClass
	};
}
