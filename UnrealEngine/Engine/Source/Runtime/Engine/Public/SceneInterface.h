// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "SceneTypes.h"
#include "SceneUtils.h"
#include "Math/SHMath.h"
#include "RenderGraphDefinitions.h"

class AWorldSettings;
class FSkyAtmosphereRenderSceneInfo;
class FSkyAtmosphereSceneProxy;
class FVolumetricCloudRenderSceneInfo;
class FVolumetricCloudSceneProxy;
class FMaterial;
class FMaterialShaderMap;
class FPrimitiveSceneInfo;
class FRenderResource;
class FRenderTarget;
class FRectLightSceneProxy;
class FSkyLightSceneProxy;
class FTexture;
class FVertexFactory;
class UDecalComponent;
class ULightComponent;
class UPlanarReflectionComponent;
class UPrimitiveComponent;
class UInstancedStaticMeshComponent;
class UReflectionCaptureComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class UTextureCube;
class FViewInfo;
class FSceneRenderer;
class FInstanceCullingManager;
class FSceneViewStateInterface;
struct FHairStrandsInstance;
struct FLightRenderParameters;

enum EBasePassDrawListType
{
	EBasePass_Default=0,
	EBasePass_Masked,
	EBasePass_MAX
};

/**
 * An interface to the private scene manager implementation of a scene.  Use GetRendererModule().AllocateScene to create.
 * The scene
 */
class FSceneInterface
{
public:
	ENGINE_API FSceneInterface(ERHIFeatureLevel::Type InFeatureLevel)
		: FeatureLevel(InFeatureLevel)
	{}

	// FSceneInterface interface

	/** 
	 * Adds a new primitive component to the scene
	 * 
	 * @param Primitive - primitive component to add
	 */
	virtual void AddPrimitive(UPrimitiveComponent* Primitive) = 0;
	/** 
	 * Removes a primitive component from the scene
	 * 
	 * @param Primitive - primitive component to remove
	 */
	virtual void RemovePrimitive(UPrimitiveComponent* Primitive) = 0;
	/** Called when a primitive is being unregistered and will not be immediately re-registered. */
	virtual void ReleasePrimitive(UPrimitiveComponent* Primitive) = 0;
	/**
	* Updates all primitive scene info additions, remobals and translation changes
	*/
	virtual void UpdateAllPrimitiveSceneInfos(FRDGBuilder& GraphBuilder, bool bAsyncCreateLPIs = false) = 0;
	virtual ENGINE_API void UpdateAllPrimitiveSceneInfos(FRHICommandListImmediate& RHICmdList, bool bAsyncCreateLPIs = false);

	/** 
	 * Updates the transform of a primitive which has already been added to the scene. 
	 * 
	 * @param Primitive - primitive component to update
	 */
	virtual void UpdatePrimitiveTransform(UPrimitiveComponent* Primitive) = 0;
	virtual void UpdatePrimitiveOcclusionBoundsSlack(UPrimitiveComponent* Primitive, float NewSlack) = 0;
	/** Updates primitive attachment state. */
	virtual void UpdatePrimitiveAttachment(UPrimitiveComponent* Primitive) = 0;

	/**
	 * Updates all the instances that have been updated through the InstanceUpdateCmdBuffer on the UPrimitiveComponent.
	 *
	 * @param Primitive - primitive component to update
	 */
	virtual void UpdatePrimitiveInstances(UInstancedStaticMeshComponent* Primitive) = 0;

	/** 
	 * Updates the custom primitive data of a primitive component which has already been added to the scene. 
	 * 
	 * @param Primitive - Primitive component to update
	 */
	virtual void UpdateCustomPrimitiveData(UPrimitiveComponent* Primitive) = 0;
	/**
	 * Updates distance field scene data (transforms, uv scale, self-shadow bias, etc.) but doesn't change allocation in the atlas
	 */
	virtual void UpdatePrimitiveDistanceFieldSceneData_GameThread(UPrimitiveComponent* Primitive) {}
	/** Finds the  primitive with the associated component id. */
	virtual FPrimitiveSceneInfo* GetPrimitiveSceneInfo(int32 PrimitiveIndex) = 0;
	/** Get the primitive previous local to world (used for motion blur). Returns true if the matrix was set. */
	virtual bool GetPreviousLocalToWorld(const FPrimitiveSceneInfo* PrimitiveSceneInfo, FMatrix& OutPreviousLocalToWorld) const { return false; }
	/** 
	 * Adds a new light component to the scene
	 * 
	 * @param Light - light component to add
	 */
	virtual void AddLight(ULightComponent* Light) = 0;
	/** 
	 * Removes a light component from the scene
	 * 
	 * @param Light - light component to remove
	 */
	virtual void RemoveLight(ULightComponent* Light) = 0;
	/** 
	 * Adds a new light component to the scene which is currently invisible, but needed for editor previewing
	 * 
	 * @param Light - light component to add
	 */
	virtual void AddInvisibleLight(ULightComponent* Light) = 0;
	virtual void SetSkyLight(FSkyLightSceneProxy* Light) = 0;
	virtual void DisableSkyLight(FSkyLightSceneProxy* Light) = 0;

	virtual bool HasSkyLightRequiringLightingBuild() const = 0;
	virtual bool HasAtmosphereLightRequiringLightingBuild() const = 0;

	/** 
	 * Adds a new decal component to the scene
	 * 
	 * @param Component - component to add
	 */
	virtual void AddDecal(UDecalComponent* Component) = 0;
	/** 
	 * Removes a decal component from the scene
	 * 
	 * @param Component - component to remove
	 */
	virtual void RemoveDecal(UDecalComponent* Component) = 0;
	/** 
	 * Updates the transform of a decal which has already been added to the scene. 
	 *
	 * @param Decal - Decal component to update
	 */
	virtual void UpdateDecalTransform(UDecalComponent* Component) = 0;
	virtual void UpdateDecalFadeOutTime(UDecalComponent* Component) = 0;
	virtual void UpdateDecalFadeInTime(UDecalComponent* Component) = 0;

	/** Adds a reflection capture to the scene. */
	virtual void AddReflectionCapture(class UReflectionCaptureComponent* Component) {}

	/** Removes a reflection capture from the scene. */
	virtual void RemoveReflectionCapture(class UReflectionCaptureComponent* Component) {}

	/** Reads back reflection capture data from the GPU.  Very slow operation that blocks the GPU and rendering thread many times. */
	virtual void GetReflectionCaptureData(UReflectionCaptureComponent* Component, class FReflectionCaptureData& OutCaptureData) {}

	/** Updates a reflection capture's transform, and then re-captures the scene. */
	virtual void UpdateReflectionCaptureTransform(class UReflectionCaptureComponent* Component) {}

	/** 
	 * Allocates reflection captures in the scene's reflection cubemap array and updates them by recapturing the scene.
	 * Existing captures will only be updated.  Must be called from the game thread.
	 */
	virtual void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures, const TCHAR* CaptureReason, bool bVerifyOnlyCapturing, bool bCapturingForMobile, bool bInsideTick) {}
	virtual void ReleaseReflectionCubemap(UReflectionCaptureComponent* CaptureComponent) {}

	/**
	  * Resets reflection capture data to allow it to be rebuilt from scratch.  Allows shrinking of the number of items in
	  * the capture array, which otherwise only grows.  This can allow the user to solve cases where an out of memory (OOM)
	  * condition prevented captures from being generated.  The user can remove some items or reduce the capture resolution
	  * to fix the issue, and run "Build Reflection Captures" (which calls this function), instead of needing to restart
	  * the editor.  The flag "bOnlyIfOOM" only does the reset if an OOM had occurred.
	  */
	virtual void ResetReflectionCaptures(bool bOnlyIfOOM) {}

	UE_DEPRECATED(5.1, "This method now accepts a bInsideTick parameter, which specifies whether it's called during the frame Tick.")
	inline void AllocateReflectionCaptures(const TArray<UReflectionCaptureComponent*>& NewCaptures, const TCHAR* CaptureReason, bool bVerifyOnlyCapturing, bool bCapturingForMobile)
	{
		AllocateReflectionCaptures(NewCaptures, CaptureReason, bVerifyOnlyCapturing, bCapturingForMobile, false);
	}

	/** 
	 * Updates the contents of the given sky capture by rendering the scene. 
	 * This must be called on the game thread.
	 */
	virtual void UpdateSkyCaptureContents(const USkyLightComponent* CaptureComponent, bool bCaptureEmissiveOnly, UTextureCube* SourceCubemap, FTexture* OutProcessedTexture, float& OutAverageBrightness, FSHVectorRGB3& OutIrradianceEnvironmentMap, TArray<FFloat16Color>* OutRadianceMap) {}

	virtual void AllocateAndCaptureFrameSkyEnvMap(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer, FViewInfo& MainView, bool bShouldRenderSkyAtmosphere, bool bShouldRenderVolumetricCloud, FInstanceCullingManager& InstanceCullingManager) {}
	virtual void ValidateSkyLightRealTimeCapture(FRDGBuilder& GraphBuilder, const FViewInfo& View, FRDGTextureRef SceneColorTexture) {}

	virtual void AddPlanarReflection(class UPlanarReflectionComponent* Component) {}
	virtual void RemovePlanarReflection(class UPlanarReflectionComponent* Component) {}
	virtual void UpdatePlanarReflectionTransform(UPlanarReflectionComponent* Component) {}

	/** 
	* Updates the contents of the given scene capture by rendering the scene. 
	* This must be called on the game thread.
	*/
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponent2D* CaptureComponent) {}
	virtual void UpdateSceneCaptureContents(class USceneCaptureComponentCube* CaptureComponent) {}
	virtual void UpdatePlanarReflectionContents(class UPlanarReflectionComponent* CaptureComponent, class FSceneRenderer& MainSceneRenderer) {}
	
	virtual void AddPrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) {}
	virtual void RemovePrecomputedLightVolume(const class FPrecomputedLightVolume* Volume) {}

	virtual bool HasPrecomputedVolumetricLightmap_RenderThread() const { return false; }
	virtual void AddPrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume, bool bIsPersistentLevel) {}
	virtual void RemovePrecomputedVolumetricLightmap(const class FPrecomputedVolumetricLightmap* Volume) {}

	/** Add a runtime virtual texture object to the scene. */
	virtual void AddRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component) {}

	/** Removes a runtime virtual texture object from the scene. */
	virtual void RemoveRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component) {}

	/* Get the bitmasks describing which virtual texture objects will hide the associated primitives. */
	virtual void GetRuntimeVirtualTextureHidePrimitiveMask(uint8& bHideMaskEditor, uint8& bHideMaskGame) const {}

	/** Invalidates pages in a runtime virtual texture object. */
	virtual void InvalidateRuntimeVirtualTexture(class URuntimeVirtualTextureComponent* Component, FBoxSphereBounds const& WorldBounds) {}

	/** Mark scene as needing to restart path tracer accumulation. */
	virtual void InvalidatePathTracedOutput() {}

	/**  Invalidates Lumen surface cache and forces it to be refreshed. Useful to make material updates more responsive. */
	virtual void InvalidateLumenSurfaceCache_GameThread(UPrimitiveComponent* Component) {}

	/** 
	 * Retrieves primitive uniform shader parameters that are internal to the renderer.
	 */
	virtual void GetPrimitiveUniformShaderParameters_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool& bHasPrecomputedVolumetricLightmap, FMatrix& PreviousLocalToWorld, int32& SingleCaptureIndex, bool& OutputVelocity) const {}
	 
	/** 
	 * Updates the transform of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightTransform(ULightComponent* Light) = 0;
	/** 
	 * Updates the color and brightness of a light which has already been added to the scene. 
	 *
	 * @param Light - light component to update
	 */
	virtual void UpdateLightColorAndBrightness(ULightComponent* Light) = 0;

	/** Sets the precomputed visibility handler for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVisibility(const class FPrecomputedVisibilityHandler* PrecomputedVisibilityHandler) {}

	/** Sets the precomputed volume distance field for the scene, or NULL to clear the current one. */
	virtual void SetPrecomputedVolumeDistanceField(const class FPrecomputedVolumeDistanceField* PrecomputedVolumeDistanceField) {}

	/** Updates all static draw lists. */
	virtual void UpdateStaticDrawLists() {}

	/** Update render states that possibly cached inside renderer, like mesh draw commands. More lightweight than re-registering the scene proxy. */
	virtual void UpdateCachedRenderStates(class FPrimitiveSceneProxy* SceneProxy) {}
	
	/** Updates the selected state values that might be cached inside the renderer */
	virtual void UpdatePrimitiveSelectedState_RenderThread(const FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsSelected) {}

	/** Updates the velocity state values that might be cached inside the renderer */
	virtual void UpdatePrimitiveVelocityState_RenderThread(FPrimitiveSceneInfo* PrimitiveSceneInfo, bool bIsBeingMoved) {}

	/** 
	 * Adds a new exponential height fog component to the scene
	 * 
	 * @param FogComponent - fog component to add
	 */	
	virtual void AddExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;
	/** 
	 * Removes a exponential height fog component from the scene
	 * 
	 * @param FogComponent - fog component to remove
	 */	
	virtual void RemoveExponentialHeightFog(class UExponentialHeightFogComponent* FogComponent) = 0;
	/**
	 * @return True if there are any exponential height fog potentially enabled in the scene
	 */
	virtual bool HasAnyExponentialHeightFog() const = 0;

	/**
	 * Adds the unique volumetric cloud component to the scene
	 *
	 * @param SkyAtmosphereSceneProxy - the sky atmosphere proxy
	 */
	virtual void AddSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy, bool bStaticLightingBuilt) = 0;
	/**
	 * Removes the unique volumetric cloud component to the scene
	 *
	 * @param SkyAtmosphereSceneProxy - the sky atmosphere proxy
	 */
	virtual void RemoveSkyAtmosphere(FSkyAtmosphereSceneProxy* SkyAtmosphereSceneProxy) = 0;
	/**
	 * Returns the scene's unique info if it exists
	 */
	virtual FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() = 0;
	virtual const FSkyAtmosphereRenderSceneInfo* GetSkyAtmosphereSceneInfo() const = 0;

	/**
	 * Adds the unique volumetric cloud component to the scene
	 *
	 * @param VolumetricCloudSceneProxy - the sky atmosphere proxy
	 */
	virtual void AddVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) = 0;
	/**
	 * Removes the unique volumetric cloud component to the scene
	 *
	 * @param VolumetricCloudSceneProxy - the sky atmosphere proxy
	 */
	virtual void RemoveVolumetricCloud(FVolumetricCloudSceneProxy* VolumetricCloudSceneProxy) = 0;

	/**
	 * Adds a hair strands proxy to the scene
	 *
	 * @param Proxy - the hair strands proxy
	 */
	virtual void AddHairStrands(FHairStrandsInstance* Proxy) = 0;

	/**
	 * Removes a hair strands proxy to the scene
	 *
	 * @param Proxy - the hair strands proxy
	 */
	virtual void RemoveHairStrands(FHairStrandsInstance* Proxy) = 0;

	/**
	 * Return the rect. light atlas slot information corresponding to the rect light proxy
	 *
	 * @param Proxy - the rect light proxy
	 * @param Out - the light parameters which will be filled with the rect light atlas information
	 */
	virtual void GetRectLightAtlasSlot(const FRectLightSceneProxy* Proxy, FLightRenderParameters* Out) = 0;

	/**
	 * Set the physics field scene proxy to the scene
	 *
	 * @param PhysicsFieldSceneProxy - the physics field scene proxy
	 */
	virtual void SetPhysicsField(class FPhysicsFieldSceneProxy* PhysicsFieldSceneProxy) = 0;

	/**
	 * Reset the physics field scene proxy
	 */
	virtual void ResetPhysicsField() = 0;

	/**
	 * Set the shader print/debug cvars to be able to show the fields
	 */
	virtual void ShowPhysicsField() = 0;

	/**
	 * Reset the physics field scene proxy
	 */
	virtual void UpdatePhysicsField(FRDGBuilder& GraphBuilder, FViewInfo& View) = 0;

	UE_DEPRECATED(5.0, "This method has been refactored to use an FRDGBuilder instead.")
	virtual void UpdatePhysicsField(FRHICommandListImmediate& RHICmdList, FViewInfo& View) {}

	/**
	 * Returns the scene's unique info if it exists
	 */
	virtual FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() = 0;
	virtual const FVolumetricCloudRenderSceneInfo* GetVolumetricCloudSceneInfo() const = 0;

	/**
	 * Adds a wind source component to the scene.
	 * @param WindComponent - The component to add.
	 */
	virtual void AddWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Removes a wind source component from the scene.
	 * @param WindComponent - The component to remove.
	 */
	virtual void RemoveWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Update a wind source component from the scene.
	 * @param WindComponent - The component to update.
	 */
	virtual void UpdateWindSource(class UWindDirectionalSourceComponent* WindComponent) = 0;
	/**
	 * Accesses the wind source list.  Must be called in the rendering thread.
	 * @return The list of wind sources in the scene.
	 */
	virtual const TArray<class FWindSourceSceneProxy*>& GetWindSources_RenderThread() const = 0;

	/** Accesses wind parameters.  XYZ will contain wind direction * Strength, W contains wind speed. */
	virtual void GetWindParameters(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** Accesses wind parameters safely for game thread applications */
	virtual void GetWindParameters_GameThread(const FVector& Position, FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** Same as GetWindParameters, but ignores point wind sources. */
	virtual void GetDirectionalWindParameters(FVector& OutDirection, float& OutSpeed, float& OutMinGustAmt, float& OutMaxGustAmt) const = 0;

	/** 
	 * Adds a SpeedTree wind computation object to the scene.
	 * @param StaticMesh - The SpeedTree static mesh whose wind to add.
	 */
	virtual void AddSpeedTreeWind(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) = 0;

	/** 
	 * Removes a SpeedTree wind computation object to the scene.
	 * @param StaticMesh - The SpeedTree static mesh whose wind to remove.
	 */
	virtual void RemoveSpeedTreeWind_RenderThread(class FVertexFactory* VertexFactory, const class UStaticMesh* StaticMesh) = 0;

	/** Ticks the SpeedTree wind object and updates the uniform buffer. */
	virtual void UpdateSpeedTreeWind(double CurrentTime) = 0;

	/** 
	 * Looks up the SpeedTree uniform buffer for the passed in vertex factory.
	 * @param VertexFactory - The vertex factory registered for SpeedTree.
	 */
	virtual FRHIUniformBuffer* GetSpeedTreeUniformBuffer(const FVertexFactory* VertexFactory) const = 0;

	virtual void AddLumenSceneCard(class ULumenSceneCardComponent* LumenSceneCardComponent) {};
	virtual void UpdateLumenSceneCardTransform(class ULumenSceneCardComponent* LumenSceneCardComponent) {};
	virtual void RemoveLumenSceneCard(class ULumenSceneCardComponent* LumenSceneCardComponent) {};

	/**
	 * Release this scene and remove it from the rendering thread
	 */
	virtual void Release() = 0;
	/**
	 * Retrieves the lights interacting with the passed in primitive and adds them to the out array.
	 *
	 * @param	Primitive				Primitive to retrieve interacting lights for
	 * @param	RelevantLights	[out]	Array of lights interacting with primitive
	 */
	virtual void GetRelevantLights( UPrimitiveComponent* Primitive, TArray<const ULightComponent*>* RelevantLights ) const = 0;
	/**
	 * Indicates if hit proxies should be processed by this scene
	 *
	 * @return true if hit proxies should be rendered in this scene.
	 */
	virtual bool RequiresHitProxies() const = 0;
	/**
	 * Get the optional UWorld that is associated with this scene
	 * 
	 * @return UWorld instance used by this scene
	 */
	virtual class UWorld* GetWorld() const = 0;
	
	/**
	 * Return the scene to be used for rendering. Note that this can return null if rendering has
	 * been disabled!
	 */
	virtual class FScene* GetRenderScene()
	{
		return nullptr;
	}

	virtual void OnWorldCleanup()
	{
	}
	virtual void UpdateSceneSettings(AWorldSettings* WorldSettings) {}

	/**
	* Gets the GPU Skin Cache system associated with the scene.
	*/
	virtual class FGPUSkinCache* GetGPUSkinCache()
	{
		return nullptr;
	}

	/**
	 * Gets the compute work scheduler objects associated with the scene.
	 */
	virtual void GetComputeTaskWorkers(TArray<class IComputeTaskWorker*>& OutWorkers) const {}

	/**
	 * Sets the FX system associated with the scene.
	 */
	virtual void SetFXSystem( class FFXSystemInterface* InFXSystem ) = 0;

	/**
	 * Get the FX system associated with the scene.
	 */
	virtual class FFXSystemInterface* GetFXSystem() = 0;

	virtual void DumpUnbuiltLightInteractions( FOutputDevice& Ar ) const { }

	/** Updates the scene's list of parameter collection id's and their uniform buffers. */
	virtual void UpdateParameterCollections(const TArray<class FMaterialParameterCollectionInstanceResource*>& InParameterCollections) {}

	/**
	 * Exports the scene.
	 *
	 * @param	Ar		The Archive used for exporting.
	 **/
	virtual void Export( FArchive& Ar ) const
	{}

	/**
	 * Shifts scene data by provided delta
	 * Called on world origin changes
	 * 
	 * @param	InOffset	Delta to shift scene by
	 */
	virtual void ApplyWorldOffset(FVector InOffset) {}

	/**
	 * Notification that level was added to a world
	 * 
	 * @param	InLevelName		Level name
	 */
	virtual void OnLevelAddedToWorld(FName InLevelName, UWorld* InWorld, bool bIsLightingScenario) {}
	virtual void OnLevelRemovedFromWorld(FName InLevelName, UWorld* InWorld, bool bIsLightingScenario) {}

	/**
	 * @return True if there are any lights in the scene
	 */
	virtual bool HasAnyLights() const = 0;

	virtual bool IsEditorScene() const { return false; }

	ERHIFeatureLevel::Type GetFeatureLevel() const { return FeatureLevel; }

	EShaderPlatform GetShaderPlatform() const { return GShaderPlatformForFeatureLevel[GetFeatureLevel()]; }

	static EShadingPath GetShadingPath(ERHIFeatureLevel::Type InFeatureLevel)
	{
		if (InFeatureLevel >= ERHIFeatureLevel::SM5)
		{
			return EShadingPath::Deferred;
		}
		else
		{
			return EShadingPath::Mobile;
		}
	}

	EShadingPath GetShadingPath() const
	{
		return GetShadingPath(GetFeatureLevel());
	}

#if WITH_EDITOR
	/**
	 * Initialize the pixel inspector buffers.
	 * @return True if implemented false otherwise.
	 */
	virtual bool InitializePixelInspector(FRenderTarget* BufferFinalColor, FRenderTarget* BufferSceneColor, FRenderTarget* BufferDepth, FRenderTarget* BufferHDR, FRenderTarget* BufferA, FRenderTarget* BufferBCDEF, int32 BufferIndex)
	{
		return false;
	}

	/**
	 * Add a pixel inspector request.
	 * @return True if implemented false otherwise.
	 */
	virtual bool AddPixelInspectorRequest(class FPixelInspectorRequest *PixelInspectorRequest)
	{
		return false;
	}
#endif //WITH_EDITOR

	/**
	 * Returns the FPrimitiveComponentId for all primitives in the scene
	 */
	virtual ENGINE_API TArray<FPrimitiveComponentId> GetScenePrimitiveComponentIds() const;

	virtual void StartFrame() {}
	virtual uint32 GetFrameNumber() const { return 0; }
	virtual void IncrementFrameNumber() {}

#if RHI_RAYTRACING
	virtual void UpdateCachedRayTracingState(class FPrimitiveSceneProxy* SceneProxy) {}
	virtual class FRayTracingDynamicGeometryCollection* GetRayTracingDynamicGeometryCollection() { return nullptr; }
	virtual class FRayTracingSkinnedGeometryUpdateQueue* GetRayTracingSkinnedGeometryUpdateQueue() { return nullptr; }
#endif

	virtual bool RequestGPUSceneUpdate(FPrimitiveSceneInfo& PrimitiveSceneInfo, EPrimitiveDirtyState PrimitiveDirtyState) { return false; }

protected:
	virtual ~FSceneInterface() {}

	/** This scene's feature level */
	ERHIFeatureLevel::Type FeatureLevel;

	friend class FSceneViewStateReference;
};
