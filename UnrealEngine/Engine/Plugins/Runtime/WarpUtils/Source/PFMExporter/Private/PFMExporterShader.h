// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RenderResource.h"

#include "IPFMExporter.h"

class FPFMExporterMesh;

class FPFMExporterShader
{
public:
	static bool ApplyPFMExporter_RenderThread(
		FRHICommandListImmediate& RHICmdList, 
		const FStaticMeshLODResources& SrcMeshResource,
		const FMatrix& MeshToOrigin,
		FPFMExporterMesh& DstPfmMesh
	);
};