// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"


LANDSCAPE_API DECLARE_LOG_CATEGORY_EXTERN(LogLandscape, Log, All);
LANDSCAPE_API DECLARE_LOG_CATEGORY_EXTERN(LogLandscapeBP, Display, All);

/**
 * Landscape stats
 */
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dynamic Draw Time"), STAT_LandscapeDynamicDrawTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Static Draw LOD Time"), STAT_LandscapeStaticDrawLODTime, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Render SetMesh Draw Time VS"), STAT_LandscapeVFDrawTimeVS, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Render SetMesh Draw Time PS"), STAT_LandscapeVFDrawTimePS, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Init View Custom Data"), STAT_LandscapeInitViewCustomData, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("PostInit View Custom Data"), STAT_LandscapePostInitViewCustomData, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compute Custom Mesh Batch LOD"), STAT_LandscapeComputeCustomMeshBatchLOD, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Compute Custom Shadow Mesh Batch LOD"), STAT_LandscapeComputeCustomShadowMeshBatchLOD, STATGROUP_Landscape, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Regenerate Layers (RenderThread)"), STAT_LandscapeLayersRegenerate_RenderThread, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Regenerate Layers Heightmap (GameThread)"), STAT_LandscapeLayersRegenerateHeightmaps, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Resolve Layers Heightmaps"), STAT_LandscapeLayersResolveHeightmaps, STATGROUP_Landscape, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Resolve Layers Texture"), STAT_LandscapeLayersResolveTexture, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Regenerate Layers Weightmap (GameThread)"), STAT_LandscapeLayersRegenerateWeightmaps, STATGROUP_Landscape, );

DECLARE_CYCLE_STAT_EXTERN(TEXT("Resolve Layers Weightmaps"), STAT_LandscapeLayersResolveWeightmaps, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Reallocate Layers Weightmaps"), STAT_LandscapeLayersReallocateWeightmaps, STATGROUP_Landscape, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update Layers Material Instances"), STAT_LandscapeLayersUpdateMaterialInstance, STATGROUP_Landscape, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Regenerate Layers DrawCalls"), STAT_LandscapeLayersRegenerateDrawCalls, STATGROUP_Landscape, );

DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Components Using SubSection DrawCall"), STAT_LandscapeComponentUsingSubSectionDrawCalls, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Tessellated Shadow Cascade"), STAT_LandscapeTessellatedShadowCascade, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Tessellated Components"), STAT_LandscapeTessellatedComponents, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Processed Triangles"), STAT_LandscapeTriangles, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Render Passes"), STAT_LandscapeComponentRenderPasses, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("DrawCalls"), STAT_LandscapeDrawCalls, STATGROUP_Landscape, );

DECLARE_MEMORY_STAT_EXTERN(TEXT("Vertex Mem"), STAT_LandscapeVertexMem, STATGROUP_Landscape, );
DECLARE_MEMORY_STAT_EXTERN(TEXT("Hole Mem"), STAT_LandscapeHoleMem, STATGROUP_Landscape, );
DECLARE_DWORD_COUNTER_STAT_EXTERN(TEXT("Component Mem"), STAT_LandscapeComponentMem, STATGROUP_Landscape, );
