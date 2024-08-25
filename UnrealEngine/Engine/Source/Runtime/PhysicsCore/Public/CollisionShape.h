// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Math/Vector.h"

/** Types of Collision Shapes that are used by Trace **/
namespace ECollisionShape
{
	enum Type
	{
		Line,
		Box,
		Sphere,
		Capsule
	};
};

/** Collision Shapes that supports Sphere, Capsule, Box, or Line **/
struct FCollisionShape
{
	ECollisionShape::Type ShapeType;

	static FORCEINLINE constexpr float MinBoxExtent() { return UE_KINDA_SMALL_NUMBER; }
	static FORCEINLINE constexpr float MinSphereRadius() { return UE_KINDA_SMALL_NUMBER; }
	static FORCEINLINE constexpr float MinCapsuleRadius() { return UE_KINDA_SMALL_NUMBER; }
	static FORCEINLINE constexpr float MinCapsuleAxisHalfHeight() { return UE_KINDA_SMALL_NUMBER; }

	/** Union that supports up to 3 floats **/
	union
	{
		struct
		{
			float HalfExtentX;
			float HalfExtentY;
			float HalfExtentZ;
		} Box;

		struct
		{
			float Radius;
		} Sphere;

		struct
		{
			float Radius;
			float HalfHeight;
		} Capsule;
	};

	FCollisionShape()
	{
		ShapeType = ECollisionShape::Line;
	}

	/** Is the shape currently a Line (Default)? */
	bool IsLine() const
	{
		return ShapeType == ECollisionShape::Line;
	}

	/** Is the shape currently a box? */
	bool IsBox() const
	{
		return ShapeType == ECollisionShape::Box;
	}

	/** Is the shape currently a sphere? */
	bool IsSphere() const
	{
		return ShapeType == ECollisionShape::Sphere;
	}

	/** Is the shape currently a capsule? */
	bool IsCapsule() const
	{
		return ShapeType == ECollisionShape::Capsule;
	}

	/** Utility function to Set Box and dimension */
	void SetBox(const FVector3f& HalfExtent)
	{
		ShapeType = ECollisionShape::Box;
		Box.HalfExtentX = HalfExtent.X;
		Box.HalfExtentY = HalfExtent.Y;
		Box.HalfExtentZ = HalfExtent.Z;
	}

	/** Utility function to set Sphere with Radius */
	void SetSphere(const float Radius)
	{
		ShapeType = ECollisionShape::Sphere;
		Sphere.Radius = Radius;
	}

	/** Utility function to set Capsule with Radius and Half Height */
	void SetCapsule(const float Radius, const float HalfHeight)
	{
		ShapeType = ECollisionShape::Capsule;
		Capsule.Radius = Radius;
		Capsule.HalfHeight = HalfHeight;
	}

	/** Utility function to set Capsule from Extent data */
	void SetCapsule(const FVector3f& Extent)
	{
		ShapeType = ECollisionShape::Capsule;
		Capsule.Radius = FMath::Max(Extent.X, Extent.Y);
		Capsule.HalfHeight = Extent.Z;
	}

	/** Return true if nearly zero. If so, it will back out and use line trace instead */
	bool IsNearlyZero() const
	{
		switch (ShapeType)
		{
		case ECollisionShape::Box:
		{
			return (Box.HalfExtentX <= FCollisionShape::MinBoxExtent() && Box.HalfExtentY <= FCollisionShape::MinBoxExtent() && Box.HalfExtentZ <= FCollisionShape::MinBoxExtent());
		}
		case  ECollisionShape::Sphere:
		{
			return (Sphere.Radius <= FCollisionShape::MinSphereRadius());
		}
		case ECollisionShape::Capsule:
		{
			// @Todo check height? It didn't check before, so I'm keeping this way for time being
			return (Capsule.Radius <= FCollisionShape::MinCapsuleRadius());
		}
		}

		return true;
	}

	/** Utility function to return Extent of the shape */
	FVector GetExtent() const
	{
		switch (ShapeType)
		{
		case ECollisionShape::Box:
		{
			return FVector(Box.HalfExtentX, Box.HalfExtentY, Box.HalfExtentZ);
		}
		case  ECollisionShape::Sphere:
		{
			return FVector(Sphere.Radius, Sphere.Radius, Sphere.Radius);
		}
		case ECollisionShape::Capsule:
		{
			// @Todo check height? It didn't check before, so I'm keeping this way for time being
			return FVector(Capsule.Radius, Capsule.Radius, Capsule.HalfHeight);
		}
		}

		return FVector::ZeroVector;
	}

	/** Get distance from center of capsule to center of sphere ends */
	float GetCapsuleAxisHalfLength() const
	{
		ensure(ShapeType == ECollisionShape::Capsule);
		return FMath::Max<float>(Capsule.HalfHeight - Capsule.Radius, FCollisionShape::FCollisionShape::MinCapsuleAxisHalfHeight());
	}

	/** Utility function to get Box Extention */
	FVector GetBox() const
	{
		return FVector(Box.HalfExtentX, Box.HalfExtentY, Box.HalfExtentZ);
	}

	/** Utility function to get Sphere Radius */
	const float GetSphereRadius() const
	{
		return Sphere.Radius;
	}

	/** Utility function to get Capsule Radius */
	const float GetCapsuleRadius() const
	{
		return Capsule.Radius;
	}

	/** Utility function to get Capsule Half Height */
	const float GetCapsuleHalfHeight() const
	{
		return Capsule.HalfHeight;
	}

	/** Used by engine in multiple places. Since LineShape doesn't need any dimension, declare once and used by all codes. */
	static struct FCollisionShape LineShape;

	/** Static utility function to make a box */
	static FCollisionShape MakeBox(const FVector& BoxHalfExtent)
	{
		FCollisionShape BoxShape;
		BoxShape.SetBox(FVector3f(BoxHalfExtent));
		return BoxShape;
	}

	/** Static utility function to make a box */
	static FCollisionShape MakeBox(const FVector3f& BoxHalfExtent)
	{
		FCollisionShape BoxShape;
		BoxShape.SetBox(BoxHalfExtent);
		return BoxShape;
	}

	/** Static utility function to make a sphere */
	static FCollisionShape MakeSphere(const float SphereRadius)
	{
		FCollisionShape SphereShape;
		SphereShape.SetSphere(SphereRadius);
		return SphereShape;
	}

	/** Static utility function to make a capsule */
	static FCollisionShape MakeCapsule(const float CapsuleRadius, const float CapsuleHalfHeight)
	{
		FCollisionShape CapsuleShape;
		CapsuleShape.SetCapsule(CapsuleRadius, CapsuleHalfHeight);
		return CapsuleShape;
	}

	/** Static utility function to make a capsule */
	static FCollisionShape MakeCapsule(const FVector& Extent)
	{
		FCollisionShape CapsuleShape;
		CapsuleShape.SetCapsule(FVector3f(Extent));
		return CapsuleShape;
	}
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
