// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Viewport/Preview/DisplayClusterViewportPreview.h"
#include "Render/Viewport/Resource/DisplayClusterViewportResource.h"
#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Render/Warp/IDisplayClusterWarpPolicy.h"

#include "Render/DisplayDevice/Components/DisplayClusterDisplayDeviceBaseComponent.h"
#include "Components/DisplayClusterCameraComponent.h"

#include "Components/StaticMeshComponent.h"

////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterViewportPreview
////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterViewportPreview::FDisplayClusterViewportPreview(const TSharedRef<FDisplayClusterViewportConfiguration, ESPMode::ThreadSafe>& InConfiguration, const FString& InViewportId)
	: Configuration(InConfiguration)
	, ViewportId(InViewportId)
	, ClusterNodeId(InConfiguration->GetClusterNodeId())
	, PreviewMesh(EDisplayClusterDisplayDeviceMeshType::PreviewMesh, InConfiguration)
	, PreviewEditableMesh(EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh, InConfiguration)
{ }

FDisplayClusterViewportPreview::~FDisplayClusterViewportPreview()
{
	Release();
}

void FDisplayClusterViewportPreview::Initialize(FDisplayClusterViewport& InViewport)
{
	ViewportWeakPtr = InViewport.AsShared();
}

void FDisplayClusterViewportPreview::Release()
{
	PreviewRTT.Reset();
	RuntimeFlags = EDisplayClusterViewportPreviewFlags::None;

	FDisplayClusterViewport* InViewport = GetViewportImpl();
	PreviewMesh.Release(InViewport);
	PreviewEditableMesh.Release(InViewport);
	}

void FDisplayClusterViewportPreview::Update()
{
	RuntimeFlags = EDisplayClusterViewportPreviewFlags::None;

	// Update viewport output RTT
	TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> NewPreviewRTT = GetOutputPreviewTargetableResources();
	if (NewPreviewRTT != PreviewRTT)
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewRTT);
		PreviewRTT = NewPreviewRTT;
	}

	if (PreviewRTT.IsValid() && EnumHasAnyFlags(PreviewRTT->GetResourceState(), EDisplayClusterViewportResourceState::UpdatedOnRenderingThread))
	{
		// Update flags when preview RTT is valid
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasValidPreviewRTT);
	}

	FDisplayClusterViewport* InViewport = GetViewportImpl();
	UDisplayClusterCameraComponent* ViewPointComponent = InViewport ? InViewport->GetViewPointCameraComponent(EDisplayClusterRootActorType::Configuration) : nullptr;
	UDisplayClusterDisplayDeviceBaseComponent* InDisplayDeviceComponent = InViewport ? InViewport->GetDisplayDeviceComponent(Configuration->GetPreviewSettings().DisplayDeviceRootActorType) : nullptr;

	// Update preview meshes only if DisplayDevice is used
	PreviewMesh.Update(InViewport, InDisplayDeviceComponent, ViewPointComponent);
	PreviewEditableMesh.Update(InViewport, InDisplayDeviceComponent, ViewPointComponent);

	// Update Runtime Flags:
	if (PreviewMesh.HasAnyFlag(EDisplayClusterViewportPreviewMeshFlags::HasDeletedMaterialInstance | EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance))
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewMeshMaterialInstance);
	}
	if (PreviewEditableMesh.HasAnyFlag(EDisplayClusterViewportPreviewMeshFlags::HasDeletedMaterialInstance | EDisplayClusterViewportPreviewMeshFlags::HasChangedMaterialInstance))
	{
		EnumAddFlags(RuntimeFlags, EDisplayClusterViewportPreviewFlags::HasChangedPreviewEditableMeshMaterialInstance);
	}

	// Update the preview mesh and materials in the DisplayDevice component
	if (InDisplayDeviceComponent)
	{
		// Update mesh and material instances
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMeshAndMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewMesh, PreviewMesh.GetCurrentMaterialType(), PreviewMesh.GetMeshComponent(), PreviewMesh.GetMaterialInstance());
		InDisplayDeviceComponent->OnUpdateDisplayDeviceMeshAndMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh, PreviewMesh.GetCurrentMaterialType(), PreviewEditableMesh.GetMeshComponent(), PreviewEditableMesh.GetMaterialInstance());
	}

	// Update the preview mesh and materials in the ViewPointComponent component
	if (ViewPointComponent)
	{
		ViewPointComponent->OnUpdateDisplayDeviceMeshAndMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewMesh, PreviewMesh.GetCurrentMaterialType(), PreviewMesh.GetMeshComponent(), PreviewMesh.GetMaterialInstance());
		ViewPointComponent->OnUpdateDisplayDeviceMeshAndMaterialInstance(*this, EDisplayClusterDisplayDeviceMeshType::PreviewEditableMesh, PreviewMesh.GetCurrentMaterialType(), PreviewEditableMesh.GetMeshComponent(), PreviewEditableMesh.GetMaterialInstance());
	}
}

IDisplayClusterViewport* FDisplayClusterViewportPreview::GetViewport() const
{
	return GetViewportImpl();
}

FDisplayClusterViewport* FDisplayClusterViewportPreview::GetViewportImpl() const
{
	return ViewportWeakPtr.IsValid() ? ViewportWeakPtr.Pin().Get() : nullptr;
}

TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe> FDisplayClusterViewportPreview::GetOutputPreviewTargetableResources() const
{
	if (FDisplayClusterViewport* InViewport = GetViewportImpl())
	{
		const TArray<TSharedPtr<FDisplayClusterViewportResource, ESPMode::ThreadSafe>>& PreviewResources = InViewport->GetViewportResources(EDisplayClusterViewportResource::OutputPreviewTargetableResources);
		if (!PreviewResources.IsEmpty())
		{
			return PreviewResources[0];
		}
	}

	return nullptr;
}

UTextureRenderTarget2D* FDisplayClusterViewportPreview::GetPreviewTextureRenderTarget2D() const
{
	if (PreviewRTT.IsValid())
	{
		return PreviewRTT->GetTextureRenderTarget2D();
	}

	return nullptr;
}
