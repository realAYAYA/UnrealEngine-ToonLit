// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskSVE.h"

#include "GeometryMaskModule.h"
#include "GeometryMaskSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

FGeometryMaskSceneViewExtension::FGeometryMaskSceneViewExtension(
	const FAutoRegister& AutoRegister,
	UWorld* InWorld)
	: FWorldSceneViewExtension(AutoRegister, InWorld)
{
	UE_LOG(LogGeometryMask, VeryVerbose, TEXT("SVE registered for world: %s"), *InWorld->GetName());
	
	GeometryMaskSubsystemWeak = GEngine->GetEngineSubsystem<UGeometryMaskSubsystem>();
}

void FGeometryMaskSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	if (UGeometryMaskSubsystem* Subsystem = GeometryMaskSubsystemWeak.Get())
	{
		Subsystem->Update(GetWorld(), InViewFamily);
	}
}

bool FGeometryMaskSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bool bIsActive = FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	if (!bIsActive)
	{
		return false;
	}

#if WITH_EDITOR
	if (GEditor)
	{
		if (GEditor->IsSimulatingInEditor())
		{
			if (Context.GetWorld()->WorldType == EWorldType::Editor)
			{
				return true;
			}
		}
		
		if (GEditor->PlayWorld)
		{
			bIsActive = Context.GetWorld()->WorldType == EWorldType::PIE;
		}
		else
		{
			bIsActive = Context.GetWorld()->WorldType == EWorldType::Editor || GetWorld()->WorldType == EWorldType::Game;
		}
	}
#endif

	return bIsActive;
}
