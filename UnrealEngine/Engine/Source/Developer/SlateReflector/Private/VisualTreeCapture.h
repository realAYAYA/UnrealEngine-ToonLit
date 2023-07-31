// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWindow;
class SWidget;
class FSlateDrawElement;
class FSlateClippingState;
class FSlateWindowElementList;
class FPaintArgs;
struct FGeometry;
class FSlateRect;
class FSlateInvalidationRoot;

class FVisualEntry
{
public:
	FVector2D TopLeft;
	FVector2D TopRight;
	FVector2D BottomLeft;
	FVector2D BottomRight;

	int32 LayerId;
	int32 ClippingIndex;
	int32 ElementIndex;
	bool bFromCache;
	TWeakPtr<const SWidget> Widget;

	FVisualEntry(const TWeakPtr<const SWidget>& Widget, int32 InElementIndex);
	FVisualEntry(const TSharedRef<const SWidget>& Widget, const FSlateDrawElement& InElement);

	void Resolve(const FSlateWindowElementList& ElementList);

	bool IsPointInside(const FVector2D& Point) const;
};

class FVisualTreeSnapshot : public TSharedFromThis<FVisualTreeSnapshot>
{
public:
	TSharedPtr<const SWidget> Pick(FVector2D Point);
	
public:
	TArray<FVisualEntry> Entries;
	TArray<FSlateClippingState> ClippingStates;
	TArray<FSlateClippingState> CachedClippingStates;
	TArray<TWeakPtr<const SWidget>> WidgetStack;
};

class FVisualTreeCapture
{
public:
	FVisualTreeCapture();
	~FVisualTreeCapture();

	/** Enables visual tree capture */
	void Enable();

	/** Disables visual tree capture */
	void Disable();

	/** Resets the visual tree capture to a pre-capture state and destroys the cached visual tree captured last. */
	void Reset();

	TSharedPtr<FVisualTreeSnapshot> GetVisualTreeForWindow(SWindow* InWindow);
	
private:

	void AddInvalidationRootCachedEntries(TSharedRef<FVisualTreeSnapshot> Tree, const FSlateInvalidationRoot* Entries);


	void BeginWindow(const FSlateWindowElementList& ElementList);
	void EndWindow(const FSlateWindowElementList& ElementList);

	void BeginWidgetPaint(const SWidget* Widget, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, const FSlateWindowElementList& ElementList, int32 LayerId);

	/**  */
	void EndWidgetPaint(const SWidget* Widget, const FSlateWindowElementList& ElementList, int32 LayerId);

	/**  */
	void ElementAdded(const FSlateWindowElementList& ElementList, int32 InElementIndex);

	void OnWindowBeingDestroyed(const SWindow& WindowBeingDestoyed);
private:
	TMap<const SWindow*, TSharedPtr<FVisualTreeSnapshot>> VisualTrees;
	bool bIsEnabled;
	int32 WindowIsInvalidationRootCounter;
	int32 WidgetIsInvalidationRootCounter;
	int32 WidgetIsInvisibleToWidgetReflectorCounter;
};