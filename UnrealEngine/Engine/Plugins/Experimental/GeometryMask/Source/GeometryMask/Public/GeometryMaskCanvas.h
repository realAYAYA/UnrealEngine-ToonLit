// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IGeometryMaskReadInterface.h"
#include "SceneView.h"
#include "UObject/Object.h"
#include "UObject/WeakInterfacePtr.h"

#include "GeometryMaskCanvas.generated.h"

class FGeometryMaskPostProcess_Blur;
class IGeometryMaskWriteInterface;
class UCanvas;
class UCanvasRenderTarget2D;
class UGeometryMaskCanvasResource;
class UGeometryMaskPrimitive;
class UGeometryMaskSubsystem;

/** Called when Writers becomes non-empty. */
using FOnGeometryMaskCanvasActivated = TDelegate<void()>;

/** Called when Writers becomes empty. */
using FOnGeometryMaskCanvasDeactivated = TDelegate<void()>;

/** A uniquely identified Canvas. */
UCLASS(BlueprintType, Transient)
class GEOMETRYMASK_API UGeometryMaskCanvas : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif

	/** Returns all writers. */
    const TArray<TWeakInterfacePtr<IGeometryMaskWriteInterface>>& GetWriters() const;

	/** Adds a Mask Writer to the canvas. */
	void AddWriter(const TScriptInterface<IGeometryMaskWriteInterface>& InWriter);

	/** Appends multiple writers to the canvas. */
	void AddWriters(const TArray<TScriptInterface<IGeometryMaskWriteInterface>>& InWriters);

	/** Remove a writer from this canvas. */
	void RemoveWriter(const TScriptInterface<IGeometryMaskWriteInterface>& InWriter);

	/** Gets the number of writers for the canvas. */
	int32 GetNumWriters() const;

	/** Returns true if this is the default/blank canvas. */
	bool IsDefaultCanvas() const;

	/** Forcibly free the canvas - removed all writers and frees the resource. */
	void Free();

	/** Get the underlying render target. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rendering")
	UCanvasRenderTarget2D* GetTexture() const;

	/** Get the unique canvas id based on the world and canvas name. */
	const FGeometryMaskCanvasId& GetCanvasId() const;

	/** Whether blur is applied or not. */
	bool IsBlurApplied() const;

	/** Sets whether blur is applied or not. */
	void SetApplyBlur(const bool bInValue);

	/** Gets the current blur strength, if applicable. */
	double GetBlurStrength() const;

	/** Sets the blur strength. 0.0 will disable blur. */
	void SetBlurStrength(const double InValue);

	/** Whether feathering is applied or not. */
	bool IsFeatherApplied() const;

	/** Sets whether feathering is applied or not. */
	void SetApplyFeather(const bool bInValue);

	/** Gets the current outer feather radius, in pixels. */
	int32 GetOuterFeatherRadius() const;

	/** Sets the outer feather radius, in pixels. */
	void SetOuterFeatherRadius(const int32 InValue);

	/** Gets the current inner feather radius, in pixels. */
	int32 GetInnerFeatherRadius() const;

	/** Sets the inner feather radius, in pixels. */
	void SetInnerFeatherRadius(const int32 InValue);

	/** Get the color channel to write to in the texture. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Rendering")
	EGeometryMaskColorChannel GetColorChannel() const;

	/** Called when Writers becomes non-empty. */
	FOnGeometryMaskCanvasActivated& OnActivated() { return OnActivatedDelegate; }

	/** Called when Writers becomes empty. */
	FOnGeometryMaskCanvasDeactivated& OnDeactivated() { return OnDeactivatedDelegate; }

	/** Setup with identifying info. */
	void Initialize(const UWorld* InWorld, FName InCanvasName);
	
	/** Updates the canvas, intended to be called every frame. */
	void Update(UWorld* InWorld, FSceneView& InView);
	
	const UGeometryMaskCanvasResource* GetResource() const { return CanvasResource; }
	void AssignResource(UGeometryMaskCanvasResource* InResource, EGeometryMaskColorChannel InColorChannel);
	void FreeResource();

public:
	static FName GetApplyBlurPropertyName();
	static FName GetBlurStrengthPropertyName();
	static FName GetApplyFeatherPropertyName();
	static FName GetOuterFeatherRadiusPropertyName();
	static FName GetInnerFeatherRadiusPropertyName();

private:
	static const FName ApplyBlurPropertyName;
	static const FName BlurStrengthPropertyName;
	static const FName ApplyFeatherPropertyName;
	static const FName OuterFeatherRadiusPropertyName;
	static const FName InnerFeatherRadiusPropertyName;

private:
	friend class UGeometryMaskCanvasResource;
	
	/** Sorts writers by various criteria for proper rendering order. */
	void SortWriters();
	
	/** Remove all invalid/stale writers. */
	void RemoveInvalidWriters();

	/** Draws all writers to the canvas. */
	void OnDrawToCanvas(const FGeometryMaskDrawingContext& InDrawingContext, FCanvas* InCanvas);

	/** Updates relevant shader parameters for the given color channel. */
	void UpdateRenderParameters();
	
private:
	FOnGeometryMaskCanvasActivated OnActivatedDelegate;
	FOnGeometryMaskCanvasDeactivated OnDeactivatedDelegate;

	UPROPERTY(Transient, DuplicateTransient)
	FGeometryMaskCanvasId CanvasId;

	/** Uniquely identifies this canvas. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Getter = "Auto", Category = "Canvas", meta = (AllowPrivateAccess = "true"))
	FName CanvasName;

	/** Optional Blur Toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsBlurApplied", Setter = "SetApplyBlur", Category = "Canvas", meta = (AllowPrivateAccess = "true"))
	bool bApplyBlur = false;
	
	/** Optional Blur Strength. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Canvas", meta = (AllowPrivateAccess = "true", EditCondition = "bApplyBlur", EditConditionHides, ClampMin = 0.0))
	double BlurStrength = 16;

	/** Optional Feather Toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "IsFeatherApplied", Setter = "SetApplyFeather", Category = "Canvas", meta = (AllowPrivateAccess = "true"))
	bool bApplyFeather = false;
	
	/** Optional Outer Feather Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Canvas", meta = (AllowPrivateAccess = "true", EditCondition = "bApplyFeather", EditConditionHides, ClampMin = 0))
	int32 OuterFeatherRadius = 16;

	/** Optional Inner Feather Radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Canvas", meta = (AllowPrivateAccess = "true", EditCondition = "bApplyFeather", EditConditionHides, ClampMin = 0))
	int32 InnerFeatherRadius = 16;

	/** Canvas GPU resource to use. */
	UPROPERTY(Transient)
	TObjectPtr<UGeometryMaskCanvasResource> CanvasResource;

	/** Color Channel being written to in the texture. */
	UPROPERTY(Transient, Getter)
	EGeometryMaskColorChannel ColorChannel = EGeometryMaskColorChannel::None;

	/** List of objects that write to this canvas. */
	TArray<TWeakInterfacePtr<IGeometryMaskWriteInterface>> Writers;
};
