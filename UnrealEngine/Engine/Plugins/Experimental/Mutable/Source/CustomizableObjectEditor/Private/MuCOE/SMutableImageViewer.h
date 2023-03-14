// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "Math/IntPoint.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "MuR/Image.h"
#include "Templates/SharedPointer.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FPaintArgs;
class FReferenceCollector;
class FSlateRect;
class FSlateWindowElementList;
class FWidgetStyle;
class UTexture;
struct FGeometry;
struct FSlateBrush;


/** Describes different modes to show an image. */
enum class EMutableImageChannels : uint8
{
	RGBA,
	RGB,
	A
};


/** Simple widget that shows a UTexture. */
class SSimpleTextureViewer : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SSimpleTextureViewer){}
		SLATE_ATTRIBUTE(FIntPoint, GridSize)
		SLATE_ARGUMENT_DEFAULT(UTexture*, Texture) { nullptr };
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs );

	// SWidgetInterface
	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	FVector2D ComputeDesiredSize(float) const override;

	// FGCObject interface
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	FString GetReferencerName() const override;

	// Own interface

	/** Set the image to show in the widget. */
	void SetTexture(UTexture* InTexture);

	/** */
	EMutableImageChannels ImageChannels = EMutableImageChannels::RGBA;

private:

	/** Reference to the texture. */
	TObjectPtr<UTexture> Texture;

	/** Brush used to render the image. */
	TSharedPtr<FSlateBrush> TextureBrush;

	/** Size of the underlying rendered grid. */
	TAttribute<FIntPoint> GridSize;

};


/** Widget showing a Mutable image, with its properties. */
class SMutableImageViewer : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS(SMutableImageViewer) {}
		SLATE_ATTRIBUTE(FIntPoint, GridSize)
		SLATE_ARGUMENT(mu::ImagePtrConst, Image)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SWidget interface
	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// Own interface

	/** Set the image to show in the widget. */
	void SetImage(const mu::ImagePtrConst& Image, int32 VisibleLOD );

private:

	/** Brush used to render the image. */
	TSharedPtr<SSimpleTextureViewer> TextureViewer;

	/** Size of the underlying rendered grid. */
	TAttribute<FIntPoint> GridSize;

	/** */
	mu::ImagePtrConst MutableImage;

	/** Mipmap shown in the widget. 0 is the biggest one. */
	int32 CurrentVisibleLOD=0;

	/** Is true, the image or the visible LOD have changed and we need to update. */
	bool bIsPendingUpdate = false;

	/** User interface callbacks */
	FText GetImageDescriptionLabel() const;
	TOptional<int32> GetCurrentImageLOD() const;
	TOptional<int32> GetImageLODMaxValue() const;
	EVisibility IsLODSelectionVisible() const;
	void OnCurrentLODChanged(int32 NewValue);
	EMutableImageChannels GetImageChannels() const;
	void SetImageChannels(EMutableImageChannels);
};
