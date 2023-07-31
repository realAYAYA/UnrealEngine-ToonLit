// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaosTestOverlap.h"

#include "HeadlessChaos.h"
#include "Chaos/Capsule.h"
#include "Chaos/Box.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Chaos/ImplicitObjectScaled.h"

namespace ChaosTest
{
	using namespace Chaos;

	void OverlapTriMesh()
	{
		// Trimesh is simple pyramid
		using namespace Chaos;
		FTriangleMeshImplicitObject::ParticlesType TrimeshParticles(
			{
				{-10.0, 0.0, 0.0},
				{10.0, 0.0, 0.0},
				{0.0, 10.0, 0.0},
				{0.0, -10.0, 0.0},
				{0.0,  0.0, 10.0},
			});

		TArray<TVec3<int32>> Indices(
			{
				{0, 1, 2},
				{0, 3, 1},
				{0, 1, 2},
				{0, 2, 4},
				{1, 2, 4},
				{0, 3, 4},
				{1, 3, 4}
			});

		TArray<uint16> Materials;
		for (int32 i = 0; i < Indices.Num(); ++i)
		{
			Materials.Emplace(0);
		}

		TUniquePtr<FTriangleMeshImplicitObject> TriangleMesh = MakeUnique<FTriangleMeshImplicitObject>(MoveTemp(TrimeshParticles), MoveTemp(Indices), MoveTemp(Materials));
		{
			// Capsule test
			const FVec3 X1 = { 0.0, 0.0, -2.0 };
			const FVec3 X2 = { 0.0, 0.0, 2.0 };
			const FReal Radius = 1.0;
			const FCapsule Capsule = FCapsule(X1, X2, Radius);

			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat(FVec3(1.0, 0.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				// Inside Mesh
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 4.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Capsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
		}
		{
			const FVec3 X1 = { 0.0, 0.0, -200.0 };
			const FVec3 X2 = { 0.0, 0.0, 200.0 };
			const FReal Radius = 100.0;
			const FCapsule Capsule = FCapsule(X1, X2, Radius);
			TSharedPtr<FCapsule> CapsuleShared = MakeShared<FCapsule>(X1, X2, Radius);
			FVec3 TriMeshScale = { 0.01, 0.01, 0.01 };
			TImplicitObjectScaled<FCapsule> ScaledCapsule = TImplicitObjectScaled<FCapsule>(CapsuleShared, TriMeshScale);
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat(FVec3(1.0, 0.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				// Inside Mesh
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 4.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledCapsule, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
		}
		{
			// Box test
			const TBox<FReal, 3> Box = TBox<FReal, 3>({ -1.0, -1.0, -2.0 }, { 1.0, 1.0, 2.0 });
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat(FVec3(1.0, 0.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				// Inside Mesh
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 4.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Box, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
		}

		// Box Scaled
		{
			TSharedPtr<TBox<FReal, 3>> BigBoxSafe = MakeShared<TBox<FReal, 3>>(FVec3(-100.0, -100.0, -100.0), FVec3(100.0, 100.0, 100.0));
			FVec3 TriMeshScale = { 0.01, 0.01, 0.02 };
			TImplicitObjectScaled<TBox<FReal, 3>> ScaledBox = TImplicitObjectScaled<TBox<FReal, 3>>(BigBoxSafe, TriMeshScale);
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat(FVec3(1.0, 0.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				// Inside Mesh
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 4.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
		}
		// Box Scaled test non uniform transform with rotation
		{
			TSharedPtr<TBox<FReal, 3>> BigBoxSafe = MakeShared<TBox<FReal, 3>>(FVec3(-100.0, -100.0, -100.0), FVec3(100.0, 100.0, 100.0));
			FVec3 TriMeshScale = { 0.01, 0.01, 0.05 };
			TImplicitObjectScaled<TBox<FReal, 3>> ScaledBox = TImplicitObjectScaled<TBox<FReal, 3>>(BigBoxSafe, TriMeshScale);

			{
				const FRigidTransform3 QueryTM(FVec3(5.5, 0.0, 10.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(7.0, 0.0, 10.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(7.0, 0.0, 10.0), FQuat(FVec3(0.0, 1.0, 0.0), -3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(7.0, 0.0, 10.0), FQuat(FVec3(0.0, 1.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(ScaledBox, QueryTM, 0.0, nullptr, TriMeshScale);
				EXPECT_EQ(bResult, true);
			}
		}
		{
			// Sphere test
			const TSphere<FReal, 3> Sphere = TSphere<FReal, 3>({ 0.0, 0.0, 0.0 }, 2.0);
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, -10.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(-10.0, 0.0, 0.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 12.0), FQuat(FVec3(1.0, 0.0, 0.0), 3.1415926 / 4.0));
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, true);
			}
			{
				// Inside Mesh
				const FRigidTransform3 QueryTM(FVec3(0.0, 0.0, 4.0), FQuat::Identity);
				bool bResult = TriangleMesh->OverlapGeom(Sphere, QueryTM, 0.0);
				EXPECT_EQ(bResult, false);
			}
		}
	}
}