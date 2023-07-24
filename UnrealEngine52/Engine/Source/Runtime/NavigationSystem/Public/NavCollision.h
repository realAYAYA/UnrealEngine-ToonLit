// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

UCLASS(config=Engine)
class NAVIGATIONSYSTEM_API UNavCollision : public UNavCollisionBase
{
	GENERATED_UCLASS_BODY()

	TNavStatArray<int32> ConvexShapeIndices;

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
	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual bool NeedsLoadForTargetPlatform(const class ITargetPlatform* TargetPlatform) const override;
	virtual bool NeedsLoadForClient() const override { return bCreateOnClient; }
	//~ End UObject Interface.

	FGuid GetGuid() const;

	/** Tries to read data from DDC, and if that fails gathers navigation
	 *	collision data, stores it and uploads to DDC */
	virtual void Setup(class UBodySetup* BodySetup) override;

	/** copy user settings from other nav collision data */
	void CopyUserSettings(const UNavCollision& OtherData);

	/** show cylinder and box collisions */
	virtual void DrawSimpleGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color) override;

	/** Get data for dynamic obstacle */
	virtual void GetNavigationModifier(FCompositeNavModifier& Modifier, const FTransform& LocalToWorld) override;

	/** Export collision data */
	virtual bool ExportGeometry(const FTransform& LocalToWorld, FNavigableGeometryExport& GeoExport) const override;

	/** Read collisions data */
	void GatherCollision();

#if WITH_EDITOR
	virtual void InvalidateCollision() override;
#endif // WITH_EDITOR

protected:
	void ClearCollision();

#if WITH_EDITOR
	void InvalidatePhysicsData();
#endif // WITH_EDITOR
	FByteBulkData* GetCookedData(FName Format);
};
