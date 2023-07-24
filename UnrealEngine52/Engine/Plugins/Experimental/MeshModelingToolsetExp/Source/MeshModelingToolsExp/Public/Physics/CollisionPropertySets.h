// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BodySetupEnums.h"
#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "CollisionPropertySets.generated.h"


class FPhysicsDataCollection;


UENUM()
enum class ECollisionGeometryMode
{
	/** Use project physics settings (DefaultShapeComplexity) */
	Default = CTF_UseDefault,
	/** Create both simple and complex shapes. Simple shapes are used for regular scene queries and collision tests. Complex shape (per poly) is used for complex scene queries.*/
	SimpleAndComplex = CTF_UseSimpleAndComplex,
	/** Create only simple shapes. Use simple shapes for all scene queries and collision tests.*/
	UseSimpleAsComplex = CTF_UseSimpleAsComplex,
	/** Create only complex shapes (per poly). Use complex shapes for all scene queries and collision tests. Can be used in simulation for static shapes only (i.e can be collided against but not moved through forces or velocity.) */
	UseComplexAsSimple = CTF_UseComplexAsSimple
};


USTRUCT()
struct MESHMODELINGTOOLSEXP_API FPhysicsSphereData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	float Radius = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FTransform Transform;

	/** Shape Element storing standard collision and physics properties for a shape */
	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FKShapeElem Element;
};

USTRUCT()
struct MESHMODELINGTOOLSEXP_API FPhysicsBoxData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FVector Dimensions = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FTransform Transform;

	/** Shape Element storing standard collision and physics properties for a shape */
	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FKShapeElem Element;
};

USTRUCT()
struct MESHMODELINGTOOLSEXP_API FPhysicsCapsuleData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	float Radius = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	float Length = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FTransform Transform;

	/** Shape Element storing standard collision and physics properties for a shape */
	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FKShapeElem Element;
};

USTRUCT()
struct MESHMODELINGTOOLSEXP_API FPhysicsConvexData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = Convex)
	int32 NumVertices = 0;

	UPROPERTY(VisibleAnywhere, Category = Convex)
	int32 NumFaces = 0;

	/** Shape Element storing standard collision and physics properties for a shape */
	UPROPERTY(VisibleAnywhere, Category = Sphere)
	FKShapeElem Element;
};

USTRUCT()
struct MESHMODELINGTOOLSEXP_API FPhysicsLevelSetData
{
	GENERATED_BODY()

	/** Shape Element storing standard collision and physics properties for a shape */
	UPROPERTY(VisibleAnywhere, Category = LevelSet)
	FKShapeElem Element;
};

UCLASS()
class MESHMODELINGTOOLSEXP_API UPhysicsObjectToolPropertySet : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Source Object Name */
	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	FString ObjectName;

	/** Collision Flags controlling how simple and complex collision shapes are used */
	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	ECollisionGeometryMode CollisionType;

	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	TArray<FPhysicsSphereData> Spheres;

	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	TArray<FPhysicsBoxData> Boxes;

	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	TArray<FPhysicsCapsuleData> Capsules;

	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	TArray<FPhysicsConvexData> Convexes;

	UPROPERTY(VisibleAnywhere, Category = PhysicsData)
	TArray<FPhysicsLevelSetData> LevelSets;

	void Reset();
};




UCLASS()
class MESHMODELINGTOOLSEXP_API UCollisionGeometryVisualizationProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** Thickness of lines used to visualize collision shapes */
	UPROPERTY(EditAnywhere, Category = "Collision Visualization")
	float LineThickness = 3.0f;

	/** Show occluded parts of the collision geometry, rendered with dashed lines */
	UPROPERTY(EditAnywhere, Category = "Collision Visualization")
	bool bShowHidden = false;

	/** Render each collision geometry with a randomly-chosen color */
	UPROPERTY(EditAnywhere, Category = "Collision Visualization")
	bool bRandomColors = true;

	/** The color to use for all collision visualizations, if random colors are not used */
	UPROPERTY(EditAnywhere, Category = "Collision Visualization", meta = (EditCondition = "!bRandomColors"))
	FColor Color = FColor::Red;
};




namespace UE
{
	namespace PhysicsTools
	{
		void InitializePhysicsToolObjectPropertySet(const FPhysicsDataCollection* PhysicsData, UPhysicsObjectToolPropertySet* PropSet);
	}
}
