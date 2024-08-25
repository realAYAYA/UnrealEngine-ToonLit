// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Serialization/BulkData.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "NavCollision.generated.h"

class FPrimitiveDrawInterface;
struct FCompositeNavModifier;
struct FNavigableGeometryExport;

USTRUCT()
struct FNavCollisionCylinder
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Cylinder)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Radius = 0.f;

	UPROPERTY(EditAnywhere, Category=Cylinder)
	float Height = 0.f;
};

USTRUCT()
struct FNavCollisionBox
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category=Box)
	FVector Extent = FVector::ZeroVector;
};

UCLASS(config=Engine, MinimalAPI)
class UNavCollision : public UNavCollisionBase
{
	GENERATED_UCLASS_BODY()

	TNavStatArray<int32> ConvexShapeIndices;

	FBox Bounds;

	/** list of nav collision cylinders */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionCylinder> CylinderCollision;

	/** list of nav collision boxes */
	UPROPERTY(EditAnywhere, Category=Navigation)
	TArray<FNavCollisionBox> BoxCollision;

	/** navigation area type that will be use when this static mesh is used as 
	 *	a navigation obstacle. See bIsDynamicObstacle.
	 *	Empty AreaClass means the default obstacle class will be used */
	UPROPERTY(EditAnywhere, Category = Navigation, meta = (EditCondition = "bIsDynamicObstacle"))
	TSubclassOf<class UNavArea> AreaClass;

	/** If set, convex collisions will be exported offline for faster runtime navmesh building (increases memory usage) */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bGatherConvexGeometry : 1;

	/** If false, will not create nav collision when connecting as a client */
	UPROPERTY(EditAnywhere, Category=Navigation, config)
	uint32 bCreateOnClient : 1;

	/** if set, convex geometry will be rebuild instead of using cooked data */
	uint32 bForceGeometryRebuild : 1;

	/** Guid of associated BodySetup */
	FGuid BodySetupGuid;

	/** Cooked data for each format */
	FFormatContainer CookedFormatData;

	//~ Begin UObject Interface.
	NAVIGATIONSYSTEM_API virtual void PostInitProperties() override;
	NAVIGATIONSYSTEM_API virtual void Serialize(FArchive& Ar) override;
	NAVIGATIONSYSTEM_API virtual void PostLoad() override;
	NAVIGATIONSYSTEM_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	NAVIGATIONSYSTEM_API virtual bool NeedsLoadForTargetPlatform(const class ITargetPlatform* TargetPlatform) const override;
	virtual bool NeedsLoadForClient() const override { return bCreateOnClient; }
	//~ End UObject Interface.

	NAVIGATIONSYSTEM_API FGuid GetGuid() const;

	/** Tries to read data from DDC, and if that fails gathers navigation
	 *	collision data, stores it and uploads to DDC */
	NAVIGATIONSYSTEM_API virtual void Setup(class UBodySetup* BodySetup) override;

	NAVIGATIONSYSTEM_API virtual FBox GetBounds() const override;

	/** copy user settings from other nav collision data */
	NAVIGATIONSYSTEM_API void CopyUserSettings(const UNavCollision& OtherData);

	/** show cylinder and box collisions */
	NAVIGATIONSYSTEM_API virtual void DrawSimpleGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color) override;

	/** Get data for dynamic obstacle */
	NAVIGATIONSYSTEM_API virtual void GetNavigationModifier(FCompositeNavModifier& Modifier, const FTransform& LocalToWorld) override;

	/** Export collision data */
	NAVIGATIONSYSTEM_API virtual bool ExportGeometry(const FTransform& LocalToWorld, FNavigableGeometryExport& GeoExport) const override;

	/** Read collisions data */
	NAVIGATIONSYSTEM_API void GatherCollision();

#if WITH_EDITOR
	NAVIGATIONSYSTEM_API virtual void InvalidateCollision() override;
#endif // WITH_EDITOR

protected:
	NAVIGATIONSYSTEM_API void ClearCollision();

#if WITH_EDITOR
	NAVIGATIONSYSTEM_API void InvalidatePhysicsData();
#endif // WITH_EDITOR
	NAVIGATIONSYSTEM_API FByteBulkData* GetCookedData(FName Format);
};
