// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FConnectionDrawingPolicy;
class FSlateRect;
class FSlateWindowElementList;
class SGraphNode;
class SGraphPin;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
class UEdGraphSchema;

/** Class that decides which widget type to create for a given data object */
class GRAPHEDITOR_API FNodeFactory
{
public:
	/** Create a widget for the supplied node */
	static TSharedPtr<SGraphNode> CreateNodeWidget(UEdGraphNode* InNode);

	/** Create a widget for the supplied pin */
	static TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* InPin);

	/** Create a K2 schema default widget for the supplied pin */
	static TSharedPtr<SGraphPin> CreateK2PinWidget(UEdGraphPin* InPin);

    /** Create a pin connection factory for the supplied schema */
    static FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);
};


class GRAPHEDITOR_API FGraphNodeFactory
{
public:
	/** Create a widget for the supplied node */
	virtual TSharedPtr<SGraphNode> CreateNodeWidget(UEdGraphNode* InNode);

	/** Create a widget for the supplied pin */
	virtual TSharedPtr<SGraphPin> CreatePinWidget(UEdGraphPin* InPin);

	/** Create a pin connection factory for the supplied schema */
	virtual FConnectionDrawingPolicy* CreateConnectionPolicy(const UEdGraphSchema* Schema, int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);
};