// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Layout/Geometry.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "FastUpdate/SlateInvalidationRoot.h"
#include "Input/HittestGrid.h"

class FPaintArgs;
class FSlateRenderDataHandle;
class FSlateWindowElementList;
class SWindow;

class SInvalidationPanel : public SCompoundWidget, public FSlateInvalidationRoot
{
public:
	SLATE_BEGIN_ARGS( SInvalidationPanel )
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_DEFAULT_SLOT(FArguments, Content)
#if !UE_BUILD_SHIPPING
		SLATE_ARGUMENT(FString, DebugName)
#endif
	SLATE_END_ARGS()

	SLATE_API SInvalidationPanel();
	SLATE_API ~SInvalidationPanel();

#if WITH_SLATE_DEBUGGING
	static SLATE_API bool AreInvalidationPanelsEnabled();
	static SLATE_API void EnableInvalidationPanels(bool bEnable);
#endif
	SLATE_API void Construct( const FArguments& InArgs );

	SLATE_API bool GetCanCache() const;

	SLATE_API void SetCanCache(bool InCanCache);

	//~ SWidget overrides
	SLATE_API virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	SLATE_API virtual FChildren* GetChildren() override;
#if WITH_SLATE_DEBUGGING
	SLATE_API virtual FChildren* Debug_GetChildrenForReflector() override;
#endif
	//~ End SWidget

	SLATE_API void SetContent(const TSharedRef< SWidget >& InContent);

protected:
	SLATE_API virtual bool CustomPrepass(float LayoutScaleMultiplier) override;
	SLATE_API virtual bool Advanced_IsInvalidationRoot() const override;
	SLATE_API virtual const FSlateInvalidationRoot* Advanced_AsInvalidationRoot() const override;
	SLATE_API virtual TSharedRef<SWidget> GetRootWidget() override;
	SLATE_API virtual int32 PaintSlowPath(const FSlateInvalidationContext& Context) override;

private:
	void OnGlobalInvalidationToggled(bool bGlobalInvalidationEnabled);
	bool UpdateCachePrequisites(FSlateWindowElementList& OutDrawElements, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, int32 LayerId, const FWidgetStyle& InWidgetStyle) const;

private:
	mutable TSharedRef<FHittestGrid> HittestGrid;

	mutable TOptional<FSlateClippingState> LastClippingState;
	mutable FGeometry LastAllottedGeometry;
	mutable FVector2D LastClipRectSize;
	mutable int32 LastIncomingLayerId;
	mutable FLinearColor LastIncomingColorAndOpacity;

	bool bCanCache;

	mutable bool bPaintedSinceLastPrepass;
#if SLATE_VERBOSE_NAMED_EVENTS
	FString DebugName;
	FString DebugTickName;
	FString DebugPaintName;
#endif
	mutable bool bWasCachable;
};
