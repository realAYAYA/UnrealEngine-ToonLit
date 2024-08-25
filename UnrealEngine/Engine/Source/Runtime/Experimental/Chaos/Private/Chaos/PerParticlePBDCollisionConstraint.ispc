// Copyright Epic Games, Inc. All Rights Reserved.

#define EXPLICIT_VECTOR4 1

#include "Math/Vector.isph"
#include "Math/Transform.isph"
#include "Chaos/PBDSofts.isph"

enum ImplicitObjectType
{
	//Note: add entries in order to avoid serialization issues (but before IsInstanced)
	Sphere = 0,
	Box,
	Plane,
	Capsule,
	Transformed,
	Union,
	LevelSet,
	Unknown,
	Convex,
	TaperedCylinder,
	Cylinder,
	TriangleMesh,
	HeightField,
	DEPRECATED_Scaled,	//needed for serialization of existing data
	Triangle,
	UnionClustered,
	TaperedCapsule,
	WeightedLatticeBone,
	WeightedLatticeLevelSetType = 38, // == LevelSet | IsWeightedLattice (cannot use expressions like this in ISPC)

	//Add entries above this line for serialization
	IsWeightedLattice = 1 << 5,
	IsInstanced = 1 << 6,
	IsScaled = 1 << 7
};

struct SerializedPtr
{
	uint8 *Ptr;
};

struct UniquePtr
{
	SerializedPtr *Data;
};

struct TArray
{
	uint8 *Data;
	uint32 ArrayNum;
	uint32 ArrayMax;
};

struct PlanesS
{
	unsigned int8 FirstHalfEdgeIndex;
	unsigned int8 NumHalfEdges;
};

struct PlanesM
{
	int16 FirstHalfEdgeIndex;
	int16 NumHalfEdges;
};

struct PlanesL
{
	int32 FirstHalfEdgeIndex;
	int32 NumHalfEdges;
};

struct HalfEdgesS
{
	unsigned int8 PlaneIndex;
	unsigned int8 VertexIndex;
	unsigned int8 TwinHalfEdgeIndex;
};

struct HalfEdgesM
{
	int16 PlaneIndex;
	int16 VertexIndex;
	int16 TwinHalfEdgeIndex;
};

struct HalfEdgesL
{
	int32 PlaneIndex;
	int32 VertexIndex;
	int32 TwinHalfEdgeIndex;
};

struct VerticesS
{
	unsigned int8 FirstHalfEdgeIndex;
};

struct VerticesM
{
	int16 FirstHalfEdgeIndex;
};

struct VerticesL
{
	int32 FirstHalfEdgeIndex;
};

struct Segment
{
	FVector Point;
	FVector Axis;
	FReal Length;
};

struct FPlaneConcrete3d
{
	FVector3d MX;
	FVector3d MNormal;
};

struct FPlaneConcrete3f
{
	FVector3f MX;
	FVector3f MNormal;
};

struct FConvexStructureDataImp
{
	TArray Planes;
	TArray HalfEdges;
	TArray Vertices;
	TArray Edges;
	// TArray VertexPlanes; // this code never uses this, and we never assume to know the sizeof(FConvexStructureDataImp)
};

enum FConvexStructureDataIndexType
{
	EIndexType_None = 0,
	EIndexType_Small,
	EIndexType_Medium,
	EIndexType_Large
};

struct FConvexStructureData
{
	FConvexStructureDataImp *Data;
	int8 IndexType;
};

struct FConvex
{
	TArray Planes;
	TArray Vertices;
	FVector3f LocalBoundingBoxMin;
	FVector3f LocalBoundingBoxMax;
	FConvexStructureData StructureData;
	// FReal Volume; // this code never uses this, and we never assume to know the sizeof(FConvex)
	// FVector CenterOfMass; // this code never uses this, and we never assume to know the sizeof(FConvex)
};

struct FTaperedCapsule
{
	FVector Origin;
	FVector Axis;
	FReal Height;
	FReal Radius1;
	FReal Radius2;
	// FAABB3 LocalBoundingBox; // this code never uses this, and we never assume to know the sizeof(FTaperedCapsule)
};

struct FTaperedCylinder
{
	FPlaneConcrete3d MPlane1;
	FPlaneConcrete3d MPlane2;
	FReal Height;
	FReal Radius1;
	FReal Radius2;
	// FAABB3 MLocalBoundingBox; // this code never uses this, and we never assume to know the sizeof(FTaperedCylinder)
};

// We want to export all of the structs above, so defining exported method that uses them.
export uniform int32 ExportHack_PerParticlePBDCollisionConstraintStructs(
	uniform ImplicitObjectType Type,
	uniform TArray* uniform Array,
	uniform PlanesS* uniform _PlanesS,
	uniform PlanesM* uniform _PlanesM,
	uniform PlanesL* uniform _PlanesL,
	uniform HalfEdgesS* uniform _HalfEdgesS,
	uniform HalfEdgesM* uniform _HalfEdgesM,
	uniform HalfEdgesL* uniform _HalfEdgesL,
	uniform VerticesS* uniform _VerticesS,
	uniform VerticesM* uniform _VerticesM,
	uniform VerticesL* uniform _VerticesL,
	uniform Segment* uniform _Segment,
	uniform FPlaneConcrete3d* uniform _Plane3d,
	uniform FPlaneConcrete3f* uniform _Plane3f,
	uniform FConvexStructureDataIndexType IndexType,
	uniform FConvex* uniform _FConvex,
	uniform FTaperedCapsule* uniform _TaperedCapsule,
	uniform FTaperedCylinder* uniform _TaperedCylinder
)
{
	return 0;
}

static inline FReal SafeNormalize(FVector &Direction)
{
	const FReal SizeSqr = VectorSizeSquared(Direction);
	const FReal Size = sqrt(SizeSqr);
	Direction = VectorSelect((SizeSqr < KINDA_SMALL_NUMBER), ForwardVector, Direction / Size);
	return (Size < KINDA_SMALL_NUMBER) ? ZERO : Size;
}

static inline bool VectorNormalize(FVector &V)
{
	const FReal SquareSum = VectorSizeSquared(V);
	if(SquareSum > SMALL_NUMBER)
	{
		const FReal Scale = InvSqrt(SquareSum);
		V = V * Scale;
		return true;
	}
	return false;
}

static inline void PhiWithNormalSphere(const uniform uint8 *uniform TypedObjectPtr, const FVector &PhiWithNormalInput, FVector &Normal, FReal &Phi, const uniform FReal Radius)
{
	const uniform FVector Center = *((const uniform FVector *uniform)&TypedObjectPtr[0]);
	Normal = PhiWithNormalInput - Center;
	Phi = SafeNormalize(Normal) - Radius;
}

static inline void PhiWithNormalPlane(const uniform FPlaneConcrete3f *varying InPlane, const FVector3f &PhiWithNormalInput, FVector3f &Normal, float &Phi)
{
	const varying FVector3f MNormal = VectorGather(&InPlane->MNormal);
	const varying FVector3f MX = VectorGather(&InPlane->MX);

	Normal = MNormal;
	Phi = VectorDot(PhiWithNormalInput - MX, MNormal);
}

static inline float SignedDistancePlane(const uniform FPlaneConcrete3f *uniform InPlane, const FVector3f &x)
{
	const uniform FPlaneConcrete3f P = *InPlane;
	return VectorDot(x - P.MX, P.MNormal);
}

static inline void PhiWithNormalCapsule(const uniform uint8 *uniform TypedObjectPtr, const FVector &PhiWithNormalInput, FVector &Normal, FReal &Phi, const uniform FReal Radius)
{
	const uniform Segment MSegment = *((const uniform Segment *uniform)&TypedObjectPtr[0]);

	const FReal Dot = clamp(VectorDot(PhiWithNormalInput - MSegment.Point, MSegment.Axis), (uniform FReal)(0), MSegment.Length);
	const FVector ProjectedPoint = SetVector(Dot) * MSegment.Axis + MSegment.Point;
	Normal = PhiWithNormalInput - ProjectedPoint;
	Phi = SafeNormalize(Normal) - Radius;
}

static inline void PhiWithNormalTaperedCapsule(const uniform uint8* uniform TypedObjectPtr, const FVector& PhiWithNormalInput, FVector& Normal, FReal& Phi)
{
	const uniform FTaperedCapsule CapsuleObj = *((const uniform FTaperedCapsule *uniform)&TypedObjectPtr[0]);

	const FReal DistanceAlongAxis = clamp(VectorDot(PhiWithNormalInput - CapsuleObj.Origin, CapsuleObj.Axis), (uniform FReal)(0), CapsuleObj.Height);
	const FVector ProjectedPoint = SetVector(DistanceAlongAxis) * CapsuleObj.Axis + CapsuleObj.Origin;
	const FReal Alpha = DistanceAlongAxis / CapsuleObj.Height;
	const FReal Radius = CapsuleObj.Radius1 * ((uniform FReal)(1.0) - Alpha) + CapsuleObj.Radius2 * Alpha;
	Normal = PhiWithNormalInput - ProjectedPoint;
	Phi = SafeNormalize(Normal) - Radius;
}

static inline void PhiWithNormalTaperedCylinder(const uniform uint8 *uniform TypedObjectPtr, const FVector &PhiWithNormalInput, FVector &Normal, FReal &Phi, const uniform int SizeofFImplicitObject)
{
	const uniform FTaperedCylinder CylinderObj = *((const uniform FTaperedCylinder *uniform)&TypedObjectPtr[0]);

	const FReal Distance1 = VectorDot(PhiWithNormalInput - CylinderObj.MPlane1.MX, CylinderObj.MPlane1.MNormal);
	// Used to be Distance2 = CylinderObj.MPlane2.PhiWithNormal(x, Normal2); but that would trigger 
	const FReal Distance2 = CylinderObj.Height - Distance1;          // the ensure on Distance2 being slightly larger than MHeight in some border cases
	const FVector SideVector = (PhiWithNormalInput - (CylinderObj.MPlane1.MNormal * Distance1 + CylinderObj.MPlane1.MX));
	const FReal Alpha = Distance1 / CylinderObj.Height;
	const FReal Radius = CylinderObj.Radius1 * ((uniform FReal)(1.0) - Alpha) + CylinderObj.Radius2 * Alpha;
	const FReal SideDistance = VectorSize(SideVector) - Radius;
	const FReal TopDistance = Distance1 < Distance2 ? Distance1 : Distance2;

	if (Distance1 < SMALL_NUMBER)
	{
		const FVector v = PhiWithNormalInput - (CylinderObj.MPlane1.MNormal * Distance1 + CylinderObj.MPlane1.MX);
		if (VectorSize(v) > CylinderObj.Radius1)
		{
			const FVector Corner = VectorGetSafeNormal(v) * CylinderObj.Radius1 + CylinderObj.MPlane1.MX;
			const FVector CornerVector = PhiWithNormalInput - Corner;
			Normal = VectorGetSafeNormal(CornerVector);
			Phi = VectorSize(CornerVector);
		}
		else
		{
			Normal = CylinderObj.MPlane1.MNormal * (uniform FReal)(-1.0);
			Phi = -Distance1;
		}
	}
	else if (Distance2 < SMALL_NUMBER)
	{
		const FVector v = PhiWithNormalInput - (CylinderObj.MPlane2.MNormal * Distance2 + CylinderObj.MPlane2.MX);
		if (VectorSize(v) > CylinderObj.Radius2)
		{
			const FVector Corner = VectorGetSafeNormal(v) * CylinderObj.Radius2 + CylinderObj.MPlane2.MX;
			const FVector CornerVector = PhiWithNormalInput - Corner;
			Normal = VectorGetSafeNormal(CornerVector);
			Phi = VectorSize(CornerVector);
		}
		else
		{
			Normal = CylinderObj.MPlane2.MNormal * (uniform FReal)(-1.0);
			Phi = -Distance2;
		}
	}
	else if (SideDistance < (uniform FReal)(0.0) && TopDistance < -SideDistance)
	{
		Normal = Distance1 < Distance2 ? CylinderObj.MPlane1.MNormal * (uniform FReal)(-1.0) : CylinderObj.MPlane2.MNormal * (uniform FReal)(-1.0);
		Phi = -TopDistance;
	}
	else
	{
		Normal = VectorGetSafeNormal(SideVector);
		Phi = SideDistance;
	}
}

static inline void PhiWithNormalUnion(const uniform uint8 *uniform TypedPtr, const FVector &PhiWithNormalInput, FVector &Normal, FReal &Phi, const uniform int SizeofFImplicitObject, const uniform int OffsetofGeometryType, const uniform int OffsetOfMargin)
{
	// This is expecting a MObjects = [TaperedCylinder, Sphere, Sphere]
	assert( ((const uniform TArray *uniform)TypedPtr)->ArrayNum == 3 );

	const uniform UniquePtr *uniform MObjects = (const uniform UniquePtr *uniform)TypedPtr;
	Phi = FLT_MAX;
	bool NeedsNormalize = false;
	
	uniform uint8 *uniform Object = MObjects->Data[0].Ptr;
	uniform uint8 *uniform TypedObjectPtr = Object + SizeofFImplicitObject;

	FVector NextNormal;
	FReal NextPhi;

	assert( (Object[OffsetofGeometryType] & 0x3F) == TaperedCylinder );
	PhiWithNormalTaperedCylinder(TypedObjectPtr, PhiWithNormalInput, NextNormal, NextPhi, SizeofFImplicitObject);

	if (NextPhi < Phi)
	{
		Phi = NextPhi;
		Normal = NextNormal;
		NeedsNormalize = false;
	}
	else if (NextPhi == Phi)
	{
		Normal = Normal + NextNormal;
		NeedsNormalize = true;
	}

	Object = MObjects->Data[1].Ptr;
	uniform FReal Margin = *((const uniform FReal *uniform)(Object + OffsetOfMargin));
	TypedObjectPtr = Object + SizeofFImplicitObject;

	assert( (Object[OffsetofGeometryType] & 0x3F) == Sphere );
	PhiWithNormalSphere(TypedObjectPtr, PhiWithNormalInput, NextNormal, NextPhi, Margin);

	if (NextPhi < Phi)
	{
		Phi = NextPhi;
		Normal = NextNormal;
		NeedsNormalize = false;
	}
	else if (NextPhi == Phi)
	{
		Normal = Normal + NextNormal;
		NeedsNormalize = true;
	}

	Object = MObjects->Data[2].Ptr;
	Margin = *((const uniform FReal *uniform)(Object + OffsetOfMargin));
	TypedObjectPtr = Object + SizeofFImplicitObject;

	assert( (Object[OffsetofGeometryType] & 0x3F) == Sphere );
	PhiWithNormalSphere(TypedObjectPtr, PhiWithNormalInput, NextNormal, NextPhi, Margin);

	if (NextPhi < Phi)
	{
		Phi = NextPhi;
		Normal = NextNormal;
		NeedsNormalize = false;
	}
	else if (NextPhi == Phi)
	{
		Normal = Normal + NextNormal;
		NeedsNormalize = true;
	}

	if(NeedsNormalize)
	{
		VectorNormalize(Normal);
	}
}

static inline FVector3f FindClosestPointOnLineSegment(const FVector3f& P0, const FVector3f& P1, const FVector3f& P)
{
	const FVector3f P10 = P1 - P0;
	const FVector3f PP0 = P - P0;
	const float Proj = VectorDot(P10, PP0);
	const float Denom2 = VectorSizeSquared(P10);
	const float NormalProj = Proj / Denom2;
	const FVector3f P2 = P0 + NormalProj * P10;
	return VectorSelect(Proj < (uniform float)(0.0f) || Denom2 < KINDA_SMALL_NUMBER, P0, VectorSelect(NormalProj > (uniform float)(1.0f), P1, P2));
}

static inline FVector3f FindClosestPointOnTriangle(const FVector3f& ClosestPointOnPlane, const FVector3f& P0, const FVector3f& P1, const FVector3f& P2, const FVector3f& P)
{
	// ComputeBarycentricInPlane
	const FVector3f P10 = P1 - P0;
	const FVector3f P20 = P2 - P0;
	const FVector3f PP0 = P - P0;
	const float Size10 = VectorSizeSquared(P10);
	const float Size20 = VectorSizeSquared(P20);
	const float ProjSides = VectorDot(P10, P20);
	const float ProjP1 = VectorDot(PP0, P10);
	const float ProjP2 = VectorDot(PP0, P20);
	const float Denom = Size10 * Size20 - ProjSides * ProjSides;
	const float BaryX = (Size20 * ProjP1 - ProjSides * ProjP2) / Denom;
	const float BaryY = (Size10 * ProjP2 - ProjSides * ProjP1) / Denom;

	const bool bClosestPoint = (BaryX >= -FLOAT_KINDA_SMALL_NUMBER && BaryX <= 1 + FLOAT_KINDA_SMALL_NUMBER && BaryY >= -FLOAT_KINDA_SMALL_NUMBER && BaryY <= 1 + FLOAT_KINDA_SMALL_NUMBER && (BaryX + BaryY) <= (1 + FLOAT_KINDA_SMALL_NUMBER));

	const FVector3f P10Closest = FindClosestPointOnLineSegment(P0, P1, P);
	const FVector3f P20Closest = FindClosestPointOnLineSegment(P0, P2, P);
	const FVector3f P21Closest = FindClosestPointOnLineSegment(P1, P2, P);

	const float P10Dist2 = VectorSizeSquared(P - P10Closest);
	const float P20Dist2 = VectorSizeSquared(P - P20Closest);
	const float P21Dist2 = VectorSizeSquared(P - P21Closest);

	return VectorSelect(bClosestPoint, ClosestPointOnPlane, VectorSelect(P10Dist2 < P20Dist2, VectorSelect(P10Dist2 < P21Dist2, P10Closest, P21Closest), VectorSelect(P20Dist2 < P21Dist2, P20Closest, P21Closest)));
}

static inline int32 NumPlaneVertices(const uniform FConvexStructureData *uniform StructureData, const varying int32 PlaneIndex)
{
	const uniform int8 IndexType = StructureData->IndexType;

	if(IndexType == EIndexType_Small)
	{
		const uniform PlanesS *uniform Planes = (const uniform PlanesS *uniform)StructureData->Data->Planes.Data;

		#pragma ignore warning(perf)
		return (int32)Planes[PlaneIndex].NumHalfEdges;
	}
	else if(IndexType == EIndexType_Medium)
	{
		const uniform PlanesM *uniform Planes = (const uniform PlanesM *uniform)StructureData->Data->Planes.Data;

		#pragma ignore warning(perf)
		return (int32)Planes[PlaneIndex].NumHalfEdges;
	}
	else if(IndexType == EIndexType_Large)
	{
		const uniform PlanesL *uniform Planes = (const uniform PlanesL *uniform)StructureData->Data->Planes.Data;

		#pragma ignore warning(perf)
		return Planes[PlaneIndex].NumHalfEdges;
	}
	else
	{
		return 0;
	}
}

static inline int32 GetPlaneVertex(const uniform FConvexStructureData *uniform StructureData, const varying int32 PlaneIndex, const uniform int32 PlaneEdgeIndex)
{
	const uniform int8 IndexType = StructureData->IndexType;

	if(IndexType == EIndexType_Small)
	{
		const uniform PlanesS *uniform Planes = (const uniform PlanesS *uniform)StructureData->Data->Planes.Data;
		const uniform HalfEdgesS *uniform HalfEdges = (const uniform HalfEdgesS *uniform)StructureData->Data->HalfEdges.Data;

		#pragma ignore warning(perf)
		const int32 HalfEdgeIndex = (int32)Planes[PlaneIndex].FirstHalfEdgeIndex + PlaneEdgeIndex;

		#pragma ignore warning(perf)
		return (int32)HalfEdges[HalfEdgeIndex].VertexIndex;
	}
	else if(IndexType == EIndexType_Medium)
	{
		const uniform PlanesM *uniform Planes = (const uniform PlanesM *uniform)StructureData->Data->Planes.Data;
		const uniform HalfEdgesM *uniform HalfEdges = (const uniform HalfEdgesM *uniform)StructureData->Data->HalfEdges.Data;

		#pragma ignore warning(perf)
		const int32 HalfEdgeIndex = (int32)Planes[PlaneIndex].FirstHalfEdgeIndex + PlaneEdgeIndex;

		#pragma ignore warning(perf)
		return (int32)HalfEdges[HalfEdgeIndex].VertexIndex;
	}
	else if(IndexType == EIndexType_Large)
	{
		const uniform PlanesL *uniform Planes = (const uniform PlanesL *uniform)StructureData->Data->Planes.Data;
		const uniform HalfEdgesL *uniform HalfEdges = (const uniform HalfEdgesL *uniform)StructureData->Data->HalfEdges.Data;

		#pragma ignore warning(perf)
		const int32 HalfEdgeIndex = Planes[PlaneIndex].FirstHalfEdgeIndex + PlaneEdgeIndex;

		#pragma ignore warning(perf)
		return HalfEdges[HalfEdgeIndex].VertexIndex;
	}
	else
	{
		return 0;
	}
}

static inline void PhiWithNormalConvex(const uniform uint8 *uniform TypedObjectPtr, const FVector3f &PhiWithNormalInput, FVector3f &Normal, float &Phi)
{
	const uniform FConvex *uniform Object = (const uniform FConvex *uniform)TypedObjectPtr;
	const uniform FPlaneConcrete3f *uniform Planes = (const uniform FPlaneConcrete3f *uniform)&Object->Planes.Data[0];
	const uniform int32 NumPlanes = Object->Planes.ArrayNum;
	if (NumPlanes == 0)
	{
		Phi = FLT_MAX;
		return;
	}

	float MaxPhi = -FLT_MAX;
	int32 MaxPlane = 0;

	for (uniform int32 Idx = 0; Idx < NumPlanes; ++Idx)
	{
		const float InnerPhi = SignedDistancePlane(&Planes[Idx], PhiWithNormalInput);
		if (InnerPhi > MaxPhi)
		{
			MaxPhi = InnerPhi;
			MaxPlane = Idx;
		}
	}

	PhiWithNormalPlane(&Planes[MaxPlane], PhiWithNormalInput, Normal, Phi);
	if (Phi <= 0)
	{
		return;
	}

	// If x is outside the convex mesh, we should find for the nearest point to triangles on the plane
	const int32 PlaneVerticesNum = NumPlaneVertices(&Object->StructureData, MaxPlane);
	const FVector3f XOnPlane = PhiWithNormalInput - Phi * Normal;

	if(PlaneVerticesNum > 2 && IsVectorEqual(XOnPlane, PhiWithNormalInput))
	{
		return;
	}

	float ClosestDistance = FLT_MAX;
	FVector3f ClosestPoint = FloatZeroVector;
	for (uniform int32 Index = 0; Index < PlaneVerticesNum - 2; Index++)
	{
		const uniform FVector3f *uniform Vertices = (const uniform FVector3f *uniform)Object->Vertices.Data;
		const uniform FVector3f *varying APtr = &Vertices[GetPlaneVertex(&Object->StructureData, MaxPlane, 0)];
		const uniform FVector3f *varying BPtr = &Vertices[GetPlaneVertex(&Object->StructureData, MaxPlane, Index + 1)];
		const uniform FVector3f *varying CPtr = &Vertices[GetPlaneVertex(&Object->StructureData, MaxPlane, Index + 2)];

		const FVector3f A = VectorGather(APtr);
		const FVector3f B = VectorGather(BPtr);
		const FVector3f C = VectorGather(CPtr);

		const FVector3f Point = FindClosestPointOnTriangle(XOnPlane, A, B, C, PhiWithNormalInput);

		const float Distance = VectorSize(Point - XOnPlane);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			ClosestPoint = Point;
		}
	}

	const FVector3f Difference = PhiWithNormalInput - ClosestPoint;
	Phi = VectorSize(Difference);
	if (Phi > FLOAT_SMALL_NUMBER)
	{
		Normal = (Difference) / Phi;
	}
}

extern "C" unmasked void GetPhiWithNormal(const uniform uint8 *uniform CollisionParticles, const uniform float *uniform V, uniform float *uniform Normal, uniform float *uniform Phi, const uniform int i, const uniform int ProgramCount, const uniform int Mask);
extern "C" unmasked void GetPhiWithNormalAndVelocityBone(const uniform uint8 *uniform CollisionParticles, const uniform float *uniform V, uniform float *uniform Normal, uniform float *uniform Phi, uniform int *uniform VelocityBone, const uniform int i, const uniform float Thickness, const uniform int ProgramCount, const uniform int Mask);
extern "C" unmasked void GetPhiWithNormalCollisionParticleRange(const uniform uint8 *uniform CollisionParticlesRange, const uniform float *uniform V, uniform float *uniform Normal, uniform float *uniform Phi, const uniform int i, const uniform int ProgramCount, const uniform int Mask);
extern "C" unmasked void GetPhiWithNormalAndVelocityBoneCollisionParticleRange(const uniform uint8 *uniform CollisionParticlesRange, const uniform float *uniform V, uniform float *uniform Normal, uniform float *uniform Phi, uniform int *uniform VelocityBone, const uniform int i, const uniform float Thickness, const uniform int ProgramCount, const uniform int Mask);

export void ApplyPerParticleCollisionFastFriction(uniform FVector4f ParticlesPandInvM[],
													const uniform FVector3f ParticlesX[],
													const uniform FVector3f CollisionV[],
													const uniform FVector3f CollisionX[],
													const uniform FVector3f CollisionW[],
													const uniform FVector4f CollisionR[],
													const uniform uint32 DynamicGroupId,
													const uniform uint32 KinematicGroupIds[],
													const uniform float PerGroupFriction,
													const uniform float PerGroupThickness,
													const uniform uint8 *uniform CollisionParticles,
													const uniform uint8 *uniform Geometry,
													const uniform int SizeofFImplicitObject,
													const uniform int OffsetofGeometryType,
													const uniform int OffsetOfMargin,
													const uniform float Dt,
													const uniform int InnerOffset,
													const uniform int InnerRange,
													const uniform int OuterOffset,
													const uniform int OuterRange)
{
	uniform int BailAt = OuterRange;
	uniform bool bBail = false;

	// Pre-check for bail out.
	foreach(Index = OuterOffset ... BailAt)
	{
		for(uniform int i = InnerOffset; i < InnerRange; i++)
		{
			const uniform uint32 KinematicGroupId = KinematicGroupIds[i];  // Collision group Id

			if (KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId)
			{
				foreach_active(j)
				{
					if(!bBail)
					{
						BailAt = extract(Index, j);
						bBail = true;
					}
				}
			}
		}
	}

	foreach(Index = OuterOffset ... BailAt)
	{
		const FVector4f PandInvM = VectorLoad(&ParticlesPandInvM[extract(Index, 0)]);
		varying FVector3f P;
		varying float IM;
		UnzipPandInvM(PandInvM, P, IM);

		if(all(IM == 0))
		{
			continue;
		}

		for(uniform int i = InnerOffset; i < InnerRange; i++) // SequentialFor
		{
			// InverseTransformPosition. Can skip Scale calculation because it is 1 for xyz.
			const FVector3f TranslatedVec = P - CollisionX[i];
			const FVector4f InverseR = QuatInverse(CollisionR[i]);
			const FVector3f PhiWithNormalInputFloat = VectorQuaternionRotateVector(InverseR, TranslatedVec);
			const FVector PhiWithNormalInput = ConvertVector3fTo3Native(PhiWithNormalInputFloat);
			FVector NormalReal;
			FReal PhiReal;
			FVector3f Normal;
			float Phi;

			const uniform SerializedPtr *uniform GeometryPtr = (const uniform SerializedPtr *uniform)Geometry;
			const uniform uint8 *uniform Ptr = GeometryPtr[i].Ptr;
			const uniform uint8 *uniform TypedPtr = Ptr + SizeofFImplicitObject;
			const uniform uint8 Type = Ptr[OffsetofGeometryType] & 0x3F;

			int32 VelocityBone = i;
			switch(Type)
			{
			case Sphere:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalSphere(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Capsule:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Union:
				{
					PhiWithNormalUnion(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, SizeofFImplicitObject, OffsetofGeometryType, OffsetOfMargin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case TaperedCapsule:
				{
					PhiWithNormalTaperedCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Convex:
				{
					PhiWithNormalConvex(TypedPtr, PhiWithNormalInputFloat, Normal, Phi);
					break;
				}
			case WeightedLatticeBone:
				{
					Phi = PerGroupThickness + 10;
					break;
				}
			case WeightedLatticeLevelSetType:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}
					GetPhiWithNormalAndVelocityBone(CollisionParticles, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, (uniform int *uniform)&VelocityBone, i, PerGroupThickness, programCount, IMMask);
					break;
				}
			default:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}

					GetPhiWithNormal(CollisionParticles, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, i, programCount, IMMask);
					break;
				}
			}

			const float Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
			if(Penetration > 0.0f)
			{
				// TransformVector. Scale is 1 so that part can be removed.
				const FVector3f NormalWorld = VectorQuaternionRotateVector(CollisionR[i], Normal);
				P = P + (Penetration * NormalWorld);
				if(Type == WeightedLatticeLevelSetType)
				{
					#pragma ignore warning(perf)
					const FVector3f CollisionXVal = CollisionX[VelocityBone];
					const FVector3f VectorToPoint = P - CollisionXVal;
					const FVector3f X = VectorLoad(&ParticlesX[extract(Index, 0)]);
					
					#pragma ignore warning(perf)
					const FVector3f CollisionVVal = CollisionV[VelocityBone];
					#pragma ignore warning(perf)
					const FVector3f CollisionWVal = CollisionW[VelocityBone];
					const FVector3f RelativeDisplacement = (P - X) - (CollisionVVal + VectorCross(CollisionWVal, VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
					const FVector3f RelativeDisplacementTangent = RelativeDisplacement - VectorDot(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
					const float RelativeDisplacementTangentLength = VectorSize(RelativeDisplacementTangent);

					const float PositionCorrection = min(Penetration * PerGroupFriction, RelativeDisplacementTangentLength);
					const float CorrectionRatio = select(RelativeDisplacementTangentLength < FLOAT_SMALL_NUMBER, 0.0f, PositionCorrection / RelativeDisplacementTangentLength);
					P = P - (CorrectionRatio * RelativeDisplacementTangent);
				
				}
				else
				{
					const FVector3f VectorToPoint = P - CollisionX[i];
					const FVector3f X = VectorLoad(&ParticlesX[extract(Index, 0)]);

					const FVector3f RelativeDisplacement = (P - X) - (CollisionV[i] + VectorCross(CollisionW[i], VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
					const FVector3f RelativeDisplacementTangent = RelativeDisplacement - VectorDot(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
					const float RelativeDisplacementTangentLength = VectorSize(RelativeDisplacementTangent);

					const float PositionCorrection = min(Penetration * PerGroupFriction, RelativeDisplacementTangentLength);
					const float CorrectionRatio = select(RelativeDisplacementTangentLength < FLOAT_SMALL_NUMBER, 0.0f, PositionCorrection / RelativeDisplacementTangentLength);
					P = P - (CorrectionRatio * RelativeDisplacementTangent);
				}
			}
		}

		if(IM != 0)
		{
			VectorStore(&ParticlesPandInvM[extract(Index, 0)], SetVector4(P, IM));
		}
	}
}

export void ApplyPerParticleCollisionFastFrictionNoGroupCheck(uniform FVector4f ParticlesPandInvM[],
													const uniform FVector3f ParticlesX[],
													const uniform FVector3f CollisionV[],
													const uniform FVector3f CollisionX[],
													const uniform FVector3f CollisionW[],
													const uniform FVector4f CollisionR[],
													const uniform float PerGroupFriction,
													const uniform float PerGroupThickness,
													const uniform uint8 *uniform CollisionParticlesRange,
													const uniform uint8 *uniform Geometry,
													const uniform int SizeofFImplicitObject,
													const uniform int OffsetofGeometryType,
													const uniform int OffsetOfMargin,
													const uniform float Dt,
													const uniform int InnerCollisionCount,
													const uniform int OuterParticleBatchBegin,
													const uniform int OuterParticleBatchEnd)
{
	foreach(Index = OuterParticleBatchBegin ... OuterParticleBatchEnd)
	{
		const FVector4f PandInvM = VectorLoad(&ParticlesPandInvM[extract(Index, 0)]);
		varying FVector3f P;
		varying float IM;
		UnzipPandInvM(PandInvM, P, IM);

		if(all(IM == 0))
		{
			continue;
		}

		for(uniform int i = 0; i < InnerCollisionCount; i++) // SequentialFor
		{
			// InverseTransformPosition. Can skip Scale calculation because it is 1 for xyz.
			const FVector3f TranslatedVec = P - CollisionX[i];
			const FVector4f InverseR = QuatInverse(CollisionR[i]);
			const FVector3f PhiWithNormalInputFloat = VectorQuaternionRotateVector(InverseR, TranslatedVec);
			const FVector PhiWithNormalInput = ConvertVector3fTo3Native(PhiWithNormalInputFloat);
			FVector NormalReal;
			FReal PhiReal;
			FVector3f Normal;
			float Phi;

			const uniform SerializedPtr *uniform GeometryPtr = (const uniform SerializedPtr *uniform)Geometry;
			const uniform uint8 *uniform Ptr = GeometryPtr[i].Ptr;
			const uniform uint8 *uniform TypedPtr = Ptr + SizeofFImplicitObject;
			const uniform uint8 Type = Ptr[OffsetofGeometryType] & 0x3F;

			int32 VelocityBone = i;
			switch(Type)
			{
			case Sphere:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalSphere(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Capsule:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Union:
				{
					PhiWithNormalUnion(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, SizeofFImplicitObject, OffsetofGeometryType, OffsetOfMargin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case TaperedCapsule:
				{
					PhiWithNormalTaperedCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Convex:
				{
					PhiWithNormalConvex(TypedPtr, PhiWithNormalInputFloat, Normal, Phi);
					break;
				}
			case WeightedLatticeBone:
				{
					Phi = PerGroupThickness + 10;
					break;
				}
			case WeightedLatticeLevelSetType:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}
					GetPhiWithNormalAndVelocityBoneCollisionParticleRange(CollisionParticlesRange, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, (uniform int *uniform)&VelocityBone, i, PerGroupThickness, programCount, IMMask);
					break;
				}
			default:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}

					GetPhiWithNormalCollisionParticleRange(CollisionParticlesRange, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, i, programCount, IMMask);
					break;
				}
			}

			const float Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
			if(Penetration > 0.0f)
			{
				// TransformVector. Scale is 1 so that part can be removed.
				const FVector3f NormalWorld = VectorQuaternionRotateVector(CollisionR[i], Normal);
				P = P + (Penetration * NormalWorld);
				if(Type == WeightedLatticeLevelSetType)
				{
					#pragma ignore warning(perf)
					const FVector3f CollisionXVal = CollisionX[VelocityBone];
					const FVector3f VectorToPoint = P - CollisionXVal;
					const FVector3f X = VectorLoad(&ParticlesX[extract(Index, 0)]);
					
					#pragma ignore warning(perf)
					const FVector3f CollisionVVal = CollisionV[VelocityBone];
					#pragma ignore warning(perf)
					const FVector3f CollisionWVal = CollisionW[VelocityBone];
					const FVector3f RelativeDisplacement = (P - X) - (CollisionVVal + VectorCross(CollisionWVal, VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
					const FVector3f RelativeDisplacementTangent = RelativeDisplacement - VectorDot(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
					const float RelativeDisplacementTangentLength = VectorSize(RelativeDisplacementTangent);

					const float PositionCorrection = min(Penetration * PerGroupFriction, RelativeDisplacementTangentLength);
					const float CorrectionRatio = select(RelativeDisplacementTangentLength < FLOAT_SMALL_NUMBER, 0.0f, PositionCorrection / RelativeDisplacementTangentLength);
					P = P - (CorrectionRatio * RelativeDisplacementTangent);
				
				}
				else
				{
					const FVector3f VectorToPoint = P - CollisionX[i];
					const FVector3f X = VectorLoad(&ParticlesX[extract(Index, 0)]);

					const FVector3f RelativeDisplacement = (P - X) - (CollisionV[i] + VectorCross(CollisionW[i], VectorToPoint)) * Dt; // This corresponds to the tangential velocity multiplied by dt (friction will drive this to zero if it is high enough)
					const FVector3f RelativeDisplacementTangent = RelativeDisplacement - VectorDot(RelativeDisplacement, NormalWorld) * NormalWorld; // Project displacement into the tangential plane
					const float RelativeDisplacementTangentLength = VectorSize(RelativeDisplacementTangent);

					const float PositionCorrection = min(Penetration * PerGroupFriction, RelativeDisplacementTangentLength);
					const float CorrectionRatio = select(RelativeDisplacementTangentLength < FLOAT_SMALL_NUMBER, 0.0f, PositionCorrection / RelativeDisplacementTangentLength);
					P = P - (CorrectionRatio * RelativeDisplacementTangent);
				}
			}
		}

		if(IM != 0)
		{
			VectorStore(&ParticlesPandInvM[extract(Index, 0)], SetVector4(P, IM));
		}
	}
}

export void ApplyPerParticleCollisionNoFriction(uniform FVector4f ParticlesPandInvM[],
												const uniform FVector3f ParticlesX[],
												const uniform FVector3f CollisionV[],
												const uniform FVector3f CollisionX[],
												const uniform FVector3f CollisionW[],
												const uniform FVector4f CollisionR[],
												const uniform uint32 DynamicGroupId,
												const uniform uint32 KinematicGroupIds[],
												const uniform float PerGroupThickness,
												const uniform uint8 *uniform CollisionParticles,
												const uniform uint8 *uniform Geometry,
												const uniform int SizeofFImplicitObject,
												const uniform int OffsetofGeometryType,
												const uniform int OffsetOfMargin,
												const uniform float Dt,
												const uniform int InnerOffset,
												const uniform int InnerRange,
												const uniform int OuterOffset,
												const uniform int OuterRange)
{
	uniform int BailAt = OuterRange;
	uniform bool bBail = false;

	// Pre-check for bail out.
	foreach(Index = OuterOffset ... BailAt)
	{
		for(uniform int i = InnerOffset; i < InnerRange; i++)
		{
			const uniform uint32 KinematicGroupId = KinematicGroupIds[i];  // Collision group Id

			if (KinematicGroupId != (uint32)INDEX_NONE && DynamicGroupId != KinematicGroupId)
			{
				foreach_active(j)
				{
					if(!bBail)
					{
						BailAt = extract(Index, j);
						bBail = true;
					}
				}
			}
		}
	}

	foreach(Index = OuterOffset ... BailAt)
	{
		const FVector4f PandInvM = VectorLoad(&ParticlesPandInvM[extract(Index, 0)]);
		varying FVector3f P;
		varying float IM;
		UnzipPandInvM(PandInvM, P, IM);

		if(all(IM == 0))
		{
			continue;
		}

		for(uniform int i = InnerOffset; i < InnerRange; i++) // SequentialFor
		{
			// InverseTransformPosition. Can skip Scale calculation because it is 1 for xyz.
			const FVector3f TranslatedVec = P - CollisionX[i];
			const FVector4f InverseR = QuatInverse(CollisionR[i]);
			const FVector3f PhiWithNormalInputFloat = VectorQuaternionRotateVector(InverseR, TranslatedVec);
			const FVector PhiWithNormalInput = ConvertVector3fTo3Native(PhiWithNormalInputFloat);
			FVector NormalReal;
			FReal PhiReal;
			FVector3f Normal;
			float Phi;

			const uniform SerializedPtr *uniform GeometryPtr = (const uniform SerializedPtr *uniform)Geometry;
			const uniform uint8 *uniform Ptr = GeometryPtr[i].Ptr;
			const uniform uint8 *uniform TypedPtr = Ptr + SizeofFImplicitObject;
			const uniform uint8 Type = Ptr[OffsetofGeometryType] & 0x3F;

			switch(Type)
			{
			case Sphere:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalSphere(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Capsule:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Union:
				{
					PhiWithNormalUnion(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, SizeofFImplicitObject, OffsetofGeometryType, OffsetOfMargin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case TaperedCapsule:
				{
					PhiWithNormalTaperedCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Convex:
				{
					PhiWithNormalConvex(TypedPtr, PhiWithNormalInputFloat, Normal, Phi);
					break;
				}
			case WeightedLatticeBone:
				{
					Phi = PerGroupThickness + 10;
					break;
				}
			default:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}

					GetPhiWithNormal(CollisionParticles, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, i, programCount, IMMask);
					break;
				}
			}

			const float Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
			if(Penetration > 0.0f)
			{
				// TransformVector. Scale is 1 so that part can be removed.
				const FVector3f NormalWorld = VectorQuaternionRotateVector(CollisionR[i], Normal);
				P = P + (Penetration * NormalWorld);
			}
		}

		if(IM != 0)
		{
			VectorStore(&ParticlesPandInvM[extract(Index, 0)], SetVector4(P, IM));
		}
	}
}

export void ApplyPerParticleCollisionNoFrictionNoGroupCheck(uniform FVector4f ParticlesPandInvM[],
												const uniform FVector3f ParticlesX[],
												const uniform FVector3f CollisionV[],
												const uniform FVector3f CollisionX[],
												const uniform FVector3f CollisionW[],
												const uniform FVector4f CollisionR[],
												const uniform float PerGroupThickness,
												const uniform uint8 *uniform CollisionParticlesRange,
												const uniform uint8 *uniform Geometry,
												const uniform int SizeofFImplicitObject,
												const uniform int OffsetofGeometryType,
												const uniform int OffsetOfMargin,
												const uniform float Dt,
												const uniform int InnerCollisionCount,
												const uniform int OuterParticleBatchBegin,
												const uniform int OuterParticleBatchEnd)
{

	foreach(Index = OuterParticleBatchBegin ... OuterParticleBatchEnd)
	{
		const FVector4f PandInvM = VectorLoad(&ParticlesPandInvM[extract(Index, 0)]);
		varying FVector3f P;
		varying float IM;
		UnzipPandInvM(PandInvM, P, IM);

		if(all(IM == 0))
		{
			continue;
		}

		for(uniform int i = 0; i < InnerCollisionCount; i++) // SequentialFor
		{
			// InverseTransformPosition. Can skip Scale calculation because it is 1 for xyz.
			const FVector3f TranslatedVec = P - CollisionX[i];
			const FVector4f InverseR = QuatInverse(CollisionR[i]);
			const FVector3f PhiWithNormalInputFloat = VectorQuaternionRotateVector(InverseR, TranslatedVec);
			const FVector PhiWithNormalInput = ConvertVector3fTo3Native(PhiWithNormalInputFloat);
			FVector NormalReal;
			FReal PhiReal;
			FVector3f Normal;
			float Phi;

			const uniform SerializedPtr *uniform GeometryPtr = (const uniform SerializedPtr *uniform)Geometry;
			const uniform uint8 *uniform Ptr = GeometryPtr[i].Ptr;
			const uniform uint8 *uniform TypedPtr = Ptr + SizeofFImplicitObject;
			const uniform uint8 Type = Ptr[OffsetofGeometryType] & 0x3F;

			switch(Type)
			{
			case Sphere:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalSphere(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Capsule:
				{
					const uniform FReal Margin = *((const uniform FReal *uniform)(Ptr + OffsetOfMargin));
					PhiWithNormalCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, Margin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Union:
				{
					PhiWithNormalUnion(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal, SizeofFImplicitObject, OffsetofGeometryType, OffsetOfMargin);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case TaperedCapsule:
				{
					PhiWithNormalTaperedCapsule(TypedPtr, PhiWithNormalInput, NormalReal, PhiReal);
					Phi = (varying float)PhiReal;
					Normal = ConvertVector3NativeTo3f(NormalReal);
					break;
				}
			case Convex:
				{
					PhiWithNormalConvex(TypedPtr, PhiWithNormalInputFloat, Normal, Phi);
					break;
				}
			case WeightedLatticeBone:
				{
					Phi = PerGroupThickness + 10;
					break;
				}
			default:
				{
					uniform int IMMask;
					if(IM != 0)
					{
						IMMask = lanemask();
					}

					GetPhiWithNormalCollisionParticleRange(CollisionParticlesRange, (const uniform float *uniform)&PhiWithNormalInputFloat, (uniform float *uniform)&Normal, (uniform float *uniform)&Phi, i, programCount, IMMask);
					break;
				}
			}

			const float Penetration = PerGroupThickness - Phi; // This is related to the Normal impulse
			if(Penetration > 0.0f)
			{
				// TransformVector. Scale is 1 so that part can be removed.
				const FVector3f NormalWorld = VectorQuaternionRotateVector(CollisionR[i], Normal);
				P = P + (Penetration * NormalWorld);
			}
		}

		if(IM != 0)
		{
			VectorStore(&ParticlesPandInvM[extract(Index, 0)], SetVector4(P, IM));
		}
	}
}