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
	UPROPERTY(VisibleAnywhere, Category=Contact)
	FVector ShapeContactPoints[2] = { FVector(ForceInit), FVector(ForceInit) };

	// Shape-space contact normal on the second shape with direction that points away from shape 1
	UPROPERTY(VisibleAnywhere, Category=Contact)
	FVector  ShapeContactNormal = FVector(ForceInit);

	// Contact separation (negative for overlap)
	UPROPERTY(VisibleAnywhere, Category=Contact)
	float Phi = 0.f;

	// Face index of the shape we hit. Only valid for Heightfield and Trimesh contact points, otherwise INDEX_NONE
	UPROPERTY(VisibleAnywhere, Category=Contact)
	int32 FaceIndex = 0;

	// Whether this is a vertex-plane contact, edge-edge contact etc.
	UPROPERTY(VisibleAnywhere, Category=Contact)
	EChaosVDContactPointType ContactType = EChaosVDContactPointType::Unknown;

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

UENUM()
enum class EChaosVDManifoldPointFlags : uint8
{
	None = 0,
	Disabled = 1 << 0,
	WasRestored = 1 << 1,
	WasReplaced = 1 << 2,
	HasStaticFrictionAnchor = 1 << 3,
	IsValid = 1 << 4,
	InsideStaticFrictionCone = 1 << 5,
};
ENUM_CLASS_FLAGS(EChaosVDManifoldPointFlags)

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDManifoldPoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=ContactData)
	uint8 bDisabled:1 = false;
	UPROPERTY()
	uint8 bWasRestored:1 = false;
	UPROPERTY()
	uint8 bWasReplaced:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ContactData)
	uint8 bHasStaticFrictionAnchor:1 = false;

	UPROPERTY()
	uint8 bIsValid:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ContactData)
	uint8 bInsideStaticFrictionCone:1 = false;

	UPROPERTY(VisibleAnywhere, Category=ContactData)
	FVector NetPushOut = FVector(ForceInit);
	UPROPERTY(VisibleAnywhere, Category=ContactData)
	FVector NetImpulse = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category=ContactData)
	float TargetPhi = 0.f;
	UPROPERTY(VisibleAnywhere, Category=ContactData)
	float InitialPhi = 0.f;
	UPROPERTY()
	FVector ShapeAnchorPoints[2] = { FVector(ForceInit), FVector(ForceInit) };
	UPROPERTY()
	FVector InitialShapeContactPoints[2] = { FVector(ForceInit), FVector(ForceInit) };
	UPROPERTY(VisibleAnywhere, Category=ContactData)
	FChaosVDContactPoint ContactPoint;

	UPROPERTY()
	FVector ShapeContactPoints[2] =  { FVector(ForceInit), FVector(ForceInit) };

	bool bIsSelectedInEditor = false;

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
struct FChaosVDCollisionMaterial
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	int32 FaceIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float MaterialDynamicFriction = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float MaterialStaticFriction = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float MaterialRestitution = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float DynamicFriction = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float StaticFriction = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float Restitution = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float RestitutionThreshold = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float InvMassScale0 = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float InvMassScale1 = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float InvInertiaScale0 = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category = MaterialData)
	float InvInertiaScale1 = 0.0f;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDCollisionMaterial> : public TStructOpsTypeTraitsBase2<FChaosVDCollisionMaterial>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDCollisionMaterial& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

UENUM()
enum class EChaosVDConstraintFlags : uint16
{
	None = 0,
	IsCurrent = 1 << 0,
	Disabled = 1 << 1,
	UseManifold = 1 << 2,
	UseIncrementalManifold = 1 << 3,
	CanRestoreManifold = 1 << 4,
	WasManifoldRestored = 1 << 5,
	IsQuadratic0 = 1 << 6,
	IsQuadratic1 = 1 << 7,
	IsProbe = 1 << 8,
	CCDEnabled = 1 << 9,
	CCDSweepEnabled = 1 << 10,
	ModifierApplied = 1 << 11,
	MaterialSet = 1 << 12,
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDConstraint
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDConstraint");

	UPROPERTY()
	uint8 bIsCurrent:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bDisabled:1 = false;
	UPROPERTY()
	uint8 bUseManifold:1 = false;
	UPROPERTY()
	uint8 bUseIncrementalManifold:1 = false;
	UPROPERTY()
	uint8 bCanRestoreManifold:1 = false;
	UPROPERTY()
	uint8 bWasManifoldRestored:1 = false;
	UPROPERTY()
	uint8 bIsQuadratic0:1 = false;
	UPROPERTY()
	uint8 bIsQuadratic1:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bIsProbe:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bCCDEnabled:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bCCDSweepEnabled:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bModifierApplied:1 = false;
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	uint8 bMaterialSet:1 = false;
	
	UPROPERTY(VisibleAnywhere, Category = ConstraintData)
	FChaosVDCollisionMaterial Material;

	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	FVector AccumulatedImpulse = FVector(ForceInit);
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	EChaosVDContactShapesType ShapesType = EChaosVDContactShapesType::Unknown;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	FTransform ShapeWorldTransforms[2] = { FTransform::Identity, FTransform::Identity };

	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	FTransform ImplicitTransforms[2] = { FTransform::Identity, FTransform::Identity };
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float CullDistance = 0.f;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	TArray<float> CollisionMargins;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float CollisionTolerance = 0.f;
	
	UPROPERTY()
	int32 ClosestManifoldPointIndex = 0;
	
	UPROPERTY()
	int32 ExpectedNumManifoldPoints = 0;
	
	UPROPERTY()
	FVector LastShapeWorldPositionDelta = FVector(ForceInit);
	
	UPROPERTY()
	FQuat LastShapeWorldRotationDelta = FQuat(ForceInit);
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float Stiffness = 0.f;

	UPROPERTY(VisibleAnywhere, Category = ConstraintData)
	float MinInitialPhi = 0.f;

	UPROPERTY(VisibleAnywhere, Category = ConstraintData)
	float InitialOverlapDepenetrationVelocity = -1.f;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float CCDTimeOfImpact = 0.f;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float CCDEnablePenetration = 0.f;
	
	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	float CCDTargetPenetration = 0.f;

	UPROPERTY(VisibleAnywhere, Category=ConstraintData)
	TArray<FChaosVDManifoldPoint> ManifoldPoints;

	UPROPERTY()
	int32 Particle0Index = INDEX_NONE;
	UPROPERTY()
	int32 Particle1Index = INDEX_NONE;

	UPROPERTY()
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

UENUM()
enum class EChaosVDMidPhaseFlags : uint8
{
	None = 0,
	IsActive = 1 << 0,
	IsCCD = 1 << 1,
	IsCCDActive = 1 << 2,
	IsSleeping = 1 << 3,
	IsModified = 1 << 4,
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDParticlePairMidPhase
{
	GENERATED_BODY()

	inline static FStringView WrapperTypeName = TEXT("FChaosVDParticlePairMidPhase");

	UPROPERTY()
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category=Flags)
	uint8 bIsActive:1 = false;
	UPROPERTY(VisibleAnywhere, Category=Flags)
	uint8 bIsCCD:1 = false;
	UPROPERTY(VisibleAnywhere, Category=Flags)
	uint8 bIsCCDActive:1 = false;
	UPROPERTY(VisibleAnywhere, Category=Flags)
	uint8 bIsSleeping:1 = false;
	UPROPERTY(VisibleAnywhere, Category=Flags)
	uint8 bIsModified:1 = false;

	UPROPERTY(VisibleAnywhere, Category=Misc)
	int32 LastUsedEpoch = 0;

	UPROPERTY(VisibleAnywhere, Category=Particle)
	int32 Particle0Idx = 0;
	UPROPERTY(VisibleAnywhere, Category=Particle)
	int32 Particle1Idx = 0;

	UPROPERTY()
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

UENUM()
enum class EChaosVDCollisionTraceFlag
{
	/** Use project physics settings (DefaultShapeComplexity) */
	UseDefault,
	/** Create both simple and complex shapes. Simple shapes are used for regular scene queries and collision tests. Complex shape (per poly) is used for complex scene queries.*/
	UseSimpleAndComplex,
	/** Create only simple shapes. Use simple shapes for all scene queries and collision tests.*/
	UseSimpleAsComplex,
	/** Create only complex shapes (per poly). Use complex shapes for all scene queries and collision tests. Can be used in simulation for static shapes only (i.e can be collided against but not moved through forces or velocity.) */
	UseComplexAsSimple,
	/** */
	MAX,
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDCollisionFilterData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint32 Word0 = 0;
	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint32 Word1 = 0;
	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint32 Word2 = 0;
	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint32 Word3 = 0;

	bool Serialize(FArchive& Ar);
};

template<>
struct TStructOpsTypeTraits<FChaosVDCollisionFilterData> : public TStructOpsTypeTraitsBase2<FChaosVDCollisionFilterData>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDCollisionFilterData& Data)
{
	Data.Serialize(Ar);
	return Ar;
}
UENUM()
enum class EChaosVDCollisionShapeDataFlags : uint8
{
	None = 0,
	SimCollision = 1 << 0,
	QueryCollision = 1 << 1,
	IsProbe = 1 << 2,
};

USTRUCT()
struct CHAOSVDRUNTIME_API FChaosVDShapeCollisionData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	EChaosVDCollisionTraceFlag CollisionTraceType = EChaosVDCollisionTraceFlag::UseDefault;

	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint8 bSimCollision : 1 = false;
	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint8 bQueryCollision : 1 = false;
	UPROPERTY(VisibleAnywhere, Category=CollisionData)
	uint8 bIsProbe : 1 = false;

	UPROPERTY(VisibleAnywhere, Category=FilterData)
	FChaosVDCollisionFilterData QueryData;

	UPROPERTY(VisibleAnywhere, Category=SimData)
	FChaosVDCollisionFilterData SimData;

	bool bIsComplex = false;

	bool bIsValid = false;

	bool Serialize(FArchive& Ar);

	bool operator==(const FChaosVDShapeCollisionData& Other) const;
};

template<>
struct TStructOpsTypeTraits<FChaosVDShapeCollisionData> : public TStructOpsTypeTraitsBase2<FChaosVDShapeCollisionData>
{
	enum
	{
		WithSerializer = true,
	};
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDShapeCollisionData& Data)
{
	Data.Serialize(Ar);
	return Ar;
}
