// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterScenePreviewModule.h"

#include "Blueprints/DisplayClusterBlueprintLib.h"
#include "CanvasTypes.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/DisplayClusterPreviewComponent.h"
#include "DisplayClusterLightCardActor.h"
#include "DisplayClusterRootActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/TransactionObjectEvent.h"

#if WITH_EDITOR
#include "LevelEditorViewport.h"
#endif

#define LOCTEXT_NAMESPACE "DisplayClusterScenePreview"

static TAutoConsoleVariable<float> CVarDisplayClusterScenePreviewRenderTickDelay(
	TEXT("nDisplay.ScenePreview.RenderTickDelay"),
	0.1f,
	TEXT("The number of seconds to wait between processing queued renders.")
);

void FDisplayClusterScenePreviewModule::StartupModule()
{
}

void FDisplayClusterScenePreviewModule::ShutdownModule()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RenderTickerHandle);
	RenderTickerHandle.Reset();

	TArray<int32> RendererIds;
	RendererConfigs.GenerateKeyArray(RendererIds);
	for (const int32 RendererId : RendererIds)
	{
		DestroyRenderer(RendererId);
	}
}

int32 FDisplayClusterScenePreviewModule::CreateRenderer()
{
	FRendererConfig& Config = RendererConfigs.Add(NextRendererId);

	Config.Renderer = MakeShared<FDisplayClusterMeshProjectionRenderer>();

	return NextRendererId++;
}

bool FDisplayClusterScenePreviewModule::DestroyRenderer(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		RegisterRootActorEvents(Config->RootActor.Get(), false);
		RendererConfigs.Remove(RendererId);

		RegisterOrUnregisterGlobalActorEvents();
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRootActorPath(int32 RendererId, const FString& ActorPath, bool bAutoUpdateLightcards)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->RootActorPath = ActorPath;

		ADisplayClusterRootActor* RootActor = FindObject<ADisplayClusterRootActor>(nullptr, *ActorPath);
		InternalSetRendererRootActor(*Config, RootActor, bAutoUpdateLightcards);

		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRootActor(int32 RendererId, ADisplayClusterRootActor* Actor, bool bAutoUpdateLightcards)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->RootActorPath.Empty();
		InternalSetRendererRootActor(*Config, Actor, bAutoUpdateLightcards);

		return true;
	}

	return false;
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewModule::GetRendererRootActor(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		return InternalGetRendererRootActor(*Config);
	}

	return nullptr;
}

bool FDisplayClusterScenePreviewModule::GetActorsInRendererScene(int32 RendererId, bool bIncludeRoot, TArray<AActor*>& OutActors)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		if (bIncludeRoot)
		{
			OutActors.Add(GetRendererRootActor(RendererId));
		}

		for (const TWeakObjectPtr<AActor>& Actor : Config->AddedActors)
		{
			if (!Actor.IsValid())
			{
				continue;
			}

			OutActors.Add(Actor.Get());
		}

		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::AddActorToRenderer(int32 RendererId, AActor* Actor)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->AddActor(Actor);
		Config->AddedActors.Add(Actor);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::AddActorToRenderer(int32 RendererId, AActor* Actor, const TFunctionRef<bool(const UPrimitiveComponent*)>& PrimitiveFilter)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->AddActor(Actor, PrimitiveFilter);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::RemoveActorFromRenderer(int32 RendererId, AActor* Actor)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->RemoveActor(Actor);
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::ClearRendererScene(int32 RendererId)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->ClearScene();
		Config->AddedActors.Empty();
		Config->AutoActors.Empty();
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererActorSelectedDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSelection ActorSelectedDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->ActorSelectedDelegate = ActorSelectedDelegate;
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererRenderSimpleElementsDelegate(int32 RendererId, FDisplayClusterMeshProjectionRenderer::FSimpleElementPass RenderSimpleElementsDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->Renderer->RenderSimpleElementsDelegate = RenderSimpleElementsDelegate;
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::SetRendererUsePostProcessTexture(int32 RendererId, bool bUsePostProcessTexture)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		Config->bUsePostProcessTexture = bUsePostProcessTexture;
		return true;
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::Render(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		return InternalRenderImmediate(*Config, RenderSettings, Canvas);
	}

	return false;
}

bool FDisplayClusterScenePreviewModule::RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const FIntPoint& Size, FRenderResultDelegate ResultDelegate)
{
	return InternalRenderQueued(RendererId, RenderSettings, nullptr, Size, ResultDelegate);
}

bool FDisplayClusterScenePreviewModule::RenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, const TWeakPtr<FCanvas> Canvas, FRenderResultDelegate ResultDelegate)
{
	if (!Canvas.IsValid())
	{
		return false;
	}

	FRenderTarget* RenderTarget = Canvas.Pin()->GetRenderTarget();
	if (!RenderTarget)
	{
		return false;
	}

	return InternalRenderQueued(RendererId, RenderSettings, Canvas, RenderTarget->GetSizeXY(), ResultDelegate);
}

bool FDisplayClusterScenePreviewModule::IsRealTimePreviewEnabled() const
{
	return bIsRealTimePreviewEnabled;
}

ADisplayClusterRootActor* FDisplayClusterScenePreviewModule::InternalGetRendererRootActor(FRendererConfig& RendererConfig)
{
	if (!RendererConfig.RootActor.IsValid() && !RendererConfig.RootActorPath.IsEmpty())
	{
		// Try to find the actor by its path
		ADisplayClusterRootActor* RootActor = FindObject<ADisplayClusterRootActor>(nullptr, *RendererConfig.RootActorPath);
		if (RootActor)
		{
			InternalSetRendererRootActor(RendererConfig, RootActor, RendererConfig.bAutoUpdateLightcards);
		}
	}

	return RendererConfig.RootActor.Get();
}

bool FDisplayClusterScenePreviewModule::InternalRenderQueued(int32 RendererId, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, TWeakPtr<FCanvas> Canvas,
	const FIntPoint& Size, FRenderResultDelegate ResultDelegate)
{
	if (FRendererConfig* Config = RendererConfigs.Find(RendererId))
	{
		RenderQueue.Enqueue(FPreviewRenderJob(RendererId, RenderSettings, Size, Canvas, ResultDelegate));

		if (!RenderTickerHandle.IsValid())
		{
			RenderTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateRaw(this, &FDisplayClusterScenePreviewModule::OnTick),
				CVarDisplayClusterScenePreviewRenderTickDelay.GetValueOnGameThread()
			);
		}

		return true;
	}

	return false;
}

void FDisplayClusterScenePreviewModule::InternalSetRendererRootActor(FRendererConfig& RendererConfig, ADisplayClusterRootActor* Actor, bool bAutoUpdateLightcards)
{
	// Determine these values before we update the config's RootActor/bAutoUpdateLightcards
	const bool bRootChanged = RendererConfig.RootActor != Actor;

	if (bRootChanged)
	{
		// Unregister events for the previous cluster
		if (RendererConfig.bAutoUpdateLightcards && RendererConfig.RootActor.IsValid())
		{
			RegisterRootActorEvents(RendererConfig.RootActor.Get(), false);
		}

		RendererConfig.RootActor = Actor;
		RendererConfig.bAutoUpdateLightcards = bAutoUpdateLightcards;
		AutoPopulateScene(RendererConfig);
	}

	RegisterRootActorEvents(Actor, true);
	RegisterOrUnregisterGlobalActorEvents();
}

bool FDisplayClusterScenePreviewModule::InternalRenderImmediate(FRendererConfig& RendererConfig, FDisplayClusterMeshProjectionRenderSettings& RenderSettings, FCanvas& Canvas)
{
	// Update this so that whoever gets the callback can immediately check whether the nDisplay preview may be out of date
	UpdateIsRealTimePreviewEnabled();

	UWorld* World = RendererConfig.RootActor.IsValid() ? RendererConfig.RootActor->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}

	if (RendererConfig.bAutoUpdateLightcards && RendererConfig.bIsSceneDirty)
	{
		AutoPopulateScene(RendererConfig);
	}

#if WITH_EDITOR
	TMap<UDisplayClusterPreviewComponent*, UTexture*> PreviewComponentOverrideTextures;
	const bool bUsePostProcessTexture = RendererConfig.bUsePostProcessTexture && (RenderSettings.RenderType == EDisplayClusterMeshProjectionOutput::Color);

	// Update the preview components with the post-processed texture
	if (bUsePostProcessTexture)
	{
		UDisplayClusterConfigurationData* Config = RendererConfig.RootActor->GetConfigData();

		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodePair : Config->Cluster->Nodes)
		{
			const UDisplayClusterConfigurationClusterNode* Node = NodePair.Value;
			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportPair : Node->Viewports)
			{
				UDisplayClusterPreviewComponent* PreviewComp = RendererConfig.RootActor->GetPreviewComponent(NodePair.Key, ViewportPair.Key);
				if (PreviewComp)
				{
					PreviewComponentOverrideTextures.Add(PreviewComp, PreviewComp->GetOverrideTexture());
					PreviewComp->SetOverrideTexture(PreviewComp->GetRenderTargetTexturePostProcess());
				}
			}
		}
	}
#endif

	RendererConfig.Renderer->Render(&Canvas, World->Scene, RenderSettings);

#if WITH_EDITOR
	// Remove the override texture so the actor renders normally
	if (bUsePostProcessTexture)
	{
		for (const TPair<UDisplayClusterPreviewComponent*, UTexture*>& PreviewPair : PreviewComponentOverrideTextures)
		{
			PreviewPair.Key->SetOverrideTexture(PreviewPair.Value);
		}
	}
#endif

	return true;
}

void FDisplayClusterScenePreviewModule::RegisterOrUnregisterGlobalActorEvents()
{
	// Check whether any of our configs need actor events
	bool bShouldBeRegistered = false;
	for (const TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		if (ConfigPair.Value.bAutoUpdateLightcards)
		{
			bShouldBeRegistered = true;
			break;
		}
	}

#if WITH_EDITOR
	if (bShouldBeRegistered && !bIsRegisteredForActorEvents)
	{
		// Register for events
		FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FDisplayClusterScenePreviewModule::OnActorPropertyChanged);
		FCoreUObjectDelegates::OnObjectTransacted.AddRaw(this, &FDisplayClusterScenePreviewModule::OnObjectTransacted);

		if (GEngine != nullptr)
		{
			GEngine->OnLevelActorDeleted().AddRaw(this, &FDisplayClusterScenePreviewModule::OnLevelActorDeleted);
			GEngine->OnLevelActorAdded().AddRaw(this, &FDisplayClusterScenePreviewModule::OnLevelActorAdded);
		}
	}
	else if (!bShouldBeRegistered && bIsRegisteredForActorEvents)
	{
		// Unregister for events
		FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
		FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

		if (GEngine != nullptr)
		{
			GEngine->OnLevelActorDeleted().RemoveAll(this);
			GEngine->OnLevelActorAdded().RemoveAll(this);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::RegisterRootActorEvents(ADisplayClusterRootActor* Actor, bool bShouldRegister)
{
#if WITH_EDITOR
	if (!Actor)
	{
		return;
	}

	const uint8* GenericThis = reinterpret_cast<uint8*>(this);

	// Register/unregister actors to use post-processing for preview renders so their lighting looks correct,
	// and add/remove preview override so our preview renders have the latest nDisplay preview texture.
	Actor->UnsubscribeFromPostProcessRenderTarget(GenericThis);
	Actor->RemovePreviewEnableOverride(GenericThis);

	if (bShouldRegister)
	{
		Actor->SubscribeToPostProcessRenderTarget(GenericThis);
		Actor->AddPreviewEnableOverride(GenericThis);
	}

	// Register/unregister for Blueprint events
	if (UBlueprint* Blueprint = UBlueprint::GetBlueprintFromClass(Actor->GetClass()))
	{
		Blueprint->OnCompiled().RemoveAll(this);

		if (bShouldRegister)
		{
			Blueprint->OnCompiled().AddRaw(this, &FDisplayClusterScenePreviewModule::OnBlueprintCompiled);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::AutoPopulateScene(FRendererConfig& RendererConfig)
{
	RendererConfig.Renderer->ClearScene();
	RendererConfig.AddedActors.Empty();
	RendererConfig.AutoActors.Empty();

	if (ADisplayClusterRootActor* RootActor = InternalGetRendererRootActor(RendererConfig))
	{
		TArray<FString> ProjectionMeshNames;

		if (UDisplayClusterConfigurationData* Config = RootActor->GetConfigData())
		{
			Config->GetReferencedMeshNames(ProjectionMeshNames);
		}

		RendererConfig.Renderer->AddActor(RootActor, [&ProjectionMeshNames](const UPrimitiveComponent* PrimitiveComponent)
		{
			// Filter out any primitive component that isn't a projection mesh (a static mesh that has a Mesh projection configured for it) or a screen component
			const bool bIsProjectionMesh = PrimitiveComponent->IsA<UStaticMeshComponent>() && ProjectionMeshNames.Contains(PrimitiveComponent->GetName());
			const bool bIsScreen = PrimitiveComponent->IsA<UDisplayClusterScreenComponent>();
			return bIsProjectionMesh || bIsScreen;
		});

		if (RendererConfig.bAutoUpdateLightcards)
		{
			// Automatically add the lightcards found on this actor
			TSet<ADisplayClusterLightCardActor*> LightCards;
			UDisplayClusterBlueprintLib::FindLightCardsForRootActor(RootActor, LightCards);

			TSet<AActor*> Actors;
			Actors.Reserve(LightCards.Num());
			for (ADisplayClusterLightCardActor* LightCard : LightCards)
			{
				Actors.Add(LightCard);
			}

			// Also check for any non-lightcard actors in the world that are valid to control from ICVFX editors
			if (UWorld* World = RootActor->GetWorld())
			{
				for (const TWeakObjectPtr<AActor> WeakActor : TActorRange<AActor>(World))
				{
					if (WeakActor.IsValid() && WeakActor->Implements<UDisplayClusterStageActor>())
					{
						Actors.Add(WeakActor.Get());
					}
				}
			}

			for (AActor* Actor : Actors)
			{
				if (RendererConfig.AddedActors.Contains(Actor))
				{
					continue;
				}

				RendererConfig.Renderer->AddActor(Actor);
				RendererConfig.AddedActors.Add(Actor);
				RendererConfig.AutoActors.Add(Actor);
			}
		}
	}

	RendererConfig.bIsSceneDirty = false;
}

bool FDisplayClusterScenePreviewModule::UpdateIsRealTimePreviewEnabled()
{
#if WITH_EDITOR
	bIsRealTimePreviewEnabled = false;

	if (!GEditor)
	{
		return false;
	}

	for (const FLevelEditorViewportClient* LevelViewport : GEditor->GetLevelViewportClients())
	{
		if (LevelViewport && LevelViewport->IsRealtime())
		{
			bIsRealTimePreviewEnabled = true;
			break;
		}
	}

	return bIsRealTimePreviewEnabled;
#else
	return false;
#endif
}

bool FDisplayClusterScenePreviewModule::OnTick(float DeltaTime)
{
	// This loop should break when we either run out of jobs or complete a single job
	while (!RenderQueue.IsEmpty())
	{
		FPreviewRenderJob Job;
		if (!RenderQueue.Dequeue(Job))
		{
			break;
		}

		ensure(Job.ResultDelegate.IsBound());

		if (FRendererConfig* Config = RendererConfigs.Find(Job.RendererId))
		{
			if (Job.bWasCanvasProvided)
			{
				// We were provided a canvas for this render job, so use it if possible
				if (!Job.Canvas.IsValid())
				{
					Job.ResultDelegate.Execute(nullptr);
					continue;
				}

				TSharedPtr<FCanvas, ESPMode::ThreadSafe> Canvas = Job.Canvas.Pin();
				FRenderTarget* RenderTarget = Canvas->GetRenderTarget();
				if (!RenderTarget)
				{
					Job.ResultDelegate.Execute(nullptr);
					continue;
				}

				InternalRenderImmediate(*Config, Job.Settings, *Canvas);
				Job.ResultDelegate.Execute(RenderTarget);
				break;
			}

			if (UWorld* World = Config->RootActor.IsValid() ? Config->RootActor->GetWorld() : nullptr)
			{
				// We need to provide the render target for this job
				UTextureRenderTarget2D* RenderTarget = Config->RenderTarget.Get();

				if (!RenderTarget)
				{
					// Create a new render target (which will be reused for this config in the future)
					RenderTarget = NewObject<UTextureRenderTarget2D>();
					RenderTarget->InitCustomFormat(Job.Size.X, Job.Size.Y, PF_B8G8R8A8, true);

					Config->RenderTarget = TStrongObjectPtr<UTextureRenderTarget2D>(RenderTarget);
				}
				else if (RenderTarget->SizeX != Job.Size.X || RenderTarget->SizeY != Job.Size.Y)
				{
					// Resize to match the new size
					RenderTarget->ResizeTarget(Job.Size.X, Job.Size.Y);

					// Flush commands so target is immediately ready to render at the new size
					FlushRenderingCommands();
				}

				FTextureRenderTargetResource* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
				FCanvas Canvas(RenderTargetResource, nullptr, FGameTime::GetTimeSinceAppStart(), World->Scene->GetFeatureLevel());

				InternalRenderImmediate(*Config, Job.Settings, Canvas);
				Job.ResultDelegate.Execute(RenderTargetResource);
				break;
			}
			
			// No canvas and no world, so try the next render
		}

		// Config no longer exists, so try the next render
		Job.ResultDelegate.Execute(nullptr);
	}

	if (RenderQueue.IsEmpty())
	{
		RenderTickerHandle.Reset();
		return false;
	}

	return true;
}

void FDisplayClusterScenePreviewModule::OnActorPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent)
{
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		FRendererConfig& Config = ConfigPair.Value;
		if (Config.bAutoUpdateLightcards && Config.RootActor == ObjectBeingModified)
		{
			Config.bIsSceneDirty = true;
		}
	}
}

void FDisplayClusterScenePreviewModule::OnLevelActorDeleted(AActor* Actor)
{
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		FRendererConfig& Config = ConfigPair.Value;
		if (Config.AutoActors.Contains(Actor))
		{
			Config.bIsSceneDirty = true;
		}
	}
}

void FDisplayClusterScenePreviewModule::OnLevelActorAdded(AActor* Actor)
{
	if (!Actor || !Actor->Implements<UDisplayClusterStageActor>())
	{
		return;
	}

	// The actor won't be added to a root actor yet, so we can't check who it belongs to. Easier to just mark all configs as dirty.
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		ConfigPair.Value.bIsSceneDirty = true;
	}
}

void FDisplayClusterScenePreviewModule::OnBlueprintCompiled(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
	{
		FRendererConfig& Config = ConfigPair.Value;
		if (Config.RootActor.IsValid() && Blueprint == UBlueprint::GetBlueprintFromClass(Config.RootActor->GetClass()))
		{
			Config.bIsSceneDirty = true;
			RegisterRootActorEvents(Config.RootActor.Get(), true);
		}
	}
#endif
}

void FDisplayClusterScenePreviewModule::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& TransactionObjectEvent)
{
	if (TransactionObjectEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		for (TPair<int32, FRendererConfig>& ConfigPair : RendererConfigs)
		{
			ConfigPair.Value.bIsSceneDirty = true;
		}
	}
}

IMPLEMENT_MODULE(FDisplayClusterScenePreviewModule, DisplayClusterScenePreview);

#undef LOCTEXT_NAMESPACE
