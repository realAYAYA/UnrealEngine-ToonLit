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
struct FSlateDebuggingElementTypeAddedEventArgs;
enum class EElementType : uint8;

class FVisualEntry
{
public:
	FVector2f TopLeft;
	FVector2f TopRight;
	FVector2f BottomLeft;
	FVector2f BottomRight;

	int32 LayerId;
	int32 ClippingIndex;
	int32 ElementIndex;
	EElementType ElementType;
	bool bFromCache;
	TWeakPtr<const SWidget> Widget;

	FVisualEntry(const TWeakPtr<const SWidget>& Widget, int32 InElementIndex, EElementType InElementType);
	FVisualEntry(const TSharedRef<const SWidget>& Widget, const FSlateDrawElement& InElement);

	void Resolve(const FSlateWindowElementList& ElementList);

	bool IsPointInside(const FVector2f& Point) const;
};

class FVisualTreeSnapshot : public TSharedFromThis<FVisualTreeSnapshot>
{
public:
	TSharedPtr<const SWidget> Pick(FVector2f Point);
	
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
	void ElementTypeAdded(const FSlateDebuggingElementTypeAddedEventArgs& ElementTypeAddedArgs);

	void OnWindowBeingDestroyed(const SWindow& WindowBeingDestoyed);
private:
	TMap<const SWindow*, TSharedPtr<FVisualTreeSnapshot>> VisualTrees;
	bool bIsEnabled;
	int32 WindowIsInvalidationRootCounter;
	int32 WidgetIsInvalidationRootCounter;
	int32 WidgetIsInvisibleToWidgetReflectorCounter;
};