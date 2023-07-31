// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Editor/UnrealEdTypes.h"
#include "GenericPlatform/ICursor.h"
#include "HitProxies.h"
#include "Math/Axis.h"
#include "Math/Color.h"
#include "Math/Matrix.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Math/Vector4.h"
#include "SceneView.h"
#include "UObject/GCObject.h"

class FPrimitiveDrawInterface;
class FReferenceCollector;
class UCustomizableObjectInstance;
class UMaterialInstanceDynamic;
class UMaterialInterface;


class FCustomizableObjectWidget : public FGCObject
{
public:
	enum EWidgetMode
	{
		WM_None			= -1,
		WM_Translate,
		WM_Rotate,
		WM_Scale,
		WM_TranslateRotateZ,
		WM_Max,
	};

	CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectWidget( class UCustomizableObjectInstance* InInstance, int InParamIndex );

	CUSTOMIZABLEOBJECTEDITOR_API void Render( const FSceneView* View ,FPrimitiveDrawInterface* PDI );

	/**
	 * Draws an arrow head line for a specific axis.
	 * @param	bCubeHead		[opt] If true, render a cube at the axis tips.  If false (the default), render a cone.
	 */
	void Render_Axis( const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, FMatrix& InMatrix, UMaterialInterface* InMaterial, FVector2D& OutAxisEnd, float InScale, bool bDrawWidget, bool bCubeHead=false );

	/**
	 * Draws a cube
	 */
	void Render_Cube( FPrimitiveDrawInterface* PDI, const FMatrix& InMatrix, const UMaterialInterface* InMaterial, float InScale );
	void Render_Quad( FPrimitiveDrawInterface* PDI, const FMatrix& InMatrix, const UMaterialInterface* InMaterial, const FColor* InColor, FVector2D InScale );

	/**
	 * Draws the translation widget.
	 */
	void Render_Translate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget );

	/**
	 * Draws the rotation widget.
	 */
	void Render_Rotate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget );

	/**
	 * Draws the scaling widget.
	 */
	void Render_Scale( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget );

	/**
	* Draws the Translate & Rotate Z widget.
	*/
	void Render_TranslateRotateZ( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget );

	/**
	 * Converts mouse movement on the screen to widget axis movement/rotation.
	 */
	//void ConvertMouseMovementToAxisMovement( FViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale );

	/**
	 * Absolute Translation conversion from mouse movement on the screen to widget axis movement/rotation.
	 */
	//void AbsoluteTranslationConvertMouseMovementToAxisMovement(FSceneView* InView, FViewportClient* InViewportClient, const FVector& InLocation, const FVector2D& InMousePosition, FVector& OutDrag, FRotator& OutRotation, FVector& OutScale );

	/** 
	 * Grab the initial offset again first time input is captured
	 */
	void ResetInitialTranslationOffset (void)
	{
		bAbsoluteTranslationInitialOffsetCached = false;
	}

	/** Only some modes support Absolute Translation Movement.  Check current mode */
	static bool AllowsAbsoluteTranslationMovement(void);

	/**
	 * Sets the axis currently being moused over.  Typically called by FMouseDeltaTracker or FEditorLevelViewportClient.
	 *
	 * @param	InCurrentAxis	The new axis value.
	 */
	void SetCurrentAxis(EAxisList::Type InCurrentAxis)
	{
		CurrentAxis = InCurrentAxis;
	}

	/**
	 * @return	The axis currently being moused over.
	 */
	EAxisList::Type GetCurrentAxis() const
	{
		return CurrentAxis;
	}

	FVector2D GetOrigin() const
	{
		return Origin;
	}

	/**
	 * Returns whether we are actively dragging
	 */
	bool IsDragging(void) const
	{
		return bDragging;
	}

	/**
	 * Sets if we are currently engaging the widget in dragging
	 */
	void SetDragging (const bool InDragging) 
	{ 
		bDragging = InDragging;
	}

	/**
	 * Sets if we are currently engaging the widget in dragging
	 */
	void SetSnapEnabled (const bool InSnapEnabled) 
	{ 
		bSnapEnabled = InSnapEnabled;
	}

	// FSerializableObject interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FCustomizableObjectWidget");
	}

	// Own interface
	CUSTOMIZABLEOBJECTEDITOR_API void SetWidgetMode( EWidgetMode Mode );
	CUSTOMIZABLEOBJECTEDITOR_API void SetTransform( FMatrix Transform );
	CUSTOMIZABLEOBJECTEDITOR_API void SetScale( FVector2D Scale );

	/**
	 * An extra matrix to apply to the widget before drawing it (allows for local/custom coordinate systems).
	 */
	FMatrix CustomCoordSystem;
	FVector2D ProjectorScale;
	
	/** */
	EWidgetMode WidgetMode;

	/** The axis currently being moused over */
	EAxisList::Type CurrentAxis;

	/** */
	UCustomizableObjectInstance* Instance;
	int ParamIndex;

private:


	struct FAbsoluteMovementParams
	{
		/** The normal of the plane to project onto */
		FVector PlaneNormal;
		/** A vector that represent any displacement we want to mute (remove an axis if we're doing axis movement)*/
		FVector NormalToRemove;
		/** The current position of the widget */
		FVector Position;

		//Coordinate System Axes
		FVector XAxis;
		FVector YAxis;
		FVector ZAxis;

		//true if camera movement is locked to the object
		float bMovementLockedToCamera;

		//Direction in world space to the current mouse location
		FVector PixelDir;
		//Direction in world space of the middle of the camera
		FVector CameraDir;
		FVector EyePos;

		//whether to snap the requested positionto the grid
		bool bPositionSnapping;
	};

	struct FThickArcParams
	{
		FThickArcParams(FPrimitiveDrawInterface* InPDI, const FVector& InPosition, UMaterialInterface* InMaterial, const float InInnerRadius, const float InOuterRadius)
			: Position(InPosition)
			, PDI(InPDI)
			, Material(InMaterial)
			, InnerRadius(InInnerRadius)
			, OuterRadius(InOuterRadius)
		{
		}

		/** The current position of the widget */
		FVector Position;

		//interface for Drawing
		FPrimitiveDrawInterface* PDI;

		//Material to use to render
		UMaterialInterface* Material;

		//Radii
		float InnerRadius;
		float OuterRadius;
	};

	/**
	 * Returns the Delta from the current position that the absolute movement system wants the object to be at
	 * @param InParams - Structure containing all the information needed for absolute movement
	 * @return - The requested delta from the current position
	 */
	FVector GetAbsoluteTranslationDelta (const FAbsoluteMovementParams& InParams);
	/**
	 * Returns the offset from the initial selection point
	 */
	FVector GetAbsoluteTranslationInitialOffset(const FVector& InNewPosition, const FVector& InCurrentPosition);

	/**
	 * Returns true if we're in Local Space editing mode or editing BSP (which uses the World axes anyway
	 */
	bool IsRotationLocalSpace (void) const;

	/**
	 * Returns the "word" representation of how far we have just rotated
	 */
	float GetDeltaRotation (void) const;

	/**
	 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
	 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
	 * @param View - Information about the scene/camera/etc
	 * @param PDI - Drawing interface
	 * @param InAxis - Enumeration of axis to rotate about
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InDirectionToWidget - Direction from camera to the widget
	 * @param InColor - The color associated with the axis of rotation
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void DrawRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FVector& InDirectionToWidget, const FColor& InColor, const float InScale);

	/**
	 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
	 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
	 * @param View - Information about the scene/camera/etc
	 * @param PDI - Drawing interface
	 * @param InAxis - Enumeration of axis to rotate about
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InStartAngle - The starting angle about (Axis0^Axis1) to render the arc
	 * @param InEndAngle - The ending angle about (Axis0^Axis1) to render the arc
	 * @param InColor - The color associated with the axis of rotation
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void DrawPartialRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const float InScale, const FVector& InDirectionToWidget);

	/**
	 * Renders a portion of an arc for the rotation widget
	 * @param InParams - Material, Radii, etc
	 * @param InStartAxis - Start of the arc
	 * @param InEndAxis - End of the arc
	 * @param InColor - Color to use for the arc
	 */
	void DrawThickArc (const FThickArcParams& InParams, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const FVector& InDirectionToWidget, bool bIsOrtho );

	/**
	 * Draws protractor like ticks where the rotation widget would snap too.
	 * Also, used to draw the wider axis tick marks
	 * @param PDI - Drawing interface
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
	 * @param InColor - The color to use for line/poly drawing
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 * @param InWidthPercent - The percent of the distance between the outer ring and inner ring to use for tangential thickness
	 * @param InPercentSize - The percent of the distance between the outer ring and inner ring to use for radial distance
	 */
	void DrawSnapMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FColor& InColor, const float InScale, const float InWidthPercent=0.0f, const float InPercentSize = 1.0f);

	/**
	 * Draw Start/Stop Marker to show delta rotations along the arc of rotation
	 * @param PDI - Drawing interface
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
	 * @param InColor - The color to use for line/poly drawing
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void DrawStartStopMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InAngle, const FColor& InColor, const float InScale);

	/**
	 * Gets the axis to use when converting mouse movement, accounting for Ortho views.
	 *
	 * @param InDiff Difference vector to determine dominant axis.
	 * @param ViewportType Type of viewport for ortho checks.
	 *
	 * @return Index of the dominant axis.
	 */
	uint32 GetDominantAxisIndex( const FVector& InDiff, ELevelViewportType ViewportType ) const;

	/** Locations of the various points on the widget */
	FVector2D Origin, XAxisEnd, YAxisEnd, ZAxisEnd;

	enum
	{
		AXIS_ARROW_SEGMENTS = 16
	};

	/** Materials and colors to be used when drawing the items for each axis */
	UMaterialInterface* TransparentPlaneMaterialXY;
	UMaterialInterface* GridMaterial;

	UMaterialInstanceDynamic* AxisMaterialX;
	UMaterialInstanceDynamic* AxisMaterialY;
	UMaterialInstanceDynamic* AxisMaterialZ;
	UMaterialInstanceDynamic* CurrentAxisMaterial;
	UMaterialInstanceDynamic* OpaquePlaneMaterialXY;

	FLinearColor AxisColorX, AxisColorY, AxisColorZ;
	FColor PlaneColorXY, ScreenSpaceColor, CurrentColor;


	/** Whether Absolute Translation cache position has been captured */
	bool bAbsoluteTranslationInitialOffsetCached;
	/** The initial offset where the widget was first clicked */
	FVector InitialTranslationOffset;
	/** The initial position of the widget before it was clicked */
	FVector InitialTranslationPosition;
	/** Whether or not the widget is actively dragging */
	bool bDragging;
	/** Whether or not snapping is enabled for all actors */
	bool bSnapEnabled;
};

/**
 * Widget hit proxy.
 */
struct HCustomizableObjectWidgetAxis : public HHitProxy
{
	DECLARE_HIT_PROXY( CUSTOMIZABLEOBJECTEDITOR_API );

	EAxis::Type Axis;

	FCustomizableObjectWidget* Widget;

	HCustomizableObjectWidgetAxis(EAxis::Type InAxis, FCustomizableObjectWidget* InWidget )
		: HHitProxy(HPP_UI)
		, Axis(InAxis)
		, Widget(InWidget)
	{
	}

	virtual EMouseCursor::Type GetMouseCursor()
	{
		return EMouseCursor::CardinalCross;
	}

	virtual bool AlwaysAllowsTranslucentPrimitives() const
	{
		return true;
	}


	/** Utility for calculating drag direction when you click on this widget. */
	void CalcVectors(FSceneView* SceneView, FVector& LocalManDir, FVector& WorldManDir, float& DragDirX, float& DragDirY)
	{
		FMatrix WidgetMatrix = Widget->CustomCoordSystem;

		if(Axis == EAxis::X)
		{
			WorldManDir = WidgetMatrix.GetScaledAxis( EAxis::X );
			LocalManDir = FVector(1,0,0);
		}
		else if(Axis == EAxis::Y)
		{
			WorldManDir = WidgetMatrix.GetScaledAxis( EAxis::Y );
			LocalManDir = FVector(0,1,0);
		}
		else
		{
			WorldManDir = WidgetMatrix.GetScaledAxis( EAxis::Z );
			LocalManDir = FVector(0,0,1);
		}

		FVector WorldDragDir = WorldManDir;

		// Transform world-space drag dir to screen space.
		FVector ScreenDir = SceneView->ViewMatrices.GetViewMatrix().TransformVector(WorldDragDir);
		ScreenDir.Z = 0.0f;

		if( ScreenDir.IsZero() )
		{
			DragDirX = 0.0f;
			DragDirY = 0.0f;
		}
		else
		{
			ScreenDir.Normalize();
			DragDirX = ScreenDir.X;
			DragDirY = ScreenDir.Y;
		}
	}

};
