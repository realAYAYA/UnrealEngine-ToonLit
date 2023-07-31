// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterRootActor.h"

#include "Blueprints/DisplayClusterBlueprint.h"
#include "Components/SceneComponent.h"
#include "Components/DisplayClusterOriginComponent.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Components/DisplayClusterSceneComponentSyncParent.h"
#include "Components/DisplayClusterPreviewComponent.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Components/LineBatchComponent.h"

#include "Config/IPDisplayClusterConfigManager.h"
#include "DisplayClusterConfigurationStrings.h"

#include "IDisplayClusterConfiguration.h"
#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterPlayerInput.h"

#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "Render/Viewport/Configuration/DisplayClusterViewportConfigurationHelpers_ICVFX.h"
#include "Render/Viewport/DisplayClusterViewportStrings.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"
#include "Render/Viewport/RenderFrame/DisplayClusterRenderFrame.h"

#include "Engine/TextureRenderTarget2D.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IN-EDITOR STUFF
//////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR

#include "Async/Async.h"
#include "LevelEditor.h"
#include "EditorSupportDelegates.h"

//////////////////////////////////////////////////////////////////////////////////////////////
int32 GDisplayClusterPreviewAllowMultiGPURendering = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewAllowMultiGPURendering(
	TEXT("DC.Preview.AllowMultiGPURendering"),
	GDisplayClusterPreviewAllowMultiGPURendering,
	TEXT("Allow mGPU for preview rendering (0 == disabled)"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewMultiGPURenderingMinIndex = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewMultiGPURenderingMinIndex(
	TEXT("DC.Preview.MultiGPURenderingMinIndex"),
	GDisplayClusterPreviewMultiGPURenderingMinIndex,
	TEXT("Distribute mGPU render on GPU from #min to #max indices"),
	ECVF_RenderThreadSafe
);

int32 GDisplayClusterPreviewMultiGPURenderingMaxIndex = 0;
static FAutoConsoleVariableRef CVarDisplayClusterPreviewMultiGPURenderingMaxIndex(
	TEXT("DC.Preview.MultiGPURenderingMaxIndex"),
	GDisplayClusterPreviewMultiGPURenderingMaxIndex,
	TEXT("Distribute mGPU render on GPU from #min to #max indices"),
	ECVF_RenderThreadSafe
);

//////////////////////////////////////////////////////////////////////////////////////////////
// ADisplayClusterRootActor
//////////////////////////////////////////////////////////////////////////////////////////////
void ADisplayClusterRootActor::ResetPreviewInternals_Editor()
{
	// Reset preview components before DCRA rebuild
	ResetPreviewComponents_Editor(true);

	PreviewRenderFrame.Reset();

	TickPerFrameCounter = 0;
	PreviewClusterNodeIndex = 0;
	PreviewViewportIndex = 0;
}

void ADisplayClusterRootActor::Constructor_Editor()
{
	// Allow tick in editor for preview rendering
	PrimaryActorTick.bStartWithTickEnabled = true;

	ResetPreviewInternals_Editor();

	FCoreUObjectDelegates::OnPackageReloaded.AddUObject(this, &ADisplayClusterRootActor::HandleAssetReload);
}

void ADisplayClusterRootActor::Destructor_Editor()
{
	OnPreviewGenerated.Unbind();
	OnPreviewDestroyed.Unbind();

	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);
}

void ADisplayClusterRootActor::Tick_Editor(float DeltaSeconds)
{
	if (!IsPreviewEnabled())
	{
		ResetPreviewInternals_Editor();
	}
	else if (!IsRunningGameOrPIE())
	{
		if (bDeferPreviewGeneration)
		{
			// Hack to generate preview components on instances during map load.
			// TODO: See if we can move InitializeRootActor out of PostLoad.
			bDeferPreviewGeneration = false;
			UpdatePreviewComponents();
		}
		
		// Update preview RTTs correspond to 'TickPerFrame' value
		if (++TickPerFrameCounter >= TickPerFrame)
		{
			TickPerFrameCounter = 0;

			// Render viewport for preview material RTTs
			if (ViewportManager.IsValid())
			{
				ImplRenderPreview_Editor();
			}
		}

		// preview frustums on each tick
		ImplRenderPreviewFrustums_Editor();
	}
}

void ADisplayClusterRootActor::PostActorCreated_Editor()
{
	ResetPreviewInternals_Editor();
}

void ADisplayClusterRootActor::PostLoad_Editor()
{
	// Generating the preview on load for instances in the world can't be done on PostLoad, components may not have loaded flags present.
	bDeferPreviewGeneration = true;

	ResetPreviewInternals_Editor();
}
void ADisplayClusterRootActor::BeginDestroy_Editor()
{
	ResetPreviewInternals_Editor();

	ReleasePreviewComponents();

	OnPreviewGenerated.Unbind();
	OnPreviewDestroyed.Unbind();
	bDeferPreviewGeneration = true;
}

void ADisplayClusterRootActor::RerunConstructionScripts_Editor()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ADisplayClusterRootActor::RerunConstructionScripts_Editor"), STAT_RerunConstructionScripts_Editor, STATGROUP_NDisplay);
	
	/* We need to reinitialize since our components are being regenerated here. */
	InitializeRootActor();

	UpdateInnerFrustumPriority();

	// Reset preview components before DCRA rebuild
	ResetPreviewComponents_Editor(false);
}

void ADisplayClusterRootActor::EnableEditorRender(bool bValue)
{
	bEnableEditorRender = bValue;
}

bool ADisplayClusterRootActor::IsPreviewEnabled() const
{
	if (bMoviePipelineRenderPass)
	{
		// Disable preview rendering for MRQ
		return false;
	}

	//@todo: (GUI) Scene preview can be disabled when the configuration window with internal preview is open.
#if 0
	bool bIsScenePreview = true; //@todo: handle GUI logic
	bool bIsConfigurationPreviewUsed = false; //@todo: handle GUI logic

	if (bIsScenePreview == bIsConfigurationPreviewUsed)
	{
		return false;
	}
#endif

	if (!PreviewEnableOverriders.IsEmpty())
	{
		return true;
	}

	return bPreviewEnable;
}

// Return all RTT RHI resources for preview
void ADisplayClusterRootActor::GetPreviewRenderTargetableTextures(const TArray<FString>& InViewportNames, TArray<FTextureRHIRef>& OutTextures)
{
	check(IsInGameThread());

	for(const TPair<FString, TObjectPtr<UDisplayClusterPreviewComponent>>& PreviewComponentIt : PreviewComponents)
	{
		if (PreviewComponentIt.Value)
		{
			const int32 OutTextureIndex = InViewportNames.Find(PreviewComponentIt.Value->GetViewportId());
			if (OutTextureIndex != INDEX_NONE)
			{
				// Add scope for func GetRenderTargetTexture()
				if (UTextureRenderTarget2D* RenderTarget2D = ShouldThisFrameOutputPreviewToPostProcessRenderTarget() ?
					PreviewComponentIt.Value->GetRenderTargetTexturePostProcess() : PreviewComponentIt.Value->GetRenderTargetTexture())
				{
					FTextureRenderTargetResource* DstRenderTarget = RenderTarget2D->GameThread_GetRenderTargetResource();
					if (DstRenderTarget != nullptr)
					{
						OutTextures[OutTextureIndex] = DstRenderTarget->TextureRHI;
					}
				}
			}
		}
	}
}

void ADisplayClusterRootActor::UpdateInnerFrustumPriority()
{
	if (InnerFrustumPriority.Num() == 0)
	{
		ResetInnerFrustumPriority();
		return;
	}
	
	TArray<UDisplayClusterICVFXCameraComponent*> Components;
	GetComponents(Components);

	TArray<FString> ValidCameras;
	for (UDisplayClusterICVFXCameraComponent* Camera : Components)
	{
		FString CameraName = Camera->GetName();
		InnerFrustumPriority.AddUnique(CameraName);
		ValidCameras.Add(CameraName);
	}

	// Removes invalid cameras or duplicate cameras.
	InnerFrustumPriority.RemoveAll([ValidCameras, this](const FDisplayClusterComponentRef& CameraRef)
	{
		return !ValidCameras.Contains(CameraRef.Name) || InnerFrustumPriority.FilterByPredicate([CameraRef](const FDisplayClusterComponentRef& CameraRefN2)
		{
			return CameraRef == CameraRefN2;
		}).Num() > 1;
	});
	
	for (int32 Idx = 0; Idx < InnerFrustumPriority.Num(); ++Idx)
	{
		if (UDisplayClusterICVFXCameraComponent* CameraComponent = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *InnerFrustumPriority[Idx].Name))
		{
			CameraComponent->CameraSettings.RenderSettings.RenderOrder = Idx;
		}
	}
}

void ADisplayClusterRootActor::ResetInnerFrustumPriority()
{
	TArray<UDisplayClusterICVFXCameraComponent*> Components;
	GetComponents(Components);

	InnerFrustumPriority.Reset(Components.Num());
	for (UDisplayClusterICVFXCameraComponent* Camera : Components)
	{
		InnerFrustumPriority.Add(Camera->GetName());
	}
	
	// Initialize based on current render priority.
	InnerFrustumPriority.Sort([this](const FDisplayClusterComponentRef& CameraA, const FDisplayClusterComponentRef& CameraB)
	{
		UDisplayClusterICVFXCameraComponent* CameraComponentA = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *CameraA.Name);
		UDisplayClusterICVFXCameraComponent* CameraComponentB = FindObjectFast<UDisplayClusterICVFXCameraComponent>(this, *CameraB.Name);

		if (CameraComponentA && CameraComponentB)
		{
			return CameraComponentA->CameraSettings.RenderSettings.RenderOrder < CameraComponentB->CameraSettings.RenderSettings.RenderOrder;
		}

		return false;
	});
}

bool ADisplayClusterRootActor::IsSelectedInEditor() const
{
	return bIsSelectedInEditor || Super::IsSelectedInEditor();
}

void ADisplayClusterRootActor::SetIsSelectedInEditor(bool bValue)
{
	bIsSelectedInEditor = bValue;
}

IDisplayClusterViewport* ADisplayClusterRootActor::FindPreviewViewport(const FString& InViewportId) const
{
	if (ViewportManager.IsValid())
	{
		return ViewportManager->FindViewport(InViewportId);
	}

	return nullptr;
}

bool ADisplayClusterRootActor::ImplUpdatePreviewConfiguration_Editor(const FString& InClusterNodeId)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ADisplayClusterRootActor::ImplUpdatePreviewConfiguration_Editor"), STAT_ImplUpdatePreviewConfiguration_Editor, STATGROUP_NDisplay);
	
	PreviewRenderFrame.Reset();

	if (IsPreviewEnabled() && ViewportManager.IsValid())
	{
		FDisplayClusterPreviewSettings PreviewSettings;
		PreviewSettings.PreviewRenderTargetRatioMult = PreviewRenderTargetRatioMult;

		PreviewSettings.bFreezePreviewRender = bFreezePreviewRender;
		PreviewSettings.bPreviewEnablePostProcess = ShouldThisFrameOutputPreviewToPostProcessRenderTarget();

		PreviewSettings.PreviewMaxTextureDimension = PreviewMaxTextureDimension;
		
		PreviewSettings.bAllowMultiGPURenderingInEditor = GDisplayClusterPreviewAllowMultiGPURendering != 0;
		PreviewSettings.MinGPUIndex = FMath::Max(0, GDisplayClusterPreviewMultiGPURenderingMinIndex);
		PreviewSettings.MaxGPUIndex = FMath::Max(0, GDisplayClusterPreviewMultiGPURenderingMaxIndex);

		return ViewportManager->UpdateConfiguration(EDisplayClusterRenderFrameMode::PreviewInScene, InClusterNodeId, this, &PreviewSettings);
	}

	return false;
}

bool ADisplayClusterRootActor::ImplUpdatePreviewRenderFrame_Editor(const FString& InClusterNodeId)
{
	if (!PreviewRenderFrame.IsValid())
	{
		// Begin render new frame for cluster node
		if (ImplUpdatePreviewConfiguration_Editor(InClusterNodeId))
		{
			// Update all preview components resources before render
			for (const TTuple<FString, TObjectPtr<UDisplayClusterPreviewComponent>>& PreviewComponentIt : PreviewComponents)
			{
				if (PreviewComponentIt.Value && (InClusterNodeId.IsEmpty() || PreviewComponentIt.Value->GetClusterNodeId() == InClusterNodeId))
				{
					PreviewComponentIt.Value->UpdatePreviewResources();
				}
			}

			// Now always use RootActor world to preview. 
			UWorld* PreviewWorld = GetWorld();
			if (PreviewWorld)
			{
				PreviewRenderFrame = MakeUnique<FDisplayClusterRenderFrame>();

				const bool bSceneNeedsStarting = !ViewportManager->GetCurrentWorld();
				
				// Update preview viewports from settings
				if (ViewportManager->BeginNewFrame(nullptr, PreviewWorld, *PreviewRenderFrame))
				{
					if (bSceneNeedsStarting)
					{
						// Fix inner frustum possibly using the wrong post process render target
						ImplUpdatePreviewConfiguration_Editor(InClusterNodeId);
					}
					
					PreviewViewportIndex = 0;
					return true;
				}

				return false;
			}
		}
	}

	return true;
}

bool ADisplayClusterRootActor::ImplRenderPassPreviewClusterNode_Editor()
{
	if (!PreviewRenderFrame.IsValid() || PreviewViewportIndex < 0)
	{
		return false;
	}

	int32 ViewportsAmount = ViewportsPerFrame - PreviewViewportsRenderedInThisFrameCnt;
	if (ViewportsAmount <= 0)
	{
		// All viewports for this pass is rendered
		return false;
	}

	bool bFrameRendered = false;
	int32 RenderedViewportsAmount = 0;

	ViewportManager->RenderInEditor(*PreviewRenderFrame, nullptr, PreviewViewportIndex, ViewportsAmount, RenderedViewportsAmount, bFrameRendered);

	// Increase viewport index
	PreviewViewportIndex += ViewportsAmount;

	// Count only rendered viewports
	PreviewViewportsRenderedInThisFrameCnt += RenderedViewportsAmount;

	if (bFrameRendered)
	{
		// current cluster node is composed
		PreviewViewportIndex = -1;
		PreviewRenderFrame.Reset();

		// Send event about RTT changed
		OnPreviewGenerated.ExecuteIfBound();

		return true;
	}

	return false;
}

void ADisplayClusterRootActor::ImplRenderPreview_Editor()
{
	if (CurrentConfigData == nullptr || !IsPreviewEnabled() || !ViewportManager.IsValid())
	{
		// no preview
		return;
	}

	if (PreviewClusterNodeIndex < 0)
	{
		if (bFreezePreviewRender)
		{
			return;
		}

		// Allow preview render
		PreviewClusterNodeIndex = 0;
	}

	// per-node render
	TArray<FString> ExistClusterNodesIDs;
	CurrentConfigData->Cluster->GetNodeIds(ExistClusterNodesIDs);

	// When viewports count changed, reset render cycle
	if (PreviewClusterNodeIndex >= ExistClusterNodesIDs.Num())
	{
		PreviewClusterNodeIndex = 0;
	}

	// Try render all nodes in one pass
	int32 NumNodesForSceneMaterialPreview = ExistClusterNodesIDs.Num();
	PreviewViewportsRenderedInThisFrameCnt = 0;

	for (int32 NodeIt = 0; NodeIt < NumNodesForSceneMaterialPreview; NodeIt++)
	{
		int32 ViewportsAmount = ViewportsPerFrame - PreviewViewportsRenderedInThisFrameCnt;
		if (ViewportsAmount <= 0)
		{
			// All viewports for this pass is rendered
			return;
		}

		const FString& ClusterNodeId = ExistClusterNodesIDs[PreviewClusterNodeIndex];

		if (PreviewRenderFrameClusterNodeId != ClusterNodeId)
		{
			PreviewRenderFrameClusterNodeId = ClusterNodeId;
			PreviewRenderFrame.Reset();
			PreviewViewportIndex = -1;
		}

		ImplUpdatePreviewRenderFrame_Editor(PreviewRenderFrameClusterNodeId);

		// Render this cluster node viewports
		if (!ImplRenderPassPreviewClusterNode_Editor())
		{
			// Cluster node render still in progress..
			return;
		}

		// Loop over cluster nodes
		PreviewClusterNodeIndex++;
		if (PreviewClusterNodeIndex >= ExistClusterNodesIDs.Num())
		{
			PreviewClusterNodeIndex = bFreezePreviewRender ? -1 : 0;

			bOutputFrameToPostProcessRenderTarget = bOutputFrameToPostProcessRenderTarget ?
				bPreviewEnablePostProcess :
				bPreviewEnablePostProcess || DoObserversNeedPostProcessRenderTarget();
		}

		if (PreviewClusterNodeIndex < 0)
		{
			// Frame captured. stop render
			break;
		}
	}
}

void ADisplayClusterRootActor::ImplRenderPreviewFrustums_Editor()
{
	if (CurrentConfigData == nullptr || !ViewportManager.IsValid())
	{
		return;
	}

	// frustum preview viewports
	TArray<IDisplayClusterViewport*> FrustumPreviewViewports;

	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Node : CurrentConfigData->Cluster->Nodes)
	{
		if (Node.Value == nullptr)
		{
			continue;
		}

		// collect node viewports
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportConfig : Node.Value->Viewports)
		{
			if (ViewportConfig.Value->bAllowPreviewFrustumRendering == false || ViewportConfig.Value->bIsVisible == false || ViewportConfig.Value == nullptr)
			{
				continue;
			}

			IDisplayClusterViewport* Viewport = ViewportManager->FindViewport(ViewportConfig.Key);
			if (Viewport != nullptr && Viewport->GetContexts().Num() > 0 && Viewport->GetRenderSettings().bEnable)
			{
				FrustumPreviewViewports.Add(Viewport);
			}
		}
	}

	// collect incameras
	if (bPreviewICVFXFrustums)
	{
		for (UActorComponent* ActorComponentIt : GetComponents())
		{
			if (ActorComponentIt)
			{
				UDisplayClusterICVFXCameraComponent* CineCameraComponent = Cast<UDisplayClusterICVFXCameraComponent>(ActorComponentIt);
				if (CineCameraComponent && CineCameraComponent->IsICVFXEnabled())
				{
					// Iterate over rendered incamera viewports (whole cluster)
					for (FDisplayClusterViewport* InCameraViewportIt : FDisplayClusterViewportConfigurationHelpers_ICVFX::PreviewGetRenderedInCameraViewports(*this, *CineCameraComponent))
					{
						FrustumPreviewViewports.Add(InCameraViewportIt);
					}
				}
			}
		}
	}

	for (IDisplayClusterViewport* ViewportIt : FrustumPreviewViewports)
	{
		const TArray<FDisplayClusterViewport_Context>& Contexts = ViewportIt->GetContexts();

		// Due to rendering optimizations for DCRA preview (node rendered over multiple frames).
		// As a result, viewport math values (stored in contexts) may not be ready at the moment.
		FFrustumPreviewViewportContextCache FrustumPreviewViewportContext;
		bool bIsValidViewportContext = false;

		// Preview rendered only in mono
		if (Contexts.Num() == 1 && Contexts[0].bIsValidProjectionMatrix && Contexts[0].bIsValidViewLocation && Contexts[0].bIsValidViewRotation)
		{
			FrustumPreviewViewportContext.ProjectionMatrix = Contexts[0].ProjectionMatrix;
			FrustumPreviewViewportContext.ViewLocation = Contexts[0].ViewLocation;
			FrustumPreviewViewportContext.ViewRotation = Contexts[0].ViewRotation;

			if (ViewportIt->GetProjectionMatrix(0, FrustumPreviewViewportContext.ProjectionMatrix))
			{
				bIsValidViewportContext = true;
			}
		}

		// Get cached value
		if (!bIsValidViewportContext)
		{
			FFrustumPreviewViewportContextCache* const LastValidContext = FrustumPreviewViewportContextCache.Find(ViewportIt->GetId());
			if (LastValidContext)
			{
				FrustumPreviewViewportContext = *LastValidContext;
				bIsValidViewportContext = true;
			}
		}

		if(bIsValidViewportContext)
		{
			// Update cache
			FrustumPreviewViewportContextCache.Emplace(ViewportIt->GetId(), FrustumPreviewViewportContext);

			// Render the frustum

			FMatrix ViewRotationMatrix = FInverseRotationMatrix(FrustumPreviewViewportContext.ViewRotation) * FMatrix(
				FPlane(0, 0, 1, 0),
				FPlane(1, 0, 0, 0),
				FPlane(0, 1, 0, 0),
				FPlane(0, 0, 0, 1));
			const FMatrix ViewMatrix = FTranslationMatrix(-FrustumPreviewViewportContext.ViewLocation) * ViewRotationMatrix;

			ImplRenderPreviewViewportFrustum_Editor(FrustumPreviewViewportContext.ProjectionMatrix, ViewMatrix, FrustumPreviewViewportContext.ViewLocation);
		}
	}
}

void ADisplayClusterRootActor::ImplRenderPreviewViewportFrustum_Editor(const FMatrix ProjectionMatrix, const FMatrix ViewMatrix, const FVector ViewOrigin)
{
	const float FarPlane = PreviewICVFXFrustumsFarDistance;
	const float NearPlane = GNearClippingPlane;
	const FColor Color = FColor::Green;
	const float Thickness = 1.0f;

	const UWorld* World = GetWorld();
	ULineBatchComponent* LineBatcher = World ? World->LineBatcher : nullptr;
	if (!LineBatcher)
	{
		return;
	}

	// Get FOV and AspectRatio from the view's projection matrix.
	const float AspectRatio = ProjectionMatrix.M[1][1] / ProjectionMatrix.M[0][0];
	const bool bIsPerspectiveProjection = true;

	// Build the camera frustum for this cascade
	const float HalfHorizontalFOV = bIsPerspectiveProjection ? FMath::Atan(1.0f / ProjectionMatrix.M[0][0]) : PI / 4.0f;
	const float HalfVerticalFOV = bIsPerspectiveProjection ? FMath::Atan(1.0f / ProjectionMatrix.M[1][1]) : FMath::Atan((FMath::Tan(PI / 4.0f) / AspectRatio));
	const float AsymmetricFOVScaleX = ProjectionMatrix.M[2][0];
	const float AsymmetricFOVScaleY = ProjectionMatrix.M[2][1];
	
	// Near plane
	const float StartHorizontalTotalLength = NearPlane * FMath::Tan(HalfHorizontalFOV);
	const float StartVerticalTotalLength = NearPlane * FMath::Tan(HalfVerticalFOV);
	const FVector StartCameraLeftOffset = ViewMatrix.GetColumn(0) * -StartHorizontalTotalLength * (1 + AsymmetricFOVScaleX);
	const FVector StartCameraRightOffset = ViewMatrix.GetColumn(0) * StartHorizontalTotalLength * (1 - AsymmetricFOVScaleX);
	const FVector StartCameraBottomOffset = ViewMatrix.GetColumn(1) * -StartVerticalTotalLength * (1 + AsymmetricFOVScaleY);
	const FVector StartCameraTopOffset = ViewMatrix.GetColumn(1) * StartVerticalTotalLength * (1 - AsymmetricFOVScaleY);
	
	// Far plane
	const float EndHorizontalTotalLength = FarPlane * FMath::Tan(HalfHorizontalFOV);
	const float EndVerticalTotalLength = FarPlane * FMath::Tan(HalfVerticalFOV);
	const FVector EndCameraLeftOffset = ViewMatrix.GetColumn(0) * -EndHorizontalTotalLength * (1 + AsymmetricFOVScaleX);
	const FVector EndCameraRightOffset = ViewMatrix.GetColumn(0) * EndHorizontalTotalLength * (1 - AsymmetricFOVScaleX);
	const FVector EndCameraBottomOffset = ViewMatrix.GetColumn(1) * -EndVerticalTotalLength * (1 + AsymmetricFOVScaleY);
	const FVector EndCameraTopOffset = ViewMatrix.GetColumn(1) * EndVerticalTotalLength * (1 - AsymmetricFOVScaleY);
	
	const FVector CameraDirection = ViewMatrix.GetColumn(2);

	// Preview frustum vertices
	FVector PreviewFrustumVerts[8];

	// Get the 4 points of the camera frustum near plane, in world space
	PreviewFrustumVerts[0] = ViewOrigin + CameraDirection * NearPlane + StartCameraRightOffset + StartCameraTopOffset;         // 0 Near  Top    Right
	PreviewFrustumVerts[1] = ViewOrigin + CameraDirection * NearPlane + StartCameraRightOffset + StartCameraBottomOffset;      // 1 Near  Bottom Right
	PreviewFrustumVerts[2] = ViewOrigin + CameraDirection * NearPlane + StartCameraLeftOffset + StartCameraTopOffset;          // 2 Near  Top    Left
	PreviewFrustumVerts[3] = ViewOrigin + CameraDirection * NearPlane + StartCameraLeftOffset + StartCameraBottomOffset;       // 3 Near  Bottom Left

	// Get the 4 points of the camera frustum far plane, in world space
	PreviewFrustumVerts[4] = ViewOrigin + CameraDirection * FarPlane + EndCameraRightOffset + EndCameraTopOffset;         // 4 Far  Top    Right
	PreviewFrustumVerts[5] = ViewOrigin + CameraDirection * FarPlane + EndCameraRightOffset + EndCameraBottomOffset;      // 5 Far  Bottom Right
	PreviewFrustumVerts[6] = ViewOrigin + CameraDirection * FarPlane + EndCameraLeftOffset + EndCameraTopOffset;          // 6 Far  Top    Left
	PreviewFrustumVerts[7] = ViewOrigin + CameraDirection * FarPlane + EndCameraLeftOffset + EndCameraBottomOffset;       // 7 Far  Bottom Left	
	
	// frustum lines
	LineBatcher->DrawLine(PreviewFrustumVerts[0], PreviewFrustumVerts[4], Color, SDPG_World, Thickness, 0.f); // right top
	LineBatcher->DrawLine(PreviewFrustumVerts[1], PreviewFrustumVerts[5], Color, SDPG_World, Thickness, 0.f); // right bottom
	LineBatcher->DrawLine(PreviewFrustumVerts[2], PreviewFrustumVerts[6], Color, SDPG_World, Thickness, 0.f); // left top
	LineBatcher->DrawLine(PreviewFrustumVerts[3], PreviewFrustumVerts[7], Color, SDPG_World, Thickness, 0.f); // left bottom

	// near plane square
	LineBatcher->DrawLine(PreviewFrustumVerts[0], PreviewFrustumVerts[1], Color, SDPG_World, Thickness, 0.f); // right top to right bottom
	LineBatcher->DrawLine(PreviewFrustumVerts[1], PreviewFrustumVerts[3], Color, SDPG_World, Thickness, 0.f); // right bottom to left bottom
	LineBatcher->DrawLine(PreviewFrustumVerts[3], PreviewFrustumVerts[2], Color, SDPG_World, Thickness, 0.f); // left bottom to left top
	LineBatcher->DrawLine(PreviewFrustumVerts[2], PreviewFrustumVerts[0], Color, SDPG_World, Thickness, 0.f); // left top to right top

	// far plane square
	LineBatcher->DrawLine(PreviewFrustumVerts[4], PreviewFrustumVerts[5], Color, SDPG_World, Thickness, 0.f); // right top to right bottom
	LineBatcher->DrawLine(PreviewFrustumVerts[5], PreviewFrustumVerts[7], Color, SDPG_World, Thickness, 0.f); // right bottom to left bottom
	LineBatcher->DrawLine(PreviewFrustumVerts[7], PreviewFrustumVerts[6], Color, SDPG_World, Thickness, 0.f); // left bottom to left top
	LineBatcher->DrawLine(PreviewFrustumVerts[6], PreviewFrustumVerts[4], Color, SDPG_World, Thickness, 0.f); // left top to right top
}

static FName Name_RelativeLocation = USceneComponent::GetRelativeLocationPropertyName();
static FName Name_RelativeRotation = USceneComponent::GetRelativeRotationPropertyName();
static FName Name_RelativeScale3D = USceneComponent::GetRelativeScale3DPropertyName();

void ADisplayClusterRootActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	
	// The AActor method, simplified and modified to skip construction scripts.
	// Component registration still needs to occur or the actor will look like it disappeared.
	auto SuperCallWithoutConstructionScripts = [&]
	{
		if (IsPropertyChangedAffectingDataLayers(PropertyChangedEvent))
		{
			FixupDataLayers(/*bRevertChangesOnLockedDataLayer*/true);
		}

		const bool bTransformationChanged = (PropertyName == Name_RelativeLocation || PropertyName == Name_RelativeRotation || PropertyName == Name_RelativeScale3D);

		if ((GEditor && GEditor->bIsSimulatingInEditor && GetWorld() != nullptr) || ReregisterComponentsWhenModified())
		{
			// If a transaction is occurring we rely on the true parent method instead.
			check (!CurrentTransactionAnnotation.IsValid());

			UnregisterAllComponents();
			ReregisterAllComponents();
		}

		// Let other systems know that an actor was moved
		if (bTransformationChanged)
		{
			GEngine->BroadcastOnActorMoved( this );
		}

		FEditorSupportDelegates::UpdateUI.Broadcast();
		UObject::PostEditChangeProperty(PropertyChangedEvent);	
	};

	bool bReinitializeActor = true;
	bool bCanSkipConstructionScripts = false;
	bool bResetPreviewComponents = false;
	if (const UDisplayClusterBlueprint* Blueprint = Cast<UDisplayClusterBlueprint>(UBlueprint::GetBlueprintFromClass(GetClass())))
	{
		bCanSkipConstructionScripts = !Blueprint->bRunConstructionScriptOnInteractiveChange;
	}
	
	if (bCanSkipConstructionScripts && PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive && !CurrentTransactionAnnotation.IsValid())
	{
		// Avoid calling construction scripts when the change occurs while the user is dragging a slider.
		SuperCallWithoutConstructionScripts();
		bReinitializeActor = false;
	}
	else
	{
		// any property update causes a DCRA rebuild. Restore preview material show
		ResetPreviewComponents_Editor(true);
		bResetPreviewComponents = true;
		Super::PostEditChangeProperty(PropertyChangedEvent);
	}
	
	// Config file has been changed, we should rebuild the nDisplay hierarchy
	// Cluster node ID has been changed
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bFreezePreviewRender)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewRenderTargetRatioMult)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bPreviewICVFXFrustums)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewICVFXFrustumsFarDistance)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewMaxTextureDimension)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, PreviewNodeId)
	|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, RenderMode))
	{
		bReinitializeActor = false;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, TickPerFrame)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, ViewportsPerFrame))
	{
		ResetPreviewInternals_Editor();

		bReinitializeActor = false;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, InnerFrustumPriority))
	{
		ResetInnerFrustumPriority();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ADisplayClusterRootActor, bPreviewEnablePostProcess))
	{
		ResetPreviewComponents_Editor(false);
		PreviewRenderFrame.Reset();
		bReinitializeActor = false;
	}
	if (bReinitializeActor)
	{
		InitializeRootActor();
	}

	if (bResetPreviewComponents)
	{
		ResetPreviewComponents_Editor(false);
	}
}

void ADisplayClusterRootActor::PostEditMove(bool bFinished)
{
	// Don't update the preview with the config data if we're just moving the actor.
	Super::PostEditMove(bFinished);

	FrustumPreviewViewportContextCache.Empty();

	ResetPreviewComponents_Editor(false);
}

void ADisplayClusterRootActor::HandleAssetReload(const EPackageReloadPhase InPackageReloadPhase,
	FPackageReloadedEvent* InPackageReloadedEvent)
{
	if (InPackageReloadPhase == EPackageReloadPhase::PrePackageFixup)
	{
		// Preview components need to be released here. During a package reload BeginDestroy will be called on the
		// actor, but the preview components are already detached and never have DestroyComponent called on them.
		// This causes the package to keep references around and cause a crash during the reload.
		BeginDestroy_Editor();
	}
}

void ADisplayClusterRootActor::ResetPreviewComponents_Editor(bool bInRestoreSceneMaterial)
{
	TArray<UDisplayClusterPreviewComponent*> AllPreviewComponents;
	GetComponents(AllPreviewComponents);

	for (UDisplayClusterPreviewComponent* ExistingComp : AllPreviewComponents)
	{
		ExistingComp->ResetPreviewComponent(bInRestoreSceneMaterial);
	}
}

void ADisplayClusterRootActor::UpdatePreviewComponents()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("ADisplayClusterRootActor::UpdatePreviewComponents"), STAT_UpdatePreviewComponents, STATGROUP_NDisplay);
	
	if (IsTemplate() || bDeferPreviewGeneration)
	{
		return;
	}

	ImplUpdatePreviewConfiguration_Editor(DisplayClusterConfigurationStrings::gui::preview::PreviewNodeAll);

	TArray<UDisplayClusterPreviewComponent*> IteratedPreviewComponents;
	
	if (CurrentConfigData != nullptr && IsPreviewEnabled())
	{
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& Node : CurrentConfigData->Cluster->Nodes)
		{
			if (Node.Value == nullptr)
			{
				continue;
			}

			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& Viewport : Node.Value->Viewports)
			{
				const FString PreviewCompId = GeneratePreviewComponentName_Editor(Node.Key, Viewport.Key);
				UDisplayClusterPreviewComponent* PreviewComp = PreviewComponents.FindRef(PreviewCompId);
				if (!PreviewComp)
				{
					PreviewComp = NewObject<UDisplayClusterPreviewComponent>(this, FName(*PreviewCompId), RF_DuplicateTransient | RF_Transactional | RF_NonPIEDuplicateTransient);
					check(PreviewComp);

					PreviewComponents.Emplace(PreviewCompId, PreviewComp);

					// Refresh preview when new component created
					PreviewClusterNodeIndex = 0;
				}

				if (GetWorld() && !PreviewComp->IsRegistered())
				{
					PreviewComp->RegisterComponent();
				}

				// Always reinitialize so changes impact the preview component.
				PreviewComp->InitializePreviewComponent(this, Node.Key, Viewport.Key, Viewport.Value);

				IteratedPreviewComponents.Add(PreviewComp);
			}
		}
	}

	// Cleanup unused components.
	TArray<UDisplayClusterPreviewComponent*> AllPreviewComponents;
	GetComponents(AllPreviewComponents);
	
	for (UDisplayClusterPreviewComponent* ExistingComp : AllPreviewComponents)
	{
		if (!IteratedPreviewComponents.Contains(ExistingComp))
		{
			PreviewComponents.Remove(ExistingComp->GetName());
			
			// Reset preview components before unregister
			ExistingComp->ResetPreviewComponent(true);

			ExistingComp->UnregisterComponent();
			ExistingComp->DestroyComponent();
		}
	}
}

void ADisplayClusterRootActor::ReleasePreviewComponents()
{
	for (const TPair<FString, TObjectPtr<UDisplayClusterPreviewComponent>>& CompPair : PreviewComponents)
	{
		if (CompPair.Value)
		{
			CompPair.Value->ResetPreviewComponent(true);

			CompPair.Value->UnregisterComponent();
			CompPair.Value->DestroyComponent();
		}
	}

	PreviewComponents.Reset();
	OnPreviewDestroyed.ExecuteIfBound();
}

int32 ADisplayClusterRootActor::SubscribeToPostProcessRenderTarget(const uint8* Object)
{
	check(Object);
	PostProcessRenderTargetObservers.Add(Object);
	return PostProcessRenderTargetObservers.Num();
}

int32 ADisplayClusterRootActor::UnsubscribeFromPostProcessRenderTarget(const uint8* Object)
{
	check(Object);
	PostProcessRenderTargetObservers.Remove(Object);
	return PostProcessRenderTargetObservers.Num();
}

bool ADisplayClusterRootActor::DoObserversNeedPostProcessRenderTarget() const
{
	return PostProcessRenderTargetObservers.Num() > 0;
}

bool ADisplayClusterRootActor::ShouldThisFrameOutputPreviewToPostProcessRenderTarget() const
{
	return bOutputFrameToPostProcessRenderTarget;
}

void ADisplayClusterRootActor::AddPreviewEnableOverride(const uint8* Object)
{
	check(Object);
	PreviewEnableOverriders.Add(Object);

	UpdatePreviewComponents();
}

void ADisplayClusterRootActor::RemovePreviewEnableOverride(const uint8* Object)
{
	check(Object);
	PreviewEnableOverriders.Remove(Object);

	UpdatePreviewComponents();
}

FString ADisplayClusterRootActor::GeneratePreviewComponentName_Editor(const FString& NodeId, const FString& ViewportId) const
{
	return FString::Printf(TEXT("%s_%s"), *NodeId, *ViewportId);
}

UDisplayClusterPreviewComponent* ADisplayClusterRootActor::GetPreviewComponent(const FString& NodeId, const FString& ViewportId) const
{
	const FString PreviewCompId = GeneratePreviewComponentName_Editor(NodeId, ViewportId);
	if (PreviewComponents.Contains(PreviewCompId))
	{
		return PreviewComponents[PreviewCompId];
	}

	return nullptr;
}

#endif
