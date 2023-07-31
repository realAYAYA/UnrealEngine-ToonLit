// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlVisualizerModule.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlComponentVisualizer.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FPhysicsControlModule"

void FPhysicsControlVisualizerModule::StartupModule()
{
	if (GUnrealEd)
	{
		TSharedPtr<FPhysicsControlComponentVisualizer> Visualizer = MakeShared<FPhysicsControlComponentVisualizer>();
		GUnrealEd->RegisterComponentVisualizer(UPhysicsControlComponent::StaticClass()->GetFName(), Visualizer);
		// This call should maybe be inside the RegisterComponentVisualizer call above, but since it's not,
		// we'll put it here.
		Visualizer->OnRegister();
		VisualizersToUnregisterOnShutdown.Add(UPhysicsControlComponent::StaticClass()->GetFName());
	}
}

void FPhysicsControlVisualizerModule::ShutdownModule()
{
	if (GUnrealEd)
	{
		for (const FName& Name : VisualizersToUnregisterOnShutdown)
		{
			GUnrealEd->UnregisterComponentVisualizer(Name);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FPhysicsControlVisualizerModule, PhysicsControl)

