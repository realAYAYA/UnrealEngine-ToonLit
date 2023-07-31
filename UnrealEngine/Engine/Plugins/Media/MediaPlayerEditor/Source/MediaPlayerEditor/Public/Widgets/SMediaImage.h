// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/GCObject.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMaterial;
class UMaterialExpressionTextureSample;
class UTexture;

class MEDIAPLAYEREDITOR_API SMediaImage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMediaImage) { }
	SLATE_ATTRIBUTE(FVector2D, BrushImageSize);
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaImage();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InTexture The source texture to render if any
	 */
	void Construct(const FArguments& InArgs, UTexture* InTexture);

	/**
	 * Tick this widget
	 * 
	 * @param InAllottedGeometry Geometry of the widget.
	 * @param InCurrentTime CurrentTime of the engine.
	 * @param InDeltaTime Deltatime since last frame.
	 */
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	class FInternalReferenceCollector : public FGCObject
	{
	public:
		FInternalReferenceCollector(SMediaImage* InObject)
			: Object(InObject)
		{
		}

		//~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& InCollector) override
		{
			InCollector.AddReferencedObject(Object->Material);
			InCollector.AddReferencedObject(Object->TextureSampler);
		}
		virtual FString GetReferencerName() const override
		{
			return TEXT("SMediaImage::FInternalReferenceCollector");
		}

	private:
		SMediaImage* Object;
	};
	friend FInternalReferenceCollector;

	/** Collector to keep the UObject alive. */
	FInternalReferenceCollector Collector;

	/** The material that wraps the video texture for display in an SImage. */
	UMaterial* Material;

	/** The Slate brush that renders the material. */
	TSharedPtr<FSlateBrush> MaterialBrush;

	/** The video texture sampler in the wrapper material. */
	UMaterialExpressionTextureSample* TextureSampler;

	/** Brush image size attribute. */
	TAttribute<FVector2D> BrushImageSize;
};

