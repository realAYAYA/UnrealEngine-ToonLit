// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "IStereoLayers.h"
#include "StereoLayerComponent.generated.h"

class UTexture;

/** Used by IStereoLayer */
UENUM()
enum EStereoLayerType
{
	/** Location within the world */
	SLT_WorldLocked		UMETA(DisplayName = "World Locked"),

	/** Location within the HMD tracking space */
	SLT_TrackerLocked	UMETA(DisplayName = "Tracker Locked"),

	/** Location within the view space */
	SLT_FaceLocked		UMETA(DisplayName = "Face Locked"),

	SLT_MAX,
};

/** The shape to use for the stereo layer.  Note that some shapes might not be supported on all platforms! */
UENUM()
enum EStereoLayerShape
{
	/** Quad layer */
	SLSH_QuadLayer		UMETA(DisplayName = "Quad Layer"),

	/** Cylinder layer */
	SLSH_CylinderLayer	UMETA(DisplayName = "Cylinder Layer"),

	/** Cubemap layer */
	SLSH_CubemapLayer	UMETA(DisplayName = "Cubemap Layer"),

	/** Equirect layer */
	SLSH_EquirectLayer	UMETA(DisplayName = "Equirect Layer"),

	SLSH_MAX,
};

UCLASS(EditInlineNew, Abstract, BlueprintType, CollapseCategories)
class ENGINE_API UStereoLayerShape : public UObject
{
	GENERATED_BODY()

public:
	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc);
#if WITH_EDITOR
	virtual void DrawShapeVisualization(const class FSceneView* View, class FPrimitiveDrawInterface* PDI);
#endif

protected:
	void MarkStereoLayerDirty();
};

UCLASS(meta = (DisplayName = "Quad Layer"))
class ENGINE_API UStereoLayerShapeQuad : public UStereoLayerShape
{
	GENERATED_BODY()
public:
	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc) override;
#if WITH_EDITOR
	virtual void DrawShapeVisualization(const class FSceneView* View, class FPrimitiveDrawInterface* PDI) override;
#endif
};

UCLASS(meta = (DisplayName = "Cylinder Layer"))
class ENGINE_API UStereoLayerShapeCylinder : public UStereoLayerShape
{
	GENERATED_BODY()

public:
	UStereoLayerShapeCylinder()
		: Radius(100)
		, OverlayArc(100)
		, Height(50)
	{}

	/** Radial size of the rendered stereo layer cylinder **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "Cylinder Properties")
	float Radius;

	/** Arc angle for the stereo layer cylinder **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "Cylinder Properties")
	float OverlayArc;

	/** Height of the stereo layer cylinder **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "Cylinder Properties")
	int Height;

	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetRadius(float InRadius);
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetOverlayArc(float InOverlayArc);
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetHeight(int InHeight);

	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc) override;
#if WITH_EDITOR
	virtual void DrawShapeVisualization(const class FSceneView* View, class FPrimitiveDrawInterface* PDI) override;
#endif
};

UCLASS(meta = (DisplayName = "Cubemap Layer"))
class ENGINE_API UStereoLayerShapeCubemap : public UStereoLayerShape
{
	GENERATED_BODY()
public:
	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc) override;
};

/** Properties for equirect layers */
USTRUCT(BlueprintType)
struct FEquirectProps
{
	GENERATED_USTRUCT_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Left source texture UVRect, specifying portion of input texture corresponding to left eye. */
	FBox2D LeftUVRect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Right source texture UVRect, specifying portion of input texture corresponding to right eye. */
	FBox2D RightUVRect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Left eye's texture coordinate scale after mapping to 2D. */
	FVector2D LeftScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Right eye's texture coordinate scale after mapping to 2D. */
	FVector2D RightScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Left eye's texture coordinate bias after mapping to 2D. */
	FVector2D LeftBias;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category = "StereoLayer | Equirect Layer Properties")
	/** Right eye's texture coordinate bias after mapping to 2D. */
	FVector2D RightBias;

public:

	FEquirectProps()
		: LeftUVRect(FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f)))
		, RightUVRect(FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f)))
		, LeftScale(FVector2D(1.0f, 1.0f))
		, RightScale(FVector2D(1.0f, 1.0f))
		, LeftBias(FVector2D(0.0f, 0.0f))
		, RightBias(FVector2D(0.0f, 0.0f))
	{}

	FEquirectProps(FBox2D InLeftUVRect, FBox2D InRightUVRect, FVector2D InLeftScale, FVector2D InRightScale, FVector2D InLeftBias, FVector2D InRightBias)
		: LeftUVRect(InLeftUVRect)
		, RightUVRect(InRightUVRect)
		, LeftScale(InLeftScale)
		, RightScale(InRightScale)
		, LeftBias(InLeftBias)
		, RightBias(InRightBias)
	{ }

	/**
	 * Compares two FEquirectProps for equality.
	 *
	 * @param Other The other FEquirectProps to compare with.
	 * @return true if the are equal, false otherwise.
	 */
	bool operator==(const FEquirectProps& Other) const;

	/**
	 * Compares FEquirectProps with an UStereoLayerShapeEquirect
	 *
	 * @param Other The UStereoLayerShapeEquirect to compare with.
	 * @return true if the are equal, false otherwise.
	 */
	bool operator==(const class UStereoLayerShapeEquirect& Other) const;
};


UCLASS(meta = (DisplayName = "Equirect Layer"))
class ENGINE_API UStereoLayerShapeEquirect : public UStereoLayerShape
{
	GENERATED_BODY()

public:
	UStereoLayerShapeEquirect()
		: LeftUVRect(FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f)))
		, RightUVRect(FBox2D(FVector2D(0.0f, 0.0f), FVector2D(1.0f, 1.0f)))
		, LeftScale(FVector2D(1.0f, 1.0f))
		, RightScale(FVector2D(1.0f, 1.0f))
		, LeftBias(FVector2D(0.0f, 0.0f))
		, RightBias(FVector2D(0.0f, 0.0f))
	{}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Left source texture UVRect, specifying portion of input texture corresponding to left eye. */
	FBox2D LeftUVRect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Right source texture UVRect, specifying portion of input texture corresponding to right eye. */
	FBox2D RightUVRect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Left eye's texture coordinate scale after mapping to 2D. */
	FVector2D LeftScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Right eye's texture coordinate scale after mapping to 2D. */
	FVector2D RightScale;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Left eye's texture coordinate bias after mapping to 2D. */
	FVector2D LeftBias;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equirect Properties")
	/** Right eye's texture coordinate bias after mapping to 2D. */
	FVector2D RightBias;

	/**
	 * Set Equirect layer properties: UVRect, Scale, and Bias
	 * @param	LeftScale: Scale for left eye
	 * @param	LeftBias: Bias for left eye
	 * @param	RightScale: Scale for right eye
	 * @param	RightBias: Bias for right eye
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	void SetEquirectProps(FEquirectProps InScaleBiases);

	virtual void ApplyShape(IStereoLayers::FLayerDesc& LayerDesc) override;
};

/** 
 * A geometry layer within the stereo rendered viewport.
 */
UCLASS(ClassGroup="HeadMountedDisplay", hidecategories=(Object,LOD,Lighting,TextureStreaming), editinlinenew, meta=(DisplayName="Stereo Layer", BlueprintSpawnableComponent))
class ENGINE_API UStereoLayerComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()
    friend class FStereoLayerComponentVisualizer;
    
public:

	//~ Begin UObject Interface
	void BeginDestroy() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	//~ Begin UActorComponent Interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface

	/** 
	 * Change the texture displayed on the stereo layer. 
	 *
	 * If stereoscopic layer textures are supported on the platform and LeftTexture is set, this property controls the texture for the right eye.
	 * @param	InTexture: new Texture2D
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetTexture(UTexture* InTexture);

	/** 
	 * Change the texture displayed on the stereo layer for left eye, if stereoscopic layer textures are supported on the platform.
	 * @param	InTexture: new Texture2D
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetLeftTexture(UTexture* InTexture);

	// @return the texture mapped to the stereo layer.
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	UTexture* GetTexture() const { return Texture; }

	// @return the texture mapped to the stereo layer for left eye, if stereoscopic layer textures are supported on the platform.
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer")
	UTexture* GetLeftTexture() const { return LeftTexture; }

	/** 
	 * Change the quad size. This is the unscaled height and width, before component scale is applied.
	 * @param	InQuadSize: new quad size.
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetQuadSize(FVector2D InQuadSize);

	// @return the height and width of the rendered quad
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	FVector2D GetQuadSize() const { return QuadSize; }

	/** 
	 * Change the UV coordinates mapped to the quad face
	 * @param	InUVRect: Min and Max UV coordinates
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetUVRect(FBox2D InUVRect);

	// @return the UV coordinates mapped to the quad face
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	FBox2D GetUVRect() const { return UVRect; }

	/**
	 * Set Equirect layer properties: UVRect, Scale, and Bias
	 * @param	LeftScale: Scale for left eye
	 * @param	LeftBias: Bias for left eye
	 * @param	RightScale: Scale for right eye
	 * @param	RightBias: Bias for right eye
	 */
	UFUNCTION(BlueprintCallable, Category = "Components|Stereo Layer", meta = (DeprecatedFunction, DeprecationMessage = "Use UStereoLayerShapeEquirect::SetEquirectProps() instead."))
	void SetEquirectProps(FEquirectProps InScaleBiases);

	/** 
	 * Change the layer's render priority, higher priorities render on top of lower priorities
	 * @param	InPriority: Priority value
	 */
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void SetPriority(int32 InPriority);

	// @return the render priority
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	int32 GetPriority() const { return Priority; }

	// Manually mark the stereo layer texture for updating
	UFUNCTION(BlueprintCallable, Category="Components|Stereo Layer")
	void MarkTextureForUpdate();

	/** True if the stereo layer texture needs to update itself every frame(scene capture, video, etc.) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "StereoLayer")
	uint32 bLiveTexture:1;

	/** True if the stereo layer needs to support depth intersections with the scene geometry, if available on the platform */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoLayer")
	uint32 bSupportsDepth : 1;

	/** True if the texture should not use its own alpha channel (1.0 will be substituted) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= "StereoLayer")
	uint32 bNoAlphaChannel:1;

	void MarkStereoLayerDirty();
protected:
	/** Texture displayed on the stereo layer (is stereocopic textures are supported on the platfrom and more than one texture is provided, this will be the right eye) **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category= "StereoLayer")
	TObjectPtr<class UTexture> Texture;

	/** Texture displayed on the stereo layer for left eye, if stereoscopic textures are supported on the platform and by the layer shape **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stereoscopic Properties")
	TObjectPtr<class UTexture> LeftTexture;

public:
	/** True if the quad should internally set it's Y value based on the set texture's dimensions */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoLayer")
	uint32 bQuadPreserveTextureRatio : 1;

protected:
	/** Size of the rendered stereo layer quad **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer")
	FVector2D QuadSize;

	/** UV coordinates mapped to the quad face **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer")
	FBox2D UVRect;

	UE_DEPRECATED(4.25, "float CylinderRadius is deprecated. Use the corresponding property of the UStereoLayerShapeCylinder subobject instead.")
	UPROPERTY()
	float CylinderRadius_DEPRECATED;

	UE_DEPRECATED(4.25, "float CylinderOverlayArc is deprecated. Use the corresponding property of the UStereoLayerShapeCylinder subobject instead.")
	UPROPERTY()
	float CylinderOverlayArc_DEPRECATED;

	UE_DEPRECATED(4.25, "int CylinderHeight is deprecated. Use the corresponding property of the UStereoLayerShapeCylinder subobject instead.")
	UPROPERTY()
	int CylinderHeight_DEPRECATED;

	UE_DEPRECATED(4.25, "FEquirectProps EquirectProps is deprecated. Use the corresponding properties of the UStereoLayerShapeEquirect subobject instead.")
	UPROPERTY()
	FEquirectProps EquirectProps_DEPRECATED;

	/** Specifies how and where the quad is rendered to the screen **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category="StereoLayer")
    TEnumAsByte<enum EStereoLayerType> StereoLayerType;

	UE_DEPRECATED(4.25, "TEnumAsByte<enum EStereoLayerShape> StereoLayerShape is deprecated. Use Shape instead.")
	UPROPERTY()
	TEnumAsByte<enum EStereoLayerShape> StereoLayerShape_DEPRECATED;

	/** Specifies which shape of layer it is.  Note that some shapes will be supported only on certain platforms! **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, NoClear, Instanced, Category = "StereoLayer", DisplayName="Stereo Layer Shape")
	TObjectPtr<UStereoLayerShape> Shape;


	/** Render priority among all stereo layers, higher priority render on top of lower priority **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, export, Category= "StereoLayer")
	int32 Priority;

private:
	/** Dirty state determines whether the stereo layer needs updating **/
	bool bIsDirty;

	/** Texture needs to be marked for update **/
	bool bTextureNeedsUpdate;

	/** IStereoLayer id, 0 is unassigned **/
	uint32 LayerId;

	/** Last transform is cached to determine if the new frames transform has changed **/
	FTransform LastTransform;

	/** Last frames visiblity state **/
	bool bLastVisible;

	/** Set if the component was loaded from an old version */
	bool bNeedsPostLoadFixup;
};

