// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/PCGGenSourceManager.h"

#include "PCGWorldActor.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"
#include "RuntimeGen/GenSources/PCGGenSourceComponent.h"
#include "RuntimeGen/GenSources/PCGGenSourceEditorCamera.h"
#include "RuntimeGen/GenSources/PCGGenSourcePlayer.h"
#include "RuntimeGen/GenSources/PCGGenSourceWPStreamingSource.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif

FPCGGenSourceManager::FPCGGenSourceManager(const UWorld* InWorld)
{
	// We capture the World so we can differentiate between editor-world and PIE-world Generation Sources.
	World = InWorld;

	// Collect PlayerControllers.
	FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogin);
	FGameModeEvents::GameModeLogoutEvent.AddRaw(this, &FPCGGenSourceManager::OnGameModePostLogout);

#if WITH_EDITOR
	EditorCameraGenSource = NewObject<UPCGGenSourceEditorCamera>();
	EditorCameraGenSource->AddToRoot();
#endif
}

FPCGGenSourceManager::~FPCGGenSourceManager()
{
	FGameModeEvents::GameModePostLoginEvent.RemoveAll(this);
	FGameModeEvents::GameModeLogoutEvent.RemoveAll(this);

#if WITH_EDITOR
	if (EditorCameraGenSource && EditorCameraGenSource->IsRooted())
	{
		EditorCameraGenSource->RemoveFromRoot();
	}
#endif

	for (UPCGGenSourceWPStreamingSource* WorldPartitionGenSource : WorldPartitionGenSources)
	{
		if (WorldPartitionGenSource && WorldPartitionGenSource->IsRooted())
		{
			WorldPartitionGenSource->RemoveFromRoot();
		}
	}
}

TSet<IPCGGenSourceBase*> FPCGGenSourceManager::GetAllGenSources(const APCGWorldActor* InPCGWorldActor)
{
	if (bDirty)
	{
		UpdatePerTickGenSources(InPCGWorldActor);
		bDirty = false;
	}

	TSet<IPCGGenSourceBase*> AllGenSources = RegisteredGenSources;

	// Acquire a generation source for each active streaming source.
	for (int32 I = 0; I < NumWorldPartitionStreamingSources; ++I)
	{
		check(WorldPartitionGenSources.IsValidIndex(I));
		AllGenSources.Add(WorldPartitionGenSources[I]);
	}

#if WITH_EDITOR
	// Acquire a generation source for the active editor viewport if one exists.
	if (EditorCameraGenSource && EditorCameraGenSource->EditorViewportClient)
	{
		AllGenSources.Add(EditorCameraGenSource);
	}
#endif

	return AllGenSources;
}

bool FPCGGenSourceManager::RegisterGenSource(IPCGGenSourceBase* InGenSource)
{
	if (UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	return RegisteredGenSources.Add(InGenSource).IsValidId();
}

bool FPCGGenSourceManager::UnregisterGenSource(const IPCGGenSourceBase* InGenSource)
{
	if (const UPCGGenSourceComponent* GenSourceComponent = Cast<UPCGGenSourceComponent>(InGenSource))
	{
		if (GenSourceComponent->GetWorld() != World)
		{
			return false;
		}
	}

	return RegisteredGenSources.Remove(InGenSource) > 0;
}

void FPCGGenSourceManager::OnGameModePostLogin(AGameModeBase* InGameMode, APlayerController* InPlayerController)
{
	if (InPlayerController == nullptr || InPlayerController->GetWorld() != World)
	{
		return;
	}

	ensure(IsInGameThread());

	UPCGGenSourcePlayer* GenSource = NewObject<UPCGGenSourcePlayer>();
	GenSource->SetPlayerController(InPlayerController);

	FSetElementId ElementId = RegisteredGenSources.Add(GenSource);
	if (ElementId.IsValidId())
	{
		GenSource->AddToRoot();
	}
}

void FPCGGenSourceManager::OnGameModePostLogout(AGameModeBase* InGameMode, AController* InController)
{
	if (InController->GetWorld() != World)
	{
		return;
	}

	ensure(IsInGameThread());

	for (IPCGGenSourceBase* GenSource : RegisteredGenSources)
	{
		if (UPCGGenSourcePlayer* GenSourcePlayer = Cast<UPCGGenSourcePlayer>(GenSource))
		{
			if (GenSourcePlayer->GetPlayerController() == InController || !GenSourcePlayer->IsValid())
			{
				GenSourcePlayer->RemoveFromRoot();
				RegisteredGenSources.Remove(GenSourcePlayer);
				break;
			}
		}
	}
}

void FPCGGenSourceManager::UpdatePerTickGenSources(const APCGWorldActor* InPCGWorldActor)
{
	if (const UWorld* InWorld = InPCGWorldActor->GetWorld())
	{
		if (const UWorldPartition* WorldPartition = InWorld->GetWorldPartition())
		{
			// TODO: Grab StreamingSourceProviders instead of StreamingSources?
			// TODO: Is it possible to avoid adding a StreamingSource for the Player, which we already capture in OnGameModePostLogin?
			// Note: GetStreamingSources only works in GameWorld, so StreamingSources do not act as generation sources in editor.
			const TArray<FWorldPartitionStreamingSource>& StreamingSources = WorldPartition->GetStreamingSources();

			NumWorldPartitionStreamingSources = StreamingSources.Num();

			for (int32 I = 0; I < NumWorldPartitionStreamingSources; ++I)
			{
				const bool bCreateNewWorldPartitionGenSource = I >= WorldPartitionGenSources.Num();

				// Allocate new WP generation sources as necessary.
				UPCGGenSourceWPStreamingSource* GenSource = bCreateNewWorldPartitionGenSource ? WorldPartitionGenSources.Add_GetRef(NewObject<UPCGGenSourceWPStreamingSource>()) : WorldPartitionGenSources[I];

				// Capture each streaming source in a single generation source.
				if (GenSource)
				{
					if (bCreateNewWorldPartitionGenSource)
					{
						// New generation sources need to be rooted.
						GenSource->AddToRoot();
					}

					GenSource->StreamingSource = &StreamingSources[I];
				}
			}
		}
	}

#if WITH_EDITOR
	EditorCameraGenSource->EditorViewportClient = nullptr;

	// Update the active editor viewport client for the EditorCameraGenSource, only if requested by the world actor, in-editor, and the viewport is visible.
	if (InPCGWorldActor->bTreatEditorViewportAsGenerationSource && !World->IsGameWorld())
	{
		if (FViewport* Viewport = GEditor->GetActiveViewport())
		{
			if (FEditorViewportClient* ViewportClient = static_cast<FEditorViewportClient*>(Viewport->GetClient()))
			{
				if (ViewportClient->IsVisible())
				{
					EditorCameraGenSource->EditorViewportClient = ViewportClient;
				}
			}
		}
	}
#endif
}
