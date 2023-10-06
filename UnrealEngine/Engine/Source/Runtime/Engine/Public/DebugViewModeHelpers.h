// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Definition and helpers for debug view modes
 */

#pragma once

#include "CoreMinimal.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#include "SceneTypes.h"
#endif

#define WITH_DEBUG_VIEW_MODES (WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

class UMaterialInterface;
struct FSlowTask;
struct FMeshMaterialShaderPermutationParameters;
enum EShaderPlatform : uint16;
namespace ERHIFeatureLevel { enum Type : int; }

namespace EMaterialQualityLevel { enum Type : uint8; }

/** 
 * Enumeration for different Quad Overdraw visualization mode.
 */
enum EDebugViewShaderMode
{
	DVSM_None,						// No debug view.
	DVSM_ShaderComplexity,			// Default shader complexity viewmode
	DVSM_ShaderComplexityContainedQuadOverhead,	// Show shader complexity with quad overdraw scaling the PS instruction count.
	DVSM_ShaderComplexityBleedingQuadOverhead,	// Show shader complexity with quad overdraw bleeding the PS instruction count over the quad.
	DVSM_QuadComplexity,			// Show quad overdraw only.
	DVSM_PrimitiveDistanceAccuracy,	// Visualize the accuracy of the primitive distance computed for texture streaming.
	DVSM_MeshUVDensityAccuracy,		// Visualize the accuracy of the mesh UV densities computed for texture streaming.
	DVSM_MaterialTextureScaleAccuracy, // Visualize the accuracy of the material texture scales used for texture streaming.
	DVSM_OutputMaterialTextureScales,  // Outputs the material texture scales.
	DVSM_RequiredTextureResolution, // Visualize the accuracy of the streamed texture resolution.
	DVSM_VirtualTexturePendingMips,	// Visualize the pending virtual texture mips.
	DVSM_LODColoration,				// Visualize primitive LOD .
	DVSM_VisualizeGPUSkinCache,		// Visualize various properties of Skin Cache.
	DVSM_MAX
};

ENGINE_API const TCHAR* DebugViewShaderModeToString(EDebugViewShaderMode InShaderMode);

#if WITH_DEBUG_VIEW_MODES
/** Returns true if the vertex shader (and potential hull and domain) should be compiled on the given platform. */
ENGINE_API bool AllowDebugViewVSDSHS(EShaderPlatform Platform);
/** Returns true if the shader mode can be enabled. This is only for UI elements as no shader platform is actually passed. */
ENGINE_API bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel);
ENGINE_API bool ShouldCompileDebugViewModeShader(const FMeshMaterialShaderPermutationParameters& Parameters);
#else
FORCEINLINE bool AllowDebugViewVSDSHS(EShaderPlatform Platform)  { return false; }
FORCEINLINE bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel) { return false; }
#endif

ENGINE_API int32 GetNumActorsInWorld(UWorld* InWorld);
ENGINE_API bool GetUsedMaterialsInWorld(UWorld* InWorld, OUT TSet<UMaterialInterface*>& OutMaterials, FSlowTask* Task);
ENGINE_API bool CompileDebugViewModeShaders(EDebugViewShaderMode Mode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<UMaterialInterface*>& Materials, FSlowTask* ProgressTask);
ENGINE_API bool WaitForShaderCompilation(const FText& Message, FSlowTask* ProgressTask);

UE_DEPRECATED(4.26, "Parameters bFullRebuild and bWaitForPreviousShaders should no longer be used")
inline bool CompileDebugViewModeShaders(EDebugViewShaderMode Mode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, bool bFullRebuild, bool bWaitForPreviousShaders, TSet<UMaterialInterface*>& Materials, FSlowTask* ProgressTask)
{
	return CompileDebugViewModeShaders(Mode, QualityLevel, FeatureLevel, Materials, ProgressTask);
}

UE_DEPRECATED(4.26, "ClearDebugViewMaterials() should no longer be called")
inline void ClearDebugViewMaterials(UMaterialInterface*) {}

UE_DEPRECATED(4.26, "UpdateDebugViewModeShaders() should no longer be called")
inline void UpdateDebugViewModeShaders() {}
