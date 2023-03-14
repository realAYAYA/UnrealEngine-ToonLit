// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportLightCardManager.h"

#include "DisplayClusterLightCardActor.h"
#include "Blueprints/DisplayClusterBlueprintLib.h"

#include "IDisplayClusterShaders.h"

#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"

#include "PreviewScene.h"
#include "SceneInterface.h"

/** Console variable used to control the size of the UV light card map texture */
static TAutoConsoleVariable<int32> CVarUVLightCardTextureSize(
	TEXT("nDisplay.render.uvlightcards.UVTextureSize"),
	512,
	TEXT("The size of the texture UV light cards are rendered to."));

//-----------------------------------------------------------------------------------------------------------------
// FDisplayClusterLightCardMap
//-----------------------------------------------------------------------------------------------------------------
void FDisplayClusterLightCardMap::InitDynamicRHI()
{
	ETextureCreateFlags CreateFlags = TexCreate_Dynamic;
	CreateFlags |= TexCreate_MultiGPUGraphIgnore;

	const FRHITextureCreateDesc Desc = FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterLightCardMap"))
		.SetExtent(GetSizeX(), GetSizeY())
		.SetFormat(PF_FloatRGBA)
		.SetFlags(CreateFlags | ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource)
		.SetInitialState(ERHIAccess::SRVMask)
		.SetClearValue(FClearValueBinding::Transparent);

	RenderTargetTextureRHI = TextureRHI = RHICreateTexture(Desc);
}

//-----------------------------------------------------------------------------------------------------------------
// FDisplayClusterViewportLightCardManager
//-----------------------------------------------------------------------------------------------------------------
FDisplayClusterViewportLightCardManager::FDisplayClusterViewportLightCardManager(FDisplayClusterViewportManager& InViewportManager)
	: ViewportManager(InViewportManager)
{
	ProxyData = MakeShared<FProxyData, ESPMode::ThreadSafe>();
}

FDisplayClusterViewportLightCardManager::~FDisplayClusterViewportLightCardManager()
{
	Release();

	ProxyData.Reset();
}

void FDisplayClusterViewportLightCardManager::Release()
{
	// The destructor is usually called from the rendering thread, so Release() must be called first from the game thread.
	const bool bIsInRenderingThread = IsInRenderingThread();
	check(!bIsInRenderingThread || (bIsInRenderingThread && PreviewWorld == nullptr));

	// Deleting PreviewScene is only called from the game thread
	DestroyPreviewWorld();

	UVLightCards.Empty();
	ReleaseUVLightCardMap();
}

void FDisplayClusterViewportLightCardManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PreviewWorld);
}

FRHITexture* FDisplayClusterViewportLightCardManager::GetUVLightCardMap_RenderThread() const
{ 
	check(IsInRenderingThread());

	return ProxyData.IsValid() ? ProxyData->GetUVLightCardMap_RenderThread() : nullptr;
}

void FDisplayClusterViewportLightCardManager::UpdateConfiguration()
{
	UVLightCards.Empty();

	if (ADisplayClusterRootActor* RootActorPtr = ViewportManager.GetRootActor())
	{
		TSet<ADisplayClusterLightCardActor*> LightCards;
		UDisplayClusterBlueprintLib::FindLightCardsForRootActor(RootActorPtr, LightCards);

		for (ADisplayClusterLightCardActor* LightCard : LightCards)
		{
			if (LightCard->bIsUVLightCard)
			{
				UVLightCards.Add(LightCard);
			}
		}
	}
}

void FDisplayClusterViewportLightCardManager::HandleStartScene()
{
	InitializePreviewWorld();
}

void FDisplayClusterViewportLightCardManager::HandleEndScene()
{
	DestroyPreviewWorld();
}

void FDisplayClusterViewportLightCardManager::RenderFrame()
{
	if (PreviewWorld && ProxyData.IsValid())
	{
		if (FSceneInterface* SceneInterface = PreviewWorld->Scene)
		{
			InitializeUVLightCardMap();

			/** A list of primitive components that have been added to the preview scene for rendering in the current frame */
			TArray<UPrimitiveComponent*> LoadedPrimitiveComponents;

			TArray<UMeshComponent*> LightCardMeshComponents;

			bool bLoadedPrimitives = false;
			for (ADisplayClusterLightCardActor* LightCard : UVLightCards)
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
						SceneInterface->AddPrimitive(LightCardMeshComp);
						LoadedPrimitiveComponents.Add(LightCardMeshComp);

						bLoadedPrimitives = true;
					}
				}
			}

			ENQUEUE_RENDER_COMMAND(DisplayClusterViewportLightCardManager_RenderFrame)(
				[InProxyData = ProxyData, bLoadedPrimitives, SceneInterface](FRHICommandListImmediate& RHICmdList)
				{
					InProxyData->RenderLightCardMap_RenderThread(RHICmdList, bLoadedPrimitives, SceneInterface);
				});

			for (UPrimitiveComponent* LoadedComponent : LoadedPrimitiveComponents)
			{
				SceneInterface->RemovePrimitive(LoadedComponent);
			}
		}
	}
}

void FDisplayClusterViewportLightCardManager::InitializePreviewWorld()
{
	if (!PreviewWorld)
	{
		FName UniqueWorldName = MakeUniqueObjectName(GetTransientPackage(), UWorld::StaticClass(), FName(TEXT("DisplayClusterLightCardManager_PreviewWorld")));
		PreviewWorld = NewObject<UWorld>(GetTransientPackage(), UniqueWorldName);
		PreviewWorld->WorldType = EWorldType::GamePreview;

		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(PreviewWorld->WorldType);
		WorldContext.SetCurrentWorld(PreviewWorld);

		PreviewWorld->InitializeNewWorld(UWorld::InitializationValues()
			.AllowAudioPlayback(false)
			.CreatePhysicsScene(false)
			.RequiresHitProxies(false)
			.CreateNavigation(false)
			.CreateAISystem(false)
			.ShouldSimulatePhysics(false)
			.SetTransactional(false));
	}
}

void FDisplayClusterViewportLightCardManager::DestroyPreviewWorld()
{
	if (PreviewWorld)
	{
		// Hack to avoid issue where the engine considers this world a leaked object; When UEngine loads a new map, it checks to see if there are any UWorlds
		// still in memory that aren't what it considers "persistent" worlds, worlds with type Inactive or EditorPreview. Even if the UWorld object has been marked for
		// GC and has no references to it, UEngine will still flag it as "leaked" unless it is one of these two types.
		PreviewWorld->WorldType = EWorldType::Inactive;

		GEngine->DestroyWorldContext(PreviewWorld);
		PreviewWorld->DestroyWorld(false);
		PreviewWorld->MarkObjectsPendingKill();
		PreviewWorld = nullptr;
	}
}

void FDisplayClusterViewportLightCardManager::InitializeUVLightCardMap()
{
	const uint32 LightCardTextureSize = CVarUVLightCardTextureSize.GetValueOnAnyThread();
	if (UVLightCardMap && UVLightCardMap->GetSizeX() != LightCardTextureSize)
	{
		ReleaseUVLightCardMap();
	}

	if (UVLightCardMap == nullptr && ProxyData.IsValid())
	{
		UVLightCardMap = new FDisplayClusterLightCardMap(LightCardTextureSize);

		ENQUEUE_RENDER_COMMAND(DisplayClusterViewportLightCardManager_InitializeUVLightCardMap)(
			[InProxyData = ProxyData, InUVLightCardMap = UVLightCardMap](FRHICommandListImmediate& RHICmdList)
			{
				InProxyData->InitializeUVLightCardMap_RenderThread(InUVLightCardMap);
			});
	}
}

void FDisplayClusterViewportLightCardManager::ReleaseUVLightCardMap()
{
	UVLightCardMap = nullptr;

	if (ProxyData.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(DisplayClusterViewportLightCardManager_ReleaseUVLightCardMap)(
			[InProxyData = ProxyData](FRHICommandListImmediate& RHICmdList)
			{
				InProxyData->ReleaseUVLightCardMap_RenderThread();
			});
	}
}

//-----------------------------------------------------------------------------------------------------------------
// FDisplayClusterViewportLightCardManager::FProxyData
//-----------------------------------------------------------------------------------------------------------------
FDisplayClusterViewportLightCardManager::FProxyData::~FProxyData()
{
	ReleaseUVLightCardMap_RenderThread();
}

void FDisplayClusterViewportLightCardManager::FProxyData::InitializeUVLightCardMap_RenderThread(FDisplayClusterLightCardMap* InUVLightCardMap)
{
	if (UVLightCardMap == nullptr)
	{
		// Store a copy of the texture's pointer on the render thread and initialize the texture's resources
		UVLightCardMap = InUVLightCardMap;
		UVLightCardMap->InitResource();
	}
}

void FDisplayClusterViewportLightCardManager::FProxyData::ReleaseUVLightCardMap_RenderThread()
{
	// Release the texture's resources and delete the texture object from the rendering thread
	if (UVLightCardMap)
	{
		UVLightCardMap->ReleaseResource();

		delete UVLightCardMap;
		UVLightCardMap = nullptr;
	}
}

void FDisplayClusterViewportLightCardManager::FProxyData::RenderLightCardMap_RenderThread(FRHICommandListImmediate& RHICmdList, const bool bLoadedPrimitives, FSceneInterface* InSceneInterface)
{
	bHasUVLightCards = bLoadedPrimitives;

	IDisplayClusterShaders& ShadersAPI = IDisplayClusterShaders::Get();
	ShadersAPI.RenderPreprocess_UVLightCards(RHICmdList, InSceneInterface, UVLightCardMap, ADisplayClusterLightCardActor::UVPlaneDefaultSize);
}

FRHITexture* FDisplayClusterViewportLightCardManager::FProxyData::GetUVLightCardMap_RenderThread() const
{
	return (bHasUVLightCards && UVLightCardMap != nullptr) ? UVLightCardMap->GetTextureRHI() : nullptr;
}

