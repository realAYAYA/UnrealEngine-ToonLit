// Copyright Epic Games, Inc. All Rights Reserved.

#include "Policy/VIOSO/Windows/DisplayClusterProjectionVIOSOPolicy.h"

#include "DisplayClusterProjectionStrings.h"
#include "DisplayClusterProjectionLog.h"

#include "IDisplayCluster.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Config/IDisplayClusterConfigManager.h"
#include "Game/IDisplayClusterGameManager.h"

#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Engine/RendererSettings.h"

#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Viewport/IDisplayClusterViewportProxy.h"

#include "ProceduralMeshComponent.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterProjectionVIOSOPolicy
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterProjectionVIOSOPolicy::FDisplayClusterProjectionVIOSOPolicy(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy, const TSharedRef<FDisplayClusterProjectionVIOSOLibrary, ESPMode::ThreadSafe>& InVIOSOLibrary)
	: FDisplayClusterProjectionPolicyBase(ProjectionPolicyId, InConfigurationProjectionPolicy)
	, VIOSOLibrary(InVIOSOLibrary)
{ }

FDisplayClusterProjectionVIOSOPolicy::~FDisplayClusterProjectionVIOSOPolicy()
{
	ImplRelease();
}


const FString& FDisplayClusterProjectionVIOSOPolicy::GetType() const
{
	static const FString Type(DisplayClusterProjectionStrings::projection::VIOSO);
	return Type;
}

bool FDisplayClusterProjectionVIOSOPolicy::HandleStartScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	// Read VIOSO config data from nDisplay config file
	if (!ViosoConfigData.Initialize(GetParameters(), InViewport))
	{
		if (!IsEditorOperationMode(InViewport))
		{
			UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't read VIOSO configuration from the config file for viewport -'%s'"), *InViewport->GetId());
		}

		return false;
	}

	// Find origin component if it exists
	InitializeOriginComponent(InViewport, ViosoConfigData.OriginCompId);

	Views.AddDefaulted(2);

	// Initialize data for all views
	FScopeLock lock(&DllAccessCS);
	int32 ViewIndex = 0;
	for (TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& ViewIt : Views)
	{
		ViewIt = MakeShared<FDisplayClusterProjectionVIOSOPolicyViewData>(VIOSOLibrary, ViosoConfigData, InViewport, ViewIndex++);
	}

	UE_LOG(LogDisplayClusterProjectionVIOSO, Verbose, TEXT("VIOSO policy has been initialized: %s"), *ViosoConfigData.ToString());

	return true;
}

void FDisplayClusterProjectionVIOSOPolicy::HandleEndScene(IDisplayClusterViewport* InViewport)
{
	check(IsInGameThread());

	ImplRelease();
}

void FDisplayClusterProjectionVIOSOPolicy::ImplRelease()
{
	ReleaseOriginComponent();

	PreviewMeshComponentRef.ResetSceneComponent();

	// Destroy VIOSO for all views
	FScopeLock lock(&DllAccessCS);
	Views.Reset();
}

bool FDisplayClusterProjectionVIOSOPolicy::CalculateView(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float InNCP, const float InFCP)
{
	check(Views.IsValidIndex(InContextNum));
	TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& CurrentView = Views[InContextNum];
	if (!CurrentView.IsValid())
	{
		return false;
	}

	// Get view location in local space
	const USceneComponent* const OriginComp = GetOriginComponent();
	const FTransform& World2LocalTransform = (OriginComp ? OriginComp->GetComponentTransform() : FTransform::Identity);

	// Get our base camera location and view offset in local space (MPCDI space)
	const FVector  LocalOrigin    = World2LocalTransform.InverseTransformPosition(InOutViewLocation - ViewOffset);
	const FVector  LocalEyeOrigin = World2LocalTransform.InverseTransformPosition(InOutViewLocation);
	const FRotator LocalRotator   = World2LocalTransform.InverseTransformRotation(InOutViewRotation.Quaternion()).Rotator();

	// Get view prj data from VIOSO
	FScopeLock lock(&DllAccessCS);
	if (!CurrentView->UpdateVIOSO(InViewport, InContextNum, LocalEyeOrigin, LocalRotator, WorldToMeters, InNCP, InFCP))
	{
		if (!FDisplayClusterProjectionPolicyBase::IsEditorOperationMode(InViewport))
		{
			// Vioso api used, but failed inside math. The config base matrix or vioso geometry is invalid
			UE_LOG(LogDisplayClusterProjectionVIOSO, Error, TEXT("Couldn't Calculate View for VIOSO viewport '%s'"), *InViewport->GetId());
		}

		return false;
	}

	// Transform rotation to world space
	InOutViewRotation = World2LocalTransform.TransformRotation(CurrentView->ViewRotation.Quaternion()).Rotator();
	InOutViewLocation = World2LocalTransform.TransformPosition(CurrentView->ViewLocation);

	return true;
}

bool FDisplayClusterProjectionVIOSOPolicy::GetProjectionMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FMatrix& OutPrjMatrix)
{
	check(IsInGameThread());
	check(Views.IsValidIndex(InContextNum));
	TSharedPtr<FDisplayClusterProjectionVIOSOPolicyViewData, ESPMode::ThreadSafe>& CurrentView = Views[InContextNum];
	if (!CurrentView.IsValid())
	{
		return false;
	}

	OutPrjMatrix = CurrentView->ProjectionMatrix;
	
	return true;
}

bool FDisplayClusterProjectionVIOSOPolicy::IsWarpBlendSupported()
{
	return true;
}

void FDisplayClusterProjectionVIOSOPolicy::ApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	if (!ImplApplyWarpBlend_RenderThread(RHICmdList, InViewportProxy))
	{
		// warp failed, just resolve texture to frame
		InViewportProxy->ResolveResources_RenderThread(RHICmdList, EDisplayClusterViewportResourceType::InputShaderResource, EDisplayClusterViewportResourceType::OutputTargetableResource);
	}
}

bool FDisplayClusterProjectionVIOSOPolicy::ImplApplyWarpBlend_RenderThread(FRHICommandListImmediate& RHICmdList, const IDisplayClusterViewportProxy* InViewportProxy)
{
	check(IsInRenderingThread());

	// Get in\out remp resources ref from viewport
	TArray<FRHITexture2D*> InputTextures, OutputTextures;

	// Use for input first MipsShader texture if enabled in viewport render settings
	//@todo: test if domeprojection support mips textures as warp input
	//if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::MipsShaderResource, InputTextures))
	{
		// otherwise inputshader texture
		if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::InputShaderResource, InputTextures))
		{
			// no source textures
			return false;
		}
	}

	// Get output resources with rects
	// warp result is now inside AdditionalRTT.  Later, from the DC ViewportManagerProxy it will be resolved to FrameRTT 
	if (!InViewportProxy->GetResources_RenderThread(EDisplayClusterViewportResourceType::AfterWarpBlendTargetableResource, OutputTextures))
	{
		return false;
	}

	check(InputTextures.Num() == OutputTextures.Num());
	check(InViewportProxy->GetContexts_RenderThread().Num() == InputTextures.Num());

	// External SDK not use our RHI flow, call flush to finish resolve context image to input resource
	RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);

	TRACE_CPUPROFILER_EVENT_SCOPE(nDisplay VIOSO::Render);
	{
		FScopeLock lock(&DllAccessCS);

		for (int32 ContextNum = 0; ContextNum < InputTextures.Num(); ContextNum++)
		{
			if (Views[ContextNum].IsValid())
			{
				if (!Views[ContextNum]->RenderVIOSO_RenderThread(RHICmdList, InputTextures[ContextNum], OutputTextures[ContextNum]))
				{
					return false;
				}
			}
		}
	}

	// warp result is now inside AdditionalRTT.  Later, from the DC ViewportManagerProxy it will be resolved to FrameRTT 
	return true;
}

bool FDisplayClusterProjectionVIOSOPolicy::HasPreviewMesh(IDisplayClusterViewport* InViewport)
{
	if (!ViosoConfigData.bIsPreviewMeshEnabled || Views.IsEmpty() || !Views[0].IsValid() || !Views[0]->IsWarperInterfaceValid())
	{
		PreviewMeshComponentRef.ResetSceneActor();

		return false;
	}

	return true;
}

UMeshComponent* FDisplayClusterProjectionVIOSOPolicy::GetOrCreatePreviewMeshComponent(IDisplayClusterViewport* InViewport, bool& bOutIsRootActorComponent)
{
	if (!HasPreviewMesh(InViewport))
	{
		return nullptr;
	}

	// Create a new DCRA mesh component
	bOutIsRootActorComponent = false;

	// If we have already created a preview mesh component before, return that component
	if (UMeshComponent* ExistsPreviewMeshComp = Cast<UMeshComponent>(PreviewMeshComponentRef.GetOrFindSceneComponent()))
	{
		return ExistsPreviewMeshComp;
	}

	USceneComponent* OriginComp = GetPreviewMeshOriginComponent(InViewport);
	TSharedPtr<FDisplayClusterProjectionVIOSOGeometryExportData, ESPMode::ThreadSafe> GeometryExportData = FDisplayClusterProjectionVIOSOGeometryExportData::Create(VIOSOLibrary, ViosoConfigData);
	if (OriginComp && GeometryExportData.IsValid())
	{
		// Create new WarpMesh component
		const FString CompName = FString::Printf(TEXT("VIOSO_%s_impl"), *GetId());

		// Creta new object
		UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(OriginComp, FName(*CompName), EObjectFlags::RF_DuplicateTransient | RF_Transient | RF_TextExportTransient);
		if (MeshComp)
		{
			MeshComp->RegisterComponent();
			MeshComp->AttachToComponent(OriginComp, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
			MeshComp->CreateMeshSection(0, GeometryExportData->Vertices, GeometryExportData->Triangles, GeometryExportData->Normal, GeometryExportData->UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);

#if WITH_EDITOR
			MeshComp->SetIsVisualizationComponent(true);
#endif

			// Because of "nDisplay.render.show.visualizationcomponents" we need extra flag to exclude this geometry from render
			MeshComp->SetHiddenInGame(true);

			// Store reference to mesh component
			PreviewMeshComponentRef.SetSceneComponent(MeshComp);

			return MeshComp;
		}
	}

	return nullptr;
}
