// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectWidget.h"

#include "DynamicMeshBuilder.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/IntRect.h"
#include "Math/NumericLimits.h"
#include "Math/Plane.h"
#include "Math/RotationMatrix.h"
#include "Math/Rotator.h"
#include "Math/ScaleMatrix.h"
#include "Math/TranslationMatrix.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "SceneManagement.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealWidget.h"

class FMaterialRenderProxy;
class UMaterialInterface;


IMPLEMENT_HIT_PROXY(HCustomizableObjectWidgetAxis,HHitProxy);

static const float AXIS_ARROW_RADIUS = 2.5f;
static const float AXIS_CIRCLE_RADIUS	= 48.0f;
static const float TRANSLATE_ROTATE_AXIS_CIRCLE_RADIUS	= 20.0f;
static const float INNER_AXIS_CIRCLE_RADIUS = 48.0f;
static const float OUTER_AXIS_CIRCLE_RADIUS = 56.0f;
static const float ROTATION_TEXT_RADIUS = 75.0f;
static const int32 AXIS_CIRCLE_SIDES		= 24;

namespace WidgetDefs
{
	static const int32 GScreenAlignedSphereSides = 16;
	static const float GScreenAlignedSphereRadius = 3.0f;
}


FCustomizableObjectWidget::FCustomizableObjectWidget(UCustomizableObjectInstance* InInstance, int InParamIndex)
{
	Instance = InInstance;
	ParamIndex = InParamIndex;
	WidgetMode = WM_Translate;

	// Compute and store sample vertices for drawing the axis arrow heads
	AxisColorX = FColor(255,0,0);
	AxisColorY = FColor(0,255,0);
	AxisColorZ = FColor(0,0,255);
	PlaneColorXY = FColor(255,255,0);
	ScreenSpaceColor  = FColor(196, 196, 196);
	CurrentColor = FColor(255,255,0);

	UMaterial* AxisMaterialBase = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/GizmoMaterial.GizmoMaterial") );

	AxisMaterialX = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialX->SetVectorParameterValue( "GizmoColor", AxisColorX );

	AxisMaterialY = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialY->SetVectorParameterValue( "GizmoColor", AxisColorY );

	AxisMaterialZ = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	AxisMaterialZ->SetVectorParameterValue( "GizmoColor", AxisColorZ );

	CurrentAxisMaterial = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	CurrentAxisMaterial->SetVectorParameterValue("GizmoColor", CurrentColor );

	OpaquePlaneMaterialXY = UMaterialInstanceDynamic::Create( AxisMaterialBase, NULL );
	OpaquePlaneMaterialXY->SetVectorParameterValue( "GizmoColor", FLinearColor::White );

	TransparentPlaneMaterialXY = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/WidgetVertexColorMaterial.WidgetVertexColorMaterial"),NULL,LOAD_None,NULL );
	
	GridMaterial = (UMaterial*)StaticLoadObject( UMaterial::StaticClass(),NULL,TEXT("/Engine/EditorMaterials/WidgetGridVertexColorMaterial_Ma.WidgetGridVertexColorMaterial_Ma"),NULL,LOAD_None,NULL );
	if (!GridMaterial)
	{
		GridMaterial = TransparentPlaneMaterialXY;
	}

	CurrentAxis = EAxisList::None;

	CustomCoordSystem = FMatrix::Identity;

	bAbsoluteTranslationInitialOffsetCached = false;
	InitialTranslationOffset = FVector::ZeroVector;
	InitialTranslationPosition = FVector(0, 0, 0);

	bDragging = false;
	bSnapEnabled = false;
}


void FCustomizableObjectWidget::SetWidgetMode( EWidgetMode Mode )
{
	WidgetMode = Mode;
}


void FCustomizableObjectWidget::SetTransform( FMatrix Transform )
{
	CustomCoordSystem = Transform;
}


void FCustomizableObjectWidget::SetScale( FVector2D InScale )
{
	ProjectorScale = InScale;
}


void FCustomizableObjectWidget::Render( const FSceneView* View,FPrimitiveDrawInterface* PDI )
{
	FVector Loc = FVector(0,0,0);
	if(!View->ScreenToPixel(View->WorldToScreen(Loc),Origin))
	{
		Origin.X = Origin.Y = 0;
	}

	// Render projected image quad
	{
		FColor Color(200,200,100,50);
		Render_Quad(PDI, CustomCoordSystem, TransparentPlaneMaterialXY, &Color, ProjectorScale*0.5f );
	}


	bool bDrawWidget = true;

	switch( WidgetMode )
	{
		case WM_Translate:
			Render_Translate( View, PDI, Loc, bDrawWidget );
			break;

		case WM_Rotate:
			Render_Rotate( View, PDI, Loc, bDrawWidget );
			break;

		case WM_Scale:
			Render_Scale( View, PDI, Loc, bDrawWidget );
			break;

		case WM_TranslateRotateZ:
			Render_TranslateRotateZ( View, PDI, Loc, bDrawWidget );
			break;

		default:
			break;
	}
};

/**
 * Draws an arrow head line for a specific axis.
 */
void FCustomizableObjectWidget::Render_Axis( const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, FMatrix& InMatrix, UMaterialInterface* InMaterial, FVector2D& OutAxisEnd, float InScale, bool bDrawWidget, bool bCubeHead )
{
	FMatrix AxisRotation = FMatrix::Identity;
	if( InAxis == EAxisList::Y )
	{
		AxisRotation = FRotationMatrix( FRotator(0,90.f,0) );
	}
	else if( InAxis == EAxisList::Z )
	{
		AxisRotation = FRotationMatrix( FRotator(90.f,0,0) );
	}

	FMatrix ArrowToWorld = AxisRotation * InMatrix;

	FScaleMatrix Scale(InScale);
	ArrowToWorld = Scale * ArrowToWorld;

	if( bDrawWidget )
	{
		const bool bDisabled = false;
		PDI->SetHitProxy( new HWidgetAxis( InAxis, bDisabled) );

		const float HalfHeight = 35/2.0f;
		const float CylinderRadius = 1.2f;
		const FVector Offset( 0,0,HalfHeight );

		const FMaterialRenderProxy* Proxy = Helper_GetMaterialProxy(InMaterial);

		switch( InAxis )
		{
			case EAxisList::X:
			{
				DrawCylinder( PDI, Scale * FRotationMatrix( FRotator(-90,0.f,0) ) * InMatrix, Offset, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), CylinderRadius, HalfHeight, 16, Proxy, SDPG_Foreground );
				break;
			}
			case EAxisList::Y:
			{
				DrawCylinder( PDI, Scale * FRotationMatrix( FRotator(0,0,90) ) * InMatrix, Offset, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), CylinderRadius, HalfHeight, 16, Proxy, SDPG_Foreground );
				break;
			}
			case EAxisList::Z:
			{
				DrawCylinder( PDI, Scale * InMatrix, Offset, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), CylinderRadius, HalfHeight, 16, Proxy, SDPG_Foreground );
				break;
			}
		}

		if ( bCubeHead )
		{
			const FVector RootPos(38,0,0);

			Render_Cube( PDI, FTranslationMatrix(RootPos)*ArrowToWorld, InMaterial, 4.0f );
		}
		else
		{
			const FVector RootPos(47,0,0);

			const float Angle = FMath::DegreesToRadians( PI*5 );
			DrawCone( PDI, FScaleMatrix(-13)*FTranslationMatrix(RootPos)*ArrowToWorld, Angle, Angle, 32, false, FColor::White, Proxy, SDPG_Foreground );
		}
	
		PDI->SetHitProxy( NULL );
	}

	if(!View->ScreenToPixel(View->WorldToScreen(ArrowToWorld.TransformPosition(FVector(64,0,0))),OutAxisEnd))
	{
		OutAxisEnd.X = OutAxisEnd.Y = 0;
	}
}

void FCustomizableObjectWidget::Render_Cube( FPrimitiveDrawInterface* PDI, const FMatrix& InMatrix, const UMaterialInterface* InMaterial, float InScale )
{
	const FMatrix CubeToWorld = FScaleMatrix(FVector(InScale,InScale,InScale)) * InMatrix;
	DrawBox( PDI, CubeToWorld, FVector(1,1,1), Helper_GetMaterialProxy(InMaterial), SDPG_Foreground );
}


void FCustomizableObjectWidget::Render_Quad( FPrimitiveDrawInterface* PDI, const FMatrix& InMatrix, const UMaterialInterface* InMaterial, const FColor* InColor, FVector2D InScale )
{
	const FMatrix CubeToWorld = FScaleMatrix(FVector(InScale.X,InScale.Y,1)) * InMatrix;
	FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

	// Compute cube vertices.
	FVector3f CubeVerts[4];
	CubeVerts[0] = FVector3f( 1, -1, 0 );
	CubeVerts[1] = FVector3f( 1, 1, 0 );
	CubeVerts[2] = FVector3f( -1, -1, 0 );
	CubeVerts[3] = FVector3f( -1, 1, 0 );

	for(uint32 i = 0; i < 4; ++i)
	{
		MeshBuilder.AddVertex( CubeVerts[i], FVector2f::ZeroVector, FVector3f::ZeroVector, FVector3f::ZeroVector, FVector3f::ZeroVector, *InColor );
	}

	MeshBuilder.AddTriangle(0,1,2);
	MeshBuilder.AddTriangle(2,1,3);

	// Draw the arrow mesh.
	MeshBuilder.Draw(PDI, CubeToWorld, Helper_GetMaterialProxy(InMaterial), SDPG_Foreground, 0.f);

#undef CUBE_FACE
}


/**
 * Draws the translation widget.
 */
void FCustomizableObjectWidget::Render_Translate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget )
{
	float Scale = View->WorldToScreen( InLocation ).W * ( 4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0] );

	// Figure out axis colors
	const FLinearColor& XColor = ( CurrentAxis&EAxisList::X ? (FLinearColor)CurrentColor : AxisColorX );
	const FLinearColor& YColor = ( CurrentAxis&EAxisList::Y ? (FLinearColor)CurrentColor : AxisColorY );
	const FLinearColor& ZColor = ( CurrentAxis&EAxisList::Z ? (FLinearColor)CurrentColor : AxisColorZ );
	FColor CurrentScreenColor = ( CurrentAxis & EAxisList::Screen ? CurrentColor : ScreenSpaceColor );

	// Figure out axis materials

	UMaterialInstanceDynamic* XMaterial = ( CurrentAxis&EAxisList::X ? CurrentAxisMaterial : AxisMaterialX );
	UMaterialInstanceDynamic* YMaterial = ( CurrentAxis&EAxisList::Y ? CurrentAxisMaterial : AxisMaterialY );
	UMaterialInstanceDynamic* ZMaterial = ( CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ );
	UMaterialInstanceDynamic* XYZMaterial = ( CurrentAxis&EAxisList::Screen) ? CurrentAxisMaterial : OpaquePlaneMaterialXY;

	
	// Figure out axis matrices

	FMatrix WidgetMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );

	const bool bIsPerspective = View->IsPerspectiveProjection();
	const bool bIsOrthoXY = !bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[2][2]) > 0.0f;
	const bool bIsOrthoXZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[1][2]) > 0.0f;
	const bool bIsOrthoYZ = !bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[0][2]) > 0.0f;

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	const bool bIsLocalSpace = true;

	const EAxisList::Type DrawAxis = EAxisList::All;

	const bool bDisabled = false;

	// Draw the axis lines with arrow heads
	if( bIsPerspective || bIsLocalSpace || !bIsOrthoYZ )
	{
		Render_Axis( View, PDI, EAxisList::X, WidgetMatrix, XMaterial, XAxisEnd, Scale, bDrawWidget );
	}

	if(bIsPerspective || bIsLocalSpace || !bIsOrthoXZ )
	{
		Render_Axis( View, PDI, EAxisList::Y, WidgetMatrix, YMaterial, YAxisEnd, Scale, bDrawWidget );
	}

	if( bIsPerspective || bIsLocalSpace || !bIsOrthoXY )
	{
		Render_Axis( View, PDI, EAxisList::Z, WidgetMatrix, ZMaterial, ZAxisEnd, Scale, bDrawWidget );
	}

	
	// Draw the grabbers
	if( bDrawWidget )
	{
		/*
		FVector CornerPos = FVector(7,0,7)*Scale;
		FVector AxisSize = FVector(12,1.2,12)*Scale;
		float CornerLength = 1.2f*Scale;

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[2][1] == 0.f )
		{

			if( (DrawAxis&EAxisList::XY) == EAxisList::XY )							// Top
			{
				UMaterialInstanceDynamic* XMaterial = ( (CurrentAxis&EAxisList::XY) == EAxisList::XY ? CurrentAxisMaterial : AxisMaterialX );
				UMaterialInstanceDynamic* YMaterial = ( (CurrentAxis&EAxisList::XY) == EAxisList::XY? CurrentAxisMaterial : AxisMaterialY );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::XY, bDisabled) );
				{
					DrawDualAxis( PDI, FTranslationMatrix( CornerPos )*FRotationMatrix( FRotator( 0, 0, 90 ) ) * WidgetMatrix, AxisSize, CornerLength, XMaterial->GetRenderProxy(false), YMaterial->GetRenderProxy(false) );

					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(16,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(16,16,0) * Scale), XColor, SDPG_Foreground );
					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(16,16,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,16,0) * Scale), YColor, SDPG_Foreground );
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[1][2] == -1.f )		// Front
		{
			if( (DrawAxis&EAxisList::XZ) == EAxisList::XZ ) 
			{
				UMaterialInstanceDynamic* XMaterial = ( (CurrentAxis&EAxisList::XZ) == EAxisList::XZ ? CurrentAxisMaterial : AxisMaterialX );
				UMaterialInstanceDynamic* ZMaterial = ( (CurrentAxis&EAxisList::XZ) == EAxisList::XZ ? CurrentAxisMaterial : AxisMaterialZ );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::XZ, bDisabled) );
				{

					DrawDualAxis( PDI, FTranslationMatrix( CornerPos )*WidgetMatrix, AxisSize, CornerLength, XMaterial->GetRenderProxy(false), ZMaterial->GetRenderProxy(false) );

					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(16,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(16,0,16) * Scale), XColor, SDPG_Foreground );
					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(16,0,16) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,16) * Scale), ZColor, SDPG_Foreground );
				}
				PDI->SetHitProxy( NULL );
			}
		}

		if( bIsPerspective || bIsLocalSpace || View->ViewMatrices.ViewMatrix.M[1][0] == 1.f )		// Side
		{
			if( (DrawAxis&EAxisList::YZ) == EAxisList::YZ ) 
			{
				UMaterialInstanceDynamic* YMaterial = ( (CurrentAxis&EAxisList::YZ) == EAxisList::YZ ? CurrentAxisMaterial : AxisMaterialY );
				UMaterialInstanceDynamic* ZMaterial = ( (CurrentAxis&EAxisList::YZ) == EAxisList::YZ ? CurrentAxisMaterial : AxisMaterialZ );

				PDI->SetHitProxy( new HWidgetAxis(EAxisList::YZ, bDisabled) );
				{
					DrawDualAxis( PDI, FTranslationMatrix( CornerPos ) *FRotationMatrix( FRotator( 0, 90, 0 ) ) * WidgetMatrix, AxisSize, CornerLength, YMaterial->GetRenderProxy(false), ZMaterial->GetRenderProxy( false ) );

					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,16,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,16,16) * Scale), YColor, SDPG_Foreground );
					//PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,16,16) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,16) * Scale), ZColor, SDPG_Foreground );
				}
				PDI->SetHitProxy( NULL );
			}
		}
		*/
	}

	// Draw screen-space movement handle (circle)
	if( bDrawWidget && ( DrawAxis & EAxisList::Screen ) && bIsPerspective )
	{
		PDI->SetHitProxy( new HWidgetAxis(EAxisList::Screen, bDisabled) );
		const FVector CameraXAxis = View->ViewMatrices.GetViewMatrix().GetColumn(0);
		const FVector CameraYAxis = View->ViewMatrices.GetViewMatrix().GetColumn(1);
		const FVector CameraZAxis = View->ViewMatrices.GetViewMatrix().GetColumn(2);

		DrawSphere( PDI, InLocation, FRotator::ZeroRotator, FVector( 4.0f * Scale ), 10, 5, Helper_GetMaterialProxy(XYZMaterial), SDPG_Foreground );

		PDI->SetHitProxy( NULL );
	}

}

/**
 * Draws the rotation widget.
 */
void FCustomizableObjectWidget::Render_Rotate( const FSceneView* View,FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget )
{
	float Scale = View->WorldToScreen( InLocation ).W * ( 4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0] );

	//get the axes 
	FVector XAxis = CustomCoordSystem.TransformVector(FVector(-1, 0, 0));
	FVector YAxis = CustomCoordSystem.TransformVector(FVector(0, -1, 0));
	FVector ZAxis = CustomCoordSystem.TransformVector(FVector(0, 0, 1));

	EAxisList::Type DrawAxis = EAxisList::All;

	FMatrix XMatrix = FRotationMatrix( FRotator(0,90.f,0) ) * FTranslationMatrix( InLocation );

	FVector DirectionToWidget = View->IsPerspectiveProjection() ? (InLocation - View->ViewMatrices.GetViewOrigin()) : -View->GetViewDirection();
	DirectionToWidget.Normalize();

	// Draw a circle for each axis
	if (bDrawWidget || bDragging)
	{
		DrawRotationArc(View, PDI, EAxisList::X, InLocation, YAxis, ZAxis, DirectionToWidget, AxisColorX.ToFColor(true), Scale);
		DrawRotationArc(View, PDI, EAxisList::Y, InLocation, ZAxis, XAxis, DirectionToWidget, AxisColorY.ToFColor(true), Scale);
		DrawRotationArc(View, PDI, EAxisList::Z, InLocation, XAxis, YAxis, DirectionToWidget, AxisColorZ.ToFColor(true), Scale);
	}

	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(96,0,0) ) ), XAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(0,96,0) ) ), YAxisEnd);
	// Update Axis by projecting the axis vector to screenspace.
	View->ScreenToPixel(View->WorldToScreen( XMatrix.TransformPosition( FVector(0,0,96) ) ), ZAxisEnd);
}

/**
 * Draws the scaling widget.
 */
void FCustomizableObjectWidget::Render_Scale( const FSceneView* View,FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget )
{
	const float Scale(View->WorldToScreen( InLocation ).W * ( 4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0] ));

	// Figure out axis colors
	const FLinearColor& XColor = ( CurrentAxis&EAxisList::X ? (FLinearColor)CurrentColor : AxisColorX );
	const FLinearColor& YColor = ( CurrentAxis&EAxisList::Y ? (FLinearColor)CurrentColor : AxisColorY );
	const FLinearColor& ZColor = ( CurrentAxis&EAxisList::Z ? (FLinearColor)CurrentColor : AxisColorZ );
	FColor CurrentScreenColor = ( CurrentAxis & EAxisList::Screen ? CurrentColor : ScreenSpaceColor );

	// Figure out axis materials

	UMaterialInstanceDynamic* XMaterial = ( CurrentAxis&EAxisList::X ? CurrentAxisMaterial : AxisMaterialX );
	UMaterialInstanceDynamic* YMaterial = ( CurrentAxis&EAxisList::Y ? CurrentAxisMaterial : AxisMaterialY );
	UMaterialInstanceDynamic* ZMaterial = ( CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ );
	UMaterialInstanceDynamic* XYZMaterial = ( CurrentAxis&EAxisList::XYZ ? CurrentAxisMaterial : OpaquePlaneMaterialXY );

	// Figure out axis matrices

	FMatrix WidgetMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	// Determine viewport

	const EAxisList::Type DrawAxis = EAxisList::All;
	const bool bIsPerspective = View->IsPerspectiveProjection();
	const bool bIsOrthoXY(!bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[2][2]) > 0.0f);
	const bool bIsOrthoXZ(!bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[1][2]) > 0.0f);
	const bool bIsOrthoYZ(!bIsPerspective && FMath::Abs(View->ViewMatrices.GetViewMatrix().M[0][2]) > 0.0f);

	// Draw the axis lines with cube heads

	if( !bIsOrthoYZ && DrawAxis&EAxisList::X )
	{
		Render_Axis( View, PDI, EAxisList::X, WidgetMatrix, XMaterial, XAxisEnd, Scale, bDrawWidget, true );
	}

	if( !bIsOrthoXZ && DrawAxis&EAxisList::Y )
	{
		Render_Axis( View, PDI, EAxisList::Y, WidgetMatrix, YMaterial, YAxisEnd, Scale, bDrawWidget, true );
	}

	if( !bIsOrthoXY &&  DrawAxis&EAxisList::Z )
	{
		//Render_Axis( View, PDI, EAxisList::Z, WidgetMatrix, ZMaterial, ZAxisEnd, Scale, bDrawWidget, true );
	}

	// Draw grabber handles and center cube
	if ( bDrawWidget )
	{
		const bool bDisabled = false;

		// Grabber handles
		if( !bIsOrthoYZ && !bIsOrthoXZ && ((DrawAxis&(EAxisList::X|EAxisList::Y)) == (EAxisList::X|EAxisList::Y)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XY, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(24,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(12,12,0) * Scale), XColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(12,12,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,24,0) * Scale), YColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( !bIsOrthoYZ && !bIsOrthoXY && ((DrawAxis&(EAxisList::X|EAxisList::Z)) == (EAxisList::X|EAxisList::Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XZ, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(24,0,0) * Scale), WidgetMatrix.TransformPosition(FVector(12,0,12) * Scale), XColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(12,0,12) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,24) * Scale), ZColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		if( !bIsOrthoXY && !bIsOrthoXZ && ((DrawAxis&(EAxisList::Y|EAxisList::Z)) == (EAxisList::Y|EAxisList::Z)) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::YZ, bDisabled) );
			{
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,24,0) * Scale), WidgetMatrix.TransformPosition(FVector(0,12,12) * Scale), YColor, SDPG_Foreground );
				PDI->DrawLine( WidgetMatrix.TransformPosition(FVector(0,12,12) * Scale), WidgetMatrix.TransformPosition(FVector(0,0,24) * Scale), ZColor, SDPG_Foreground );
			}
			PDI->SetHitProxy( NULL );
		}

		// Center cube
		if( (DrawAxis&(EAxisList::XYZ)) == EAxisList::XYZ )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxisList::XYZ, bDisabled) );

			Render_Cube(PDI, WidgetMatrix, XYZMaterial, Scale * 4 );

			PDI->SetHitProxy( NULL );
		}
	}
}


/**
* Draws the Translate & Rotate Z widget.
*/

void FCustomizableObjectWidget::Render_TranslateRotateZ( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, bool bDrawWidget )
{
	float Scale = View->WorldToScreen( InLocation ).W * ( 4.0f / View->UnscaledViewRect.Width() / View->ViewMatrices.GetProjectionMatrix().M[0][0] );

	// Figure out axis colors

	FColor XYPlaneColor  = ( (CurrentAxis&EAxisList::XY) == EAxisList::XY ) ? CurrentColor : PlaneColorXY;
	FColor ZRotateColor  = ( (CurrentAxis&EAxisList::ZRotation) == EAxisList::ZRotation ) ? CurrentColor : AxisColorZ.ToFColor(true);
	FColor XColor        = ( (CurrentAxis&EAxisList::X) == EAxisList::X ) ? CurrentColor : AxisColorX.ToFColor(true);
	FColor YColor        = ( (CurrentAxis&EAxisList::Y) == EAxisList::Y && CurrentAxis != EAxisList::ZRotation ) ? CurrentColor : AxisColorY.ToFColor(true);
	FColor ZColor        = ( (CurrentAxis&EAxisList::Z) == EAxisList::Z ) ? CurrentColor : AxisColorZ.ToFColor(true);

	// Figure out axis materials
	UMaterialInstance* ZRotateMaterial = ( ( (CurrentAxis&EAxisList::ZRotation) == EAxisList::ZRotation ) ? CurrentAxisMaterial : AxisMaterialZ );
	UMaterialInstance* XMaterial = ( CurrentAxis&EAxisList::X ? CurrentAxisMaterial : AxisMaterialX );
	UMaterialInstance* YMaterial = ( ( CurrentAxis&EAxisList::Y && CurrentAxis != EAxisList::ZRotation ) ? CurrentAxisMaterial : AxisMaterialY );
	UMaterialInstance* ZMaterial = ( CurrentAxis&EAxisList::Z ? CurrentAxisMaterial : AxisMaterialZ );

	// Figure out axis matrices
	FMatrix XMatrix = CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix YMatrix = FRotationMatrix( FRotator(0,90.f,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );
	FMatrix ZMatrix = FRotationMatrix( FRotator(90.f,0,0) ) * CustomCoordSystem * FTranslationMatrix( InLocation );

	const bool bIsPerspective = View->IsPerspectiveProjection();

	// For local space widgets, we always want to draw all three axis, since they may not be aligned with
	// the orthographic projection anyway.
	bool bIsLocalSpace = false;//( GEditorModeTools().GetCoordSystem() == COORD_Local );

	int32 DrawAxis = EAxisList::XYZ;//GEditorModeTools().GetWidgetAxisToDraw( GEditorModeTools().GetWidgetMode() );

	// Draw the grabbers
	if( bDrawWidget )
	{
		// Draw the axis lines with arrow heads
		if( DrawAxis&EAxisList::X && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.GetViewMatrix().M[0][2] != -1.f) )
		{
			Render_Axis( View, PDI, EAxisList::X, XMatrix, XMaterial, XAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&EAxisList::Y && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.GetViewMatrix().M[1][2] != -1.f) )
		{
			Render_Axis( View, PDI, EAxisList::Y, YMatrix, YMaterial, YAxisEnd, Scale, bDrawWidget );
		}

		if( DrawAxis&EAxisList::Z && (bIsPerspective || bIsLocalSpace || View->ViewMatrices.GetViewMatrix().M[0][1] != 1.f) )
		{
			Render_Axis( View, PDI, EAxisList::Z, ZMatrix, ZMaterial, ZAxisEnd, Scale, bDrawWidget );
		}

		bool bDisabled = false;//GEditorModeTools().IsModeActive(EM_Default) && GEditor->HasLockedActors();

		//ZRotation
		/*
		if( DrawAxis&EAxis::ZROTATION && (bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][2] != -1.f) )
		{
			PDI->SetHitProxy( new HWidgetAxis(EAxis::ZROTATION, bDisabled) );
			{
				float ScaledRadius = TRANSLATE_ROTATE_EAxis::CIRCLE_RADIUS*Scale;
				FVector XAxis = CustomCoordSystem.TransformPosition( FVector(1,0,0).RotateAngleAxis(GEditorModeTools().TranslateRotateXAxisAngle, FVector(0,0,1)) );
				FVector YAxis = CustomCoordSystem.TransformPosition( FVector(0,1,0).RotateAngleAxis(GEditorModeTools().TranslateRotateXAxisAngle, FVector(0,0,1)) );
				FVector BaseArrowPoint = InLocation + XAxis*ScaledRadius;
				DrawFlatArrow(PDI, BaseArrowPoint, XAxis, YAxis, ZRotateColor, ScaledRadius, ScaledRadius*.5f, ZRotateMaterial->GetRenderProxy(false), SDPG_Foreground);
			}
			PDI->SetHitProxy( NULL );
		}

		//XY Plane
		if( bIsPerspective || bIsLocalSpace || View->ViewMatrix.M[0][1] != 1.f )
		{
			if( (DrawAxis&(EAxis::XY)) == (EAxis::XY) ) 
			{
				PDI->SetHitProxy( new HWidgetAxis(EAxis::XY, bDisabled) );
				{
					DrawCircle( PDI, InLocation, CustomCoordSystem.TransformPosition( FVector(1,0,0) ), CustomCoordSystem.TransformPosition( FVector(0,1,0) ), XYPlaneColor, TRANSLATE_ROTATE_EAxis::CIRCLE_RADIUS*Scale, EAxis::CIRCLE_SIDES, SDPG_Foreground );
					XYPlaneColor.A = ((CurrentAxis&EAxis::XY)==EAxis::XY) ? 0x3f : 0x0f;	//make the disc transparent
					DrawDisc  ( PDI, InLocation, CustomCoordSystem.TransformPosition( FVector(1,0,0) ), CustomCoordSystem.TransformPosition( FVector(0,1,0) ), XYPlaneColor, TRANSLATE_ROTATE_EAxis::CIRCLE_RADIUS*Scale, EAxis::CIRCLE_SIDES, TransparentPlaneMaterialXY->GetRenderProxy(false), SDPG_Foreground );
				}
				PDI->SetHitProxy( NULL );
			}
		}
		*/
	}
}


/** Only some modes support Absolute Translation Movement */
bool FCustomizableObjectWidget::AllowsAbsoluteTranslationMovement(void)
{
	EWidgetMode CurrentMode = WM_Translate;//GEditorModeTools().GetWidgetMode();
	if ((CurrentMode == WM_Translate) || (CurrentMode == WM_TranslateRotateZ))
	{
		return true;
	}
	return false;
}

/** 
 * Serializes the widget references so they don't get garbage collected.
 *
 * @param Ar	FArchive to serialize with
 */
void FCustomizableObjectWidget::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( AxisMaterialX );
	Collector.AddReferencedObject( AxisMaterialY );
	Collector.AddReferencedObject( AxisMaterialZ );
	Collector.AddReferencedObject( OpaquePlaneMaterialXY );
	Collector.AddReferencedObject( TransparentPlaneMaterialXY );
	Collector.AddReferencedObject( GridMaterial );
	Collector.AddReferencedObject( CurrentAxisMaterial );
}

#define CAMERA_LOCK_DAMPING_FACTOR .1f
#define MAX_CAMERA_MOVEMENT_SPEED 512.0f
/**
 * Returns the Delta from the current position that the absolute movement system wants the object to be at
 * @param InParams - Structure containing all the information needed for absolute movement
 * @return - The requested delta from the current position
 */
FVector FCustomizableObjectWidget::GetAbsoluteTranslationDelta (const FAbsoluteMovementParams& InParams)
{
	FPlane MovementPlane(InParams.Position, InParams.PlaneNormal);
	FVector ProposedEndofEyeVector = InParams.EyePos + InParams.PixelDir;

	//default to not moving
	FVector RequestedPosition = InParams.Position;

	float DotProductWithPlaneNormal = InParams.PixelDir|InParams.PlaneNormal;
	//check to make sure we're not co-planar
	if (FMath::Abs(DotProductWithPlaneNormal) > DELTA)
	{
		//Get closest point on plane
		RequestedPosition = FMath::LinePlaneIntersection(InParams.EyePos, ProposedEndofEyeVector, MovementPlane);
	}

	//drag is a delta position, so just update the different between the previous position and the new position
	FVector DeltaPosition = RequestedPosition - InParams.Position;

	//Retrieve the initial offset, passing in the current requested position and the current position
	FVector InitialOffset = GetAbsoluteTranslationInitialOffset(RequestedPosition, InParams.Position);

	//subtract off the initial offset (where the widget was clicked) to prevent popping
	DeltaPosition -= InitialOffset;

	//remove the component along the normal we want to mute
	float MovementAlongMutedAxis = DeltaPosition|InParams.NormalToRemove;
	FVector OutDrag = DeltaPosition - (InParams.NormalToRemove*MovementAlongMutedAxis);

	if (InParams.bMovementLockedToCamera)
	{
		//DAMPEN ABSOLUTE MOVEMENT when the camera is locked to the object
		OutDrag *= CAMERA_LOCK_DAMPING_FACTOR;
		OutDrag.X = FMath::Clamp(OutDrag.X, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Y = FMath::Clamp(OutDrag.Y, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
		OutDrag.Z = FMath::Clamp(OutDrag.Z, -MAX_CAMERA_MOVEMENT_SPEED, MAX_CAMERA_MOVEMENT_SPEED);
	}

	//the they requested position snapping and we're not moving with the camera
	if (InParams.bPositionSnapping && !InParams.bMovementLockedToCamera && bSnapEnabled)
	{
		FVector MovementAlongAxis = FVector(OutDrag|InParams.XAxis, OutDrag|InParams.YAxis, OutDrag|InParams.ZAxis);
		//translation (either xy plane or z)
		//GEditor->Snap( MovementAlongAxis, FVector(GEditor->GetGridSize(),GEditor->GetGridSize(),GEditor->GetGridSize()) );
		OutDrag = MovementAlongAxis.X*InParams.XAxis + MovementAlongAxis.Y*InParams.YAxis + MovementAlongAxis.Z*InParams.ZAxis;
	}

	//get the distance from the original position to the new proposed position 
	FVector DeltaFromStart = InParams.Position + OutDrag - InitialTranslationPosition;

	//Get the vector from the eye to the proposed new position (to make sure it's not behind the camera
	FVector EyeToNewPosition = (InParams.Position + OutDrag) - InParams.EyePos;
	float BehindTheCameraDotProduct = EyeToNewPosition|InParams.CameraDir;

	//clamp so we don't lose objects off the edge of the world, or the requested position is behind the camera
	if ((DeltaFromStart.Size() > HALF_WORLD_MAX*.5) || ( BehindTheCameraDotProduct <= 0))
	{
		OutDrag = OutDrag.ZeroVector;
	}
	return OutDrag;
}

/**
 * Returns the offset from the initial selection point
 */
FVector FCustomizableObjectWidget::GetAbsoluteTranslationInitialOffset(const FVector& InNewPosition, const FVector& InCurrentPosition)
{
	if (!bAbsoluteTranslationInitialOffsetCached)
	{
		bAbsoluteTranslationInitialOffsetCached = true;
		InitialTranslationOffset = InNewPosition - InCurrentPosition;
		InitialTranslationPosition = InCurrentPosition;
	}
	return InitialTranslationOffset;
}



/**
 * Returns true if we're in Local Space editing mode or editing BSP (which uses the World axes anyway
 */
bool FCustomizableObjectWidget::IsRotationLocalSpace (void) const
{
	bool bIsLocalSpace = false;//( GEditorModeTools().GetCoordSystem() == COORD_Local );
	//for bsp and things that don't have a "true" local space, they will always use world.  So do NOT invert.
	if (bIsLocalSpace && CustomCoordSystem.Equals(FMatrix::Identity))
	{
		bIsLocalSpace = false;
	}
	return bIsLocalSpace;
}

/**
 * Returns the angle in degrees representation of how far we have just rotated
 */
float FCustomizableObjectWidget::GetDeltaRotation (void) const
{
	bool bIsLocalSpace = IsRotationLocalSpace();
	return (bIsLocalSpace ? -1 : 1)*30.0f;//GEditorModeTools().TotalDeltaRotation;
}


constexpr uint8 LargeInnerAlpha = 0x1f;
constexpr uint8 SmallInnerAlpha = 0x0f;
constexpr uint8 LargeOuterAlpha = 0x5f;
constexpr uint8 SmallOuterAlpha = 0x0f;

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
void FCustomizableObjectWidget::DrawRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FVector& InDirectionToWidget, const FColor& InColor, const float InScale)
{
	bool bIsPerspective = View->IsPerspectiveProjection();
	bool bIsOrtho = !bIsPerspective;
	//if we're in an ortho viewport and the ring is perpendicular to the camera (both Axis0 & Axis1 are perpendicular)
	bool bIsOrthoDrawingFullRing = bIsOrtho && (FMath::Abs(Axis0|InDirectionToWidget) < UE_KINDA_SMALL_NUMBER) && (FMath::Abs(Axis1|InDirectionToWidget) < UE_KINDA_SMALL_NUMBER);

	FColor ArcColor = InColor;
	ArcColor.A = LargeOuterAlpha;

	if (bDragging || (bIsOrthoDrawingFullRing))
	{
		if ((CurrentAxis&InAxis) || (bIsOrthoDrawingFullRing))
		{
			float DeltaRotation = GetDeltaRotation();
			float AbsRotation = FRotator::ClampAxis(fabs(DeltaRotation));
			float AngleOfChangeRadians (AbsRotation * PI / 180.f);

			//always draw clockwise, so if we're negative we need to flip the angle
			float StartAngle = DeltaRotation < 0.0f ? -AngleOfChangeRadians : 0.0f;
			float FilledAngle = AngleOfChangeRadians;

			//the axis of rotation
			FVector ZAxis = Axis0 ^ Axis1;

			ArcColor.A = LargeOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle, StartAngle + FilledAngle, ArcColor, InScale, InDirectionToWidget);
			ArcColor.A = SmallOuterAlpha;
			DrawPartialRotationArc(View, PDI, InAxis, InLocation,  Axis0, Axis1, StartAngle + FilledAngle, StartAngle + 2*PI, ArcColor, InScale, InDirectionToWidget);

			ArcColor = (CurrentAxis&InAxis) ? CurrentColor : ArcColor;
			//Hallow Arrow
			ArcColor.A = 0;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, 0, ArcColor, InScale);
			//Filled Arrow
			ArcColor.A = LargeOuterAlpha;
			DrawStartStopMarker(PDI, InLocation, Axis0, Axis1, DeltaRotation, ArcColor, InScale);

			ArcColor.A = 255;


			// Push out the snap marks so they dont z-fight with the background arcs
			FVector SnapLocation = InLocation;
			if( bIsOrtho)
			{
				SnapLocation += InDirectionToWidget * .5f;
			}
			else if( (InDirectionToWidget|ZAxis) <= 0.0f )
			{
				SnapLocation += ZAxis * .5f;
			}
			else
			{
				SnapLocation += ZAxis * -.5f;
			}

			
			//if (GEditor->GetUserSettings().RotGridEnabled)
			//{
			//	float DeltaAngle = GEditor->GetUserSettings().RotGridSize.Yaw;
			//	//every 22.5 degrees
			//	float TickMarker = 22.5f;
			//	for (float Angle = 0; Angle < 360.f; Angle+=DeltaAngle)
			//	{ 
			//		FVector GridAxis = Axis0.RotateAngleAxis(Angle, ZAxis);
			//		float PercentSize = (FMath::Fmod(Angle, TickMarker)==0) ? .75f : .25f;
			//		if (FMath::Fmod(Angle, 90.f) != 0)
			//		{
			//			DrawSnapMarker(PDI, SnapLocation,  GridAxis,  FVector::ZeroVector, ArcColor, InScale, 0.0f, PercentSize);
			//		}
			//	}
			//}

			//draw axis tick marks
			FColor AxisColor = InColor;
			//Rotate Colors to match Axis 0
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (DeltaRotation == 0) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation,  Axis0,  Axis1, AxisColor, InScale, .25f);
			AxisColor.A = (DeltaRotation == 180.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation, -Axis0, -Axis1, AxisColor, InScale, .25f);

			//Rotate Colors to match Axis 1
			Swap(AxisColor.R, AxisColor.G);
			Swap(AxisColor.B, AxisColor.R);
			AxisColor.A = (DeltaRotation == 90.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation,  Axis1, -Axis0, AxisColor, InScale, .25f);
			AxisColor.A = (DeltaRotation == 270.f) ? MAX_uint8 : LargeOuterAlpha;
			DrawSnapMarker(PDI, SnapLocation, -Axis1,  Axis0, AxisColor, InScale, .25f);

			if (bDragging)
			{
				float OffsetAngle = IsRotationLocalSpace() ? 0 : DeltaRotation;

				//CacheRotationHUDText(View, PDI, InLocation, Axis0.RotateAngleAxis(OffsetAngle, ZAxis), Axis1.RotateAngleAxis(OffsetAngle, ZAxis), DeltaRotation, InScale);
			}
		}
	}
	else
	{
		//Reverse the axes based on camera view
		FVector RenderAxis0 = ((Axis0|InDirectionToWidget) <= 0.0f) ? Axis0 : -Axis0;
		FVector RenderAxis1 = ((Axis1|InDirectionToWidget) <= 0.0f) ? Axis1 : -Axis1;

		DrawPartialRotationArc(View, PDI, InAxis, InLocation, RenderAxis0, RenderAxis1, 0, PI/2, ArcColor, InScale, InDirectionToWidget);
	}
}

/**
 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
 * @param View - Information about the scene/camera/etc
 * @param PDI - Drawing interface
 * @param InAxis - Enumeration of axis to rotate about
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InStartAngle - The starting angle about (Axis0^Axis1) to render the arc, in radians
 * @param InEndAngle - The ending angle about (Axis0^Axis1) to render the arc, in radians
 * @param InColor - The color associated with the axis of rotation
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FCustomizableObjectWidget::DrawPartialRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxisList::Type InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const float InScale, const FVector& InDirectionToWidget )
{
	bool bIsPerspective = View->IsPerspectiveProjection();
	PDI->SetHitProxy( new HWidgetAxis( InAxis ) );
	{
		FThickArcParams OuterArcParams(PDI, InLocation, TransparentPlaneMaterialXY, INNER_AXIS_CIRCLE_RADIUS*InScale, OUTER_AXIS_CIRCLE_RADIUS*InScale);
		FColor OuterColor = ( CurrentAxis&InAxis ? CurrentColor : InColor );
		//Pass through alpha
		OuterColor.A = InColor.A;
		DrawThickArc(OuterArcParams, Axis0, Axis1, InStartAngle, InEndAngle, OuterColor, InDirectionToWidget, !bIsPerspective );
	}
	PDI->SetHitProxy( NULL );

	if (bIsPerspective)
	{
		FThickArcParams InnerArcParams(PDI, InLocation, GridMaterial, 0.0f, INNER_AXIS_CIRCLE_RADIUS*InScale);
		FColor InnerColor = InColor;
		//if something is selected and it's not this
		InnerColor.A = ((CurrentAxis & InAxis) && !bDragging) ? LargeInnerAlpha : SmallInnerAlpha;
		DrawThickArc(InnerArcParams, Axis0, Axis1, InStartAngle, InEndAngle, InnerColor, InDirectionToWidget, false );
	}
}

/**
 * Renders a portion of an arc for the rotation widget
 * @param InParams - Material, Radii, etc
 * @param InStartAxis - Start of the arc, in radians
 * @param InEndAxis - End of the arc, in radians
 * @param InColor - Color to use for the arc
 */
void FCustomizableObjectWidget::DrawThickArc (const FThickArcParams& InParams, const FVector& Axis0, const FVector& Axis1, const float InStartAngle, const float InEndAngle, const FColor& InColor, const FVector& InDirectionToWidget, bool bIsOrtho )
{
	if (InColor.A == 0)
	{
		return;
	}
	const int32 NumPoints = FMath::TruncToInt(AXIS_CIRCLE_SIDES * (InEndAngle-InStartAngle)/(PI/2)) + 1;

	FColor TriangleColor = InColor;
	FColor RingColor = InColor;
	RingColor.A = MAX_uint8;

	FVector ZAxis = Axis0 ^ Axis1;
	FVector LastVertex;

	FDynamicMeshBuilder MeshBuilder(InParams.PDI->View->GetFeatureLevel());

	for (int32 RadiusIndex = 0; RadiusIndex < 2; ++RadiusIndex)
	{
		float Radius = (RadiusIndex == 0) ? InParams.OuterRadius : InParams.InnerRadius;
		float TCRadius = Radius / (float) InParams.OuterRadius;
		//Compute vertices for base circle.
		for(int32 VertexIndex = 0;VertexIndex <= NumPoints;VertexIndex++)
		{
			float Percent = VertexIndex/(float)NumPoints;
			float Angle = FMath::Lerp(InStartAngle, InEndAngle, Percent);
			float AngleDeg = FRotator::ClampAxis(Angle * 180.f / PI);

			FVector3f VertexDir = FVector3f(Axis0.RotateAngleAxis(AngleDeg, ZAxis));
			VertexDir.Normalize();

			float TCAngle = Percent*(PI/2);
			FVector2f TC(TCRadius*FMath::Cos(Angle), TCRadius*FMath::Sin(Angle));

			const FVector3f VertexPosition = FVector3f(InParams.Position) + VertexDir*Radius;
			FVector3f Normal = VertexPosition - FVector3f(InParams.Position);
			Normal.Normalize();

			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = FVector3f(VertexPosition);
			MeshVertex.Color = TriangleColor;
			MeshVertex.TextureCoordinate[0] = TC;

			MeshVertex.SetTangents(
				(FVector3f)-ZAxis,
				FVector3f(-ZAxis) ^ Normal,
				Normal
				);

			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex

			// Push out the arc line borders so they dont z-fight with the mesh arcs
			FVector StartLinePos = LastVertex;
			FVector EndLinePos = FVector(VertexPosition);
			if (VertexIndex != 0)
			{
				if( bIsOrtho )
				{
					StartLinePos += InDirectionToWidget * .5f;
					EndLinePos += InDirectionToWidget * .5f;
				}
				else if( (InDirectionToWidget|ZAxis) <= 0.0f )
				{
					StartLinePos += ZAxis * .5f;
					EndLinePos += ZAxis * .5f;
				}
				else
				{
					StartLinePos += ZAxis * -.5f;
					EndLinePos += ZAxis * -.5f;
				}

				InParams.PDI->DrawLine(StartLinePos,EndLinePos,RingColor,SDPG_Foreground);
			}
			LastVertex = FVector(VertexPosition);
		}
	}

	//Add top/bottom triangles, in the style of a fan.
	int32 InnerVertexStartIndex = NumPoints + 1;
	for(int32 VertexIndex = 0; VertexIndex < NumPoints; VertexIndex++)
	{
		MeshBuilder.AddTriangle(VertexIndex, VertexIndex+1, InnerVertexStartIndex+VertexIndex);
		MeshBuilder.AddTriangle(VertexIndex+1, InnerVertexStartIndex+VertexIndex+1, InnerVertexStartIndex+VertexIndex);
	}

	MeshBuilder.Draw(InParams.PDI, FMatrix::Identity, Helper_GetMaterialProxy(InParams.Material),SDPG_Foreground,0.f);
}

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
void FCustomizableObjectWidget::DrawSnapMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FColor& InColor, const float InScale, const float InWidthPercent, const float InPercentSize)
{
	const float InnerDistance = (INNER_AXIS_CIRCLE_RADIUS*InScale);
	const float OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS*InScale);
	const float MaxMarkerHeight = OuterDistance - InnerDistance;
	const float MarkerWidth = MaxMarkerHeight*InWidthPercent;
	const float MarkerHeight = MaxMarkerHeight*InPercentSize;

	FVector Vertices[4];
	Vertices[0] = InLocation + (OuterDistance)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[1] = Vertices[0] + (MarkerWidth)*Axis1;
	Vertices[2] = InLocation + (OuterDistance-MarkerHeight)*Axis0 - (MarkerWidth*.5)*Axis1;
	Vertices[3] = Vertices[2] + (MarkerWidth)*Axis1;

	//draw at least one line
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_Foreground);

	//if there should be thickness, draw the other lines
	if (InWidthPercent > 0.0f)
	{
		PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_Foreground);
		PDI->DrawLine(Vertices[1], Vertices[3], InColor, SDPG_Foreground);
		PDI->DrawLine(Vertices[2], Vertices[3], InColor, SDPG_Foreground);

		//fill in the box
		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

		for(int32 VertexIndex = 0;VertexIndex < 4; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = (FVector3f)Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate[0] = FVector2f(0.0f, 0.0f);
			MeshVertex.SetTangents(
				(FVector3f)Axis0,
				(FVector3f)Axis1,
				(FVector3f)(Axis0 ^ Axis1)
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.AddTriangle(1, 3, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, Helper_GetMaterialProxy(TransparentPlaneMaterialXY),SDPG_Foreground,0.f);
	}
}

/**
 * Draw Start/Stop Marker to show delta rotations along the arc of rotation
 * @param PDI - Drawing interface
 * @param InLocation - The Origin of the widget
 * @param Axis0 - The Axis that describes a 0 degree rotation
 * @param Axis1 - The Axis that describes a 90 degree rotation
 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1), units are degrees
 * @param InColor - The color to use for line/poly drawing
 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
 */
void FCustomizableObjectWidget::DrawStartStopMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const float InAngle, const FColor& InColor, const float InScale)
{
	const float ArrowHeightPercent = .8f;
	const float InnerDistance = (INNER_AXIS_CIRCLE_RADIUS*InScale);
	const float OuterDistance = (OUTER_AXIS_CIRCLE_RADIUS*InScale);
	const float RingHeight = OuterDistance - InnerDistance;
	const float ArrowHeight = RingHeight*ArrowHeightPercent;
	const float ThirtyDegrees = PI / 6.0f;
	const float HalfArrowidth = ArrowHeight*FMath::Tan(ThirtyDegrees);

	FVector ZAxis = Axis0 ^ Axis1;
	FVector RotatedAxis0 = Axis0.RotateAngleAxis(InAngle, ZAxis);
	FVector RotatedAxis1 = Axis1.RotateAngleAxis(InAngle, ZAxis);

	FVector Vertices[3];
	Vertices[0] = InLocation + (OuterDistance)*RotatedAxis0;
	Vertices[1] = Vertices[0] + (ArrowHeight)*RotatedAxis0 - HalfArrowidth*RotatedAxis1;
	Vertices[2] = Vertices[1] + (2*HalfArrowidth)*RotatedAxis1;

	PDI->DrawLine(Vertices[0], Vertices[1], InColor, SDPG_Foreground);
	PDI->DrawLine(Vertices[1], Vertices[2], InColor, SDPG_Foreground);
	PDI->DrawLine(Vertices[0], Vertices[2], InColor, SDPG_Foreground);

	if (InColor.A > 0)
	{
		//fill in the box
		FDynamicMeshBuilder MeshBuilder(PDI->View->GetFeatureLevel());

		for(int32 VertexIndex = 0;VertexIndex < 3; VertexIndex++)
		{
			FDynamicMeshVertex MeshVertex;
			MeshVertex.Position = (FVector3f)Vertices[VertexIndex];
			MeshVertex.Color = InColor;
			MeshVertex.TextureCoordinate[0] = FVector2f(0.0f, 0.0f);
			MeshVertex.SetTangents(
				(FVector3f)RotatedAxis0,
				(FVector3f)RotatedAxis1,
				(FVector3f)(RotatedAxis0 ^ RotatedAxis1)
				);
			MeshBuilder.AddVertex(MeshVertex); //Add bottom vertex
		}

		MeshBuilder.AddTriangle(0, 1, 2);
		MeshBuilder.Draw(PDI, FMatrix::Identity, Helper_GetMaterialProxy(TransparentPlaneMaterialXY),SDPG_Foreground,0.f);
	}
}


uint32 FCustomizableObjectWidget::GetDominantAxisIndex( const FVector& InDiff, ELevelViewportType ViewportType ) const
{
	uint32 DominantIndex = 0;
	if( FMath::Abs(InDiff.X) < FMath::Abs(InDiff.Y) )
	{
		DominantIndex = 1;
	}

	//const int32 WidgetMode = EAxisList::XYZ;//GEditorModeTools().GetWidgetMode();

	//switch(EAxisList::XYZ)
	//{
    //    case EAxisList::WM_Translate:
			switch( ViewportType )
			{
				case LVT_OrthoXY:
					if( CurrentAxis == EAxisList::X )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Y )
					{
						DominantIndex = 1;
					}
					break;
				case LVT_OrthoXZ:
					if( CurrentAxis == EAxisList::X )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Z )
					{
						DominantIndex = 1;
					}
					break;
				case LVT_OrthoYZ:
					if( CurrentAxis == EAxisList::Y )
					{
						DominantIndex = 0;
					}
					else if( CurrentAxis == EAxisList::Z )
					{
						DominantIndex = 1;
					}
					break;
				default:
					break;
			}
	//		break;
	//	default:
	//		break;
	//}

	return DominantIndex;
}

