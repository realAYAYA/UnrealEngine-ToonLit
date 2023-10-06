// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "RenderGraphFwd.h"
#include "RHIDefinitions.h"
#include "SceneRenderTargetParameters.h"
#include "UObject/ObjectMacros.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_3
#include "CoreMinimal.h"
#endif

class FMaterial;
class FPrimitiveSceneProxy;
class FRDGBuilder;
class FRHIShaderResourceView;
class FSceneInterface;
class FSceneView;
class FSceneViewFamily;
class FShaderParametersMetadata;
class FSceneUniformBuffer;
DECLARE_UNIFORM_BUFFER_STRUCT(FSceneUniformParameters, RENDERER_API)

#if RHI_RAYTRACING
class FRHIRayTracingScene;
class FVisibleRayTracingMeshCommand;
#endif

namespace UE::FXRenderingUtils
{
	RENDERER_API TConstStridedView<FSceneView> ConvertViewArray(TConstArrayView<FViewInfo> Views);
	RENDERER_API FIntRect GetRawViewRectUnsafe(const FSceneView& View);

	RENDERER_API bool CanMaterialRenderBeforeFXPostOpaque(const FSceneViewFamily& ViewFamily, const FPrimitiveSceneProxy& SceneProxy, const FMaterial& Material);

	RENDERER_API const FGlobalDistanceFieldParameterData* GetGlobalDistanceFieldParameterData(TConstStridedView<FSceneView> Views);
	RENDERER_API FRDGTextureRef GetSceneVelocityTexture(const FSceneView& View);

	RENDERER_API TRDGUniformBufferRef<FSceneTextureUniformParameters> GetOrCreateSceneTextureUniformBuffer(
		FRDGBuilder& GraphBuilder,
		TConstStridedView<FSceneView> Views,
		ERHIFeatureLevel::Type FeatureLevel,
		ESceneTextureSetupMode SetupMode = ESceneTextureSetupMode::All);

	RENDERER_API TRDGUniformBufferRef<FMobileSceneTextureUniformParameters> GetOrCreateMobileSceneTextureUniformBuffer(
		FRDGBuilder& GraphBuilder,
		TConstStridedView<FSceneView> Views,
		EMobileSceneTextureSetupMode SetupMode = EMobileSceneTextureSetupMode::All);

	namespace DistanceFields
	{
		RENDERER_API const FShaderParametersMetadata* GetObjectBufferParametersMetadata();
		RENDERER_API const FShaderParametersMetadata* GetAtlasParametersMetadata();

		RENDERER_API bool HasDataToBind(const FSceneView& View);

		RENDERER_API void SetupObjectBufferParameters(FRDGBuilder& GraphBuilder, uint8* DestinationData, const FSceneView* View);
		RENDERER_API void SetupAtlasParameters(FRDGBuilder& GraphBuilder, uint8* DestinationData, const FSceneView* View);
	}

	/**
	 * Creates a minimal Scene Uniform buffer for the given Scene, allocated using the FRDGBuilder (making it safe to keep reference).
	 */
	RENDERER_API FSceneUniformBuffer &CreateSceneUniformBuffer(FRDGBuilder& GraphBuilder, const FSceneInterface* Scene);
	/**
	 * Get the RDG uniform buffer from the FSceneUniformBuffer (not exposed in public API).
	 */
	RENDERER_API TRDGUniformBufferRef<FSceneUniformParameters> GetSceneUniformBuffer(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer);

#if RHI_RAYTRACING
	namespace RayTracing
	{
		RENDERER_API bool HasRayTracingScene(const FSceneInterface* Scene);
		RENDERER_API FRHIRayTracingScene* GetRayTracingScene(const FSceneInterface* Scene);
		RENDERER_API FRHIShaderResourceView* GetRayTracingSceneView(FRHICommandListBase& RHICmdList, const FSceneInterface* Scene);
		
		UE_DEPRECATED(5.3, "GetRayTracingSceneView now requires a command list.")
		RENDERER_API FRHIShaderResourceView* GetRayTracingSceneView(const FSceneInterface* Scene);

		RENDERER_API TConstArrayView<FVisibleRayTracingMeshCommand> GetVisibleRayTracingMeshCommands(const FSceneView& View);
	}
#endif
}

/**
 * This class exposes methods required by FX rendering that must access rendering internals.
 */ 
class FFXRenderingUtils
{
public:
	FFXRenderingUtils() = delete;
	FFXRenderingUtils(const FFXRenderingUtils&) = delete;
	FFXRenderingUtils& operator=(const FFXRenderingUtils&) = delete;

	/** Utility to determine if a material might render before the FXSystem's PostRenderOpaque is called for the view family */
	UE_DEPRECATED(5.3, "Use UE::FXRenderingUtils::CanMaterialRenderBeforeFXPostOpaque")
	static bool CanMaterialRenderBeforeFXPostOpaque(
		const FSceneViewFamily& ViewFamily,
		const FPrimitiveSceneProxy& SceneProxy,
		const FMaterial& Material)
	{
		return UE::FXRenderingUtils::CanMaterialRenderBeforeFXPostOpaque(ViewFamily, SceneProxy, Material);
	}
};
