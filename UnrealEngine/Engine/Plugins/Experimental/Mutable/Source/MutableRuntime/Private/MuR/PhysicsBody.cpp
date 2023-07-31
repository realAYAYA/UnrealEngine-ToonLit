// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/PhysicsBody.h"

#include "MuR/SerialisationPrivate.h"

namespace mu
{


	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::Serialise(const PhysicsBody* p, OutputArchive& arch)
	{
		arch << *p;
	}


	mu::Ptr<PhysicsBody> PhysicsBody::StaticUnserialise(InputArchive& arch)
	{
		mu::Ptr<PhysicsBody> pResult = new PhysicsBody();
		arch >> *pResult;
		return pResult;
	}

	//-------------------------------------------------------------------------------------------------
	mu::Ptr<PhysicsBody> PhysicsBody::Clone() const
	{
		mu::Ptr<PhysicsBody> pResult = new PhysicsBody();

		pResult->Bodies = Bodies;
		pResult->Bones = Bones;
		pResult->CustomIds = CustomIds;

		return pResult;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBodyCount(int32 Count)
	{
		Bodies.SetNum(Count);
		Bones.SetNum(Count);
		CustomIds.Init(-1, Count);
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetBodyCount() const
	{
		return Bodies.Num();
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetBodyBoneName(int32 B) const
	{
		check(B >= 0 && B < Bones.Num());

		return Bones[B].c_str();
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBodyBoneName(int32 B, const char* BoneName)
	{
		check(B >= 0 && B < Bones.Num());

		Bones[B] = BoneName;
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetBodyCustomId(int32 B) const
	{
		check(B >= 0 && B < CustomIds.Num());

		return CustomIds[B];
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBodyCustomId(int32 B, int32 CustomId)
	{
		check(B >= 0 && B < CustomIds.Num());

		CustomIds[B] = CustomId;
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetSphereCount(int32 B) const
	{
		return Bodies[B].Spheres.Num();
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetBoxCount(int32 B) const
	{
		return Bodies[B].Boxes.Num();
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetConvexCount(int32 B) const
	{
		return Bodies[B].Convex.Num();
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetSphylCount(int32 B) const
	{
		return Bodies[B].Sphyls.Num();
	}

	//-------------------------------------------------------------------------------------------------
	int32 PhysicsBody::GetTaperedCapsuleCount(int32 B) const
	{
		return Bodies[B].TaperedCapsules.Num();
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphereCount(int32 B, int32 Count)
	{
		Bodies[B].Spheres.SetNum(Count);
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBoxCount(int32 B, int32 Count)
	{
		Bodies[B].Boxes.SetNum(Count);
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetConvexCount(int32 B, int32 Count)
	{
		Bodies[B].Convex.SetNum(Count);
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphylCount(int32 B, int32 Count)
	{
		Bodies[B].Sphyls.SetNum(Count);
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetTaperedCapsuleCount(int32 B, int32 Count)
	{
		Bodies[B].TaperedCapsules.SetNum(Count);
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphere(
		int32 B, int32 I,
		FVector3f Position, float Radius)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		Bodies[B].Spheres[I].Position = Position;
		Bodies[B].Spheres[I].Radius = Radius;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBox(
		int32 B, int32 I,
		FVector3f Position, FQuat4f Orientation, FVector3f Size)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		Bodies[B].Boxes[I].Position = Position;
		Bodies[B].Boxes[I].Orientation = Orientation;
		Bodies[B].Boxes[I].Size = Size;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetConvex(
		int32 B, int32 I,
		const FVector3f* Vertices, int32 VerticesCount,
		const int32* Indices, int32 IndicesCount,
		const FTransform3f& Transform)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		Bodies[B].Convex[I].Vertices = TArray<FVector3f>(Vertices, VerticesCount);
		Bodies[B].Convex[I].Indices = TArray<int32>(Indices, IndicesCount);
		Bodies[B].Convex[I].Transform = Transform;

	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphyl(
		int32 B, int32 I,
		FVector3f Position, FQuat4f Orientation, float Radius, float Length)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		Bodies[B].Sphyls[I].Position = Position;
		Bodies[B].Sphyls[I].Orientation = Orientation;
		Bodies[B].Sphyls[I].Radius = Radius;
		Bodies[B].Sphyls[I].Length = Length;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphereFlags(int32 B, int32 I, uint32 Flags)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		Bodies[B].Spheres[I].Flags = Flags;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBoxFlags(int32 B, int32 I, uint32 Flags)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		Bodies[B].Boxes[I].Flags = Flags;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetConvexFlags(int32 B, int32 I, uint32 Flags)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		Bodies[B].Convex[I].Flags = Flags;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphylFlags(int32 B, int32 I, uint32 Flags)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		Bodies[B].Sphyls[I].Flags = Flags;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetTaperedCapsuleFlags(int32 B, int32 I, uint32 Flags)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		Bodies[B].TaperedCapsules[I].Flags = Flags;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphereName(int32 B, int32 I, const char* Name)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		Bodies[B].Spheres[I].Name = Name;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetBoxName(int32 B, int32 I, const char* Name)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		Bodies[B].Boxes[I].Name = Name;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetConvexName(int32 B, int32 I, const char* Name)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		Bodies[B].Convex[I].Name = Name;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetSphylName(int32 B, int32 I, const char* Name)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		Bodies[B].Sphyls[I].Name = Name;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetTaperedCapsuleName(int32 B, int32 I, const char* Name)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		Bodies[B].TaperedCapsules[I].Name = Name;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::SetTaperedCapsule(
		int32 B, int32 I,
		FVector3f Position, FQuat4f Orientation,
		float Radius0, float Radius1, float Length)
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		Bodies[B].TaperedCapsules[I].Position = Position;
		Bodies[B].TaperedCapsules[I].Orientation = Orientation;
		Bodies[B].TaperedCapsules[I].Radius0 = Radius0;
		Bodies[B].TaperedCapsules[I].Radius1 = Radius1;
		Bodies[B].TaperedCapsules[I].Length = Length;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::GetSphere(
		int32 B, int32 I,
		FVector3f& OutPosition, float& OutRadius) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		OutPosition = Bodies[B].Spheres[I].Position;
		OutRadius = Bodies[B].Spheres[I].Radius;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::GetBox(
		int32 B, int32 I,
		FVector3f& OutPosition, FQuat4f& OutOrientation, FVector3f& OutSize) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		OutPosition = Bodies[B].Boxes[I].Position;
		OutOrientation = Bodies[B].Boxes[I].Orientation;
		OutSize = Bodies[B].Boxes[I].Size;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::GetConvex(
		int32 B, int32 I,
		FVector3f const*& OutVertices, int32& OutVerticesCount,
		int32 const*& OutIndices, int32& OutIndicesCount,
		FTransform3f& OutTransform) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		OutVertices = Bodies[B].Convex[I].Vertices.GetData();
		OutVerticesCount = Bodies[B].Convex[I].Vertices.Num();

		OutIndices = Bodies[B].Convex[I].Indices.GetData();
		OutIndicesCount = Bodies[B].Convex[I].Indices.Num();

		OutTransform = Bodies[B].Convex[I].Transform;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::GetSphyl(
		int32 B, int32 I,
		FVector3f& OutPosition, FQuat4f& OutOrientation, float& OutRadius, float& OutLength) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		OutPosition = Bodies[B].Sphyls[I].Position;
		OutOrientation = Bodies[B].Sphyls[I].Orientation;
		OutRadius = Bodies[B].Sphyls[I].Radius;
		OutLength = Bodies[B].Sphyls[I].Length;
	}

	//-------------------------------------------------------------------------------------------------
	void PhysicsBody::GetTaperedCapsule(
		int32 B, int32 I,
		FVector3f& OutPosition, FQuat4f& OutOrientation,
		float& OutRadius0, float& OutRadius1, float& OutLength) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		OutPosition = Bodies[B].TaperedCapsules[I].Position;
		OutOrientation = Bodies[B].TaperedCapsules[I].Orientation;
		OutRadius0 = Bodies[B].TaperedCapsules[I].Radius0;
		OutRadius1 = Bodies[B].TaperedCapsules[I].Radius1;
		OutLength = Bodies[B].TaperedCapsules[I].Length;
	}


	//-------------------------------------------------------------------------------------------------
	uint32 PhysicsBody::GetSphereFlags(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		return Bodies[B].Spheres[I].Flags;
	}

	//-------------------------------------------------------------------------------------------------
	uint32 PhysicsBody::GetBoxFlags(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		return Bodies[B].Boxes[I].Flags;
	}

	//-------------------------------------------------------------------------------------------------
	uint32 PhysicsBody::GetConvexFlags(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		return Bodies[B].Convex[I].Flags;
	}

	//-------------------------------------------------------------------------------------------------
	uint32 PhysicsBody::GetSphylFlags(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		return Bodies[B].Sphyls[I].Flags;
	}

	//-------------------------------------------------------------------------------------------------
	uint32 PhysicsBody::GetTaperedCapsuleFlags(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		return Bodies[B].TaperedCapsules[I].Flags;
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetSphereName(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Spheres.Num());

		return Bodies[B].Spheres[I].Name.c_str();
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetBoxName(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Boxes.Num());

		return Bodies[B].Boxes[I].Name.c_str();
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetConvexName(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Convex.Num());

		return Bodies[B].Convex[I].Name.c_str();
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetSphylName(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].Sphyls.Num());

		return Bodies[B].Sphyls[I].Name.c_str();
	}

	//-------------------------------------------------------------------------------------------------
	const char* PhysicsBody::GetTaperedCapsuleName(int32 B, int32 I) const
	{
		check(B >= 0 && B < Bodies.Num());
		check(I >= 0 && I < Bodies[B].TaperedCapsules.Num());

		return Bodies[B].TaperedCapsules[I].Name.c_str();
	}

}
