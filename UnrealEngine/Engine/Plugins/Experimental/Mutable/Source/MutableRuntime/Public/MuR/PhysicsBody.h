// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <string>

#include "MuR/Serialisation.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"


namespace mu
{
	struct FBodyShape
	{
		FString Name;
		uint32 Flags = 0;

		bool operator==(const FBodyShape& Other) const 
		{
			return Flags == Other.Flags && Name == Other.Name;
		}
	
		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};

	struct FSphereBody : FBodyShape
	{
		FVector3f Position = FVector3f::ZeroVector;
		float Radius = 0.0f;

		bool operator==( const FSphereBody& Other ) const
		{
			return FBodyShape::operator==(Other) && Position == Other.Position && Radius == Other.Radius;
		}
		 
		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};
	
	struct FBoxBody : FBodyShape
	{
		FVector3f Position = FVector3f::ZeroVector;
		FQuat4f Orientation = FQuat4f::Identity;
		FVector3f Size = FVector3f::ZeroVector;
		 
		inline bool operator==( const FBoxBody& Other ) const 
		{
			return FBodyShape::operator==(Other) && Position == Other.Position && Orientation == Other.Orientation && Size == Other.Size;
		}

		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};

	struct FSphylBody : FBodyShape
	{
		FVector3f Position = FVector3f::ZeroVector;
		FQuat4f Orientation = FQuat4f::Identity;
		float Radius = 0.0f;
		float Length = 0.0f;

		inline bool operator==( const FSphylBody& Other ) const 
		{
			return FBodyShape::operator==(Other) && 
				Position == Other.Position && 
				Orientation == Other.Orientation && 
				Radius == Other.Radius && 
				Length == Other.Length;
		}
		 
		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};
	
	struct FTaperedCapsuleBody : FBodyShape
	{
		FVector3f Position;
		FQuat4f Orientation;
		float Radius0 = 0.0f;
		float Radius1 = 0.0f;
		float Length = 0.0f;
		 
		inline bool operator==( const FTaperedCapsuleBody& Other ) const 
		{
			return FBodyShape::operator==(Other) && 
				Position == Other.Position && 
				Orientation == Other.Orientation && 
				Radius0 == Other.Radius0 && 
				Radius1 == Other.Radius1 && 
				Length == Other.Length;
		}
		 
		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};
	
	struct FConvexBody : FBodyShape
	{
		TArray<FVector3f> Vertices;
		TArray<int32> Indices;

		FTransform3f Transform = FTransform3f::Identity;
		
		inline bool operator==( const FConvexBody& Other ) const
		{
			return FBodyShape::operator==(Other) && 
				Vertices == Other.Vertices && 
				Indices == Other.Indices && 
				Transform.Equals(Other.Transform);
		}

		//!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};
	
	struct FPhysicsBodyAggregate
	{
		TArray<FSphereBody> Spheres;
		TArray<FBoxBody> Boxes;
		TArray<FConvexBody> Convex;
		TArray<FSphylBody> Sphyls;
		TArray<FTaperedCapsuleBody> TaperedCapsules;

		inline bool operator==( const FPhysicsBodyAggregate& Other ) const
		{
			return Spheres == Other.Spheres &&
					Boxes == Other.Boxes &&
					Sphyls == Other.Sphyls &&
					TaperedCapsules == Other.TaperedCapsules &&
					Convex == Other.Convex;
		}

		 //!
		inline void Serialise( OutputArchive& arch ) const;

		//!
		inline void Unserialise( InputArchive& arch );
	};

	class MUTABLERUNTIME_API PhysicsBody : public RefCounted
	{
	public:

		PhysicsBody() 
		{
		}
		
		mu::Ptr<PhysicsBody> Clone() const; 

		//! Serialisation
        static void Serialise( const PhysicsBody* p, OutputArchive& arch );
		static mu::Ptr<PhysicsBody> StaticUnserialise( InputArchive& arch );

        //bool operator==( const PhysicsBody& Other ) const;

		void SetCustomId(int32 Id);
		int32 GetCustomId() const;

		void SetBodyCount(int32 B);
		int32 GetBodyCount() const;
	
		void SetBodyBoneId(int32 B, uint16 BoneId);
		uint16 GetBodyBoneId(int32 B) const;
		
		void SetBodyCustomId(int32 B, int32 BodyCustomId);
		int32 GetBodyCustomId(int32 B) const;
		
		int32 GetSphereCount(int32 B) const;
		int32 GetBoxCount(int32 B) const;
		int32 GetConvexCount(int32 B) const;
		int32 GetSphylCount(int32 B) const;
		int32 GetTaperedCapsuleCount(int32 B) const;
		
		void SetSphereCount(int32 B, int32 Count);
		void SetBoxCount(int32 B, int32 Count);
		void SetConvexCount(int32 B, int32 Count);
		void SetSphylCount(int32 B, int32 Count);
		void SetTaperedCapsuleCount(int32 B, int32 Count);
		
		void SetSphere( 
				int32 B, int32 I, 
				FVector3f Position, float Radius);

		void SetBox( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, FVector3f Size);

		void SetConvexMesh( 
				int32 B, int32 I,
				TArrayView<const FVector3f> Vertices, TArrayView<const int32> Indices);

		void SetConvexTransform( 
				int32 B, int32 I, 
				const FTransform3f& Transform);

		void SetSphyl( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, 
				float Radius, float Length);

		void SetTaperedCapsule( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, 
				float Radius0, float Radius1, float Length);

		void SetSphereFlags(int32 B, int32 I, uint32 Flags);
		void SetBoxFlags(int32 B, int32 I, uint32 Flags);
		void SetConvexFlags(int32 B, int32 I, uint32 Flags);
		void SetSphylFlags(int32 B, int32 I, uint32 Flags);
		void SetTaperedCapsuleFlags(int32 B, int32 I, uint32 Flags);

		void SetSphereName(int32 B, int32 I, const char* Name);
		void SetBoxName(int32 B, int32 I, const char* Name);
		void SetConvexName(int32 B, int32 I, const char* Name);
		void SetSphylName(int32 B, int32 I, const char* Name);
		void SetTaperedCapsuleName(int32 B, int32 I, const char* Name);
	
		void GetSphere( 
				int32 B, int32 I, 
				FVector3f& OutPosition, float& OutRadius) const;

		void GetBox( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, FVector3f& OutSize) const;

		void GetConvex( 
				int32 B, int32 I, 
				TArrayView<const FVector3f>& OutVertices, TArrayView<const int32>& OutIndices, FTransform3f& OutTransform) const;

		void GetConvexMeshView(
				int32 B, int32 I, 
			    TArrayView<FVector3f>& OutVerticesView, TArrayView<int32>& OutIndicesView);

		void GetConvexTransform(
				int32 B, int32 I,
				FTransform3f& OutTransform) const;

		void GetSphyl( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, 
				float& OutRadius, float& OutLength) const;

		void GetTaperedCapsule( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, 
				float& OutRadius0, float& OutRadius1, float& OutLength) const;
		
		uint32 GetSphereFlags(int32 B, int32 I) const;
		uint32 GetBoxFlags(int32 B, int32 I) const;
		uint32 GetConvexFlags(int32 B, int32 I) const;
		uint32 GetSphylFlags(int32 B, int32 I) const;
		uint32 GetTaperedCapsuleFlags(int32 B, int32 I) const;

		const FString& GetSphereName(int32 B, int32 I) const;
		const FString& GetBoxName(int32 B, int32 I) const;
		const FString& GetConvexName(int32 B, int32 I) const;
		const FString& GetSphylName(int32 B, int32 I) const;
		const FString& GetTaperedCapsuleName(int32 B, int32 I) const;

	protected:
		//! Forbidden. Manage with the Ptr<> template.
		~PhysicsBody()
		{
		}

	public:
		int32 CustomId = -1;

		// Bone name the physics volume aggregate is bound to. 
		TArray<uint16> BoneIds;
		TArray<FPhysicsBodyAggregate> Bodies;
		TArray<int32> BodiesCustomIds;


		bool bBodiesModified = false;

        //!
        inline void Serialise( OutputArchive& arch ) const;

		//!
        inline void Unserialise( InputArchive& arch );

		//!
		inline bool operator==(const PhysicsBody& Other) const
        {
        	return CustomId        == Other.CustomId        &&
				   BoneIds == Other.BoneIds					&&
        		   Bodies          == Other.Bodies          &&
        	       BodiesCustomIds == Other.BodiesCustomIds &&
				   bBodiesModified == Other.bBodiesModified;
        }
	};
}
