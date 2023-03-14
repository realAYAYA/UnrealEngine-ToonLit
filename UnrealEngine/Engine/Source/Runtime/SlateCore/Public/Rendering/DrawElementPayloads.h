// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateRenderBatch.h"
#include "Types/SlateVector2.h"

struct FSlateGradientStop
{
	FVector2f Position;
	FLinearColor Color;

	/**
	 * Construct a Gradient Stop from a Position and a Color.
	 * @param InPosition - The position in widget space for this stop. Both X and Y are used for a single-axis gradient.
						  A two stop gradient should go from (0,0), to (Width,Height).
	 * @param InColor	- The color to lerp towards at this stop.
	 */
	FSlateGradientStop(const FVector2d InPosition, const FLinearColor InColor)
		: Position(UE::Slate::CastToVector2f(InPosition))
		, Color(InColor)
	{
	}
	FSlateGradientStop(const FVector2f InPosition, const FLinearColor InColor)
		: Position(InPosition)
		, Color(InColor)
	{

	}
};
template <> struct TIsPODType<FSlateGradientStop> { enum { Value = true }; };

enum class ETextOverflowDirection : uint8
{
	// No overflow
	NoOverflow,
	// Left justification overflow
	LeftToRight,
	// Right justification overflow
	RightToLeft
};

struct FSlateDataPayload
{
	virtual ~FSlateDataPayload() {}
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
};

struct FSlateTintableElement
{
	FLinearColor Tint;

	FORCEINLINE void SetTint(const FLinearColor& InTint) { Tint = InTint; }
	FORCEINLINE FLinearColor GetTint() const { return Tint; }
};

struct FSlateBoxPayload : public FSlateDataPayload, public FSlateTintableElement
{
	FMargin Margin;
	FBox2f UVRegion;
	const FSlateShaderResourceProxy* ResourceProxy;
	ESlateBrushTileType::Type Tiling;
	ESlateBrushMirrorType::Type Mirroring;
	ESlateBrushDrawType::Type DrawType;

	const FMargin& GetBrushMargin() const { return Margin; }
	const FBox2f& GetBrushUVRegion() const { return UVRegion; }
	ESlateBrushTileType::Type GetBrushTiling() const { return Tiling; }
	ESlateBrushMirrorType::Type GetBrushMirroring() const { return Mirroring; }
	ESlateBrushDrawType::Type GetBrushDrawType() const { return DrawType; }
	const FSlateShaderResourceProxy* GetResourceProxy() const { return ResourceProxy; }

	void SetBrush(const FSlateBrush* InBrush, FVector2D LocalSize, float DrawScale)
	{
		SetBrush(InBrush, UE::Slate::CastToVector2f(LocalSize), DrawScale);
	}

	void SetBrush(const FSlateBrush* InBrush, FVector2f LocalSize, float DrawScale)
	{
		check(InBrush);
		ensureMsgf(InBrush->GetDrawType() != ESlateBrushDrawType::NoDrawType, TEXT("This should have been filtered out earlier in the Make... call."));

		// Note: Do not store the brush.  It is possible brushes are destroyed after an element is enqueued for rendering
		Margin = InBrush->GetMargin();
		UVRegion = InBrush->GetUVRegion();
		Tiling = InBrush->GetTiling();
		Mirroring = InBrush->GetMirroring();
		DrawType = InBrush->GetDrawType();
		FSlateResourceHandle Handle = InBrush->GetRenderingResource(FVector2D(LocalSize), DrawScale);
		if (Handle.IsValid())
		{
			ResourceProxy = Handle.GetResourceProxy();
		}
		else
		{
			ResourceProxy = nullptr;
		}
	}

	FORCENOINLINE virtual ~FSlateBoxPayload()
	{
	}
};

struct FSlateRoundedBoxPayload : public FSlateBoxPayload
{
	FLinearColor OutlineColor;
	FVector4f Radius;
	float OutlineWeight;

	FORCEINLINE void SetRadius(FVector4f InRadius) { Radius = InRadius; }
	FORCEINLINE FVector4f GetRadius() const { return Radius; }

	FORCEINLINE void SetOutline(const FLinearColor& InOutlineColor, float InOutlineWeight) { OutlineColor = InOutlineColor; OutlineWeight = InOutlineWeight; }
	FORCEINLINE FLinearColor GetOutlineColor() const { return OutlineColor; }
	FORCEINLINE float GetOutlineWeight() const { return OutlineWeight; }
};

struct FSlateTextPayload : public FSlateDataPayload, public FSlateTintableElement
{
	// The font to use when rendering
	FSlateFontInfo FontInfo;
	// Basic text data
	FString ImmutableText;

	const FSlateFontInfo& GetFontInfo() const { return FontInfo; }
	const TCHAR* GetText() const { return *ImmutableText; }
	int32 GetTextLength() const { return ImmutableText.Len(); }

	void SetText(const FString& InText, const FSlateFontInfo& InFontInfo, int32 InStartIndex, int32 InEndIndex)
	{
		FontInfo = InFontInfo;
		const int32 StartIndex = FMath::Min<int32>(InStartIndex, InText.Len());
		const int32 EndIndex = FMath::Min<int32>(InEndIndex, InText.Len());
		const int32 TextLength = (EndIndex > StartIndex) ? EndIndex - StartIndex : 0;
		if (TextLength > 0)
		{
			ImmutableText = InText.Mid(StartIndex, TextLength);
		}
	}

	void SetText(const FString& InText, const FSlateFontInfo& InFontInfo)
	{
		FontInfo = InFontInfo;
		ImmutableText = InText;
	}


	virtual void AddReferencedObjects(FReferenceCollector& Collector)
	{
		FontInfo.AddReferencedObjects(Collector);
	}
};



struct FTextOverflowArgs
{
	FTextOverflowArgs(FShapedGlyphSequencePtr& InOverflowText, ETextOverflowDirection InOverflowDirection)
		: OverflowTextPtr(InOverflowText)
		, OverflowDirection(InOverflowDirection)
		, bForceEllipsisDueToClippedLine(false)
		
	{}

	FTextOverflowArgs()
		: OverflowDirection(ETextOverflowDirection::NoOverflow)
		, bForceEllipsisDueToClippedLine(false)
	{}

	/** Sequence that represents the ellipsis glyph */
	FShapedGlyphSequencePtr OverflowTextPtr;
	ETextOverflowDirection OverflowDirection;
	bool bForceEllipsisDueToClippedLine;
	
};

struct FSlateShapedTextPayload : public FSlateDataPayload, public FSlateTintableElement
{ 
	// Shaped text data
	FShapedGlyphSequencePtr ShapedGlyphSequence;

	FLinearColor OutlineTint;

	FTextOverflowArgs OverflowArgs;

	const FShapedGlyphSequencePtr& GetShapedGlyphSequence() const { return ShapedGlyphSequence; }
	FLinearColor GetOutlineTint() const { return OutlineTint; }

	void SetShapedText(FSlateWindowElementList& ElementList, const FShapedGlyphSequencePtr& InShapedGlyphSequence, const FLinearColor& InOutlineTint)
	{
		ShapedGlyphSequence = InShapedGlyphSequence;
		OutlineTint = InOutlineTint;
	}

	void SetOverflowArgs(const FTextOverflowArgs& InArgs)
	{
		OverflowArgs = InArgs;
		check(InArgs.OverflowDirection == ETextOverflowDirection::NoOverflow || InArgs.OverflowTextPtr.IsValid());
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector)
	{
		if (ShapedGlyphSequence.IsValid())
		{
			const_cast<FShapedGlyphSequence*>(ShapedGlyphSequence.Get())->AddReferencedObjects(Collector);
		}

		if (OverflowArgs.OverflowTextPtr.IsValid())
		{
			const_cast<FShapedGlyphSequence*>(OverflowArgs.OverflowTextPtr.Get())->AddReferencedObjects(Collector);
		}
	}
};

struct FSlateGradientPayload : public FSlateDataPayload
{
	TArray<FSlateGradientStop> GradientStops;
	EOrientation GradientType;
	FVector4f CornerRadius;

	void SetGradient(TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, FVector4f InCornerRadius)
	{
		GradientStops = MoveTemp(InGradientStops);
		GradientType = InGradientType;
		CornerRadius = InCornerRadius;
	}
};

struct FSlateSplinePayload : public FSlateDataPayload, public FSlateTintableElement
{
	TArray<FSlateGradientStop> GradientStops;
	// Bezier Spline Data points. E.g.
	//
	//       P1 + - - - - + P2                P1 +
	//         /           \                    / \
	//     P0 *             * P3            P0 *   \   * P3
	//                                              \ /
	//                                               + P2	
	FVector2f P0;
	FVector2f P1;
	FVector2f P2;
	FVector2f P3;

	float Thickness;

	// Thickness
	void SetThickness(float InThickness) { Thickness = InThickness; }
	float GetThickness() const { return Thickness; }

	void SetCubicBezier(const FVector2d InP0, const FVector2d InP1, const FVector2d InP2, const FVector2d InP3, float InThickness, const FLinearColor InTint)
	{
		SetCubicBezier(
			UE::Slate::CastToVector2f(InP0),
			UE::Slate::CastToVector2f(InP1),
			UE::Slate::CastToVector2f(InP2),
			UE::Slate::CastToVector2f(InP3),
			InThickness,
			InTint);
	}

	void SetCubicBezier(const FVector2f InP0, const FVector2f InP1, const FVector2f InP2, const FVector2f InP3, float InThickness, const FLinearColor InTint)
	{
		Tint = InTint;
		P0 = InP0;
		P1 = InP1;
		P2 = InP2;
		P3 = InP3;
		Thickness = InThickness;
	}

	void SetHermiteSpline(const FVector2d InStart, const FVector2d InStartDir, const FVector2d InEnd, const FVector2d InEndDir, float InThickness, const FLinearColor InTint)
	{
		SetHermiteSpline(
			UE::Slate::CastToVector2f(InStart),
			UE::Slate::CastToVector2f(InStartDir),
			UE::Slate::CastToVector2f(InEnd),
			UE::Slate::CastToVector2f(InEndDir),
			InThickness,
			InTint);
	}

	void SetHermiteSpline(const FVector2f InStart, const FVector2f InStartDir, const FVector2f InEnd, const FVector2f InEndDir, float InThickness, const FLinearColor InTint)
	{
		Tint = InTint;
		P0 = InStart;
		P1 = InStart + InStartDir / 3.0f;
		P2 = InEnd - InEndDir / 3.0f;
		P3 = InEnd;
		Thickness = InThickness;
	}

	void SetGradientHermiteSpline(const FVector2d InStart, const FVector2d InStartDir, const FVector2d InEnd, const FVector2d InEndDir, float InThickness, TArray<FSlateGradientStop> InGradientStops)
	{
		SetGradientHermiteSpline(
			UE::Slate::CastToVector2f(InStart),
			UE::Slate::CastToVector2f(InStartDir),
			UE::Slate::CastToVector2f(InEnd),
			UE::Slate::CastToVector2f(InEndDir),
			InThickness,
			MoveTemp(InGradientStops));
	}

	void SetGradientHermiteSpline(const FVector2f InStart, const FVector2f InStartDir, const FVector2f InEnd, const FVector2f InEndDir, float InThickness, TArray<FSlateGradientStop> InGradientStops)
	{
		P0 = InStart;
		P1 = InStart + InStartDir / 3.0f;
		P2 = InEnd - InEndDir / 3.0f;
		P3 = InEnd;
		Thickness = InThickness;
		GradientStops = MoveTemp(InGradientStops);
	}
};


struct FSlateLinePayload : public FSlateDataPayload, public FSlateTintableElement
{ 
	TArray<FVector2f> Points;
	TArray<FLinearColor> PointColors;
	float Thickness;

	bool bAntialias;

	bool IsAntialiased() const { return bAntialias; }
	const TArray<FVector2f>& GetPoints() const { return Points; }
	const TArray<FLinearColor>& GetPointColors() const { return PointColors; }
	float GetThickness() const { return Thickness; }

	void SetThickness(float InThickness)
	{
		Thickness = InThickness;
	}

	void SetLines(const TArray<FVector2D>& InPoints, bool bInAntialias, const TArray<FLinearColor>* InPointColors = nullptr)
	{
		TArray<FVector2f> NewPoints;
		NewPoints.Reserve(InPoints.Num());
		for (FVector2D Vect : InPoints)
		{
			NewPoints.Add(UE::Slate::CastToVector2f(Vect));
		}
		if (InPointColors)
		{
			SetLines(MoveTemp(NewPoints), bInAntialias, *InPointColors);
		}
		else
		{
			SetLines(MoveTemp(NewPoints), bInAntialias);
		}
	}


	void SetLines(TArray<FVector2f> InPoints, bool bInAntialias)
	{
		bAntialias = bInAntialias;
		Points = MoveTemp(InPoints);
		PointColors.Reset();
	}

	void SetLines(TArray<FVector2f> InPoints, bool bInAntialias, TArray<FLinearColor> InPointColors)
	{
		bAntialias = bInAntialias;
		Points = MoveTemp(InPoints);
		PointColors = MoveTemp(InPointColors);
	}
};

struct FSlateViewportPayload : public FSlateDataPayload, public FSlateTintableElement
{
	FSlateShaderResource* RenderTargetResource;
	uint8 bAllowViewportScaling : 1;
	uint8 bViewportTextureAlphaOnly : 1;
	uint8 bRequiresVSync : 1;

	void SetViewport(const TSharedPtr<const ISlateViewport>& InViewport, const FLinearColor& InTint)
	{
		Tint = InTint;
		RenderTargetResource = InViewport->GetViewportRenderTargetTexture();
		bAllowViewportScaling = InViewport->AllowScaling();
		bViewportTextureAlphaOnly = InViewport->IsViewportTextureAlphaOnly();
		bRequiresVSync = InViewport->RequiresVsync();
	}
};

struct FSlateCustomDrawerPayload : public FSlateDataPayload
{
	// Custom drawer data
	TWeakPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer;

	void SetCustomDrawer(const TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe>& InCustomDrawer)
	{
		CustomDrawer = InCustomDrawer;
	}
};

struct FSlateLayerPayload : public FSlateDataPayload
{
	class FSlateDrawLayerHandle* LayerHandle;

	void SetLayer(FSlateDrawLayerHandle* InLayerHandle)
	{
		LayerHandle = InLayerHandle;
		checkSlow(LayerHandle);
	}

};

struct FSlateCachedBufferPayload : public FSlateDataPayload
{
	// Cached render data
	class FSlateRenderDataHandle* CachedRenderData;
	FVector2f CachedRenderDataOffset;

	// Cached Buffers
	void SetCachedBuffer(FSlateRenderDataHandle* InRenderDataHandle, const FVector2d Offset)
	{
		SetCachedBuffer(InRenderDataHandle, UE::Slate::CastToVector2f(Offset));
	}

	void SetCachedBuffer(FSlateRenderDataHandle* InRenderDataHandle, const FVector2f Offset)
	{
		check(InRenderDataHandle);

		CachedRenderData = InRenderDataHandle;
		CachedRenderDataOffset = Offset;
	}
};

struct FSlateCustomVertsPayload : public FSlateDataPayload
{
	const FSlateShaderResourceProxy* ResourceProxy;

	TArray<FSlateVertex> Vertices;
	TArray<SlateIndex> Indices;

	// Instancing support
	ISlateUpdatableInstanceBufferRenderProxy* InstanceData;
	uint32 InstanceOffset;
	uint32 NumInstances;

	void SetCustomVerts(const FSlateShaderResourceProxy* InRenderProxy, TArray<FSlateVertex> InVerts, TArray<SlateIndex> InIndices, ISlateUpdatableInstanceBufferRenderProxy* InInstanceData, uint32 InInstanceOffset, uint32 InNumInstances)
	{
		ResourceProxy = InRenderProxy;

		Vertices = MoveTemp(InVerts);
		Indices = MoveTemp(InIndices);

		InstanceData = InInstanceData;
		InstanceOffset = InInstanceOffset;
		NumInstances = InNumInstances;
	}
};

struct FSlatePostProcessPayload : public FSlateDataPayload
{
	// Post Process Data
	FVector4f PostProcessData;
	FVector4f CornerRadius;
	int32 DownsampleAmount;
};