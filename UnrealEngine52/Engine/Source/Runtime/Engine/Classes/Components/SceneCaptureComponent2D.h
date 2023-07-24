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
UCLASS(hidecategories=(Collision, Object, Physics, SceneComponent), ClassGroup=Rendering, editinlinenew, meta=(BlueprintSpawnableComponent))
class ENGINE_API USceneCaptureComponent2D : public USceneCaptureComponent
{
	GENERATED_UCLASS_BODY()
		
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Projection Type"))
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;

	/** Camera field of view (in degrees). */
	UPROPERTY(interp, EditAnywhere, BlueprintReadWrite, Category=Projection, meta=(DisplayName = "Field of View", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0"))
	float FOVAngle;

	/** The desired width (in world units) of the orthographic view (ignored in Perspective mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection)
	float OrthoWidth;

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

	UPROPERTY(EditAnywhere, Category = Projection, meta = (InlineEditConditionToggle))
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

	/** In case of orthographic camera, generate a fake view position that has a non-zero W component. The view position will be derived based on the view matrix. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = Projection, meta = (editcondition = "ProjectionType==1"))
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
	
	/** 
	 * True if we did a camera cut this frame. Automatically reset to false at every capture.
	 * This flag affects various things in the renderer (such as whether to use the occlusion queries from last frame, and motion blur).
	 * Similar to UPlayerCameraManager::bGameCameraCutThisFrame.
	 */
	UPROPERTY(Transient, BlueprintReadWrite, Category = SceneCapture)
	uint32 bCameraCutThisFrame : 1;

	/** Treat unrendered opaque pixels as fully translucent. This is important for effects such as exponential weight fog, so it does not get applied on unrendered opaque pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	uint32 bConsiderUnrenderedOpaquePixelAsFullyTranslucent : 1;

	/** Array of scene view extensions specifically to apply to this scene capture */
	TArray< TWeakPtr<ISceneViewExtension, ESPMode::ThreadSafe> > SceneViewExtensions;

	/** Which tile to render of the orthographic view (ignored in Perspective mode) */
	int32 TileID = 0;

	//~ Begin UActorComponent Interface
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	virtual void OnRegister() override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual bool RequiresGameThreadEndOfFrameUpdates() const override
	{
		// this method could probably be removed allowing them to run on any thread, but it isn't worth the trouble
		return true;
	}
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Reset Orthographic tiling counter */
	void ResetOrthographicTilingCounter();

	//~ End UActorComponent Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void Serialize(FArchive& Ar);

	//~ End UObject Interface

	void SetCameraView(const FMinimalViewInfo& DesiredView);

	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& OutDesiredView);

	/** Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void AddOrUpdateBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight = 1.0f) { PostProcessSettings.AddBlendable(InBlendableObject, InWeight); }

	/** Removes a blendable. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void RemoveBlendable(TScriptInterface<IBlendableInterface> InBlendableObject) { PostProcessSettings.RemoveBlendable(InBlendableObject); }

	/** Render the scene to the texture the next time the main view is rendered. */
	void CaptureSceneDeferred();

	// For backwards compatibility
	void UpdateContent() { CaptureSceneDeferred(); }

	/** 
	 * Render the scene to the texture target immediately.  
	 * This should not be used if bCaptureEveryFrame is enabled, or the scene capture will render redundantly. 
	 */
	UFUNCTION(BlueprintCallable,Category = "Rendering|SceneCapture")
	void CaptureScene();

	void UpdateSceneCaptureContents(FSceneInterface* Scene) override;

	/* Return if orthographic tiling rendering is enabled or not */
	bool GetEnableOrthographicTiling() const;

	/* Return number of X tiles to render (to be used when orthographic tiling rendering is enabled) */
	int32 GetNumXTiles() const;

	/* Return number of Y tiles to render (to be used when orthographic tiling rendering is enabled) */
	int32 GetNumYTiles() const;

#if WITH_EDITORONLY_DATA
	void UpdateDrawFrustum();

	/** The frustum component used to show visually where the camera field of view is */
	class UDrawFrustumComponent* DrawFrustum;
#endif
};