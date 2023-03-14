// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MoviePipelineImagePassBase.h"
#include "ActorLayerUtilities.h"
#include "OpenColorIODisplayExtension.h"
#include "MoviePipelineDeferredPasses.generated.h"

class UTextureRenderTarget2D;
struct FImageOverlappedAccumulator;
class FSceneViewFamily;
class FSceneView;
struct FAccumulatorPool;

USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINERENDERPASSES_API FMoviePipelinePostProcessPass
{
	GENERATED_BODY()

public:
	/** Additional passes add a significant amount of render time. May produce multiple output files if using Screen Percentage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bEnabled = false;

	/** 
	* Material should be set to Post Process domain, and Blendable Location = After Tonemapping. 
	* This will need bDisableMultisampleEffects enabled for pixels to line up(ie : no DoF, MotionBlur, TAA)
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	TSoftObjectPtr<UMaterialInterface> Material;
};

UCLASS(BlueprintType)
class MOVIERENDERPIPELINERENDERPASSES_API UMoviePipelineDeferredPassBase : public UMoviePipelineImagePassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPassBase();
	
protected:
	// UMoviePipelineRenderPass API
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState) override;
	virtual void TeardownImpl() override;
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_Lit", "Deferred Rendering"); }
#endif
	virtual void MoviePipelineRenderShowFlagOverride(FEngineShowFlags& OutShowFlag) override;
	virtual void GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses) override;
	virtual bool IsAntiAliasingSupported() const { return !bDisableMultisampleEffects; }
	virtual int32 GetOutputFileSortingOrder() const override { return 0; }
	virtual bool IsAlphaInTonemapperRequiredImpl() const override { return bAccumulatorIncludesAlpha; }
	virtual FSceneViewStateInterface* GetSceneViewStateInterface(IViewCalcPayload* OptPayload = nullptr) override;
	virtual void AddViewExtensions(FSceneViewFamilyContext& InContext, FMoviePipelineRenderPassMetrics& InOutSampleState) override;
	virtual bool IsAutoExposureAllowed(const FMoviePipelineRenderPassMetrics& InSampleState) const override;
	virtual void BlendPostProcessSettings(FSceneView* InView, FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr);
	virtual UE::MoviePipeline::FImagePassCameraViewData GetCameraInfo(FMoviePipelineRenderPassMetrics& InOutSampleState, IViewCalcPayload* OptPayload = nullptr) const;
	// ~UMoviePipelineRenderPass

	// FGCObject Interface
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	// ~FGCObject Interface

	TFunction<void(TUniquePtr<FImagePixelData>&&)> MakeForwardingEndpoint(const FMoviePipelinePassIdentifier InPassIdentifier, const FMoviePipelineRenderPassMetrics& InSampleState);
	virtual void PostRendererSubmission(const FMoviePipelineRenderPassMetrics& InSampleState, const FMoviePipelinePassIdentifier InPassIdentifier, const int32 InSortingOrder, FCanvas& InCanvas);

	virtual int32 GetNumCamerasToRender() const;
	virtual FString GetCameraName(const int32 InCameraIndex) const;
	virtual FString GetCameraNameOverride(const int32 InCameraIndex) const;

	virtual FMoviePipelineRenderPassMetrics GetRenderPassMetricsForCamera(const int32 InCameraIndex, const FMoviePipelineRenderPassMetrics& InSampleState) const;
	virtual FIntPoint GetEffectiveOutputResolutionForCamera(const int32 InCameraIndex) const;

	bool IsUsingDataLayers() const;
	int32 GetNumStencilLayers() const;
	TArray<FString> GetStencilLayerNames() const;
	bool IsActorInLayer(AActor* InActor, int32 InLayerIndex) const;
	bool IsActorInAnyStencilLayer(AActor* InActor) const;
	FSoftObjectPath GetValidDataLayerByIndex(const int32 InIndex) const;

	bool CheckIfPathTracerIsSupported() const;
	void PathTracerValidationImpl();

public:
	/**
	* Should multiple temporal/spatial samples accumulate the alpha channel? This requires r.PostProcessing.PropagateAlpha
	* to be set to 1 or 2 (see "Enable Alpha Channel Support in Post Processing" under Project Settings > Rendering). This adds
	* ~30% cost to the accumulation so you should not enable it unless necessary. You must delete both the sky and fog to ensure
	* that they do not make all pixels opaque.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	bool bAccumulatorIncludesAlpha;

	/**
	* Certain passes don't support post-processing effects that blend pixels together. These include effects like
	* Depth of Field, Temporal Anti-Aliasing, Motion Blur and chromattic abberation. When these post processing
	* effects are used then each final output pixel is composed of the influence of many other pixels which is
	* undesirable when rendering out an object id pass (which does not support post processing). This checkbox lets
	* you disable them on a per-render basis instead of having to disable them in the editor as well.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post Processing")
	bool bDisableMultisampleEffects;

	/**
	* Should the additional post-process materials write out to a 32-bit render target instead of 16-bit?
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deferred Renderer Data")
	bool bUse32BitPostProcessMaterials;

	/**
	* An array of additional post-processing materials to run after the frame is rendered. Using this feature may add a notable amount of render time.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Deferred Renderer Data")
	TArray<FMoviePipelinePostProcessPass> AdditionalPostProcessMaterials;

	/**
	* This can be turned off if you're only doing a stencil-layer based render and don't need the main non-stencil approach.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stencil Clip Layers")
	bool bRenderMainPass;

	/**
	* If true, an additional stencil layer will be rendered which contains all objects which do not belong to layers
	* specified in the Stencil Layers. This is useful for wanting to isolate one or two layers but still have everything
	* else to composite them over without having to remember to add all objects to a default layer.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stencil Clip Layers")
	bool bAddDefaultLayer;

	/** 
	* For each layer in the array, the world will be rendered and then a stencil mask will clip all pixels not affected
	* by the objects on that layer. This is NOT a true layer system, as translucent objects will show opaque objects from
	* another layer behind them. Does not write out additional post-process materials per-layer as they will match the
	* base layer. Only works with materials that can write to custom depth.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stencil Clip Layers")
	TArray<FActorLayer> ActorLayers;
	
	UE_DEPRECATED(5.1, "Use ActorLayers property instead.")
	TArray<FActorLayer> StencilLayers;
	
	/**
	* If the map you are working with is a World Partition map, you can specify Data layers instead of Actor Layers. If any
	* Data Layers are specified, this will take precedence over any ActorLayers in this config. Does not affect whether or
	* not the Data Layers are actually loaded, you must ensure layers are loaded for rendering.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowedClasses = "/Script/Engine.DataLayerAsset"), Category = "Stencil Clip Layers")
	TArray<FSoftObjectPath> DataLayers;

protected:
	/** While rendering, store an array of the non-null valid materials loaded from AdditionalPostProcessMaterials. Cleared on teardown. */
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<UMaterialInterface>> ActivePostProcessMaterials;

	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UMaterialInterface> StencilLayerMaterial;

	struct FMultiCameraViewStateData
	{
		struct FPerTile
		{
			TArray<FSceneViewStateReference> SceneViewStates;
		};

		TMap<FIntPoint, FPerTile> TileData;
	};

	TArray<FMultiCameraViewStateData> CameraViewStateData;

	// Cache the custom stencil value. Only has meaning if they have stencil layers.
	TOptional<int32> PreviousCustomDepthValue;
	/** Cache the previous dump frames as HDR value. Only used if using 32-bit post processing. */
	TOptional<int32> PreviousDumpFramesValue;
	/** Cache the previous color format value. Only used if using 32-bit post processing. */
	TOptional<int32> PreviousColorFormatValue;

	TSharedPtr<FAccumulatorPool, ESPMode::ThreadSafe> AccumulatorPool;
	/** The lifetime of this SceneViewExtension is only during the rendering process. It is destroyed as part of TearDown. */
	TSharedPtr<FOpenColorIODisplayExtension, ESPMode::ThreadSafe> OCIOSceneViewExtension;
public:
	static FString StencilLayerMaterialAsset;
	static FString DefaultDepthAsset;
	static FString DefaultMotionVectorsAsset;
};



UCLASS(BlueprintType)
class UMoviePipelineDeferredPass_Unlit : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPass_Unlit() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("Unlit");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_Unlit", "Deferred Rendering (Unlit)"); }
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutViewModeIndex = EViewModeIndex::VMI_Unlit;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }

};

UCLASS(BlueprintType)
class UMoviePipelineDeferredPass_DetailLighting : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPass_DetailLighting() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("DetailLightingOnly");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_DetailLighting", "Deferred Rendering (Detail Lighting)"); }
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutShowFlag.SetLightingOnlyOverride(true);
		OutViewModeIndex = EViewModeIndex::VMI_Lit_DetailLighting;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }

};

UCLASS(BlueprintType)
class UMoviePipelineDeferredPass_LightingOnly : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPass_LightingOnly() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("LightingOnly");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_LightingOnly", "Deferred Rendering (Lighting Only)"); }
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutShowFlag.SetLightingOnlyOverride(true);
		OutViewModeIndex = EViewModeIndex::VMI_LightingOnly;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }

};

UCLASS(BlueprintType)
class UMoviePipelineDeferredPass_ReflectionsOnly : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPass_ReflectionsOnly() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("ReflectionsOnly");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_ReflectionsOnly", "Deferred Rendering (Reflections Only)"); }
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutShowFlag.SetReflectionOverride(true);
		OutViewModeIndex = EViewModeIndex::VMI_ReflectionOverride;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }
};


UCLASS(BlueprintType)
class UMoviePipelineDeferredPass_PathTracer : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UMoviePipelineDeferredPass_PathTracer() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("PathTracer");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_PathTracer", "Path Tracer"); }
	virtual FText GetFooterText(UMoviePipelineExecutorJob* InJob) const override;
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutShowFlag.SetPathTracing(true);
		OutShowFlag.SetMotionBlur(!bReferenceMotionBlur);
		OutViewModeIndex = EViewModeIndex::VMI_PathTracing;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }

	virtual bool IsAntiAliasingSupported() const { return false; }
	virtual void ValidateStateImpl() override;
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;

	/** When enabled, the path tracer will blend all spatial and temporal samples prior to the denoising and will disable post-processed motion blur.
	 *  In this mode it is possible to use higher temporal sample counts to improve the motion blur quality.
	 *  When this option is disabled, the path tracer will accumulate spatial samples, but denoise them prior to accumulation of temporal samples.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reference Motion Blur")
	bool bReferenceMotionBlur;
};
