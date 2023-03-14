// Copyright Epic Games, Inc. All Rights Reserved.

#include "Rendering/DrawElements.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "HAL/IConsoleManager.h"
#include "Types/ReflectionMetadata.h"
#include "Fonts/ShapedTextFwd.h"
#include "Fonts/FontCache.h"
#include "Rendering/SlateObjectReferenceCollector.h"
#include "Debugging/SlateDebugging.h"
#include "Application/SlateApplicationBase.h"

#include <limits>

DECLARE_CYCLE_STAT(TEXT("FSlateDrawElement::Make Time"), STAT_SlateDrawElementMakeTime, STATGROUP_SlateVerbose);
DECLARE_CYCLE_STAT(TEXT("FSlateDrawElement::MakeCustomVerts Time"), STAT_SlateDrawElementMakeCustomVertsTime, STATGROUP_Slate);
DECLARE_CYCLE_STAT(TEXT("FSlateDrawElement::Prebatch Time"), STAT_SlateDrawElementPrebatchTime, STATGROUP_Slate);

DEFINE_STAT(STAT_SlateBufferPoolMemory);
DEFINE_STAT(STAT_SlateCachedDrawElementMemory);

static bool IsResourceObjectValid(UObject*& InObject)
{
	if (InObject != nullptr && (!IsValidChecked(InObject) || InObject->IsUnreachable() || InObject->HasAnyFlags(RF_BeginDestroyed)))
	{
		UE_LOG(LogSlate, Warning, TEXT("Attempted to access resource for %s which is pending kill, unreachable or pending destroy"), *InObject->GetName());
		return false;
	}

	return true;
}

static bool ShouldCull(const FSlateWindowElementList& ElementList)
{
	const FSlateClippingManager& ClippingManager = ElementList.GetClippingManager();
	const int32 CurrentIndex = ClippingManager.GetClippingIndex();
	if (CurrentIndex != INDEX_NONE)
	{
		const FSlateClippingState& ClippingState = ClippingManager.GetClippingStates()[CurrentIndex];
		if (ClippingState.GetClippingMethod() == EClippingMethod::Scissor)
		{
			return ClippingState.HasZeroArea();
		}
		else if (ClippingState.GetClippingMethod() == EClippingMethod::Stencil)
		{
			FSlateRect WindowRect = FSlateRect(FVector2f(0, 0), ElementList.GetWindowSize());
			if (WindowRect.GetArea() > 0)
			{
				for (const FSlateClippingZone& Stencil : ClippingState.StencilQuads)
				{
					bool bOverlapping = false;
					FSlateRect ClippedStencil = Stencil.GetBoundingBox().IntersectionWith(WindowRect, bOverlapping);
					
					if (!bOverlapping || ClippedStencil.GetArea() <= 0)
					{
						return true;
					}
				}
			}
		}

	}

	return false;
}

static bool ShouldCull(const FSlateWindowElementList& ElementList, const FPaintGeometry& PaintGeometry)
{
	const FVector2f LocalSize = UE::Slate::CastToVector2f(PaintGeometry.GetLocalSize());
	const float DrawScale = PaintGeometry.DrawScale;

	const FVector2f PixelSize = (LocalSize * DrawScale);
	if (PixelSize.X <= 0.f || PixelSize.Y <= 0.f)
	{
		return true;
	}

	return ShouldCull(ElementList);
}

static bool ShouldCull(const FSlateWindowElementList& ElementList, const FPaintGeometry& PaintGeometry, const FSlateBrush* InBrush)
{
	if (ShouldCull(ElementList, PaintGeometry))
	{
		return true;
	}

	if (InBrush->GetDrawType() == ESlateBrushDrawType::NoDrawType)
	{
		return true;
	}

	UObject* ResourceObject = InBrush->GetResourceObject();
	if (!IsResourceObjectValid(ResourceObject))
	{
		return true;
	}

	return false;
}


static bool ShouldCull(const FSlateWindowElementList& ElementList, const FPaintGeometry& PaintGeometry, const FLinearColor& InTint)
{
	if (InTint.A == 0 || ShouldCull(ElementList, PaintGeometry))
	{
		return true;
	}

	return false;
}

static bool ShouldCull(const FSlateWindowElementList& ElementList, const FPaintGeometry& PaintGeometry, const FLinearColor& InTint, const FString& InText)
{
	if (InTint.A == 0 || InText.Len() == 0 || ShouldCull(ElementList, PaintGeometry))
	{
		return true;
	}

	return false;
}

static bool ShouldCull(const FSlateWindowElementList& ElementList, const FPaintGeometry& PaintGeometry, const FSlateBrush* InBrush, const FLinearColor& InTint)
{
	bool bIsFullyTransparent = InTint.A == 0 && (InBrush->OutlineSettings.Color.GetSpecifiedColor().A == 0 || InBrush->OutlineSettings.bUseBrushTransparency);
	if (bIsFullyTransparent || ShouldCull(ElementList, PaintGeometry, InBrush))
	{
		return true;
	}

	return false;
}



FSlateWindowElementList::FSlateWindowElementList(const TSharedPtr<SWindow>& InPaintWindow)
	: WeakPaintWindow(InPaintWindow)
	, RawPaintWindow(InPaintWindow.Get())
	, MemManager()
#if STATS
	, MemManagerAllocatedMemory(0)
#endif
	, RenderTargetWindow(nullptr)
	, bNeedsDeferredResolve(false)
	, ResolveToDeferredIndex()
	, WindowSize(FVector2f(0.0f, 0.0f))
	//, bReportReferences(true)
{
	if (InPaintWindow.IsValid())
	{
		WindowSize = UE::Slate::CastToVector2f(InPaintWindow->GetSizeInScreen());
	}

	// Only keep UObject resources alive if this window element list is born on the game thread.
/*
	if (IsInGameThread())
	{
		ResourceGCRoot = MakeUnique<FWindowElementGCObject>(this);
	}*/
}

FSlateWindowElementList::~FSlateWindowElementList()
{
	/*if (ResourceGCRoot.IsValid())
	{
		ResourceGCRoot->ClearOwner();
	}*/
}

void FSlateDrawElement::Init(FSlateWindowElementList& ElementList, EElementType InElementType, uint32 InLayer, const FPaintGeometry& PaintGeometry, ESlateDrawEffect InDrawEffects)
{
	RenderTransform = PaintGeometry.GetAccumulatedRenderTransform();
	Position = PaintGeometry.DrawPosition;
	Scale = PaintGeometry.DrawScale;
	LocalSize = FVector2f(PaintGeometry.GetLocalSize());
	ClipStateHandle.SetPreCachedClipIndex(ElementList.GetClippingIndex());

#if UE_SLATE_VERIFY_PIXELSIZE
	const FVector2f PixelSize = FVector2f(LocalSize * Scale);
	ensureMsgf(PixelSize.X >= 0.f && PixelSize.X <= (float)std::numeric_limits<uint16>::max(), TEXT("The size X '%f' is too small or big to fit in the SlateVertex buffer."), PixelSize.X);
	ensureMsgf(PixelSize.Y >= 0.f && PixelSize.Y <= (float)std::numeric_limits<uint16>::max(), TEXT("The size Y '%f' is too small or big to fit in the SlateVertex buffer."), PixelSize.Y);
#endif

	LayerId = InLayer;

	ElementType = InElementType;
	DrawEffects = InDrawEffects;
	
	// Calculate the layout to render transform as this is needed by several calculations downstream.
	const FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(Scale, Position)));

	{
		// This is a workaround because we want to keep track of the various Scenes 
		// in use throughout the UI. We keep a synchronized set with the render thread on the SlateRenderer and 
		// use indices to synchronize between them.
		FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer();
		checkSlow(Renderer);
		int32 SceneIndexLong = Renderer->GetCurrentSceneIndex();
		ensureMsgf(SceneIndexLong <= std::numeric_limits<int8>::max(), TEXT("The Scene index is saved as an int8 in the DrawElements."));
		SceneIndex = (int8)SceneIndexLong;
	}

	BatchFlags = ESlateBatchDrawFlag::None;
	BatchFlags |= static_cast<ESlateBatchDrawFlag>(static_cast<uint32>(InDrawEffects) & static_cast<uint32>(ESlateDrawEffect::NoBlending | ESlateDrawEffect::PreMultipliedAlpha | ESlateDrawEffect::NoGamma | ESlateDrawEffect::InvertAlpha));

	static_assert(((__underlying_type(ESlateDrawEffect))ESlateDrawEffect::NoBlending) == ((__underlying_type(ESlateBatchDrawFlag))ESlateBatchDrawFlag::NoBlending), "Must keep ESlateBatchDrawFlag and ESlateDrawEffect partial matches");
	static_assert(((__underlying_type(ESlateDrawEffect))ESlateDrawEffect::PreMultipliedAlpha) == ((__underlying_type(ESlateBatchDrawFlag))ESlateBatchDrawFlag::PreMultipliedAlpha), "Must keep ESlateBatchDrawFlag and ESlateDrawEffect partial matches");
	static_assert(((__underlying_type(ESlateDrawEffect))ESlateDrawEffect::NoGamma) == ((__underlying_type(ESlateBatchDrawFlag))ESlateBatchDrawFlag::NoGamma), "Must keep ESlateBatchDrawFlag and ESlateDrawEffect partial matches");
	static_assert(((__underlying_type(ESlateDrawEffect))ESlateDrawEffect::InvertAlpha) == ((__underlying_type(ESlateBatchDrawFlag))ESlateBatchDrawFlag::InvertAlpha), "Must keep ESlateBatchDrawFlag and ESlateDrawEffect partial matches");
	if ((InDrawEffects & ESlateDrawEffect::ReverseGamma) != ESlateDrawEffect::None)
	{
		BatchFlags |= ESlateBatchDrawFlag::ReverseGamma;
	}
}

void FSlateDrawElement::ApplyPositionOffset(FVector2d InOffset)
{
	ApplyPositionOffset(UE::Slate::CastToVector2f(InOffset));
}

void FSlateDrawElement::ApplyPositionOffset(FVector2f InOffset)
{
	SetPosition(Position + InOffset);
	RenderTransform = Concatenate(RenderTransform, InOffset);

	// Recompute cached layout to render transform
	const FSlateLayoutTransform InverseLayoutTransform(Inverse(FSlateLayoutTransform(Scale, Position)));
}

void FSlateDrawElement::MakeDebugQuad( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, FLinearColor Tint)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();
	FSlateBoxPayload& BoxPayload = ElementList.CreatePayload<FSlateBoxPayload>(Element);

	BoxPayload.SetTint(Tint);
	Element.Init(ElementList, EElementType::ET_DebugQuad, InLayer, PaintGeometry, ESlateDrawEffect::None);
}

FSlateDrawElement& FSlateDrawElement::MakeBoxInternal(
	FSlateWindowElementList& ElementList,
	uint32 InLayer,
	const FPaintGeometry& PaintGeometry,
	const FSlateBrush* InBrush,
	ESlateDrawEffect InDrawEffects,
	const FLinearColor& InTint
)
{
	EElementType ElementType = (InBrush->DrawAs == ESlateBrushDrawType::Border) ? EElementType::ET_Border : (InBrush->DrawAs == ESlateBrushDrawType::RoundedBox) ? EElementType::ET_RoundedBox : EElementType::ET_Box;

	// Cast to Rounded Rect to get the internal parameters 
	// New payload type - inherit from BoxPayload 

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateBoxPayload* BoxPayload;
	if ( ElementType == EElementType::ET_RoundedBox )
	{
		FSlateRoundedBoxPayload* RBoxPayload = &ElementList.CreatePayload<FSlateRoundedBoxPayload>(Element);
		FVector4f CornerRadii = FVector4f(InBrush->OutlineSettings.CornerRadii);
		if (InBrush->OutlineSettings.RoundingType == ESlateBrushRoundingType::HalfHeightRadius)
		{
			const float UniformRadius = PaintGeometry.GetLocalSize().Y / 2.0f;
			CornerRadii = FVector4f(UniformRadius, UniformRadius, UniformRadius, UniformRadius);
		}
		RBoxPayload->SetRadius(CornerRadii);

		if (InBrush->OutlineSettings.bUseBrushTransparency)
		{
			FLinearColor Color = InBrush->OutlineSettings.Color.GetSpecifiedColor().CopyWithNewOpacity(InTint.A);
			RBoxPayload->SetOutline(Color, InBrush->OutlineSettings.Width);
		}
		else
		{
			RBoxPayload->SetOutline(InBrush->OutlineSettings.Color.GetSpecifiedColor(), InBrush->OutlineSettings.Width);
		}
		BoxPayload = RBoxPayload;
	}
	else 
	{
		BoxPayload = &ElementList.CreatePayload<FSlateBoxPayload>(Element);
	}

	BoxPayload->SetTint(InTint);
	BoxPayload->SetBrush(InBrush, PaintGeometry.GetLocalSize(), PaintGeometry.DrawScale);

	Element.Init(ElementList, ElementType, InLayer, PaintGeometry, InDrawEffects);

	return Element;
}

void FSlateDrawElement::MakeBox(
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush,
	ESlateDrawEffect InDrawEffects, 
	const FLinearColor& InTint)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList, PaintGeometry, InBrush, InTint))
	{
		return;
	}

	MakeBoxInternal(ElementList, InLayer, PaintGeometry, InBrush, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeBox( 
	FSlateWindowElementList& ElementList,
	uint32 InLayer, 
	const FPaintGeometry& PaintGeometry, 
	const FSlateBrush* InBrush, 
	const FSlateResourceHandle& InRenderingHandle,
	ESlateDrawEffect InDrawEffects, 
	const FLinearColor& InTint )
{
	MakeBox(ElementList, InLayer, PaintGeometry, InBrush, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeRotatedBox(
	FSlateWindowElementList& ElementList,
	uint32 InLayer,
	const FPaintGeometry& PaintGeometry,
	const FSlateBrush* InBrush,
	ESlateDrawEffect InDrawEffects,
	float Angle2D,
	TOptional<FVector2d> InRotationPoint,
	ERotationSpace RotationSpace,
	const FLinearColor& InTint)
{
	TOptional<FVector2f> RotationPoint = InRotationPoint.IsSet() ? UE::Slate::CastToVector2f(InRotationPoint.GetValue()) : TOptional<FVector2f>();
	MakeRotatedBox(ElementList, InLayer, PaintGeometry, InBrush, InDrawEffects, Angle2D, RotationPoint, RotationSpace, InTint);
}

void FSlateDrawElement::MakeRotatedBox(
	FSlateWindowElementList& ElementList,
	uint32 InLayer,
	const FPaintGeometry& PaintGeometry,
	const FSlateBrush* InBrush,
	ESlateDrawEffect InDrawEffects,
	float Angle2D,
	TOptional<FVector2f> InRotationPoint,
	ERotationSpace RotationSpace,
	const FLinearColor& InTint)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList, PaintGeometry, InBrush, InTint))
	{
		return;
	}

	FSlateDrawElement& DrawElement = MakeBoxInternal(ElementList, InLayer, PaintGeometry, InBrush, InDrawEffects, InTint);
	
	if (Angle2D != 0.0f)
	{
		const FVector2f RotationPoint = GetRotationPoint(PaintGeometry, InRotationPoint, RotationSpace);
		const FSlateRenderTransform RotationTransform = Concatenate(Inverse(RotationPoint), FQuat2f(Angle2D), RotationPoint);
		DrawElement.SetRenderTransform(Concatenate(RotationTransform, DrawElement.GetRenderTransform()));
	}
}

void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const int32 StartIndex, const int32 EndIndex, const FSlateFontInfo& InFontInfo, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList, PaintGeometry, InTint, InText))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();
	FSlateTextPayload& DataPayload = ElementList.CreatePayload<FSlateTextPayload>(Element);

	DataPayload.SetTint(InTint);
	DataPayload.SetText(InText, InFontInfo, StartIndex, EndIndex);

	Element.Init(ElementList, EElementType::ET_Text, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeText( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FString& InText, const FSlateFontInfo& InFontInfo, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint )
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	// Don't try and render empty text
	if (InText.Len() == 0)
	{
		return;
	}

	if (ShouldCull(ElementList, PaintGeometry, InTint, InText))
	{
		return;
	}

	// Don't do anything if there the font would be completely transparent 
	if (InTint.A == 0 && !InFontInfo.OutlineSettings.IsVisible())
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateTextPayload& DataPayload = ElementList.CreatePayload<FSlateTextPayload>(Element);

	DataPayload.SetTint(InTint);
	DataPayload.SetText(InText, InFontInfo);

	Element.Init(ElementList, EElementType::ET_Text, InLayer, PaintGeometry, InDrawEffects);
}

namespace SlateDrawElement
{
#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	const FName MaterialInterfaceClassName = "MaterialInterface";
	void CheckInvalidUMaterial(const UObject* InMaterialResource, const TCHAR* MaterialMessage)
	{
		if (InMaterialResource && GSlateCheckUObjectRenderResources)
		{
			bool bIsValidLowLevel = InMaterialResource->IsValidLowLevelFast(false);
			if (!bIsValidLowLevel || !IsValid(InMaterialResource) || InMaterialResource->GetClass()->GetFName() == MaterialInterfaceClassName)
			{
				UE_LOG(LogSlate, Error, TEXT("Material '%s' is not valid. PendingKill:'%d'. ValidLowLevelFast:'%d'. InvalidClass:'%d'")
					, MaterialMessage
					, (bIsValidLowLevel ? !IsValid(InMaterialResource) : false)
					, bIsValidLowLevel
					, (bIsValidLowLevel ? InMaterialResource->GetClass()->GetFName() == MaterialInterfaceClassName : false));

				if (bIsValidLowLevel)
				{
					UE_LOG(LogSlate, Error, TEXT("Material name: '%s'"), *InMaterialResource->GetFullName());
				}

				const TCHAR* Message = TEXT("We detected an invalid resource in FSlateDrawElement. Check the log for more detail.");
				if (GSlateCheckUObjectRenderResourcesShouldLogFatal)
				{
					UE_LOG(LogSlate, Fatal, TEXT("%s"), Message);
				}
				else
				{
					ensureAlwaysMsgf(false, TEXT("%s"), Message);
				}
			}
		}
	}
#endif
}

void FSlateDrawElement::MakeShapedText(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FShapedGlyphSequenceRef& InShapedGlyphSequence, ESlateDrawEffect InDrawEffects, const FLinearColor& BaseTint, const FLinearColor& OutlineTint, FTextOverflowArgs TextOverflowArgs)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeTime)
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (InShapedGlyphSequence->GetGlyphsToRender().Num() == 0)
	{
		return;
	}

	if (ShouldCull(ElementList, PaintGeometry))
	{
		return;
	}

	// Don't do anything if there the font would be completely transparent 
	if ((BaseTint.A == 0 && InShapedGlyphSequence->GetFontOutlineSettings().OutlineSize == 0) || 
		(BaseTint.A == 0 && OutlineTint.A == 0))
	{
		return;
	}

#if SLATE_CHECK_UOBJECT_RENDER_RESOURCES
	SlateDrawElement::CheckInvalidUMaterial(InShapedGlyphSequence->GetFontMaterial(), TEXT("Font Material"));
	SlateDrawElement::CheckInvalidUMaterial(InShapedGlyphSequence->GetFontOutlineSettings().OutlineMaterial, TEXT("Outline Material"));
#endif

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateShapedTextPayload& DataPayload = ElementList.CreatePayload<FSlateShapedTextPayload>(Element);
	DataPayload.SetTint(BaseTint);
	DataPayload.SetShapedText(ElementList, InShapedGlyphSequence, OutlineTint);
	DataPayload.SetOverflowArgs(TextOverflowArgs);

	Element.Init(ElementList, EElementType::ET_ShapedText, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeGradient( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FSlateGradientStop> InGradientStops, EOrientation InGradientType, ESlateDrawEffect InDrawEffects, FVector4f CornerRadius)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList, PaintGeometry))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateGradientPayload& DataPayload = ElementList.CreatePayload<FSlateGradientPayload>(Element);

	DataPayload.SetGradient(InGradientStops, InGradientType, CornerRadius);

	Element.Init(ElementList, EElementType::ET_Gradient, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2d InStart, const FVector2d InStartDir, const FVector2d InEnd, const FVector2d InEndDir, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	MakeSpline(ElementList, InLayer, PaintGeometry, UE::Slate::CastToVector2f(InStart), UE::Slate::CastToVector2f(InStartDir), UE::Slate::CastToVector2f(InEnd), UE::Slate::CastToVector2f(InEndDir), InThickness, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2f InStart, const FVector2f InStartDir, const FVector2f InEnd, const FVector2f InEndDir, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}
	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateSplinePayload& DataPayload = ElementList.CreatePayload<FSlateSplinePayload>(Element);

	DataPayload.SetHermiteSpline(InStart, InStartDir, InEnd, InEndDir, InThickness, InTint);

	Element.Init(ElementList, EElementType::ET_Spline, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeCubicBezierSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2d P0, const FVector2d P1, const FVector2d P2, const FVector2d P3, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	MakeCubicBezierSpline(ElementList, InLayer, PaintGeometry, UE::Slate::CastToVector2f(P0), UE::Slate::CastToVector2f(P1), UE::Slate::CastToVector2f(P2), UE::Slate::CastToVector2f(P3), InThickness, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeCubicBezierSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector2f P0, const FVector2f P1, const FVector2f P2, const FVector2f P3, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}
	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateSplinePayload& DataPayload = ElementList.CreatePayload<FSlateSplinePayload>(Element);

	DataPayload.SetCubicBezier(P0, P1, P2, P3, InThickness, InTint);

	Element.Init(ElementList, EElementType::ET_Spline, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeDrawSpaceSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2d InStart, const FVector2d InStartDir, const FVector2d InEnd, const FVector2d InEndDir, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	MakeSpline(ElementList, InLayer, FPaintGeometry(), InStart, InStartDir, InEnd, InEndDir, InThickness, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeDrawSpaceSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2f InStart, const FVector2f InStartDir, const FVector2f InEnd, const FVector2f InEndDir, float InThickness, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint)
{
	MakeSpline(ElementList, InLayer, FPaintGeometry(), InStart, InStartDir, InEnd, InEndDir, InThickness, InDrawEffects, InTint);
}

void FSlateDrawElement::MakeDrawSpaceGradientSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const TArray<FSlateGradientStop>& InGradientStops, float InThickness, ESlateDrawEffect InDrawEffects)
{
	const FPaintGeometry PaintGeometry;
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateSplinePayload& DataPayload = ElementList.CreatePayload<FSlateSplinePayload>(Element);
	DataPayload.SetGradientHermiteSpline(InStart, InStartDir, InEnd, InEndDir, InThickness, InGradientStops);

	Element.Init(ElementList, EElementType::ET_Spline, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeDrawSpaceGradientSpline(FSlateWindowElementList& ElementList, uint32 InLayer, const FVector2D& InStart, const FVector2D& InStartDir, const FVector2D& InEnd, const FVector2D& InEndDir, const FSlateRect InClippingRect, const TArray<FSlateGradientStop>& InGradientStops, float InThickness, ESlateDrawEffect InDrawEffects)
{
	const FPaintGeometry PaintGeometry;
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateSplinePayload& DataPayload = ElementList.CreatePayload<FSlateSplinePayload>(Element);
	DataPayload.SetGradientHermiteSpline(InStart, InStartDir, InEnd, InEndDir, InThickness, InGradientStops);

	Element.Init(ElementList, EElementType::ET_Spline, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeLines(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2d>& Points, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint, bool bAntialias, float Thickness)
{
	if (ShouldCull(ElementList) || Points.Num() < 2)
	{
		return;
	}

	TArray<FVector2f> NewVector;
	NewVector.Reserve(Points.Num());
	for (FVector2d Vect : Points)
	{
		NewVector.Add(UE::Slate::CastToVector2f(Vect));
	}
	MakeLines(ElementList, InLayer, PaintGeometry, MoveTemp(NewVector), InDrawEffects, InTint, bAntialias, Thickness);
}

void FSlateDrawElement::MakeLines(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FVector2f> Points, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint, bool bAntialias, float Thickness)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList) || Points.Num() < 2)
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateLinePayload& DataPayload = ElementList.CreatePayload<FSlateLinePayload>(Element);

	DataPayload.SetTint(InTint);
	DataPayload.SetThickness(Thickness);
	DataPayload.SetLines(MoveTemp(Points), bAntialias);

	ESlateDrawEffect DrawEffects = InDrawEffects;
	if (bAntialias)
	{
		// If the line is to be anti-aliased, we cannot reliably snap
		// the generated vertices
		DrawEffects |= ESlateDrawEffect::NoPixelSnapping;
	}

	Element.Init(ElementList, EElementType::ET_Line, InLayer, PaintGeometry, DrawEffects);
}

void FSlateDrawElement::MakeLines(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const TArray<FVector2d>& Points, const TArray<FLinearColor>& PointColors, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint, bool bAntialias, float Thickness)
{
	if (ShouldCull(ElementList) || Points.Num() < 2)
	{
		return;
	}

	TArray<FVector2f> NewVector;
	NewVector.Reserve(Points.Num());
	for (FVector2d Vect : Points)
	{
		NewVector.Add(UE::Slate::CastToVector2f(Vect));
	}
	MakeLines(ElementList, InLayer, PaintGeometry, MoveTemp(NewVector), PointColors, InDrawEffects, InTint, bAntialias, Thickness);
}

void FSlateDrawElement::MakeLines( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TArray<FVector2f> Points, TArray<FLinearColor> PointColors, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint, bool bAntialias, float Thickness )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList) || Points.Num() < 2)
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateLinePayload& DataPayload = ElementList.CreatePayload<FSlateLinePayload>(Element);
	DataPayload.SetTint(InTint);
	DataPayload.SetThickness(Thickness);
	DataPayload.SetLines(MoveTemp(Points), bAntialias, MoveTemp(PointColors));

	Element.Init(ElementList, EElementType::ET_Line, InLayer, PaintGeometry, InDrawEffects);
}

void FSlateDrawElement::MakeViewport( FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, TSharedPtr<const ISlateViewport> Viewport, ESlateDrawEffect InDrawEffects, const FLinearColor& InTint )
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();
	FSlateViewportPayload& DataPayload = ElementList.CreatePayload<FSlateViewportPayload>(Element);

	DataPayload.SetViewport(Viewport, InTint);
	check(DataPayload.RenderTargetResource == nullptr || !DataPayload.RenderTargetResource->Debug_IsDestroyed());

	Element.Init(ElementList, EElementType::ET_Viewport, InLayer, PaintGeometry, InDrawEffects);
}


void FSlateDrawElement::MakeCustom( FSlateWindowElementList& ElementList, uint32 InLayer, TSharedPtr<ICustomSlateElement, ESPMode::ThreadSafe> CustomDrawer )
{
	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlateCustomDrawerPayload& DataPayload = ElementList.CreatePayload<FSlateCustomDrawerPayload>(Element);
	DataPayload.SetCustomDrawer(CustomDrawer);
	
	Element.Init(ElementList, EElementType::ET_Custom, InLayer, FPaintGeometry(), ESlateDrawEffect::None);
	Element.RenderTransform = FSlateRenderTransform();
}


void FSlateDrawElement::MakeCustomVerts(FSlateWindowElementList& ElementList, uint32 InLayer, const FSlateResourceHandle& InRenderResourceHandle, const TArray<FSlateVertex>& InVerts, const TArray<SlateIndex>& InIndexes, ISlateUpdatableInstanceBuffer* InInstanceData, uint32 InInstanceOffset, uint32 InNumInstances, ESlateDrawEffect InDrawEffects)
{
	SCOPE_CYCLE_COUNTER(STAT_SlateDrawElementMakeCustomVertsTime);
	
	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();
	FSlateCustomVertsPayload& DataPayload = ElementList.CreatePayload<FSlateCustomVertsPayload>(Element);

	const FSlateShaderResourceProxy* RenderingProxy = InRenderResourceHandle.GetResourceProxy();
	ISlateUpdatableInstanceBufferRenderProxy* RenderProxy = InInstanceData ? InInstanceData->GetRenderProxy() : nullptr;
	DataPayload.SetCustomVerts(RenderingProxy, InVerts, InIndexes, RenderProxy, InInstanceOffset, InNumInstances);

	Element.Init(ElementList, EElementType::ET_CustomVerts, InLayer, FPaintGeometry(), InDrawEffects);
	Element.RenderTransform = FSlateRenderTransform();
}

void FSlateDrawElement::MakePostProcessPass(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry, const FVector4f& Params, int32 DownsampleAmount, FVector4f CornerRadius)
{
	PaintGeometry.CommitTransformsIfUsingLegacyConstructor();

	if (ShouldCull(ElementList))
	{
		return;
	}

	FSlateDrawElement& Element = ElementList.AddUninitialized();

	FSlatePostProcessPayload& DataPayload = ElementList.CreatePayload<FSlatePostProcessPayload>(Element);
	DataPayload.DownsampleAmount = DownsampleAmount;
	DataPayload.PostProcessData = Params;
	DataPayload.CornerRadius = CornerRadius;

	Element.Init(ElementList, EElementType::ET_PostProcessPass, InLayer, PaintGeometry, ESlateDrawEffect::None);
}

FSlateDrawElement::FSlateDrawElement()
	: DataPayload(nullptr)
	, bIsCached(false)
{

}

FSlateDrawElement::~FSlateDrawElement()
{
	if (bIsCached)
	{
		delete DataPayload;
	}
	else if (DataPayload)
	{
		// Allocated by a memstack so we just need to call the destructor manually
		DataPayload->~FSlateDataPayload();
	}
}

FVector2f FSlateDrawElement::GetRotationPoint(const FPaintGeometry& PaintGeometry, const TOptional<FVector2f>& UserRotationPoint, ERotationSpace RotationSpace)
{
	FVector2f RotationPoint(0, 0);

	const FVector2f LocalSize = UE::Slate::CastToVector2f(PaintGeometry.GetLocalSize());

	switch (RotationSpace)
	{
		case RelativeToElement:
		{
			// If the user did not specify a rotation point, we rotate about the center of the element
			RotationPoint = UserRotationPoint.Get(LocalSize * 0.5f);
		}
		break;
		case RelativeToWorld:
		{
			// its in world space, must convert the point to local space.
			RotationPoint = TransformPoint(Inverse(PaintGeometry.GetAccumulatedRenderTransform()), UserRotationPoint.Get(FVector2f::ZeroVector));
		}
		break;
	default:
		check(0);
		break;
	}

	return RotationPoint;
}

void FSlateDrawElement::AddReferencedObjects(FReferenceCollector& Collector)
{
	if(DataPayload)
	{
		DataPayload->AddReferencedObjects(Collector);
	}
}


FSlateDrawElement& FSlateWindowElementList::AddUninitialized()
{
	const bool bAllowCache = CachedElementDataListStack.Num() > 0 && WidgetDrawStack.Num() && !WidgetDrawStack.Top().bIsVolatile;

	if (bAllowCache)
	{
		// @todo get working with slate debugging
		return AddCachedElement();
	}
	else
	{
		FSlateDrawElementArray& Elements = UncachedDrawElements;
		const int32 InsertIdx = Elements.AddDefaulted();

#if WITH_SLATE_DEBUGGING
		FSlateDebugging::ElementAdded.Broadcast(*this, InsertIdx);
#endif

		FSlateDrawElement& NewElement = Elements[InsertIdx];
		return Elements[InsertIdx];
	}
}


FSlateWindowElementList::FDeferredPaint::FDeferredPaint( const TSharedRef<const SWidget>& InWidgetToPaint, const FPaintArgs& InArgs, const FGeometry InAllottedGeometry, const FWidgetStyle& InWidgetStyle, bool InParentEnabled )
	: WidgetToPaintPtr( InWidgetToPaint )
	, Args( InArgs )
	, AllottedGeometry( InAllottedGeometry )
	, WidgetStyle( InWidgetStyle )
	, bParentEnabled( InParentEnabled )
{
	const_cast<FPaintArgs&>(Args).SetDeferredPaint(true);

#if WITH_SLATE_DEBUGGING
	// We need to perform this update here, because otherwise we'll warn that this widget
	// was not painted along the fast path, which, it will be, but later because it's deferred,
	// but we need to go ahead and update the painted frame to match the current one, so
	// that we don't think this widget was forgotten.
	const_cast<SWidget&>(InWidgetToPaint.Get()).Debug_UpdateLastPaintFrame();
#endif
}

FSlateWindowElementList::FDeferredPaint::FDeferredPaint(const FDeferredPaint& Copy, const FPaintArgs& InArgs)
	: WidgetToPaintPtr(Copy.WidgetToPaintPtr)
	, Args(InArgs)
	, AllottedGeometry(Copy.AllottedGeometry)
	, WidgetStyle(Copy.WidgetStyle)
	, bParentEnabled(Copy.bParentEnabled)
{
	const_cast<FPaintArgs&>(Args).SetDeferredPaint(true);
}

int32 FSlateWindowElementList::FDeferredPaint::ExecutePaint(int32 LayerId, FSlateWindowElementList& OutDrawElements, const FSlateRect& MyCullingRect) const
{
	TSharedPtr<const SWidget> WidgetToPaint = WidgetToPaintPtr.Pin();
	if ( WidgetToPaint.IsValid() )
	{
		return WidgetToPaint->Paint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, WidgetStyle, bParentEnabled );
	}

	return LayerId;
}

FSlateWindowElementList::FDeferredPaint FSlateWindowElementList::FDeferredPaint::Copy(const FPaintArgs& InArgs)
{
	return FDeferredPaint(*this, InArgs);
}


void FSlateWindowElementList::QueueDeferredPainting( const FDeferredPaint& InDeferredPaint )
{
	DeferredPaintList.Add(MakeShared<FDeferredPaint>(InDeferredPaint));
}

int32 FSlateWindowElementList::PaintDeferred(int32 LayerId, const FSlateRect& MyCullingRect)
{
	bNeedsDeferredResolve = false;

	int32 ResolveIndex = ResolveToDeferredIndex.Pop(false);

	for ( int32 i = ResolveIndex; i < DeferredPaintList.Num(); ++i )
	{
		LayerId = DeferredPaintList[i]->ExecutePaint(LayerId, *this, MyCullingRect);
	}

	for ( int32 i = DeferredPaintList.Num() - 1; i >= ResolveIndex; --i )
	{
		DeferredPaintList.RemoveAt(i, 1, false);
	}

	return LayerId;
}

void FSlateWindowElementList::BeginDeferredGroup()
{
	ResolveToDeferredIndex.Add(DeferredPaintList.Num());
}

void FSlateWindowElementList::EndDeferredGroup()
{
	bNeedsDeferredResolve = true;
}

void FSlateWindowElementList::PushPaintingWidget(const SWidget& CurrentWidget, int32 StartingLayerId, FSlateCachedElementsHandle& CurrentCacheHandle)
{
	FSlateCachedElementData* CurrentCachedElementData = GetCurrentCachedElementData();
	if (CurrentCachedElementData)
	{
		// When a widget is pushed reset its draw elements.  They are being recached or possibly going away
		if (CurrentCacheHandle.IsValid())
		{
#if WITH_SLATE_DEBUGGING
			check(CurrentCacheHandle.IsOwnedByWidget(&CurrentWidget));
#endif
			CurrentCacheHandle.ClearCachedElements();
		}

		WidgetDrawStack.Emplace(CurrentCacheHandle, CurrentWidget.IsVolatileIndirectly() || CurrentWidget.IsVolatile(), &CurrentWidget);
	}
}

FSlateCachedElementsHandle FSlateWindowElementList::PopPaintingWidget(const SWidget& CurrentWidget)
{
	FSlateCachedElementData* CurrentCachedElementData = GetCurrentCachedElementData();
	if (CurrentCachedElementData)
	{
#if WITH_SLATE_DEBUGGING
		check(WidgetDrawStack.Top().Widget == &CurrentWidget);
#endif

		const bool bAllowShrinking = false;
		return WidgetDrawStack.Pop(bAllowShrinking).CacheHandle;
	}

	return FSlateCachedElementsHandle::Invalid;
}

/*
int32 FSlateWindowElementList::PushBatchPriortyGroup(const SWidget& CurrentWidget)
{
	int32 NewPriorityGroup = 0;
/ *
	if (GSlateEnableGlobalInvalidation)
	{
		NewPriorityGroup = BatchDepthPriorityStack.Add_GetRef(CurrentWidget.FastPathProxyHandle.IsValid() ? CurrentWidget.FastPathProxyHandle.GetIndex() : 0);
	}
	else
	{
		NewPriorityGroup = BatchDepthPriorityStack.Add_GetRef(MaxPriorityGroup + 1);
		//NewPriorityGroup = BatchDepthPriorityStack.Add_GetRef(0);
	}

	// Should be +1 or the first overlay slot will not appear on top of stuff below it?
	// const int32 NewPriorityGroup = BatchDepthPriorityStack.Add_GetRef(BatchDepthPriorityStack.Num() ? BatchDepthPriorityStack.Top()+1 : 1);

	MaxPriorityGroup = FMath::Max(NewPriorityGroup, MaxPriorityGroup);* /
	return NewPriorityGroup;
}

int32 FSlateWindowElementList::PushAbsoluteBatchPriortyGroup(int32 BatchPriorityGroup)
{
	return 0;// return BatchDepthPriorityStack.Add_GetRef(BatchPriorityGroup);
}

void FSlateWindowElementList::PopBatchPriortyGroup()
{
	//BatchDepthPriorityStack.Pop();
}*/

FSlateDrawElement& FSlateWindowElementList::AddCachedElement()
{
	FSlateCachedElementData* CurrentCachedElementData = GetCurrentCachedElementData();
	check(CurrentCachedElementData);

	FWidgetDrawElementState& CurrentWidgetState = WidgetDrawStack.Top();
	check(!CurrentWidgetState.bIsVolatile);

	if (!CurrentWidgetState.CacheHandle.IsValid())
	{
		CurrentWidgetState.CacheHandle = CurrentCachedElementData->AddCache(CurrentWidgetState.Widget);
	}

	return CurrentCachedElementData->AddCachedElement(CurrentWidgetState.CacheHandle, GetClippingManager(), CurrentWidgetState.Widget);
}

void FSlateWindowElementList::PushCachedElementData(FSlateCachedElementData& CachedElementData)
{
	check(&CachedElementData); 
	const int32 Index = CachedElementDataList.AddUnique(&CachedElementData);
	CachedElementDataListStack.Push(Index);
}

void FSlateWindowElementList::PopCachedElementData()
{
	CachedElementDataListStack.Pop();
}

int32 FSlateWindowElementList::PushClip(const FSlateClippingZone& InClipZone)
{
	const int32 NewClipIndex = ClippingManager.PushClip(InClipZone);

	return NewClipIndex;
}

int32 FSlateWindowElementList::GetClippingIndex() const
{
	return ClippingManager.GetClippingIndex();
}

TOptional<FSlateClippingState> FSlateWindowElementList::GetClippingState() const
{
	return ClippingManager.GetActiveClippingState();
}

void FSlateWindowElementList::PopClip()
{
	ClippingManager.PopClip();
}

void FSlateWindowElementList::PopClipToStackIndex(int32 Index)
{
	ClippingManager.PopToStackIndex(Index);
}


void FSlateWindowElementList::SetRenderTargetWindow(SWindow* InRenderTargetWindow)
{
	check(IsThreadSafeForSlateRendering());
	RenderTargetWindow = InRenderTargetWindow;
}

DECLARE_MEMORY_STAT(TEXT("FSlateWindowElementList MemManager"), STAT_FSlateWindowElementListMemManager, STATGROUP_SlateMemory);
DECLARE_DWORD_COUNTER_STAT(TEXT("FSlateWindowElementList MemManager Count"), STAT_FSlateWindowElementListMemManagerCount, STATGROUP_SlateMemory);

void FSlateWindowElementList::ResetElementList()
{
	QUICK_SCOPE_CYCLE_COUNTER(Slate_ResetElementList);

	DeferredPaintList.Reset();

	BatchData.ResetData();
	BatchDataHDR.ResetData();

	ClippingManager.ResetClippingState();

	UncachedDrawElements.Reset();

#if STATS
	const int32 DeltaMemory = MemManager.GetByteCount() - MemManagerAllocatedMemory;
	INC_DWORD_STAT(STAT_FSlateWindowElementListMemManagerCount);
	INC_MEMORY_STAT_BY(STAT_FSlateWindowElementListMemManager, DeltaMemory);

	MemManagerAllocatedMemory = MemManager.GetByteCount();
#endif

	MemManager.Flush();
	
	CachedElementDataList.Empty();
	CachedElementDataListStack.Empty();

	check(WidgetDrawStack.Num() == 0);
	check(ResolveToDeferredIndex.Num() == 0);

	RenderTargetWindow = nullptr;
}


void FSlateWindowElementList::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FSlateDrawElement& Element : UncachedDrawElements)
	{
		Element.AddReferencedObjects(Collector);
	}
}

static const FSlateClippingState* GetClipStateFromParent(const FSlateClippingManager& ParentClipManager)
{
	const int32 ClippingIndex = ParentClipManager.GetClippingIndex();

	if(ClippingIndex != INDEX_NONE)
	{
		return &ParentClipManager.GetClippingStates()[ClippingIndex];
	}
	else
	{
		return nullptr;
	}
}

void FSlateCachedElementData::Empty()
{
	if (CachedElementLists.Num())
	{
		UE_LOG(LogSlate, Verbose, TEXT("Resetting cached element data.  Num: %d"), CachedElementLists.Num());
	}

#if WITH_SLATE_DEBUGGING
	for (TSharedPtr<FSlateCachedElementList>& CachedElementList : CachedElementLists)
	{
		ensure(CachedElementList.IsUnique());
	}
#endif

	CachedElementLists.Empty();
	CachedBatches.Empty();
	CachedClipStates.Empty();
	ListsWithNewData.Empty();
}

FSlateCachedElementsHandle FSlateCachedElementData::AddCache(const SWidget* Widget)
{
#if WITH_SLATE_DEBUGGING
	for (TSharedPtr<FSlateCachedElementList>& CachedElementList : CachedElementLists)
	{
		ensure(CachedElementList->OwningWidget != Widget);
	}
#endif

	TSharedRef<FSlateCachedElementList> NewList = MakeShared<FSlateCachedElementList>(this, Widget);
	NewList->Initialize();

	CachedElementLists.Add(NewList);

	return FSlateCachedElementsHandle(NewList);
}

FSlateDrawElement& FSlateCachedElementData::AddCachedElement(FSlateCachedElementsHandle& CacheHandle, const FSlateClippingManager& ParentClipManager, const SWidget* CurrentWidget)
{
	TSharedPtr<FSlateCachedElementList> List = CacheHandle.Ptr.Pin();

#if WITH_SLATE_DEBUGGING
	check(List->OwningWidget == CurrentWidget);
	check(CurrentWidget->GetParentWidget().IsValid());
#endif

	FSlateDrawElement& NewElement = List->DrawElements.AddDefaulted_GetRef();
	NewElement.SetIsCached(true);

	// Check if slow vs checking a flag on the list to see if it contains new data.
	ListsWithNewData.AddUnique(List.Get());

	const FSlateClippingState* ExistingClipState = GetClipStateFromParent(ParentClipManager);

	if (ExistingClipState)
	{
		// We need to cache this clip state for the next time the element draws
		FSlateCachedClipState& CachedClipState = FindOrAddCachedClipState(ExistingClipState);
		List->AddCachedClipState(CachedClipState);
		NewElement.SetCachedClippingState(&CachedClipState.ClippingState.Get());
	}

	return NewElement;
}

FSlateRenderBatch& FSlateCachedElementData::AddCachedRenderBatch(FSlateRenderBatch&& NewBatch, int32& OutIndex)
{
	// Check perf against add.  AddAtLowest makes it generally re-add elements at the same index it just removed which is nicer on the cache
	int32 LowestFreedIndex = 0;
	OutIndex = CachedBatches.EmplaceAtLowestFreeIndex(LowestFreedIndex, NewBatch);
	return CachedBatches[OutIndex];
}

void FSlateCachedElementData::RemoveCachedRenderBatches(const TArray<int32>& CachedRenderBatchIndices)
{
	for (int32 Index : CachedRenderBatchIndices)
	{
		CachedBatches.RemoveAt(Index);
	}
}

FSlateCachedClipState& FSlateCachedElementData::FindOrAddCachedClipState(const FSlateClippingState* RefClipState)
{
	for (auto& CachedState : CachedClipStates)
	{
		if (*CachedState.ClippingState == *RefClipState)
		{
			return CachedState;
		}
	}

	return CachedClipStates.Emplace_GetRef(FSlateCachedClipState(*RefClipState));
}

void FSlateCachedElementData::CleanupUnusedClipStates()
{
	for(int32 CachedStateIdx = 0; CachedStateIdx < CachedClipStates.Num();)
	{
		const FSlateCachedClipState& CachedState = CachedClipStates[CachedStateIdx];
		if (CachedState.ClippingState.IsUnique())
		{
			CachedClipStates.RemoveAtSwap(CachedStateIdx);
		}
		else
		{
			++CachedStateIdx;
		}
	}
}

void FSlateCachedElementData::RemoveList(FSlateCachedElementsHandle& CacheHandle)
{
	TSharedPtr<FSlateCachedElementList> CachedList = CacheHandle.Ptr.Pin();
	CachedElementLists.RemoveSingleSwap(CachedList);
	ListsWithNewData.RemoveSingleSwap(CachedList.Get());

	CachedList->ClearCachedElements();
}

void FSlateCachedElementData::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TSharedPtr<FSlateCachedElementList>& CachedElementList : CachedElementLists)
	{
		CachedElementList->AddReferencedObjects(Collector);
	}
}

FSlateCachedElementList::~FSlateCachedElementList()
{
	DestroyCachedData();
}

void FSlateCachedElementList::ClearCachedElements()
{
	// Destroy vertex data in a thread safe way
	DestroyCachedData();

	CachedRenderingData = new FSlateCachedFastPathRenderingData;

#if 0 // enable this if you want to know why a widget is invalidated after it has been drawn but before it has been batched (probably a child or parent invalidating a relation)
	if (ensure(!bNewData))
	{
		UE_LOG(LogSlate, Log, TEXT("Cleared out data in cached ElementList for Widget: %s before it was batched"), *Widget->GetTag().ToString());
	}
#endif
}

FSlateRenderBatch& FSlateCachedElementList::AddRenderBatch(int32 InLayer, const FShaderParams& InShaderParams, const FSlateShaderResource* InResource, ESlateDrawPrimitive InPrimitiveType, ESlateShader InShaderType, ESlateDrawEffect InDrawEffects, ESlateBatchDrawFlag InDrawFlags, int8 SceneIndex)
{
	FSlateRenderBatch NewRenderBatch(InLayer, InShaderParams, InResource, InPrimitiveType, InShaderType, InDrawEffects, InDrawFlags, SceneIndex, &CachedRenderingData->Vertices, &CachedRenderingData->Indices, CachedRenderingData->Vertices.Num(), CachedRenderingData->Indices.Num());
	int32 RenderBatchIndex = INDEX_NONE;
	FSlateRenderBatch& AddedBatchRef = ParentData->AddCachedRenderBatch(MoveTemp(NewRenderBatch), RenderBatchIndex);
	
	check(RenderBatchIndex != INDEX_NONE);

	CachedRenderBatchIndices.Add(RenderBatchIndex);

	return AddedBatchRef;
	
	//return CachedBatches.Emplace_GetRef(InLayer, InShaderParams, InResource, InPrimitiveType, InShaderType, InDrawEffects, InDrawFlags, SceneIndex, &CachedRenderingData->Vertices, &CachedRenderingData->Indices, CachedRenderingData->Vertices.Num(), CachedRenderingData->Indices.Num());
}

void FSlateCachedElementList::AddCachedClipState(FSlateCachedClipState& ClipStateToCache)
{
	CachedRenderingData->CachedClipStates.Add(ClipStateToCache);
}

void FSlateCachedElementList::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FSlateDrawElement& Element : DrawElements)
	{
		Element.AddReferencedObjects(Collector);
	}
}

void FSlateCachedElementList::DestroyCachedData()
{
	// Clear out any cached draw elements
	DrawElements.Reset();

	// Clear out any cached render batches
	if (CachedRenderBatchIndices.Num())
	{
		ParentData->RemoveCachedRenderBatches(CachedRenderBatchIndices);
		CachedRenderBatchIndices.Reset();
	}

	// Destroy any cached rendering data we own.
	if (CachedRenderingData)
	{
		if (FSlateApplicationBase::IsInitialized())
		{
			if (FSlateRenderer* SlateRenderer = FSlateApplicationBase::Get().GetRenderer())
			{
				SlateRenderer->DestroyCachedFastPathRenderingData(CachedRenderingData);
			}
		}
		else
		{
			delete CachedRenderingData;
		}

		CachedRenderingData = nullptr;
	}
}

FSlateCachedElementsHandle FSlateCachedElementsHandle::Invalid;

void FSlateCachedElementsHandle::ClearCachedElements()
{
	if (TSharedPtr<FSlateCachedElementList> List = Ptr.Pin())
	{
		List->ClearCachedElements();
	}
}

void FSlateCachedElementsHandle::RemoveFromCache()
{
	if (TSharedPtr<FSlateCachedElementList> List = Ptr.Pin())
	{
		List->GetOwningData()->RemoveList(*this);
		ensure(List.IsUnique());
	}

	check(!Ptr.IsValid());
}

bool FSlateCachedElementsHandle::IsOwnedByWidget(const SWidget* Widget) const
{
	if (const TSharedPtr<FSlateCachedElementList> List = Ptr.Pin())
	{
		return List->OwningWidget == Widget;
	}

	return false;
}

bool FSlateCachedElementsHandle::HasCachedElements() const
{
	if (const TSharedPtr<FSlateCachedElementList> List = Ptr.Pin())
	{
		return List->DrawElements.Num() > 0;
	}

	return false;
}
