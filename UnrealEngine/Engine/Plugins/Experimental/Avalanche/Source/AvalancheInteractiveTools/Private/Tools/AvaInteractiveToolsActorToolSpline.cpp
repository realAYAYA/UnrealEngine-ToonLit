// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/AvaInteractiveToolsActorToolSpline.h"
#include "AvaInteractiveToolsCommands.h"
#include "AvalancheInteractiveToolsModule.h"
#include "Builders/AvaInteractiveToolsToolBuilder.h"
#include "Framework/AvaSplineActor.h"

UAvaInteractiveToolsActorToolSpline::UAvaInteractiveToolsActorToolSpline()
{
	ActorClass = AAvaSplineActor::StaticClass();
}

FName UAvaInteractiveToolsActorToolSpline::GetCategoryName()
{
	return FAvalancheInteractiveToolsModule::CategoryNameActor;
}

FAvaInteractiveToolsToolParameters UAvaInteractiveToolsActorToolSpline::GetToolParameters() const
{
	return {
		FAvaInteractiveToolsCommands::Get().Tool_Actor_Spline,
		TEXT("Spline Actor Tool"),
		5000,
		FAvalancheInteractiveToolsCreateBuilder::CreateLambda(
			[](UEdMode* InEdMode)
			{
				return UAvaInteractiveToolsToolBuilder::CreateToolBuilder<UAvaInteractiveToolsActorToolSpline>(InEdMode);
			}),
		ActorClass
	};
}
