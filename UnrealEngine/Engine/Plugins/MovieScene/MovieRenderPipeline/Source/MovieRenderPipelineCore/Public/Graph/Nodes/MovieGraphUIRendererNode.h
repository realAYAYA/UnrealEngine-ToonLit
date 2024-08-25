// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/Nodes/MovieGraphWidgetRendererBaseNode.h"
#include "Styling/AppStyle.h"

#include "MovieGraphUIRendererNode.generated.h"

// Forward Declares
class UMovieGraphDefaultRenderer;
class UWidget;
struct FMovieGraphRenderPassLayerData;

/** A node which renders the viewport's UMG widget to a standalone image, or composited on top of a render layer. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphUIRendererNode : public UMovieGraphWidgetRendererBaseNode
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
#endif

public:
	static const FString RendererName;

protected:
	/** A UI pass for a specific render layer. Instances are stored on the UMovieGraphWidgetRendererBaseNode CDO. */
	struct FMovieGraphUIPass final : public FMovieGraphWidgetPass
	{
		virtual void Setup(TWeakObjectPtr<UMovieGraphDefaultRenderer> InRenderer, const FMovieGraphRenderPassLayerData& InLayer) override;
		virtual TSharedPtr<SWidget> GetWidget() override;
		virtual int32 GetCompositingSortOrder() const override;
	};
	
	// UMovieGraphWidgetRendererBaseNode Interface
	virtual TUniquePtr<FMovieGraphWidgetPass> GeneratePass() override;
	// ~UMovieGraphWidgetRendererBaseNode Interface
	
	// UMovieGraphRenderPassNode Interface
	virtual FString GetRendererNameImpl() const override { return RendererName; }
	// ~UMovieGraphRenderPassNode Interface
};