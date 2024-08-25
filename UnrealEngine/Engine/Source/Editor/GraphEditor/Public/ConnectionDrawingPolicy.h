// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditorSettings.h"
#include "GraphSplineOverlapResult.h"
#include "HAL/Platform.h"
#include "Layout/ArrangedWidget.h"
#include "Logging/LogMacros.h"
#include "Math/Color.h"
#include "Math/Vector2D.h"
#include "SGraphPin.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;
class FArrangedWidget;
class FSlateRect;
class FSlateWindowElementList;
class SGraphPin;
class SWidget;
class UGraphEditorSettings;
struct FGeometry;
struct FSlateBrush;
template <class T> class FInterpCurve;

/////////////////////////////////////////////////////

GRAPHEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogConnectionDrawingPolicy, Log, All);

/////////////////////////////////////////////////////
// FGeometryHelper

class GRAPHEDITOR_API FGeometryHelper
{
public:
	static FVector2D VerticalMiddleLeftOf(const FGeometry& SomeGeometry);
	static FVector2D VerticalMiddleRightOf(const FGeometry& SomeGeometry);
	static FVector2D CenterOf(const FGeometry& SomeGeometry);
	static void ConvertToPoints(const FGeometry& Geom, TArray<FVector2D>& Points);

	/** Find the point on line segment from LineStart to LineEnd which is closest to Point */
	static FVector2D FindClosestPointOnLine(const FVector2D& LineStart, const FVector2D& LineEnd, const FVector2D& TestPoint);

	static FVector2D FindClosestPointOnGeom(const FGeometry& Geom, const FVector2D& TestPoint);
};

/////////////////////////////////////////////////////
// FConnectionParameters

struct GRAPHEDITOR_API FConnectionParams
{
	FLinearColor WireColor;
	UEdGraphPin* AssociatedPin1;
	UEdGraphPin* AssociatedPin2;

	float WireThickness;
	bool bDrawBubbles;
	bool bUserFlag1;
	bool bUserFlag2;

	EEdGraphPinDirection StartDirection;
	EEdGraphPinDirection EndDirection;

	FVector2D StartTangent;
	FVector2D EndTangent;

	FConnectionParams()
		: WireColor(FLinearColor::White)
		, AssociatedPin1(nullptr)
		, AssociatedPin2(nullptr)
		, WireThickness(1.5f)
		, bDrawBubbles(false)
		, bUserFlag1(false)
		, bUserFlag2(false)
		, StartDirection(EGPD_Output)
		, EndDirection(EGPD_Input)
		, StartTangent(FVector2D::ZeroVector)
		, EndTangent(FVector2D::ZeroVector)
	{
	}
};

/////////////////////////////////////////////////////
// FConnectionDrawingPolicy

// This class draws the connections for an UEdGraph composed of pins and nodes
class GRAPHEDITOR_API FConnectionDrawingPolicy
{
protected:
	int32 WireLayerID;
	int32 ArrowLayerID;

	const FSlateBrush* ArrowImage;
	const FSlateBrush* MidpointImage;
	const FSlateBrush* BubbleImage;

	const UGraphEditorSettings* Settings;

public:
	FVector2D ArrowRadius;
	FVector2D MidpointRadius;

	FGraphSplineOverlapResult SplineOverlapResult;

	/** Handle for a currently relinked connection. */
	struct FRelinkConnection
	{
		UEdGraphPin* SourcePin;
		UEdGraphPin* TargetPin;
	};

protected:
	float ZoomFactor; 
	float HoverDeemphasisDarkFraction;
	const FSlateRect& ClippingRect;
	FSlateWindowElementList& DrawElementsList;
	TMap< UEdGraphPin*, TSharedPtr<SGraphPin> > PinToPinWidgetMap;
	TSet< FEdGraphPinReference > HoveredPins;
	TMap<TSharedRef<SWidget>, FArrangedWidget>* PinGeometries;
	double LastHoverTimeEvent;
	FVector2D LocalMousePosition;

	/** List of currently relinked connections. */
	TArray<FRelinkConnection> RelinkConnections;

	/** Selected nodes in the graph panel. */
	TArray<UEdGraphNode*> SelectedGraphNodes;
public:
	virtual ~FConnectionDrawingPolicy() {}
	
	FConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements);

	// Update the drawing policy with the set of hovered pins (which can be empty)
	void SetHoveredPins(const TSet< FEdGraphPinReference >& InHoveredPins, const TArray< TSharedPtr<SGraphPin> >& OverridePins, double HoverTime);

	void SetMousePosition(const FVector2D& InMousePos);

	// Update the drawing policy with the marked pin (which may not be valid)
	void SetMarkedPin(TWeakPtr<SGraphPin> InMarkedPin);

	// Set the selected nodes from the graph panel.
	void SetSelectedNodes(const TArray<UEdGraphNode*>& InSelectedNodes) { SelectedGraphNodes = InSelectedNodes; }

	// Set the list of currently relinked connections.
	void SetRelinkConnections(const TArray<FRelinkConnection>& Connections) { RelinkConnections = Connections; }

	static float MakeSplineReparamTable(const FVector2D& P0, const FVector2D& P0Tangent, const FVector2D& P1, const FVector2D& P1Tangent, FInterpCurve<float>& OutReparamTable);

	virtual void DrawSplineWithArrow(const FVector2D& StartPoint, const FVector2D& EndPoint, const FConnectionParams& Params);
	
	virtual void DrawSplineWithArrow(const FGeometry& StartGeom, const FGeometry& EndGeom, const FConnectionParams& Params);

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const;
	virtual void DrawConnection(int32 LayerId, const FVector2D& Start, const FVector2D& End, const FConnectionParams& Params);
	virtual void DrawPreviewConnector(const FGeometry& PinGeometry, const FVector2D& StartPoint, const FVector2D& EndPoint, UEdGraphPin* Pin);

	// Give specific editor modes a chance to highlight this connection or darken non-interesting connections
	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params);

	virtual void Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes);

	virtual void DetermineLinkGeometry(
		FArrangedChildren& ArrangedNodes, 
		TSharedRef<SWidget>& OutputPinWidget,
		UEdGraphPin* OutputPin,
		UEdGraphPin* InputPin,
		/*out*/ FArrangedWidget*& StartWidgetGeometry,
		/*out*/ FArrangedWidget*& EndWidgetGeometry
		);

	// Choose whether we want to cache the pins draw state to avoid resetting it for every tick 
	virtual bool UseDrawStateCaching() const { return false; }
	
	virtual void SetIncompatiblePinDrawState(const TSharedPtr<SGraphPin>& StartPin, const TSet< TSharedRef<SWidget> >& VisiblePins);
	virtual void ResetIncompatiblePinDrawState(const TSet< TSharedRef<SWidget> >& VisiblePins);

	virtual void ApplyHoverDeemphasis(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ float& Thickness, /*inout*/ FLinearColor& WireColor);

	virtual bool IsConnectionCulled( const FArrangedWidget& StartLink, const FArrangedWidget& EndLink ) const;

	virtual TSharedPtr<IToolTip> GetConnectionToolTip(const SGraphPanel& GraphPanel, const FGraphSplineOverlapResult& OverlapData) const;
	
protected:
	// Helper function used by Draw(). Called before DrawPinGeometries to populate PinToPinWidgetMap
	virtual void BuildPinToPinWidgetMap(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries);

	// Helper function used by Draw(). Iterates over the pin geometries, drawing connections between them
	virtual void DrawPinGeometries(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries, FArrangedChildren& ArrangedNodes);
};
