// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryMaskTypes.h"
#include "UObject/Object.h"

#include "GeometryMaskCanvasResource.generated.h"

class FGeometryMaskPostProcess_Blur;
class FGeometryMaskPostProcess_DistanceField;
class FGeometryMaskPostProcess_SobelTest;
class FCanvas;
class FSceneView;
class IGeometryMaskPostProcess;
class UCanvas;
class UCanvasRenderTarget2D;
enum class EGeometryMaskColorChannel : uint8;

using FOnGeometryMaskCanvasDraw = TMulticastDelegate<void(const FGeometryMaskDrawingContext&, FCanvas*)>;

/**  */
UCLASS()
class GEOMETRYMASK_API UGeometryMaskCanvasResource
	: public UObject
{
	GENERATED_BODY()

public:
	/** (R, G, B, A) */
	static constexpr int32 MaxNumChannels = 4;
	static constexpr int32 MaxNumAvailableChannels = 3; // Skip alpha, as it's not universally supported (RT format)
	static constexpr int32 MaxTextureSize = 8192;
	
	UGeometryMaskCanvasResource();
	virtual ~UGeometryMaskCanvasResource() override;
	
	/** Will return the first available color channel without a canvas assigned. EGeometryMaskColorChannel::None if not available. */
	const EGeometryMaskColorChannel GetNextAvailableColorChannel() const;

	/** Requests usage of the given color channel for this resource. Will return true if successful. */
	bool Checkout(const EGeometryMaskColorChannel InColorChannel, const FGeometryMaskCanvasId& InRequestingCanvasId);

	/** Returns/frees the color channel associated with the given canvas name. Returns true if canvas name found. */
	bool Checkin(const FGeometryMaskCanvasId& InRequestingCanvasId);

	/** Re-arranges used channels such that they are used sequentially. Returns number of unused channels. */
	int32 Compact();

	int32 GetNumChannelsUsed() const;

	bool IsAnyChannelUsed() const;

	/** Get the list of dependent canvases, for debug purposes. */
	TArray<FGeometryMaskCanvasId> GetDependentCanvasIds() const;

	void UpdateViewportSize();

	void SetViewportSize(FGeometryMaskDrawingContext& InDrawingContext, const FIntPoint& InViewportSize);

	/** Returns required viewport padding, in pixels - determined by certain effects. */
	int32 GetViewportPadding(const FGeometryMaskDrawingContext& InDrawingContext) const;

	UCanvasRenderTarget2D* GetRenderTargetTexture();

	FOnGeometryMaskCanvasDraw& OnDrawToCanvas() { return OnDrawToCanvasDelegate; }

	void UpdateRenderParameters(EGeometryMaskColorChannel InColorChannel, bool bInApplyBlur, double InBlurStrength, bool bInApplyFeather, int32 InOuterFeatherRadius, int32 InInnerFeatherRadius);

	/** Resets the render parameters for the given channel. */
	void ResetRenderParameters(EGeometryMaskColorChannel InColorChannel);

	/** Updates the canvas, intended to be called every frame. */
	void Update(UWorld* InWorld, FSceneView& InView, int32 InViewIndex = 0);

private:
	/** Draws all writers to the canvas. */
	void Draw(UWorld* InWorld, FSceneView& InView, int32 InViewIndex = 0);

	FGeometryMaskDrawingContext* GetDrawingContextForWorld(const UWorld* InWorld, uint8 InSceneViewIndex);
	FGeometryMaskDrawingContext* GetDrawingContextForCanvas(const FGeometryMaskCanvasId& InCanvasId);
	FGeometryMaskDrawingContext* GetDrawingContextForChannel(EGeometryMaskColorChannel InColorChannel);

	[[maybe_unused]] bool RemoveInvalidDrawingContexts();

private:
	UPROPERTY(NoClear, EditFixedSize, meta = (EditFixedOrder))
	TMap<EGeometryMaskColorChannel, FGeometryMaskCanvasId> DependentCanvasIds;

	TBitArray<TInlineAllocator<MaxNumChannels>> UsedChannelMask;

	UPROPERTY()
	int32 NumUsedChannels = 0;

	/** Underlying UCanvas object. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvas> CanvasObject;

	/** The default viewport size to use when it can't be resolved from the actual viewport. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "Auto", Category = "Rendering", meta = (AllowPrivateAccess = "true"))
	FIntPoint MaxViewportSize = FIntPoint(1920, 1080);

	/** The underlying Render Target texture. */
	UPROPERTY(DuplicateTransient)
	TObjectPtr<UCanvasRenderTarget2D> RenderTargetTexture;

	FOnGeometryMaskCanvasDraw OnDrawToCanvasDelegate;

	TSet<FGeometryMaskDrawingContext> DrawingContextCache;

	FGeometryMaskDrawingContext DefaultDrawingContext;

	bool bApplyBlur = false;
	bool bApplyDF = false;

	TSharedPtr<FGeometryMaskPostProcess_Blur> PostProcess_Blur;
	TSharedPtr<FGeometryMaskPostProcess_DistanceField> PostProcess_DistanceField;
};
