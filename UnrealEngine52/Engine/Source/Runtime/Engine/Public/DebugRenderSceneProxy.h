// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugRenderSceneProxy.h: Useful scene proxy for rendering non performance-critical information.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/Material.h"
#include "DynamicMeshBuilder.h"

class APlayerController;
class FMeshElementCollector;
class FPrimitiveDrawInterface;
class FRegisterComponentContext;
class UCanvas;
class UPrimitiveComponent;

DECLARE_DELEGATE_TwoParams(FDebugDrawDelegate, class UCanvas*, class APlayerController*);

class FDebugRenderSceneProxy : public FPrimitiveSceneProxy
{
public:
	ENGINE_API virtual ~FDebugRenderSceneProxy() {};
	
	ENGINE_API SIZE_T GetTypeHash() const override;

	enum EDrawType
	{
		SolidMesh = 0,
		WireMesh = 1,
		SolidAndWireMeshes = 2,
	};
	ENGINE_API FDebugRenderSceneProxy(const UPrimitiveComponent* InComponent);
	ENGINE_API FDebugRenderSceneProxy(FDebugRenderSceneProxy const&) = default;

	// FPrimitiveSceneProxy interface.

	/** 
	 * Draw the scene proxy as a dynamic element
	 */
	ENGINE_API virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	/**
	 * Draws a line with an arrow at the end.
	 *
	 * @param PDI		Draw interface to render to
	 * @param Start		Starting point of the line.
	 * @param End		Ending point of the line.
	 * @param Color		Color of the line.
	 * @param Mag		Size of the arrow.
	 */
	void DrawLineArrow(FPrimitiveDrawInterface* PDI,const FVector &Start,const FVector &End,const FColor &Color,float Mag) const;

	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	ENGINE_API uint32 GetAllocatedSize(void) const;

	FORCEINLINE static bool PointInView(const FVector& Location, const FSceneView* View)
	{
		return View ? View->ViewFrustum.IntersectBox(Location, FVector::ZeroVector) : false;
	}

	FORCEINLINE static bool PointInRange(const FVector& Start, const FSceneView* View, float Range)
	{
		return FVector::DistSquared(Start, View->ViewMatrices.GetViewOrigin()) <= FMath::Square(Range);
	}

	/** Struct to hold info about lines to render. */
	struct FDebugLine
	{
		FDebugLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor, float InThickness = 0) :
		Start(InStart),
		End(InEnd),
		Color(InColor),
		Thickness(InThickness) {}

		FVector Start;
		FVector End;
		FColor Color;
		float Thickness;
	};

	/** Struct to hold info about boxes to render. */
	struct FDebugBox
	{
		FDebugBox(const FBox& InBox, const FColor& InColor)
			: Box(InBox), Color(InColor)
		{
		}

		FDebugBox(const FBox& InBox, const FColor& InColor, const FTransform& InTransform)
			: Box(InBox), Color(InColor), Transform(InTransform)
		{
		}

		FBox Box;
		FColor Color;
		FTransform Transform;
	};

	/** Struct to hold info about cylinders to render. */
	struct FWireCylinder
	{
		FWireCylinder(const FVector &InBase, const float InRadius, const float InHalfHeight, const FColor &InColor) :
		Base(InBase),
		Radius(InRadius),
		HalfHeight(InHalfHeight),
		Color(InColor) {}

		FVector Base;
		float Radius;
		float HalfHeight;
		FColor Color;
	};

	/** Struct to hold info about lined stars to render. */
	struct FWireStar
	{
		FWireStar(const FVector &InPosition, const FColor &InColor, const float &InSize) : 
		Position(InPosition),
		Color(InColor),
		Size(InSize) {}

		FVector Position;
		FColor Color;
		float Size;
	};

	/** Struct to hold info about arrowed lines to render. */
	struct FArrowLine
	{
		FArrowLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor) : 
		Start(InStart),
		End(InEnd),
		Color(InColor) {}

		FVector Start;
		FVector End;
		FColor Color;
	};

	/** Struct to gold info about dashed lines to render. */
	struct FDashedLine
	{
		FDashedLine(const FVector &InStart, const FVector &InEnd, const FColor &InColor, const float InDashSize) :
		Start(InStart),
		End(InEnd),
		Color(InColor),
		DashSize(InDashSize) {}

		FVector Start;
		FVector End;
		FColor Color;
		float DashSize;
	};

	/** Struct to hold info about spheres to render */
	struct FSphere
	{
		FSphere() {}
		FSphere(const float& InRadius, const FVector& InLocation, const FLinearColor& InColor) :
		Radius(InRadius),
		Location(InLocation),
		Color(InColor.ToFColor(true)) {}

		float Radius;
		FVector Location;
		FColor Color;
	};

	/** Struct to hold info about texts to render using 3d coordinates */
	struct FText3d
	{
		FText3d() {}
		FText3d(const FString& InString, const FVector& InLocation, const FLinearColor& InColor) :
		Text(InString),
		Location(InLocation),
		Color(InColor.ToFColor(true)) {}

		FString Text;
		FVector Location;
		FColor Color;
	};

	struct FCone
	{
		FCone() {}
		FCone(const FMatrix& InConeToWorld, const float InAngle1, const float InAngle2, const FLinearColor& InColor) :
			ConeToWorld(InConeToWorld),
			Angle1(InAngle1),
			Angle2(InAngle2),
			Color(InColor.ToFColor(true)) {}

		FMatrix ConeToWorld;
		float Angle1;
		float Angle2;
		FColor Color;
	};

	struct FMesh
	{
		TArray<FDynamicMeshVertex>	Vertices;
		TArray <uint32> Indices;
		FColor Color;
	};

	struct FCapsule
	{
		FCapsule() {}
		FCapsule(const FVector& InBase, const float& InRadius, const FVector& x, const FVector& y, const FVector &z, const float& InHalfHeight, const FLinearColor& InColor)
			: Radius(InRadius)
			, Base(InBase)
			, Color(InColor.ToFColor(true))
			, HalfHeight(InHalfHeight)
			, X(x)
			, Y(y)
			, Z(z)
		{

		}

		float Radius;
		FVector Base; //Center point of the base of the cylinder.
		FColor Color;
		float HalfHeight;
		FVector X, Y, Z; //X, Y, and Z alignment axes to draw along.
	};

	TArray<FDebugLine> Lines;
	TArray<FDashedLine>	DashedLines;
	TArray<FArrowLine> ArrowLines;
	TArray<FWireCylinder> Cylinders;
	TArray<FWireStar> Stars;
	TArray<FDebugBox> Boxes;
	TArray<FSphere> Spheres;
	TArray<FText3d> Texts;
	TArray<FCone> Cones;
	TArray<FMesh> Meshes;
	TArray<FCapsule> Capsules;

	uint32 ViewFlagIndex;
	float TextWithoutShadowDistance;
	FString ViewFlagName;
	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	EDrawType DrawType;
	uint32 DrawAlpha;

	TWeakObjectPtr<class UMaterial> SolidMeshMaterial;
};


struct FDebugDrawDelegateHelper
{
	FDebugDrawDelegateHelper()
		: State(UndefinedState)
		, ViewFlagName(TEXT("Game"))
		, TextWithoutShadowDistance(1500)
	{}

	virtual ~FDebugDrawDelegateHelper() {}

protected:
	typedef TArray<FDebugRenderSceneProxy::FText3d> TextArray;

public:
	void InitDelegateHelper(const FDebugRenderSceneProxy* InSceneProxy)
	{
		check(IsInParallelGameThread() || IsInGameThread());

		Texts.Reset();
		Texts.Append(InSceneProxy->Texts);
		ViewFlagName = InSceneProxy->ViewFlagName;
		TextWithoutShadowDistance = InSceneProxy->TextWithoutShadowDistance;
		State = (State == UndefinedState) ? InitializedState : State;
	}

	UE_DEPRECATED(5.0, "This method is deprecated. Call RequestRegisterDebugDrawDelegate or override RegisterDebugDrawDelegateInternal instead.")
	ENGINE_API virtual void RegisterDebugDrawDelgate() final { RegisterDebugDrawDelegateInternal(); }
	UE_DEPRECATED(5.0, "This method is deprecated. Use UnregisterDebugDrawDelegate instead.")
	ENGINE_API virtual void UnregisterDebugDrawDelgate() { UnregisterDebugDrawDelegate(); }
	UE_DEPRECATED(5.0, "This method is deprecated. Use ReregisterDebugDrawDelegate instead.")
	ENGINE_API void ReregisterDebugDrawDelgate() { ReregisterDebugDrawDelegate(); }
	UE_DEPRECATED(5.0, "This method is deprecated. Call RequestRegisterDebugDrawDelegate or override RegisterDebugDrawDelegateInternal instead.")
	ENGINE_API virtual void RegisterDebugDrawDelegate() final { RegisterDebugDrawDelegateInternal(); }

	/**
	 * Method that should be called at render state creation (i.e. CreateRenderState_Concurrent).
	 * It will either call `RegisterDebugDrawDelegate` when deferring context is not provided
	 * or mark for deferred registration that should be flushed by calling `ProcessDeferredRegister`
	 * on scene proxy creation.
	 * @param Context valid context is provided when primitives are batched for deferred 'add'
	 */
	ENGINE_API void RequestRegisterDebugDrawDelegate(FRegisterComponentContext* Context);

	/**
	 * Method that should be called when creating scene proxy (i.e. CreateSceneProxy) to process any pending registration that might have
	 * been requested from deferred primitive batching (i.e. CreateRenderState_Concurrent(FRegisterComponentContext != nullptr)).
	 */
	ENGINE_API void ProcessDeferredRegister();

	/** called to clean up debug drawing delegate in UDebugDrawService */
	ENGINE_API virtual void UnregisterDebugDrawDelegate();

	ENGINE_API void ReregisterDebugDrawDelegate();

protected:
	/** called to set up debug drawing delegate in UDebugDrawService if you want to draw labels */
	ENGINE_API virtual void RegisterDebugDrawDelegateInternal();

	ENGINE_API virtual void DrawDebugLabels(UCanvas* Canvas, APlayerController*);
	ENGINE_API void ResetTexts() { Texts.Reset(); }

protected:
	FDebugDrawDelegate DebugTextDrawingDelegate;
	FDelegateHandle DebugTextDrawingDelegateHandle;
	enum EState
	{
		UndefinedState,
		InitializedState,
		RegisteredState,
	} State;

	bool bDeferredRegister = false;

private:
	TextArray Texts;
	FString ViewFlagName;
	float TextWithoutShadowDistance;
};
