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
	bool ImplInitializeOutputRemap();

private:
	// Name of the external file used for remapping. Empty if not used.
	FString ExternalFile;

	// Reference to the mesh component that is used for remapping.
	TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> OutputRemapMesh;
};
