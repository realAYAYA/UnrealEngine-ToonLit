// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "RHIUtilities.h"

class IDisplayClusterViewportManager;
class IDisplayClusterRender_MeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;

/**
 * Display Cluster Viewport PostProcess OutputRemap
 */
class FDisplayClusterViewportPostProcessOutputRemap
{
public:
	FDisplayClusterViewportPostProcessOutputRemap();
	virtual ~FDisplayClusterViewportPostProcessOutputRemap();

	// Game thread update calls
	bool UpdateConfiguration_ExternalFile(const FString& InExternalFile);
	bool UpdateConfiguration_StaticMesh(UStaticMesh* InStaticMesh);
	bool UpdateConfiguration_StaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent);
	bool UpdateConfiguration_ProceduralMeshComponent(UProceduralMeshComponent* InProceduralMeshComponent);
	void UpdateConfiguration_Disabled();

	bool MarkProceduralMeshComponentGeometryDirty(const FName& InComponentName);

	bool IsEnabled() const
	{
		return OutputRemapMesh.IsValid();
	}

public:
	void PerformPostProcessFrame_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<FRHITexture2D*>* InFrameTargets = nullptr, const TArray<FRHITexture2D*>* InAdditionalFrameTargets = nullptr) const;

private:
	bool ImplInitializeConfiguration();

private:
	FString ExternalFile;

private:
	TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> OutputRemapMesh;
	class IDisplayClusterShaders& ShadersAPI;
};
