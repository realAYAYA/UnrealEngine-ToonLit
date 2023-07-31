// Copyright Epic Games, Inc. All Rights Reserved.
#include "ChaosCloth/ChaosClothingSimulationCollider.h"
#include "ChaosCloth/ChaosClothingSimulationSolver.h"
#include "ChaosCloth/ChaosClothingSimulationCloth.h"
#include "ChaosCloth/ChaosClothPrivate.h"
#include "Chaos/Capsule.h"
#include "Chaos/Plane.h"
#include "Chaos/Sphere.h"
#include "Chaos/TaperedCylinder.h"
#include "Chaos/TaperedCapsule.h"
#include "Chaos/Convex.h"
#include "Chaos/Levelset.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "ClothingSimulation.h" // For context
#include "ClothingAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "HAL/PlatformMath.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"

DECLARE_CYCLE_STAT(TEXT("Chaos Cloth Update Collider"), STAT_ChaosClothingSimulationColliderUpdate, STATGROUP_ChaosCloth);

namespace Chaos
{

namespace ClothingSimulationColliderConsoleVariables
{
	TAutoConsoleVariable<bool> CVarUseOptimizedTaperedCapsule(TEXT("p.ChaosCloth.UseOptimizedTaperedCapsule"), true, TEXT("Use the optimized TaperedCapsule code instead of using a tapered cylinder and two spheres"));
}

void FClothingSimulationCollider::FLODData::Add(
	FClothingSimulationSolver* Solver,
	FClothingSimulationCloth* Cloth,
	const FClothCollisionData& InClothCollisionData,
	const TArray<FLevelSetCollisionData>& InLevelSetCollisionData,
	const FReal InScale,
	const TArray<int32>& UsedBoneIndices)
{
	check(Solver);
	check(Cloth);

	// Keep a list of all collisions
	ClothCollisionData = InClothCollisionData;

	// Calculate the number of geometries
	int32 NumSpheres = ClothCollisionData.Spheres.Num();
	TBitArray<TInlineAllocator<8>> CapsuleEnds(false, NumSpheres);  // Init on stack for 256 spheres, any number over this will be allocated

	const int32 NumCapsules = ClothCollisionData.SphereConnections.Num();
	for (int32 Index = 0; Index < NumCapsules; ++Index)
	{
		const FClothCollisionPrim_SphereConnection& Connection = ClothCollisionData.SphereConnections[Index];
		for (int32 SphereIndex : Connection.SphereIndices)
		{
			FBitReference IsCapsuleEnd = CapsuleEnds[SphereIndex];
			if (!IsCapsuleEnd)
			{
				IsCapsuleEnd = true;
				--NumSpheres;
			}
		}
	}

	const uint32 NumConvexes = ClothCollisionData.Convexes.Num();
	const int32 NumBoxes = ClothCollisionData.Boxes.Num();
	const int32 NumLevelSets = InLevelSetCollisionData.Num();
	NumGeometries = NumSpheres + NumCapsules + NumConvexes + NumBoxes + NumLevelSets;

	// Retrieve cloth group Id, or use INDEX_NONE if this collider applies to all cloths (when Cloth == nullptr)
	const uint32 GroupId = Cloth ? Cloth->GetGroupId() : INDEX_NONE;

	// The offset will be set to the first collision particle's index
	// Try to reuse existing offsets when Add is called during the collider update (ie Offset isn't INDEX_NONE)
	int32* const OffsetPtr = Offsets.Find(FSolverClothPair(Solver, Cloth));
	const bool bIsNewCollider = !OffsetPtr;
	int32& Offset = bIsNewCollider ? Offsets.Add(FSolverClothPair(Solver, Cloth)) : *OffsetPtr;
	Offset = Solver->AddCollisionParticles(NumGeometries, GroupId, bIsNewCollider ? INDEX_NONE : Offset);

	// Capsules
	const int32 CapsuleOffset = Offset;
	if (NumCapsules)
	{
		int32* const BoneIndices = Solver->GetCollisionBoneIndices(CapsuleOffset);
		Softs::FSolverRigidTransform3* const BaseTransforms = Solver->GetCollisionBaseTransforms(CapsuleOffset);

		for (int32 Index = 0; Index < NumCapsules; ++Index)
		{
			const FClothCollisionPrim_SphereConnection& Connection = ClothCollisionData.SphereConnections[Index];

			const int32 SphereIndex0 = Connection.SphereIndices[0];
			const int32 SphereIndex1 = Connection.SphereIndices[1];
			checkSlow(SphereIndex0 != SphereIndex1);
			const FClothCollisionPrim_Sphere& Sphere0 = ClothCollisionData.Spheres[SphereIndex0];
			const FClothCollisionPrim_Sphere& Sphere1 = ClothCollisionData.Spheres[SphereIndex1];

			BoneIndices[Index] = GetMappedBoneIndex(UsedBoneIndices, Sphere0.BoneIndex);
			checkSlow(Sphere0.BoneIndex == Sphere1.BoneIndex);
			UE_CLOG(Sphere0.BoneIndex != Sphere1.BoneIndex,
				LogChaosCloth, Warning, TEXT("Found a legacy cloth asset with a collision capsule spanning across two bones. This is not supported with the current system."));
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Found collision capsule on bone index %d."), BoneIndices[Index]);

			const FVec3 X0 = Sphere0.LocalPosition * InScale;
			const FVec3 X1 = Sphere1.LocalPosition * InScale;
			const FVec3 Center = (X0 + X1) * 0.5f;
			const FVec3 Axis = (X1 - X0) * 0.5f;
			const FVec3 P0 = Center - Axis;
			const FVec3 P1 = Center + Axis;

			const FReal Radius0 = Sphere0.Radius * InScale;
			const FReal Radius1 = Sphere1.Radius * InScale;
			FReal MinRadius, MaxRadius;
			if (Radius0 <= Radius1) { MinRadius = Radius0; MaxRadius = Radius1; }
			else { MinRadius = Radius1; MaxRadius = Radius0; }

			BaseTransforms[Index] = Softs::FSolverRigidTransform3::Identity;

			if (Axis.SizeSquared() < SMALL_NUMBER)
			{
				// Sphere
				Solver->SetCollisionGeometry(CapsuleOffset, Index,
					MakeUnique<FSphere>(Center, MaxRadius));
			}
			else if (MaxRadius - MinRadius < KINDA_SMALL_NUMBER)
			{
				// Capsule
				Solver->SetCollisionGeometry(CapsuleOffset, Index, 
					MakeUnique<FCapsule>(P0, P1, MaxRadius));
			}
			else
			{
				if (ClothingSimulationColliderConsoleVariables::CVarUseOptimizedTaperedCapsule.GetValueOnAnyThread())
				{
					Solver->SetCollisionGeometry(CapsuleOffset, Index,
						MakeUnique<FTaperedCapsule>(P0, P1, Radius0, Radius1));
				}
				else
				{
					// Tapered capsule approximate by a union of tapered cylinder and two spheres
					TArray<TUniquePtr<FImplicitObject>> Objects;
					Objects.Reserve(3);
					Objects.Add(TUniquePtr<FImplicitObject>(
						new FTaperedCylinder(P0, P1, Radius0, Radius1)));
					Objects.Add(TUniquePtr<FImplicitObject>(
						new FSphere(P0, Radius0)));
					Objects.Add(TUniquePtr<FImplicitObject>(
						new FSphere(P1, Radius1)));
					Solver->SetCollisionGeometry(CapsuleOffset, Index,
						MakeUnique<FImplicitObjectUnion>(MoveTemp(Objects)));
				}
			}
		}
	}

	// Spheres
	const int32 SphereOffset = CapsuleOffset + NumCapsules;
	if (NumSpheres != 0)
	{
		int32* const BoneIndices = Solver->GetCollisionBoneIndices(SphereOffset);
		Softs::FSolverRigidTransform3* const BaseTransforms = Solver->GetCollisionBaseTransforms(SphereOffset);

		for (int32 Index = 0, SphereIndex = 0; SphereIndex < ClothCollisionData.Spheres.Num(); ++SphereIndex)
		{
			// Skip spheres that are the end caps of capsules.
			if (CapsuleEnds[SphereIndex])
			//if (CapsuleEnds.Contains(SphereIndex))
			{
				continue;
			}

			const FClothCollisionPrim_Sphere& Sphere = ClothCollisionData.Spheres[SphereIndex];

			BoneIndices[Index] = GetMappedBoneIndex(UsedBoneIndices, Sphere.BoneIndex);
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Found collision sphere on bone index %d."), BoneIndices[Index]);

			BaseTransforms[Index] = Softs::FSolverRigidTransform3::Identity;

			Solver->SetCollisionGeometry(SphereOffset, Index,
				MakeUnique<FSphere>(
					Sphere.LocalPosition * InScale,
					Sphere.Radius * InScale));

			++Index;
		}
	}

	// Convexes
	const int32 ConvexOffset = SphereOffset + NumSpheres;
	if (NumConvexes != 0)
	{
		int32* const BoneIndices = Solver->GetCollisionBoneIndices(ConvexOffset);
		Softs::FSolverRigidTransform3* const BaseTransforms = Solver->GetCollisionBaseTransforms(ConvexOffset);

		for (uint32 Index = 0; Index < NumConvexes; ++Index)
		{
			const FClothCollisionPrim_Convex& Convex = ClothCollisionData.Convexes[Index];

			// Always initialize the collision particle transforms before setting any geometry as otherwise NaNs gets detected during the bounding box updates
			BaseTransforms[Index] = Softs::FSolverRigidTransform3::Identity;

			BoneIndices[Index] = GetMappedBoneIndex(UsedBoneIndices, Convex.BoneIndex);
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Found collision convex on bone index %d."), BoneIndices[Index]);

			TArray<FConvex::FPlaneType> Planes;
			TArray<TArray<int32>> FaceIndices;

			const int32 NumSurfacePoints = Convex.SurfacePoints.Num();
			const int32 NumFaces = Convex.Faces.Num();

			if (NumSurfacePoints < 4)
			{
				UE_LOG(LogChaosCloth, Warning, TEXT("Invalid convex collision: not enough surface points."));
			}
			else if (NumFaces < 4)
			{
				UE_LOG(LogChaosCloth, Warning, TEXT("Invalid convex collision: not enough faces."));
			}
			else
			{
				// Retrieve convex planes
				Planes.Reserve(NumFaces);
				FaceIndices.Reserve(NumFaces);
				for (const FClothCollisionPrim_ConvexFace& Face : Convex.Faces)
				{
					const FPlane& Plane = Face.Plane;
					FPlane NormalizedPlane(Plane);
					if (!NormalizedPlane.Normalize())
					{
						UE_LOG(LogChaosCloth, Warning, TEXT("Invalid convex collision: bad plane normal."));
						break;
					}
					if (!Face.Indices.Num())
					{
						UE_LOG(LogChaosCloth, Warning, TEXT("Invalid convex collision: no face indices, this asset might be using a deprecated Apex collision shape."));
						break;
					}
					const FConvex::FVec3Type Normal(static_cast<FVector>(NormalizedPlane));
					const FConvex::FVec3Type Base = Normal * NormalizedPlane.W * InScale;
					Planes.Add(FConvex::FPlaneType(Base, Normal));
					FaceIndices.Add(Face.Indices);
				}
			}

			if (Planes.Num() == NumFaces)
			{
				check(Planes.Num() == FaceIndices.Num());

				// Retrieve the convex vertices and face indices
				TArray<FConvex::FVec3Type> Vertices;
				Vertices.SetNum(NumSurfacePoints);
				for (int32 ParticleIndex = 0; ParticleIndex < NumSurfacePoints; ++ParticleIndex)
				{
					Vertices[ParticleIndex] = Convex.SurfacePoints[ParticleIndex];
				}

				// Setup the collision particle geometry
				Solver->SetCollisionGeometry(ConvexOffset, Index, MakeUnique<FConvex>(MoveTemp(Planes), MoveTemp(FaceIndices), MoveTemp(Vertices)));
			}
			else
			{
				UE_LOG(LogChaosCloth, Warning, TEXT("Replacing invalid convex collision by a default unit sphere."));
				Solver->SetCollisionGeometry(ConvexOffset, Index, MakeUnique<FSphere>(FVec3(0.0f), 1.0f));  // Default to a unit sphere to replace the faulty convex
			}
		}
	}

	// Boxes
	const int32 BoxOffset = ConvexOffset + NumConvexes;
	if (NumBoxes != 0)
	{
		int32* const BoneIndices = Solver->GetCollisionBoneIndices(BoxOffset);
		Softs::FSolverRigidTransform3* const BaseTransforms = Solver->GetCollisionBaseTransforms(BoxOffset);
		
		for (int32 Index = 0; Index < NumBoxes; ++Index)
		{
			const FClothCollisionPrim_Box& Box = ClothCollisionData.Boxes[Index];
			
			BaseTransforms[Index] = Softs::FSolverRigidTransform3(Box.LocalPosition, Box.LocalRotation);
			
			BoneIndices[Index] = GetMappedBoneIndex(UsedBoneIndices, Box.BoneIndex);
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Found collision box on bone index %d."), BoneIndices[Index]);

			const FVec3 HalfExtents = Box.HalfExtents * InScale;
			Solver->SetCollisionGeometry(BoxOffset, Index, MakeUnique<TBox<FReal, 3>>(-HalfExtents, HalfExtents));
		}
	}

	const int32 LevelSetOffset = BoxOffset + NumBoxes;
	if (NumLevelSets != 0)
	{
		int32* const BoneIndices = Solver->GetCollisionBoneIndices(LevelSetOffset);
		Softs::FSolverRigidTransform3* const BaseTransforms = Solver->GetCollisionBaseTransforms(LevelSetOffset);

		for (int32 Index = 0; Index < NumLevelSets; ++Index)
		{
			// Always initialize the collision particle transforms before setting any geometry as otherwise NaNs gets detected during the bounding box updates
			BaseTransforms[Index] = Softs::FSolverRigidTransform3::Identity;

			BoneIndices[Index] = GetMappedBoneIndex(UsedBoneIndices, InLevelSetCollisionData[Index].BoneIndex);
			UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Found collision level set on bone index %d."), BoneIndices[Index]);

			// Setup the collision particle geometry
			TUniquePtr<FImplicitObject> TransformedLevelSet = MakeUnique<TImplicitObjectTransformed<FReal, 3>>(InLevelSetCollisionData[Index].LevelSet->DeepCopy(), 
				TRigidTransform<FReal, 3>(InLevelSetCollisionData[Index].Transform));
			Solver->SetCollisionGeometry(LevelSetOffset, Index, MoveTemp(TransformedLevelSet));
		}
	}

	UE_LOG(LogChaosCloth, VeryVerbose, TEXT("Added collisions: %d spheres, %d capsules, %d convexes, %d boxes, % level sets."), NumSpheres, NumCapsules, NumConvexes, NumBoxes, NumLevelSets);
}

void FClothingSimulationCollider::FLODData::Remove(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	Offsets.Remove(FSolverClothPair(Solver, Cloth));
}

void FClothingSimulationCollider::FLODData::Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, const FClothingSimulationContextCommon* Context)
{
	check(Solver);
	check(Cloth);
	if (NumGeometries)
	{
		const int32 Offset = Offsets.FindChecked(FSolverClothPair(Solver, Cloth));
		const int32* const BoneIndices = Solver->GetCollisionBoneIndices(Offset);
		const Softs::FSolverRigidTransform3* BaseTransforms = Solver->GetCollisionBaseTransforms(Offset);
		Softs::FSolverRigidTransform3* const CollisionTransforms = Solver->GetCollisionTransforms(Offset);

		const TArrayView<const FTransform> BoneTransforms = Context ? Context->BoneTransforms : TArrayView<const FTransform>();

		FTransform ComponentToLocalSpaceReal = Context ? Context->ComponentToWorld : FTransform::Identity;
		ComponentToLocalSpaceReal.AddToTranslation(-Solver->GetLocalSpaceLocation());
		const Softs::FSolverTransform3 ComponentToLocalSpace(ComponentToLocalSpaceReal);  // LWC, now in local space, therefore it is safe to use the solver transform type
	
		// Update the collision transforms
		for (int32 Index = 0; Index < NumGeometries; ++Index)
		{
			const int32 BoneIndex = BoneIndices[Index];
			CollisionTransforms[Index] = BoneTransforms.IsValidIndex(BoneIndex) ?
				BaseTransforms[Index] * Softs::FSolverTransform3(BoneTransforms[BoneIndex]) * ComponentToLocalSpace :  // LWC requires the BoneTransform cast to the solver underlying type
				BaseTransforms[Index] * ComponentToLocalSpace;  // External collisions don't map to a bone
		}
	}
}

void FClothingSimulationCollider::FLODData::Enable(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth, bool bEnable)
{
	check(Solver);
	check(Cloth);
	if (NumGeometries)
	{
		const int32 Offset = Offsets.FindChecked(FSolverClothPair(Solver, Cloth));
		Solver->EnableCollisionParticles(Offset, bEnable);
	}
}

void FClothingSimulationCollider::FLODData::ResetStartPose(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	check(Solver);
	check(Cloth);
	if (NumGeometries)
	{
		const int32 Offset = Offsets.FindChecked(FSolverClothPair(Solver, Cloth));
		Solver->ResetCollisionStartPose(Offset, NumGeometries);
	}
}

FClothingSimulationCollider::FClothingSimulationCollider(
	const UClothingAssetCommon* InAsset,
	const USkeletalMeshComponent* InSkeletalMeshComponent,
	bool bInUseLODIndexOverride,
	int32 InLODIndexOverride)
	: Asset(InAsset)
	, SkeletalMeshComponent(InSkeletalMeshComponent)
	, CollisionData(nullptr)
	, bUseLODIndexOverride(bInUseLODIndexOverride)
	, LODIndexOverride(InLODIndexOverride)
	, Scale(1.f)
{
	// Prepare LOD array
	const int32 NumLODs = Asset ? Asset->LodData.Num() : 0;
	LODData.SetNum((int32)ECollisionDataType::LODs + NumLODs);
}

FClothingSimulationCollider::~FClothingSimulationCollider()
{
}

FClothCollisionData FClothingSimulationCollider::GetCollisionData(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth) const
{
	FClothCollisionData ClothCollisionData;
	ClothCollisionData.Append(LODData[(int32)ECollisionDataType::LODless].ClothCollisionData);
	ClothCollisionData.Append(LODData[(int32)ECollisionDataType::External].ClothCollisionData);

	const int32 LODIndex = LODIndices.FindChecked(FSolverClothPair(Solver, Cloth));
	if (LODIndex >= (int32)ECollisionDataType::LODs)
	{
		ClothCollisionData.Append(LODData[LODIndex].ClothCollisionData);
	}
	return ClothCollisionData;
}

void FClothingSimulationCollider::ExtractPhysicsAssetCollision(FClothCollisionData& ClothCollisionData, TArray<FLevelSetCollisionData>& LevelSetCollisions, TArray<int32>& UsedBoneIndices)
{
	ClothCollisionData.Reset();
	UsedBoneIndices.Reset();

	if (!Asset)
	{
		return;
	}

	if (const UPhysicsAsset* const PhysAsset = Asset->PhysicsAsset)
	{
		const USkeletalMesh* const TargetMesh = CastChecked<USkeletalMesh>(Asset->GetOuter());

		UsedBoneIndices.Reserve(PhysAsset->SkeletalBodySetups.Num());

		for (const USkeletalBodySetup* BodySetup : PhysAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			const int32 MeshBoneIndex = TargetMesh->GetRefSkeleton().FindBoneIndex(BodySetup->BoneName);
			const int32 MappedBoneIndex = UsedBoneIndices.Add(MeshBoneIndex);
			
			// Add capsules
			const FKAggregateGeom& AggGeom = BodySetup->AggGeom;
			if (AggGeom.SphylElems.Num())
			{
				for (const FKSphylElem& SphylElem : AggGeom.SphylElems)
				{
					if (SphylElem.Length == 0.0f)
					{
						// Add extracted sphere collision data
						FClothCollisionPrim_Sphere Sphere;
						Sphere.LocalPosition = SphylElem.Center;
						Sphere.Radius = SphylElem.Radius;
						Sphere.BoneIndex = MappedBoneIndex;
						ClothCollisionData.Spheres.Add(Sphere);
					}
					else
					{
						// Add extracted spheres collision data
						FClothCollisionPrim_Sphere Sphere0;
						FClothCollisionPrim_Sphere Sphere1;
						const FVector OrientedDirection = SphylElem.Rotation.RotateVector(FVector::UpVector);
						const FVector HalfDim = OrientedDirection * (SphylElem.Length / 2.f);
						Sphere0.LocalPosition = SphylElem.Center - HalfDim;
						Sphere1.LocalPosition = SphylElem.Center + HalfDim;
						Sphere0.Radius = SphylElem.Radius;
						Sphere1.Radius = SphylElem.Radius;
						Sphere0.BoneIndex = MappedBoneIndex;
						Sphere1.BoneIndex = MappedBoneIndex;

						// Add extracted sphere connection collision data
						FClothCollisionPrim_SphereConnection SphereConnection;
						SphereConnection.SphereIndices[0] = ClothCollisionData.Spheres.Add(Sphere0);
						SphereConnection.SphereIndices[1] = ClothCollisionData.Spheres.Add(Sphere1);
						ClothCollisionData.SphereConnections.Add(SphereConnection);
					}
				}
			}

			// Add spheres
			for (const FKSphereElem& SphereElem : AggGeom.SphereElems)
			{
				// Add extracted sphere collision data
				FClothCollisionPrim_Sphere Sphere;
				Sphere.LocalPosition = SphereElem.Center;
				Sphere.Radius = SphereElem.Radius;
				Sphere.BoneIndex = MappedBoneIndex;
				ClothCollisionData.Spheres.Add(Sphere);
			}

			// Add boxes
			for (const FKBoxElem& BoxElem : AggGeom.BoxElems)
			{
				// Add extracted box collision data
				FClothCollisionPrim_Box Box;
				Box.LocalPosition = BoxElem.Center;
				Box.LocalRotation = BoxElem.Rotation.Quaternion();
				Box.HalfExtents = FVector(BoxElem.X, BoxElem.Y, BoxElem.Z) * 0.5f;
				Box.BoneIndex = MappedBoneIndex;
				ClothCollisionData.Boxes.Add(Box);
			}

			// Add tapered capsules
			for (const FKTaperedCapsuleElem& TaperedCapsuleElem : AggGeom.TaperedCapsuleElems)
			{
				if (TaperedCapsuleElem.Length == 0)
				{
					// Add extracted sphere collision data
					FClothCollisionPrim_Sphere Sphere;
					Sphere.LocalPosition = TaperedCapsuleElem.Center;
					Sphere.Radius = FMath::Max(TaperedCapsuleElem.Radius0, TaperedCapsuleElem.Radius1);
					Sphere.BoneIndex = MappedBoneIndex;
					ClothCollisionData.Spheres.Add(Sphere);
				}
				else
				{
					// Add extracted spheres collision data
					FClothCollisionPrim_Sphere Sphere0;
					FClothCollisionPrim_Sphere Sphere1;
					const FVector OrientedDirection = TaperedCapsuleElem.Rotation.RotateVector(FVector::UpVector);
					const FVector HalfDim = OrientedDirection * (TaperedCapsuleElem.Length / 2.f);
					Sphere0.LocalPosition = TaperedCapsuleElem.Center + HalfDim;
					Sphere1.LocalPosition = TaperedCapsuleElem.Center - HalfDim;
					Sphere0.Radius = TaperedCapsuleElem.Radius0;
					Sphere1.Radius = TaperedCapsuleElem.Radius1;
					Sphere0.BoneIndex = MappedBoneIndex;
					Sphere1.BoneIndex = MappedBoneIndex;

					// Add extracted sphere connection collision data
					FClothCollisionPrim_SphereConnection SphereConnection;
					SphereConnection.SphereIndices[0] = ClothCollisionData.Spheres.Add(Sphere0);
					SphereConnection.SphereIndices[1] = ClothCollisionData.Spheres.Add(Sphere1);
					ClothCollisionData.SphereConnections.Add(SphereConnection);
				}
			}

			// Add convexes
			for (const FKConvexElem& ConvexElem : AggGeom.ConvexElems)
			{
				// Add stub for extracted collision data
				FClothCollisionPrim_Convex Convex;
				Convex.BoneIndex = MappedBoneIndex;
				const FImplicitObject& ChaosConvexMesh = *ConvexElem.GetChaosConvexMesh();
				const FConvex& ChaosConvex = ChaosConvexMesh.GetObjectChecked<FConvex>();

				// Copy faces' planes and indices
				const TArray<FConvex::FPlaneType>& Faces = ChaosConvex.GetFaces();
				Convex.Faces.SetNum(Faces.Num());
				for (int32 FaceIndex = 0; FaceIndex < Faces.Num(); ++FaceIndex)
				{
					FClothCollisionPrim_ConvexFace& Face = Convex.Faces[FaceIndex];

					// Copy face plane
					const FConvex::FPlaneType& PlaneConcrete = Faces[FaceIndex];
					Face.Plane = FPlane((FVector)PlaneConcrete.X(), (FVector)PlaneConcrete.Normal());

					// Copy face indices
					const int32 NumVertexIndices = ChaosConvex.NumPlaneVertices(FaceIndex);
					Face.Indices.SetNum(NumVertexIndices);
					for (int32 Vertex = 0; Vertex < NumVertexIndices; ++Vertex)
					{
						Face.Indices[Vertex] = ChaosConvex.GetPlaneVertex(FaceIndex, Vertex);
					}
				}

				// Copy surface points
				const int32 NumSurfacePoints = ChaosConvex.NumVertices();
				Convex.SurfacePoints.Reserve(NumSurfacePoints);
				for (int32 ParticleIndex = 0; ParticleIndex < NumSurfacePoints; ++ParticleIndex)
				{
					Convex.SurfacePoints.Add((FVector)ChaosConvex.GetVertex(ParticleIndex));
				}

				// Add extracted collision data
				ClothCollisionData.Convexes.Add(Convex);
			}

			for (const FKLevelSetElem& LevelSetElem : AggGeom.LevelSetElems)
			{
				if (LevelSetElem.GetLevelSet().IsValid())
				{
					LevelSetCollisions.Add({ LevelSetElem.GetLevelSet(), LevelSetElem.GetTransform(), MappedBoneIndex });
				}
			}

		}  // End for PhysAsset->SkeletalBodySetups
	}  // End if Asset->PhysicsAsset
}


int32 FClothingSimulationCollider::GetNumGeometries(int32 InSlotIndex) const
{
	return LODData.IsValidIndex(InSlotIndex) ? LODData[InSlotIndex].NumGeometries : 0;
}

int32 FClothingSimulationCollider::GetOffset(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, int32 InSlotIndex) const
{
	const int32* const Offset = LODData.IsValidIndex(InSlotIndex) ? LODData[InSlotIndex].Offsets.Find(FSolverClothPair(Solver, Cloth)) : nullptr;
	return Offset ? *Offset : INDEX_NONE;
}

bool FClothingSimulationCollider::GetOffsetAndNumGeometries(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType, int32& OutOffset, int32& OutNumGeometries) const
{
	OutOffset = INDEX_NONE;
	OutNumGeometries = 0;

	const int32 LODIndex = LODIndices.FindChecked(FSolverClothPair(Solver, Cloth));
	const int32 SlotIndex = 
		(CollisionDataType < ECollisionDataType::LODs) ? (int32)CollisionDataType :
		(LODIndex >= (int32)ECollisionDataType::LODs) ? LODIndex : INDEX_NONE;

	if (LODData.IsValidIndex(SlotIndex))
	{
		OutOffset = LODData[SlotIndex].Offsets.FindChecked(FSolverClothPair(Solver, Cloth));
		OutNumGeometries = LODData[SlotIndex].NumGeometries;
	}

	return OutOffset != INDEX_NONE && OutNumGeometries > 0;
}

void FClothingSimulationCollider::Add(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	check(Solver);
	check(Cloth);

	// Can't add a collider twice to the same solver/cloth pair
	check(!LODIndices.Find(FSolverClothPair(Solver, Cloth)));

	// Initialize LODIndex
	int32& LODIndex = LODIndices.Add(FSolverClothPair(Solver, Cloth));
	LODIndex = INDEX_NONE;

	// Initialize scale
	const FClothingSimulationContextCommon* const Context = SkeletalMeshComponent ? static_cast<const FClothingSimulationContextCommon*>(SkeletalMeshComponent->GetClothingSimulationContext()) : nullptr;
	const FVec3 Scale3D = Context ? (FVec3)Context->ComponentToWorld.GetScale3D() : FVec3(1.f);
	UE_CLOG(FMath::Abs(Scale3D.X - Scale3D.Y) > KINDA_SMALL_NUMBER || FMath::Abs(Scale3D.X - Scale3D.Z) > KINDA_SMALL_NUMBER,
		LogChaosCloth, Warning, TEXT(
			"Actor '%s' component '%s' has a non uniform scale, and has a cloth simulation attached. "
			"The collision volumes might no longer correctly match the shape of the mesh. "
			"Please update this component transform scale with the same value for all scale axis."),
		SkeletalMeshComponent->GetOwner() ? *SkeletalMeshComponent->GetOwner()->GetName() : TEXT("None"),
		*SkeletalMeshComponent->GetName());
	Scale = Scale3D.X;

	// Create physics asset collisions, this will affect all LODs, so store it at index 0
	FClothCollisionData PhysicsAssetCollisionData;
	TArray<FLevelSetCollisionData> LevelSetCollisions;
	TArray<int32> UsedBoneIndices;
	ExtractPhysicsAssetCollision(PhysicsAssetCollisionData, LevelSetCollisions, UsedBoneIndices);

	LODData[(int32)ECollisionDataType::LODless].Add(Solver, Cloth, PhysicsAssetCollisionData, LevelSetCollisions, Scale, UsedBoneIndices);

	// Create legacy asset LOD collisions
	const int32 NumLODs = Asset ? Asset->LodData.Num() : 0;
	for (int32 Index = 0; Index < NumLODs; ++Index)
	{
		// Warn about legacy apex collisions
		const FClothCollisionData& AssetCollisionData = Asset->LodData[Index].CollisionData;
		UE_CLOG(AssetCollisionData.Spheres.Num() > 0 || AssetCollisionData.SphereConnections.Num() > 0 || AssetCollisionData.Convexes.Num() > 0,
			LogChaosCloth, Warning, TEXT(
				"Actor '%s' component '%s' has %d sphere, %d capsule, and %d convex collision objects for "
				"physics authored as part of a LOD construct, probably by the Apex cloth authoring system. "
				"This is deprecated. Please update your asset!"),
			SkeletalMeshComponent->GetOwner() ? *SkeletalMeshComponent->GetOwner()->GetName() : TEXT("None"),
			*SkeletalMeshComponent->GetName(),
			AssetCollisionData.Spheres.Num(),
			AssetCollisionData.SphereConnections.Num(),
			AssetCollisionData.Convexes.Num());

		const TArray<FLevelSetCollisionData> AssetLevelSetCollisions;		// Shouldn't be any level set collider on legacy asset

		// Add legacy collision
		LODData[(int32)ECollisionDataType::LODs + Index].Add(Solver, Cloth, AssetCollisionData, AssetLevelSetCollisions, Scale, Asset->UsedBoneIndices);
	}
}

void FClothingSimulationCollider::Remove(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	LODIndices.Remove(FSolverClothPair(Solver, Cloth));

	for (FLODData& LODDatum: LODData)
	{
		LODDatum.Remove(Solver, Cloth);
	}
}

void FClothingSimulationCollider::PreUpdate(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	check(Solver);
	check(Cloth);

	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulationCollider_PreUpdate);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothingSimulationColliderUpdate);

	// Add or re-add the external collision particles
	const int32 ExternalCollisionNumGeometries = GetNumGeometries((int32)ECollisionDataType::External);
	const int32 ExternalCollisionOffset = GetOffset(Solver, Cloth, (int32)ECollisionDataType::External);

	// TODO: Get level sets?
	const TArray<FLevelSetCollisionData> LevelSetCollisions;

	LODData[(int32)ECollisionDataType::External].Add(Solver, Cloth, CollisionData ? *CollisionData : FClothCollisionData(), LevelSetCollisions);
	
	// TODO: Find a better way in case the same number but different collisions are being re-added (hash collision data? Provide user dirty function?)
	bHasExternalCollisionChanged =
		ExternalCollisionNumGeometries != GetNumGeometries((int32)ECollisionDataType::External) ||
		ExternalCollisionOffset != GetOffset(Solver, Cloth, (int32)ECollisionDataType::External);
}

void FClothingSimulationCollider::Update(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	check(Solver);
	check(Cloth);
	TRACE_CPUPROFILER_EVENT_SCOPE(FClothingSimulationCollider_Update);
	SCOPE_CYCLE_COUNTER(STAT_ChaosClothingSimulationColliderUpdate);

	// Update the collision transforms
	// Note: All the LODs are updated at once, so that the previous transforms are always correct during LOD switching
	const FClothingSimulationContextCommon* const Context = SkeletalMeshComponent ? static_cast<const FClothingSimulationContextCommon*>(SkeletalMeshComponent->GetClothingSimulationContext()) : nullptr;
	for (FLODData& LODDatum : LODData)
	{
		LODDatum.Update(Solver, Cloth, Context);
	}

	// Update current LOD index
	int32& LODIndex = LODIndices.FindChecked(FSolverClothPair(Solver, Cloth));
	const int32 PrevLODIndex = LODIndex;
	LODIndex = (int32)ECollisionDataType::LODs + (bUseLODIndexOverride ? LODIndexOverride : Cloth ? Cloth->GetLODIndex(Solver) : INDEX_NONE);
	if (!LODData.IsValidIndex(LODIndex))
	{
		LODIndex = (int32)ECollisionDataType::LODs + INDEX_NONE;
	}

	// Enable particle if the external collisions have changed
	if (bHasExternalCollisionChanged)
	{
		// Enable non LOD collisions at first initialization
		LODData[(int32)ECollisionDataType::External].Enable(Solver, Cloth, true);

		// Update initial state for collisions
		LODData[(int32)ECollisionDataType::External].ResetStartPose(Solver, Cloth);
	}

	if (LODIndex != PrevLODIndex)
	{
		if (PrevLODIndex == INDEX_NONE)  // First run
		{
			// Enable non LOD collisions at first initialization
			LODData[(int32)ECollisionDataType::LODless].Enable(Solver, Cloth, true);

			// Update initial state for collisions
			LODData[(int32)ECollisionDataType::LODless].ResetStartPose(Solver, Cloth);
		}
		else if (PrevLODIndex >= (int32)ECollisionDataType::LODs)
		{
			// Disable previous LOD
			LODData[PrevLODIndex].Enable(Solver, Cloth, false);
		}
		if (LODIndex >= (int32)ECollisionDataType::LODs)
		{
			// Enable new LOD
			LODData[LODIndex].Enable(Solver, Cloth, true);

			if (PrevLODIndex == INDEX_NONE)
			{
				// Update initial state for collisions
				LODData[LODIndex].ResetStartPose(Solver, Cloth);
			}
		}
	}
}

void FClothingSimulationCollider::ResetStartPose(FClothingSimulationSolver* Solver, FClothingSimulationCloth* Cloth)
{
	for (FLODData& LODDatum : LODData)
	{
		LODDatum.ResetStartPose(Solver, Cloth);
	}
}

TConstArrayView<FSolverVec3> FClothingSimulationCollider::GetCollisionTranslations(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const
{
	check(Solver);
	check(Cloth);

	int32 Offset, NumGeometries;
	return GetOffsetAndNumGeometries(Solver, Cloth, CollisionDataType, Offset, NumGeometries) ?
		TConstArrayView<FSolverVec3>(Solver->GetCollisionParticleXs(Offset), NumGeometries) :
		TConstArrayView<FSolverVec3>();
}

TConstArrayView<Softs::FSolverRotation3> FClothingSimulationCollider::GetCollisionRotations(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const
{
	check(Solver);
	check(Cloth);

	int32 Offset, NumGeometries;
	return GetOffsetAndNumGeometries(Solver, Cloth, CollisionDataType, Offset, NumGeometries) ?
		TConstArrayView<Softs::FSolverRotation3>(Solver->GetCollisionParticleRs(Offset), NumGeometries) :
		TConstArrayView<Softs::FSolverRotation3>();
}

TConstArrayView<Softs::FSolverRigidTransform3> FClothingSimulationCollider::GetOldCollisionTransforms(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const
{
	check(Solver);
	check(Cloth);

	int32 Offset, NumGeometries;
	return GetOffsetAndNumGeometries(Solver, Cloth, CollisionDataType, Offset, NumGeometries) ?
		TConstArrayView<Softs::FSolverRigidTransform3>(Solver->GetOldCollisionTransforms(Offset), NumGeometries) :
		TConstArrayView<Softs::FSolverRigidTransform3>();
}

TConstArrayView<TUniquePtr<FImplicitObject>> FClothingSimulationCollider::GetCollisionGeometries(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const
{
	check(Solver);
	check(Cloth);

	int32 Offset, NumGeometries;
	return GetOffsetAndNumGeometries(Solver, Cloth, CollisionDataType, Offset, NumGeometries) ?
		TConstArrayView<TUniquePtr<FImplicitObject>>(Solver->GetCollisionGeometries(Offset), NumGeometries) :
		TConstArrayView<TUniquePtr<FImplicitObject>>();
}

TConstArrayView<bool> FClothingSimulationCollider::GetCollisionStatus(const FClothingSimulationSolver* Solver, const FClothingSimulationCloth* Cloth, ECollisionDataType CollisionDataType) const
{
	check(Solver);
	check(Cloth);

	int32 Offset, NumGeometries;
	return GetOffsetAndNumGeometries(Solver, Cloth, CollisionDataType, Offset, NumGeometries) ?
		TConstArrayView<bool>(Solver->GetCollisionStatus(Offset), NumGeometries) :
		TConstArrayView<bool>();
}

}  // End namespace Chaos
