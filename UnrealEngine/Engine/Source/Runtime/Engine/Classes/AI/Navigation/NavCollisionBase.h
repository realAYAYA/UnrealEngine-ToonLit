// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavigationTypes.h"
#include "NavCollisionBase.generated.h"


class FPrimitiveDrawInterface;

struct FNavCollisionConvex
{
	TNavStatArray<FVector> VertexBuffer;
	TNavStatArray<int32> IndexBuffer;
};

UCLASS(abstract, config=Engine, MinimalAPI)
class UNavCollisionBase : public UObject
{
	GENERATED_BODY()

protected:
	DECLARE_DELEGATE_RetVal_OneParam(UNavCollisionBase*, FConstructNew, UObject& /*Outer*/);

	static ENGINE_API FConstructNew ConstructNewInstanceDelegate;
	struct FDelegateInitializer
	{
		FDelegateInitializer();
	};
	static ENGINE_API FDelegateInitializer DelegateInitializer;

	/** If set, mesh will be used as dynamic obstacle (don't create navmesh on top, much faster adding/removing) */
	UPROPERTY(EditAnywhere, Category = Navigation, config)
	uint32 bIsDynamicObstacle : 1;

	/** convex collisions are ready to use */
	uint32 bHasConvexGeometry : 1;

	FNavCollisionConvex TriMeshCollision;
	FNavCollisionConvex ConvexCollision;

public:
	static UNavCollisionBase* ConstructNew(UObject& Outer) { return ConstructNewInstanceDelegate.Execute(Outer); }

	ENGINE_API UNavCollisionBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Tries to read data from DDC, and if that fails gathers navigation
	*	collision data, stores it and uploads to DDC */
	ENGINE_API virtual void Setup(class UBodySetup* BodySetup) PURE_VIRTUAL(UNavCollisionBase::Setup, );

	[[nodiscard]] ENGINE_API virtual FBox GetBounds() const PURE_VIRTUAL(UNavCollisionBase::GetBounds, static FBox InvalidBox; return InvalidBox; );

	/** Export collision data */
	ENGINE_API virtual bool ExportGeometry(const FTransform& LocalToWorld, FNavigableGeometryExport& GeoExport) const PURE_VIRTUAL(UNavCollisionBase::ExportGeometry, return false; );

	/** Get data for dynamic obstacle */
	ENGINE_API virtual void GetNavigationModifier(FCompositeNavModifier& Modifier, const FTransform& LocalToWorld) PURE_VIRTUAL(UNavCollisionBase::GetNavigationModifier, );

	/** draw cylinder and box collisions */
	virtual void DrawSimpleGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color) {}

#if WITH_EDITOR
	ENGINE_API virtual void InvalidateCollision() PURE_VIRTUAL(UNavCollisionBase::InvalidateCollision, );
#endif // WITH_EDITOR

	bool IsDynamicObstacle() const { return bIsDynamicObstacle; }
	bool HasConvexGeometry() const { return bHasConvexGeometry; }
	const FNavCollisionConvex& GetTriMeshCollision() const { return TriMeshCollision; }
	const FNavCollisionConvex& GetConvexCollision() const { return ConvexCollision;	}
	FNavCollisionConvex& GetMutableTriMeshCollision() { return TriMeshCollision; }
	FNavCollisionConvex& GetMutableConvexCollision() { return ConvexCollision; }
};
