// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWidgetComponent.h"

#include "Slate/WidgetRenderer.h"

UDisplayClusterWidgetComponent::UDisplayClusterWidgetComponent()
{
#if WITH_EDITOR
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &UDisplayClusterWidgetComponent::OnWorldCleanup);
#endif
}

UDisplayClusterWidgetComponent::~UDisplayClusterWidgetComponent()
{
#if WITH_EDITOR
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
#endif
}

void UDisplayClusterWidgetComponent::OnRegister()
{
	// Set this prior to registering the scene component so that bounds are calculated correctly.
	CurrentDrawSize = DrawSize;

	UPrimitiveComponent::OnRegister();

#if !UE_SERVER
	FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &ThisClass::OnLevelRemovedFromWorld);

	if ( !IsRunningDedicatedServer() )
	{
		const bool bIsGameWorld = GetWorld()->IsGameWorld();
		if ( Space != EWidgetSpace::Screen )
		{
			if ( CanReceiveHardwareInput() && bIsGameWorld )
			{
				TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
				RegisterHitTesterWithViewport(GameViewportWidget);
			}

			if ( !WidgetRenderer && !GUsingNullRHI )
			{
				WidgetRenderer = new FWidgetRenderer(bApplyGammaCorrection);
			}
		}

		BodySetup = nullptr;

		// No editor init -- optimization
	}
#endif // !UE_SERVER
}

void UDisplayClusterWidgetComponent::OnUnregister()
{
#if !UE_SERVER
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

	if ( GetWorld()->IsGameWorld() )
	{
		TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
		if ( GameViewportWidget.IsValid() )
		{
			UnregisterHitTesterWithViewport(GameViewportWidget);
		}
	}
#endif

	// No editor release -- optimization

	UPrimitiveComponent::OnUnregister();
}

#if WITH_EDITOR
void UDisplayClusterWidgetComponent::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	const UWorld* World = GetWorld();
	
	if (World == InWorld && bCleanupResources && World != nullptr)
	{
		if (!World->IsGameWorld())
		{
			// Prevents crash when a world is being cleaned up and CheckForWorldGCLeaks is called.
			ReleaseResources();
		}
	}
}
#endif
