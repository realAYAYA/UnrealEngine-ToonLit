// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptInterface.h"
#include "Engine/BlendableInterface.h"
#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent.h"
#include "SceneCaptureComponent2D.generated.h"

class ISceneViewExtension;
class FSceneInterface;

/**
 *	Used to capture a 'snapshot' of the scene from a single plane and feed it to a render target.
 */
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta=(BlueprintSpawnableComponent), MinimalAPI)
class USceneCaptureComponent2D : public USceneCaptureComponent
{
	GENERATED_UCLASS_BODY()
		
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Projection Type"))
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;

	/** Camera field of view (in degrees). */
	UPROPERTY(interp, EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Field of View", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", editcondition = "ProjectionType==0"))
	float FOVAngle;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta = (editcondition = "ProjectionType==1"))
	float OrthoWidth;

	/** Automatically determine a min/max Near/Far clip plane position depending on OrthoWidth value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta = (editcondition = "ProjectionType==1"))
	bool bAutoCalculateOrthoPlanes;

	/** Manually adjusts the planes of this camera, maintaining the distance between them. Positive moves out to the farplane, negative towards the near plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta = (editcondition = "ProjectionType==1 && bAutoCalculateOrthoPlanes"))
	float AutoPlaneShift;

	/** Adjusts the near/far planes and the view origin of the current camera automatically to avoid clipping and light artefacting*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta = (editcondition = "ProjectionType==1"))
	bool bUpdateOrthoPlanes;

	/** If UpdateOrthoPlanes is enabled, this setting will use the cameras current height to compensate the distance to the general view (as a pseudo distance to view target when one isn't present) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta = (editcondition = "ProjectionType==1 && bUpdateOrthoPlanes"))
	bool bUseCameraHeightAsViewTarget;

	/** Output render target of the scene capture that can be read in materials. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TObjectPtr<class UTextureRenderTarget2D> TextureTarget;

	/** When enabled, the scene capture will composite into the render target instead of overwriting its contents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TEnumAsByte<enum ESceneCaptureCompositeMode> CompositeMode;

	UPROPERTY(interp, Category=PostProcessVolume, meta=(ShowOnlyInnerProperties))
	struct FPostProcessSettings PostProcessSettings;

	/** Range (0.0, 1.0) where 0 indicates no effect, 1 indicates full effect. */
	UPROPERTY(interp, Category=PostProcessVolume, BlueprintReadWrite, meta=(UIMin = "0.0", UIMax = "1.0"))
	float PostProcessBlendWeight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Projection, meta = (InlineEditConditionToggle))
	uint32 bOverride_CustomNearClippingPlane : 1;

	/** 
	 * Set bOverride_CustomNearClippingPlane to true if you want to use a custom clipping plane instead of GNearClippingPlane.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Projection, meta = (editcondition = "bOverride_CustomNearClippingPlane"))
	float CustomNearClippingPlane = 0;

	/** Whether a custom projection matrix will be used during rendering. Use with caution. Does not currently affect culling */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Projection)
	bool bUseCustomProjectionMatrix;

	/** The custom projection matrix to use */
	UPROPERTY(BlueprintReadWrite, AdvancedDisplay, Category = Projection)
	FMatrix CustomProjectionMatrix;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "5.4 - bUseFauxOrthoViewPos has been deprecated alongside updates to Orthographic camera fixes"))
	bool bUseFauxOrthoViewPos = false;

	/** Render the scene in n frames (i.e TileCount) - Ignored in Perspective mode, works only in Orthographic mode when CaptureSource uses SceneColor (not FinalColor)
	* If CaptureSource uses FinalColor, tiling will be ignored and a Warning message will be logged	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Projection, meta = (editcondition = "ProjectionType==1"))
	bool bEnableOrthographicTiling = false;

	/** Number of X tiles to render. Ignored in Perspective mode, works only in Orthographic mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Projection, meta = (ClampMin = "1", ClampMax = "64", editcondition = "ProjectionType==1 && bEnableOrthographicTiling"))
	int32 NumXTiles = 4;

	/** Number of Y tiles to render. Ignored in Perspective mode, works only in Orthographic mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Projection, meta = (ClampMin = "1", ClampMax = "64", editcondition = "ProjectionType==1 && bEnableOrthographicTiling"))
	int32 NumYTiles = 4;

	/**
	 * Enables a clip plane while rendering the scene capture which is useful for portals.  
	 * The global clip plane must be enabled in the renderer project settings for this to work.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	bool bEnableClipPlane;

	/** Base position for the clip plane, can be any position on the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	FVector ClipPlaneBase;

	/** Normal for the plane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
	FVector ClipPlaneNormal;
	
	/** Render scene capture as additional render passes of the main renderer rather than as an independent renderer. Can only apply to scene depth and device depth modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = SceneCapture, meta = (EditCondition = "CaptureSource == ESceneCaptureSource::SCS_SceneDepth || CaptureSource == ESceneCaptureSource::SCS_DeviceDepth"))
	bool bRenderInMainRenderer = false;

	/** 
	 * True if we did a camera cut this frame. Automatically reset to false at every capture.
	 * This flag affects various things in the renderer (such as whether to use the occlusion queries from last frame, and motion blur).
	 * Similar to UPlayerCameraManager::bGameCameraCutThisFrame.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = SceneCapture)
	uint32 bCameraCutThisFrame : 1;

	/** Whether to only render exponential height fog on opaque pixels which were rendered by the scene capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture, meta = (DisplayName = "Fog only on rendered pixels"))
	uint32 bConsiderUnrenderedOpaquePixelAsFullyTranslucent : 1;

	/** Array of scene view extensions specifically to apply to this scene capture */
	TArray< TWeakPtr<ISceneViewExtension, ESPMode::ThreadSafe> > SceneViewExtensions;

	/** Which tile to render of the orthographic view (ignored in Perspective mode) */
	int32 TileID = 0;

	//~ Begin UActorComponent Interface
	ENGINE_API virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	ENGINE_API virtual void OnRegister() override;
	ENGINE_API virtual void SendRenderTransform_Concurrent() override;
	virtual bool RequiresGameThreadEndOfFrameUpdates() const override
	{
		// this method could probably be removed allowing them to run on any thread, but it isn't worth the trouble
		return true;
	}
	ENGINE_API virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Reset Orthographic tiling counter */
	ENGINE_API void ResetOrthographicTilingCounter();

	//~ End UActorComponent Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	static ENGINE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	ENGINE_API virtual void Serialize(FArchive& Ar);

	//~ End UObject Interface

	ENGINE_API void SetCameraView(const FMinimalViewInfo& DesiredView);

	ENGINE_API virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& OutDesiredView);

	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { PostProcessSettings.AddBlendable(InBlendableObject, InWeight); }

	/** Removes a blendable. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void RemoveBlendable(TScriptInterface<IBlendableInterface> InBlendableObject) { PostProcessSettings.RemoveBlendable(InBlendableObject); }

	/**
	 * Render the scene to the texture the next time the main view is rendered.
	 * If r.SceneCapture.CullByDetailMode is set, nothing will happen if DetailMode is higher than r.DetailMode.
	 */
	ENGINE_API void CaptureSceneDeferred();

	// For backwards compatibility
	void UpdateContent() { CaptureSceneDeferred(); }

	/** 
	 * Render the scene to the texture target immediately.  
	 * This should not be used if bCaptureEveryFrame is enabled, or the scene capture will render redundantly. 
	 * If r.SceneCapture.CullByDetailMode is set, nothing will happen if DetailMode is higher than r.DetailMode.
	 */
	UFUNCTION(BlueprintCallable,Category = "Rendering|SceneCapture")
	ENGINE_API void CaptureScene();

	ENGINE_API void UpdateSceneCaptureContents(FSceneInterface* Scene) override;

	/* Return if orthographic tiling rendering is enabled or not */
	ENGINE_API bool GetEnableOrthographicTiling() const;

	/* Return number of X tiles to render (to be used when orthographic tiling rendering is enabled) */
	ENGINE_API int32 GetNumXTiles() const;

	/* Return number of Y tiles to render (to be used when orthographic tiling rendering is enabled) */
	ENGINE_API int32 GetNumYTiles() const;

#if WITH_EDITORONLY_DATA
	ENGINE_API void UpdateDrawFrustum();

	/** The frustum component used to show visually where the camera field of view is */
	TObjectPtr<class UDrawFrustumComponent> DrawFrustum;
#endif
};
