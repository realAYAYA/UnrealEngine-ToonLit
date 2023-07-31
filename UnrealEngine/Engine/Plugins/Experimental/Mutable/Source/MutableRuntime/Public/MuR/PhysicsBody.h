// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/SerialisationPrivate.h"
#include "MuR/MemoryPrivate.h"
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
		string Name;
		uint32 Flags = 0;

		bool operator==(const FBodyShape& Other) const 
		{
			return Flags == Other.Flags && Name == Other.Name;
		}
	
		//!
		inline void Serialise( OutputArchive& arch ) const
		{
			const uint32 ver = 0;
			arch << ver;

			arch << Name;
			arch << Flags;
		}

		//!
		inline void Unserialise( InputArchive& arch )
		{
			uint32 ver;
			arch >> ver;
			check(ver == 0);

			arch >> Name;
			arch >> Flags;
		}
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
		inline void Serialise( OutputArchive& arch ) const
		{
			FBodyShape::Serialise(arch);

			const uint32 ver = 0;
			arch << ver;

			arch << Position;
			arch << Radius;
		}

		//!
		inline void Unserialise( InputArchive& arch )
		{
			FBodyShape::Unserialise(arch);

			uint32 ver;
			arch >> ver;
			check(ver == 0);

			arch >> Position;
			arch >> Radius;
		} 
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
		inline void Serialise( OutputArchive& arch ) const
		{
			FBodyShape::Serialise( arch );

			const uint32 ver = 0;
			arch << ver;

			arch << Position;
			arch << Orientation;
			arch << Size;
		}

		  //!
		inline void Unserialise( InputArchive& arch )
		{
			FBodyShape::Unserialise( arch );

			uint32 ver;
			arch >> ver;
			check(ver == 0);

			arch >> Position;
			arch >> Orientation;
			arch >> Size;
		} 
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
		inline void Serialise( OutputArchive& arch ) const
		{
			FBodyShape::Serialise( arch );

			const uint32 ver = 0;
			arch << ver;

			arch << Position;
			arch << Orientation;
			arch << Radius;
			arch << Length;
		}

		//!
		inline void Unserialise( InputArchive& arch )
		{
			FBodyShape::Unserialise( arch );

			uint32 ver;
			arch >> ver;
			check(ver == 0);

			arch >> Position;
			arch >> Orientation;
			arch >> Radius;
			arch >> Length;
		}
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
		inline void Serialise( OutputArchive& arch ) const
		{
			FBodyShape::Serialise( arch );

			const uint32 ver = 0;
			arch << ver;

			arch << Position;
			arch << Orientation;
			arch << Radius0;
			arch << Radius1;
			arch << Length;
		}

		 //!
		inline void Unserialise( InputArchive& arch )
		{
			FBodyShape::Unserialise( arch );
			
			uint32 ver;
			arch >> ver;
			check(ver == 0);

			arch >> Position;
			arch >> Orientation;
			arch >> Radius0;
			arch >> Radius1;
			arch >> Length;
		 }
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
		inline void Serialise( OutputArchive& arch ) const
		{
			FBodyShape::Serialise( arch );
	
		 	uint32 ver = 0;
		 	arch << ver;

		 	arch << Vertices;
		 	arch << Indices;
		 	arch << Transform;
		}

		  //!
		inline void Unserialise( InputArchive& arch )
		{
		 	uint32 ver;
		 	arch >> ver;
		 	check(ver == 0);

		 	arch >> Vertices;
		 	arch >> Indices;
		 	arch >> Transform;
		}
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
		inline void Serialise( OutputArchive& arch ) const
		{
		 	uint32 ver = 0;
		 	arch << ver;
		
		 	arch << Spheres;
		 	arch << Boxes;
		 	arch << Convex;
		 	arch << Sphyls;
		 	arch << TaperedCapsules;
		}

		 //!
		inline void Unserialise( InputArchive& arch )
		{
		 	uint32 ver;
		 	arch >> ver;
		 	check(ver == 0);
		
		 	arch >> Spheres;
		 	arch >> Boxes;
		 	arch >> Convex;
		 	arch >> Sphyls;
		 	arch >> TaperedCapsules;
		}
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

		void SetBodyCount( int32 B );
		int32 GetBodyCount( ) const;

		void SetBodyBoneName( int32 B, const char* BoneName );
		const char* GetBodyBoneName( int32 B ) const;
	
		void SetBodyCustomId( int32 B, int32 CustomId );
		int32 GetBodyCustomId( int32 B ) const;
		
		int32 GetSphereCount( int32 B ) const;
		int32 GetBoxCount( int32 B) const;
		int32 GetConvexCount( int32 B) const;
		int32 GetSphylCount( int32 B ) const;
		int32 GetTaperedCapsuleCount(int32 B ) const;
		
		void SetSphereCount(int32 B, int32 Count);
		void SetBoxCount(int32 B, int32 Count);
		void SetConvexCount(int32 B, int32 Count);
		void SetSphylCount(int32 B, int32 Count);
		void SetTaperedCapsuleCount( int32 B, int32 Count);
		
		void SetSphere( 
				int32 B, int32 I, 
				FVector3f Position, float Radius );

		void SetBox( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, FVector3f Size );

		void SetConvex( 
				int32 B, int32 I,
				const FVector3f* Vertices, int32 VerticesCount, 
				const int32* Indices, int32 IndicesCount, 
				const FTransform3f& Transform );

		void SetSphyl( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, 
				float Radius, float Length );

		void SetTaperedCapsule( 
				int32 B, int32 I, 
				FVector3f Position, FQuat4f Orientation, 
				float Radius0, float Radius1, float Length );

		void SetSphereFlags( int32 B, int32 I, uint32 Flags );
		void SetBoxFlags( int32 B, int32 I, uint32 Flags );
		void SetConvexFlags( int32 B, int32 I, uint32 Flags );
		void SetSphylFlags( int32 B, int32 I, uint32 Flags );
		void SetTaperedCapsuleFlags( int32 B, int32 I, uint32 Flags );

		void SetSphereName( int32 B, int32 I, const char* Name );
		void SetBoxName( int32 B, int32 I, const char* Name );
		void SetConvexName( int32 B, int32 I, const char* Name );
		void SetSphylName( int32 B, int32 I, const char* Name );
		void SetTaperedCapsuleName( int32 B, int32 I, const char* Name );
	
		void GetSphere( 
				int32 B, int32 I, 
				FVector3f& OutPosition, float& OutRadius) const;

		void GetBox( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, FVector3f& OutSize ) const;

		void GetConvex( 
				int32 B, int32 I, 
				FVector3f const*& OutVertices, int32& OutVerticesCount, 
				int32 const*& OutIndices, int32& OutIndicesCount, FTransform3f& OutTransform ) const;

		void GetSphyl( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, 
				float& OutRadius, float& OutLength) const;

		void GetTaperedCapsule( 
				int32 B, int32 I, 
				FVector3f& OutPosition, FQuat4f& OutOrientation, 
				float& OutRadius0, float& OutRadius1, float& OutLength) const;
		
		uint32 GetSphereFlags( int32 B, int32 I ) const;
		uint32 GetBoxFlags( int32 B, int32 I ) const;
		uint32 GetConvexFlags( int32 B, int32 I ) const;
		uint32 GetSphylFlags( int32 B, int32 I ) const;
		uint32 GetTaperedCapsuleFlags( int32 B, int32 I ) const;

		const char* GetSphereName( int32 B, int32 I ) const;
		const char* GetBoxName( int32 B, int32 I ) const;
		const char* GetConvexName( int32 B, int32 I ) const;
		const char* GetSphylName( int32 B, int32 I ) const;
		const char* GetTaperedCapsuleName( int32 B, int32 I ) const;

	protected:
		//! Forbidden. Manage with the Ptr<> template.
		~PhysicsBody()
		{
		}

	public:
		// Bone name the physics volume aggregate is bound to. 
		TArray<string> Bones;
		TArray<FPhysicsBodyAggregate> Bodies;
		TArray<int32> CustomIds;

		bool bBodiesModified = false;

        //!
        inline void Serialise( OutputArchive& arch ) const
		{
            uint32 ver = 1;
			arch << ver;
        	
        	arch << Bodies;
        	arch << Bones;
        	arch << CustomIds;
			arch << bBodiesModified;
        }

        //!
        inline void Unserialise( InputArchive& arch )
		{
            uint32 ver;
			arch >> ver;
            check(ver <= 1);
        	
        	arch >> Bodies;
        	arch >> Bones;
        	arch >> CustomIds;

			if (ver >= 1)
			{
				arch >> bBodiesModified;
			}
		}
		
        //!
		inline bool operator==( const PhysicsBody& o ) const
        {
        	return Bones == o.Bones &&
        		   Bodies == o.Bodies &&
        	       CustomIds == o.CustomIds &&
				   bBodiesModified == o.bBodiesModified;
        }
	};
}
