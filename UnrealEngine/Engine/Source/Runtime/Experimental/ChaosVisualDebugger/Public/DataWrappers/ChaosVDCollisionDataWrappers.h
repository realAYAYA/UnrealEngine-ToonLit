// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"

#include "ChaosVDCollisionDataWrappers.generated.h"

namespace Chaos
{
	enum class EParticleType : uint8;
	class FChaosArchive;
}

UENUM()
enum class EChaosVDContactShapesType
{
	Unknown,
	SphereSphere,
	SphereCapsule,
	SphereBox,
	SphereConvex,
	SphereTriMesh,
	SphereHeightField,
	SpherePlane,
	CapsuleCapsule,
	CapsuleBox,
	CapsuleConvex,
	CapsuleTriMesh,
	CapsuleHeightField,
	BoxBox,
	BoxConvex,
	BoxTriMesh,
	BoxHeightField,
	BoxPlane,
	ConvexConvex,
	ConvexTriMesh,
	ConvexHeightField,
	GenericConvexConvex,
	LevelSetLevelSet,

	NumShapesTypes
};

UENUM()
enum class EChaosVDContactPointType : int8
{
	Unknown,
	VertexPlane,
	EdgeEdge,
	PlaneVertex,
	VertexVertex,
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDContactPoint
{
	GENERATED_BODY()

	// Shape-space contact points on the two bodies
	UPROPERTY(EditAnywhere, Category=Contact)
	TArray<FVector> ShapeContactPoints;

	// Shape-space contact normal on the second shape with direction that points away from shape 1
	UPROPERTY(EditAnywhere, Category=Contact)
	FVector  ShapeContactNormal;

	// Contact separation (negative for overlap)
	UPROPERTY(EditAnywhere, Category=Contact)
	float Phi;

	// Face index of the shape we hit. Only valid for Heightfield and Trimesh contact points, otherwise INDEX_NONE
	UPROPERTY(EditAnywhere, Category=Contact)
	int32 FaceIndex;

	// Whether this is a vertex-plane contact, edge-edge contact etc.
	UPROPERTY(EditAnywhere, Category=Contact)
	EChaosVDContactPointType ContactType;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDContactPoint> : public TStructOpsTypeTraitsBase2<FChaosVDContactPoint>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDContactPoint& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDManifoldPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category=ManifoldPointFalgs)
	uint8 bDisabled:1 = false;
	UPROPERTY(EditAnywhere, Category=ManifoldPointFalgs)
	uint8 bWasRestored:1 = false;
	UPROPERTY(EditAnywhere, Category=ManifoldPointFalgs)
	uint8 bWasReplaced:1 = false;
	UPROPERTY(EditAnywhere, Category=ManifoldPointFalgs)
	uint8 bHasStaticFrictionAnchor:1 = false;

	UPROPERTY(EditAnywhere, Category=ManifoldPointResult)
	uint8 bIsValid:1 = false;
	UPROPERTY(EditAnywhere, Category=ManifoldPointResult)
	uint8 bInsideStaticFrictionCone:1 = false;

	UPROPERTY(EditAnywhere, Category=ManifoldPointResult)
	FVector NetPushOut;
	UPROPERTY(EditAnywhere, Category=ManifoldPointResult)
	FVector NetImpulse;

	UPROPERTY(EditAnywhere, Category=ManifoldPoint)
	float TargetPhi;
	UPROPERTY(EditAnywhere, Category=ManifoldPoint)
	TArray<FVector> ShapeAnchorPoints;
	UPROPERTY(EditAnywhere, Category=ManifoldPoint)
	TArray<FVector> InitialShapeContactPoints;
	UPROPERTY(EditAnywhere, Category=ManifoldPoint)
	FChaosVDContactPoint ContactPoint;

	UPROPERTY(EditAnywhere, Category=SavedManifoldPoint)
	TArray<FVector> ShapeContactPoints;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDManifoldPoint> : public TStructOpsTypeTraitsBase2<FChaosVDManifoldPoint>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDManifoldPoint& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDConstraint
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDConstraint");

	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bDisabled:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bUseManifold:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bUseIncrementalManifold:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bCanRestoreManifold:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bWasManifoldRestored:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsQuadratic0:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsQuadratic1:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsProbeUnmodified:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsProbe:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bCCDEnabled:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bCCDSweepEnabled:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bModifierApplied:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bMaterialSet:1 = false;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	FVector AccumulatedImpulse;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	EChaosVDContactShapesType ShapesType;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	TArray<FTransform> ShapeWorldTransforms;

	UPROPERTY(EditAnywhere, Category=Collision)
	TArray<FTransform> ImplicitTransforms;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float CullDistance;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	TArray<float> CollisionMargins;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float CollisionTolerance;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	int32 ClosestManifoldPointIndex;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	int32 ExpectedNumManifoldPoints;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	FVector LastShapeWorldPositionDelta;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	FQuat LastShapeWorldRotationDelta;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float Stiffness;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float CCDTimeOfImpact;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float CCDEnablePenetration;
	
	UPROPERTY(EditAnywhere, Category=Collision)
	float CCDTargetPenetration;

	UPROPERTY(EditAnywhere, Category=Collision)
	TArray<FChaosVDManifoldPoint> ManifoldPoints;

	UPROPERTY(VisibleAnywhere, Category=Collision)
	int32 Particle0Index = INDEX_NONE;
	UPROPERTY(VisibleAnywhere, Category=Collision)
	int32 Particle1Index = INDEX_NONE;

	int32 SolverID = INDEX_NONE;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDConstraint> : public TStructOpsTypeTraitsBase2<FChaosVDConstraint>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDConstraint& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDParticlePairMidPhase
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDParticlePairMidPhase");

	UPROPERTY(VisibleAnywhere, Category=Solver)
	int32 SolverID = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsActive:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsCCD:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsCCDActive:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsSleeping:1 = false;
	UPROPERTY(EditAnywhere, Category=Falgs)
	uint8 bIsModified:1 = false;

	UPROPERTY(EditAnywhere, Category=Misc)
	int32 LastUsedEpoch = 0;

	UPROPERTY(EditAnywhere, Category=Particle)
	int32 Particle0Idx = 0;
	UPROPERTY(EditAnywhere, Category=Particle)
	int32 Particle1Idx = 0;

	UPROPERTY(EditAnywhere, Category=Particle)
	TArray<FChaosVDConstraint> Constraints;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDParticlePairMidPhase> : public TStructOpsTypeTraitsBase2<FChaosVDParticlePairMidPhase>
{
	enum
	{
		WithSerializer = true,
	};
};


inline FArchive& operator<<(FArchive& Ar, FChaosVDParticlePairMidPhase& Data)
{
	Data.Serialize(Ar);
	return Ar;
}
