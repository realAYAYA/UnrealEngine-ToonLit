// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfiguration.h"

#include "DisplayClusterViewportConfiguration_Viewport.h"
#include "DisplayClusterViewportConfiguration_ViewportManager.h"
#include "DisplayClusterViewportConfiguration_Postprocess.h"
#include "DisplayClusterViewportConfiguration_ProjectionPolicy.h"
#include "DisplayClusterViewportConfiguration_ICVFX.h"
#include "DisplayClusterViewportConfiguration_ICVFXCamera.h"
#include "DisplayClusterViewportConfiguration_Tile.h"

#include "DisplayClusterViewportConfigurationHelpers_RenderFrameSettings.h"

#include "DisplayClusterConfigurationStrings.h"

#include "DisplayClusterRootActor.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/DisplayClusterViewportManagerProxy.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"

#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrameSettings.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"

#include "Render/IPDisplayClusterRenderManager.h"
#include "Misc/DisplayClusterLog.h"

#include "Engine/RendererSettings.h"

namespace UE::DisplayCluster::Configuration
{
	static inline ADisplayClusterRootActor* ImplGetRootActor(const FDisplayClusterActorRef& InConfigurationRootActorRef)
	{
		if (AActor* ActorPtr = InConfigurationRootActorRef.GetOrFindSceneActor())
		{
			if (ActorPtr->IsA<ADisplayClusterRootActor>())
			{
				return static_cast<ADisplayClusterRootActor*>(ActorPtr);
			}
		}

		return nullptr;
	}

	static inline bool ImplIsChangedRootActor(const ADisplayClusterRootActor* InRootActor, FDisplayClusterActorRef& InOutConfigurationRootActorRef)
	{
		const bool bIsDefined = InOutConfigurationRootActorRef.IsDefinedSceneActor();
		if (InRootActor == nullptr)
		{
			return bIsDefined;
		}

		if (!bIsDefined || ImplGetRootActor(InOutConfigurationRootActorRef) != InRootActor)
		{
			return true;
		}

		return false;
	}

	static inline void ImplSetRootActor(const ADisplayClusterRootActor* InRootActor, FDisplayClusterActorRef& InOutConfigurationRootActorRef)
	{
		if (InRootActor == nullptr)
		{
			InOutConfigurationRootActorRef.ResetSceneActor();
		}
		else
		{
			InOutConfigurationRootActorRef.SetSceneActor(InRootActor);
		}
	}
};

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfiguration
///////////////////////////////////////////////////////////////////
FDisplayClusterViewportConfiguration::FDisplayClusterViewportConfiguration()
	: Proxy(MakeShared<FDisplayClusterViewportConfigurationProxy, ESPMode::ThreadSafe>())
{ }

FDisplayClusterViewportConfiguration::~FDisplayClusterViewportConfiguration()
{ }

void FDisplayClusterViewportConfiguration::Initialize(FDisplayClusterViewportManager& InViewportManager)
{
	// Set weak refs to viewport manager and proxy
	ViewportManagerWeakPtr = InViewportManager.AsShared();
	Proxy->Initialize_GameThread(InViewportManager.GetViewportManagerProxy());
}

void FDisplayClusterViewportConfiguration::SetRootActor(const EDisplayClusterRootActorType InRootActorType, const ADisplayClusterRootActor* InRootActor)
{
	check(IsInGameThread());

	using namespace UE::DisplayCluster::Configuration;

	// COllect all required RootActor refs changes
	EDisplayClusterRootActorType RootActorChanges = (EDisplayClusterRootActorType)0;
	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Preview) && ImplIsChangedRootActor(InRootActor, PreviewRootActorRef))
	{
		EnumAddFlags(RootActorChanges, EDisplayClusterRootActorType::Preview);
	}
	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Scene) && ImplIsChangedRootActor(InRootActor, SceneRootActorRef))
	{
		EnumAddFlags(RootActorChanges, EDisplayClusterRootActorType::Scene);
	}
	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Configuration) && ImplIsChangedRootActor(InRootActor, ConfigurationRootActorRef))
	{
		EnumAddFlags(RootActorChanges, EDisplayClusterRootActorType::Configuration);
	}

	if (RootActorChanges != (EDisplayClusterRootActorType)0)
	{

		if (IsSceneOpened())
		{
			if (FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl())
			{
				ViewportManager->HandleEndScene();

				// Always reset entore cluster preview rendering when RootActor chagned
				ViewportManager->GetViewportManagerPreview().ResetEntireClusterPreviewRendering();
			}
		}

		// Change RootActor
		if (EnumHasAnyFlags(RootActorChanges, EDisplayClusterRootActorType::Preview))
		{
			ImplSetRootActor(InRootActor, PreviewRootActorRef);
		}
		if (EnumHasAnyFlags(RootActorChanges, EDisplayClusterRootActorType::Scene))
		{
			ImplSetRootActor(InRootActor, SceneRootActorRef);
		}
		if (EnumHasAnyFlags(RootActorChanges, EDisplayClusterRootActorType::Configuration))
		{
			ImplSetRootActor(InRootActor, ConfigurationRootActorRef);
		}
	}
}

ADisplayClusterRootActor* FDisplayClusterViewportConfiguration::GetRootActor(const EDisplayClusterRootActorType InRootActorType) const
{
	using namespace UE::DisplayCluster::Configuration;

	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Preview))
	{
		if (ADisplayClusterRootActor* OutRootActor = ImplGetRootActor(PreviewRootActorRef))
		{
			return OutRootActor;
		}
	}

	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Scene))
	{
		if (ADisplayClusterRootActor* OutRootActor = ImplGetRootActor(SceneRootActorRef))
		{
			return OutRootActor;
		}
	}

	if (EnumHasAnyFlags(InRootActorType, EDisplayClusterRootActorType::Configuration))
	{
		if (ADisplayClusterRootActor* OutRootActor = ImplGetRootActor(ConfigurationRootActorRef))
		{
			return OutRootActor;
		}
	}

	return nullptr;
}

void FDisplayClusterViewportConfiguration::OnHandleStartScene()
{
	bCurrentSceneActive = true;
}

void FDisplayClusterViewportConfiguration::OnHandleEndScene()
{
	bCurrentSceneActive = false;
	bCurrentRenderFrameViewportsNeedsToBeUpdated = true;

	// Always reset world ptr at the end of the scene
	CurrentWorldRef.Reset();
}

void FDisplayClusterViewportConfiguration::SetCurrentWorldImpl(const UWorld* InWorld)
{
	if (GetCurrentWorld() != InWorld)
	{
		if (IsSceneOpened())
		{
			if (FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl())
			{
				ViewportManager->HandleEndScene();
			}
		}

		// Ignore const UWorld
		CurrentWorldRef = (UWorld*)InWorld;
	}
}

UWorld* FDisplayClusterViewportConfiguration::GetCurrentWorld() const
{
	check(IsInGameThread());

	if (!CurrentWorldRef.IsValid() || CurrentWorldRef.IsStale())
	{
		return nullptr;
	}

	return CurrentWorldRef.Get();
}

const UDisplayClusterConfigurationData* FDisplayClusterViewportConfiguration::GetConfigurationData() const
{
	ADisplayClusterRootActor* ConfigurationRootActor = GetRootActor(EDisplayClusterRootActorType::Configuration);

	return ConfigurationRootActor ? ConfigurationRootActor->GetConfigData() : nullptr;
}

const FDisplayClusterConfigurationICVFX_StageSettings* FDisplayClusterViewportConfiguration::GetStageSettings() const
{
	if (const UDisplayClusterConfigurationData* ConfigurationData = GetConfigurationData())
	{
		return &ConfigurationData->StageSettings;
	}

	return nullptr;
}

const FDisplayClusterConfigurationRenderFrame* FDisplayClusterViewportConfiguration::GetConfigurationRenderFrameSettings() const
{
	if (const UDisplayClusterConfigurationData* ConfigurationData = GetConfigurationData())
	{
		return &ConfigurationData->RenderFrameSettings;
	}

	return nullptr;
}

bool FDisplayClusterViewportConfiguration::IsCurrentWorldHasAnyType(const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2, const EWorldType::Type InWorldType3) const
{
	if (UWorld* CurrentWorld = GetCurrentWorld())
	{
		return (CurrentWorld->WorldType == InWorldType1 && InWorldType1 != EWorldType::None)
			|| (CurrentWorld->WorldType == InWorldType2 && InWorldType2 != EWorldType::None)
			|| (CurrentWorld->WorldType == InWorldType3 && InWorldType3 != EWorldType::None);
	}

	return false;
}

bool FDisplayClusterViewportConfiguration::IsRootActorWorldHasAnyType(const EDisplayClusterRootActorType InRootActorType, const EWorldType::Type InWorldType1, const EWorldType::Type InWorldType2, const EWorldType::Type InWorldType3) const
{
	if (ADisplayClusterRootActor* RootActor = GetRootActor(InRootActorType))
	{
		if (UWorld* CurrentWorld = RootActor->GetWorld())
		{
			return (CurrentWorld->WorldType == InWorldType1 && InWorldType1 != EWorldType::None)
				|| (CurrentWorld->WorldType == InWorldType2 && InWorldType2 != EWorldType::None)
				|| (CurrentWorld->WorldType == InWorldType3 && InWorldType3 != EWorldType::None);
		}
	}

	return false;
}

const float FDisplayClusterViewportConfiguration::GetWorldToMeters() const
{
	// Get world scale
	float OutWorldToMeters = 100.f;
	if (UWorld* World = GetCurrentWorld())
	{
		if (AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			OutWorldToMeters = WorldSettings->WorldToMeters;
		}
	}

	return OutWorldToMeters;
}

EDisplayClusterRenderFrameMode FDisplayClusterViewportConfiguration::GetRenderModeForPIE() const
{
#if WITH_EDITOR
	if (ADisplayClusterRootActor* PreviewRootActor = GetRootActor(EDisplayClusterRootActorType::Preview))
	{
		switch (PreviewRootActor->RenderMode)
		{
		case EDisplayClusterConfigurationRenderMode::SideBySide:
			return EDisplayClusterRenderFrameMode::PIE_SideBySide;

		case EDisplayClusterConfigurationRenderMode::TopBottom:
			return EDisplayClusterRenderFrameMode::PIE_TopBottom;

		default:
			break;
		}
	}
#endif

	return EDisplayClusterRenderFrameMode::PIE_Mono;
}

IDisplayClusterViewportManager* FDisplayClusterViewportConfiguration::GetViewportManager() const
{
	return GetViewportManagerImpl();
}

void FDisplayClusterViewportConfiguration::ReleaseConfiguration()
{
	if (FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl())
	{
		ViewportManager->HandleEndScene();
		ViewportManager->ReleaseTextures();
	}

	RenderFrameSettings = FDisplayClusterRenderFrameSettings();
}

bool FDisplayClusterViewportConfiguration::ImplUpdateConfiguration(EDisplayClusterRenderFrameMode InRenderMode, const UWorld* InWorld, const FString& InClusterNodeId, const TArray<FString>* InViewportNames)
{
	check(IsInGameThread());

	FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl();
	if (!ViewportManager || !FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::UpdateRenderFrameConfiguration(ViewportManager, InRenderMode, *this))
	{
		return false;
	}

	if (!InWorld)
	{
		// The world is required
		return false;
	}

	// The current world should always be initialized immediately before updating the configuration, since there is a dependency (e.g., visibility lists, etc.)
	SetCurrentWorldImpl(InWorld);

	if (!IsSceneOpened())
	{
		// Before render we need to start scene
		ViewportManager->HandleStartScene();
	}

	FDisplayClusterViewportConfiguration_ViewportManager  ConfigurationViewportManager(*this);
	FDisplayClusterViewportConfiguration_Postprocess      ConfigurationPostprocess(*this);
	FDisplayClusterViewportConfiguration_ProjectionPolicy ConfigurationProjectionPolicy(*this);
	FDisplayClusterViewportConfiguration_ICVFX            ConfigurationICVFX(*this);
	FDisplayClusterViewportConfiguration_Tile             ConfigurationTile(*this);

	// when InClusterNodeId==PreviewNodeAll, means that it is an undefined cluster node
	const FString ClusterNodeId = InClusterNodeId == DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll ? TEXT("") : InClusterNodeId;

	if (InViewportNames)
	{
		ConfigurationViewportManager.UpdateCustomViewports(*InViewportNames);

		// Do not use the cluster node name for this pass type
		SetClusterNodeId(FString());
	}
	else
	{
		ConfigurationViewportManager.UpdateClusterNodeViewports(ClusterNodeId);
		// Use only valid values of cluster node id
		SetClusterNodeId(ClusterNodeId);

	}

	ConfigurationICVFX.Update();
	ConfigurationTile.Update();

	ConfigurationProjectionPolicy.Update();

	ConfigurationICVFX.PostUpdate();
	ConfigurationTile.PostUpdate();

	ImplUpdateConfigurationVisibility();

	if (!InViewportNames)
	{
		// Update postprocess for current cluster node
		ConfigurationPostprocess.UpdateClusterNodePostProcess(ClusterNodeId);
	}

	FDisplayClusterViewportConfigurationHelpers_RenderFrameSettings::PostUpdateRenderFrameConfiguration(*this);

	return true;
}

void FDisplayClusterViewportConfiguration::ImplUpdateConfigurationVisibility() const
{
	ADisplayClusterRootActor* SceneRootActor = GetRootActor(EDisplayClusterRootActorType::Scene);
	FDisplayClusterViewportManager* ViewportManager = GetViewportManagerImpl();

	// Hide root actor components for all viewports
	TSet<FPrimitiveComponentId> RootActorHidePrimitivesList;
	if (ViewportManager && SceneRootActor && SceneRootActor->GetHiddenInGamePrimitives(RootActorHidePrimitivesList))
	{
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager->ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				ViewportIt->GetVisibilitySettingsImpl().SetRootActorHideList(RootActorHidePrimitivesList);
			}
		}
	}
}
