// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportLightCardManager.h"
#include "DisplayClusterViewportLightCardManagerProxy.h"
#include "DisplayClusterViewportLightCardResource.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfiguration.h"

#include "ShaderParameters/DisplayClusterShaderParameters_UVLightCards.h"

#include "DisplayClusterLightCardActor.h"
#include "Blueprints/DisplayClusterBlueprintLib.h"

#include "IDisplayClusterShaders.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"

#include "PreviewScene.h"

///////////////////////////////////////////////////////////////////////////////////////////////
/** Console variable used to control the size of the UV light card map texture */
static TAutoConsoleVariable<int32> CVarUVLightCardTextureSize(
	TEXT("nDisplay.render.uvlightcards.UVTextureSize"),
	4096,
	TEXT("The size of the texture UV light cards are rendered to.")
);

///////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportLightCardManager
///////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportLightCardManager::FDisplayClusterViewportLightCardManager(const TSharedRef<const FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration)
	: Configuration(InConfiguration)
	, LightCardManagerProxy(MakeShared<FDisplayClusterViewportLightCardManagerProxy, ESPMode::ThreadSafe>())
{ }

FDisplayClusterViewportLightCardManager::~FDisplayClusterViewportLightCardManager()
{
	Release();
}

void FDisplayClusterViewportLightCardManager::Release()
{
	// The destructor is usually called from the rendering thread, so Release() must be called first from the game thread.
	check(IsInGameThread());

	// Release UVLightCard
	ReleaseUVLightCardData();
	ReleaseUVLightCardResource();
}

///////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportLightCardManager::OnHandleStartScene()
{
}

void FDisplayClusterViewportLightCardManager::OnHandleEndScene()
{
	ReleaseUVLightCardData();
}

void FDisplayClusterViewportLightCardManager::RenderFrame()
{
	UpdateUVLightCardData();
	RenderUVLightCard();
}

///////////////////////////////////////////////////////////////////////////////////////////////
FIntPoint FDisplayClusterViewportLightCardManager::GetUVLightCardResourceSize() const
{
	return UVLightCardResource.IsValid() ? UVLightCardResource->GetSizeXY() : FIntPoint(0, 0);
}

bool FDisplayClusterViewportLightCardManager::IsUVLightCardEnabled() const
{
	return !UVLightCardPrimitiveComponents.IsEmpty();
}

void FDisplayClusterViewportLightCardManager::ReleaseUVLightCardData()
{
	UVLightCardPrimitiveComponents.Empty();
}

void FDisplayClusterViewportLightCardManager::UpdateUVLightCardData()
{
	ReleaseUVLightCardData();

	/** The list of UV light card actors that are referenced by the root actor */
	TArray<ADisplayClusterLightCardActor*> UVLightCardActors;

	if (ADisplayClusterRootActor* SceneRootActorPtr = Configuration->GetRootActor(EDisplayClusterRootActorType::Scene))
	{
		TSet<ADisplayClusterLightCardActor*> LightCards;
		UDisplayClusterBlueprintLib::FindLightCardsForRootActor(SceneRootActorPtr, LightCards);

		for (ADisplayClusterLightCardActor* LightCard : LightCards)
		{
			if (LightCard->bIsUVLightCard)
			{
				UVLightCardActors.Add(LightCard);
			}
		}
	}

	TArray<UMeshComponent*> LightCardMeshComponents;
	for (ADisplayClusterLightCardActor* LightCard : UVLightCardActors)
	{
		if (LightCard->IsHidden() || LightCard->IsActorBeingDestroyed() || LightCard->GetWorld() == nullptr)
		{
			continue;
		}

		LightCardMeshComponents.Empty(LightCardMeshComponents.Num());
		LightCard->GetLightCardMeshComponents(LightCardMeshComponents);

		for (UMeshComponent* LightCardMeshComp : LightCardMeshComponents)
		{
			if (LightCardMeshComp && LightCardMeshComp->SceneProxy == nullptr)
			{
				UVLightCardPrimitiveComponents.Add(LightCardMeshComp);
			}
		}
	}
}

void FDisplayClusterViewportLightCardManager::CreateUVLightCardResource(const FIntPoint& InResourceSize)
{
	UVLightCardResource = MakeShared<FDisplayClusterViewportLightCardResource>(InResourceSize);
	LightCardManagerProxy->UpdateUVLightCardResource(UVLightCardResource);
}

void FDisplayClusterViewportLightCardManager::ReleaseUVLightCardResource()
{
	if (UVLightCardResource.IsValid())
	{
		LightCardManagerProxy->ReleaseUVLightCardResource();
	}

	UVLightCardResource.Reset();
}

void FDisplayClusterViewportLightCardManager::UpdateUVLightCardResource()
{
	const uint32 UVLightCardTextureSize = CVarUVLightCardTextureSize.GetValueOnGameThread();
	const FIntPoint UVLightCardResourceSize = FIntPoint(UVLightCardTextureSize, UVLightCardTextureSize);

	if (UVLightCardResource.IsValid())
	{
		if (UVLightCardResource->GetSizeXY() != UVLightCardResourceSize)
		{
			ReleaseUVLightCardResource();
		}
	}

	if (!UVLightCardResource.IsValid())
	{
		CreateUVLightCardResource(UVLightCardResourceSize);
	}
}

void FDisplayClusterViewportLightCardManager::RenderUVLightCard()
{
	// Render UV LightCard:
	FDisplayClusterViewportManager* ViewportManager = Configuration->GetViewportManagerImpl();
	UWorld* CurrentWorld = Configuration->GetCurrentWorld();
	if (IsUVLightCardEnabled() && CurrentWorld && ViewportManager)
	{
		UpdateUVLightCardResource();

		if (UVLightCardResource.IsValid())
		{
			FDisplayClusterShaderParameters_UVLightCards UVLightCardParameters;
			UVLightCardParameters.ProjectionPlaneSize = ADisplayClusterLightCardActor::UVPlaneDefaultSize;
			UVLightCardParameters.bRenderFinalColor = ViewportManager->ShouldRenderFinalColor();
			UVLightCardParameters.LightCardGamma = 2.2;

			// Store any components that were invisible but forced to be visible so they can be set back to invisible after the render
			TArray<UPrimitiveComponent*> ComponentsToUnload;
			for (UPrimitiveComponent* PrimitiveComponent : UVLightCardPrimitiveComponents)
			{
				// Set the component's visibility to true and force it to generate its scene proxies
				if (!PrimitiveComponent->IsVisible())
				{
					PrimitiveComponent->SetVisibility(true);
					PrimitiveComponent->RecreateRenderState_Concurrent();
					ComponentsToUnload.Add(PrimitiveComponent);
				}

				if (PrimitiveComponent->SceneProxy)
				{
					UVLightCardParameters.PrimitivesToRender.Add(PrimitiveComponent->SceneProxy);
				}
			}

			LightCardManagerProxy->RenderUVLightCard(CurrentWorld->Scene, UVLightCardParameters);

			for (UPrimitiveComponent* LoadedComponent : ComponentsToUnload)
			{
				LoadedComponent->SetVisibility(false);
				LoadedComponent->RecreateRenderState_Concurrent();
			}
		}
	}
	else
	{
		ReleaseUVLightCardResource();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterViewportLightCardManager::AddReferencedObjects(FReferenceCollector& Collector)
{
}
