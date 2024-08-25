// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Cylinder.h"
#include "Chaos/GJKShape.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectUnion.h"
#include "Chaos/Sphere.h"
#include "Chaos/Segment.h"
#include "ChaosArchive.h"

#include "Math/VectorRegister.h"

#include "UObject/ReleaseObjectVersion.h"

namespace Chaos
{
	struct FCapsuleSpecializeSamplingHelper;

	class FCapsule final : public FImplicitObject
	{
	public:
		using FImplicitObject::SignedDistance;
		using FImplicitObject::GetTypeName;

		FCapsule()
		    : FImplicitObject(EImplicitObject::FiniteConvex, ImplicitObjectType::Capsule)
		{}
		FCapsule(const FVec3& x1, const FVec3& x2, const FReal Radius)
		    : FImplicitObject(EImplicitObject::FiniteConvex, ImplicitObjectType::Capsule)
			, MSegment(x1, x2)
		{
			SetRadius(Radius);
		}

		FCapsule(const FCapsule& Other)
		    : FImplicitObject(EImplicitObject::FiniteConvex, ImplicitObjectType::Capsule)
			, MSegment(Other.MSegment)
		{
			SetRadius(Other.GetRadius());
		}

		FCapsule(FCapsule&& Other)
		    : FImplicitObject(EImplicitObject::FiniteConvex, ImplicitObjectType::Capsule)
			, MSegment(MoveTemp(Other.MSegment))
		{
			SetRadius(Other.GetRadius());
		}

		FCapsule& operator=(FCapsule&& InSteal)
		{
			this->Type = InSteal.Type;
			this->bIsConvex = InSteal.bIsConvex;
			this->bDoCollide = InSteal.bDoCollide;
			this->bHasBoundingBox = InSteal.bHasBoundingBox;

			MSegment = MoveTemp(InSteal.MSegment);
			SetRadius(InSteal.GetRadius());

			return *this;
		}

		~FCapsule() {}

		static constexpr EImplicitObjectType StaticType() { return ImplicitObjectType::Capsule; }

		static FCapsule NewFromOriginAndAxis(const FVec3& Origin, const FVec3& Axis, const FReal Height, const FReal Radius)
		{
			auto X1 = Origin + Axis * Radius;
			auto X2 = Origin + Axis * (Radius + Height);
			return FCapsule(X1, X2, Radius);
		}

		virtual FReal GetRadius() const override
		{
			return Margin;
		}

		/**
		 * Returns sample points centered about the origin.
		 *
		 * \p NumPoints specifies how many points to generate.
		 */
		TArray<FVec3> ComputeLocalSamplePoints(const int32 NumPoints) const;

		/** 
		 * Returns sample points centered about the origin. 
		 *
		 * \p PointsPerUnitArea specifies how many points to generate per square 
		 *    unit (cm). 0.5 would generate 1 point per 2 square cm.
		 */
		TArray<FVec3> ComputeLocalSamplePoints(const FReal PointsPerUnitArea, const int32 MinPoints = 0, const int32 MaxPoints = 1000) const
		{ return ComputeLocalSamplePoints(FMath::Clamp(static_cast<int32>(ceil(PointsPerUnitArea * GetArea())), MinPoints, MaxPoints)); }

		/**
		 * Returns sample points at the current location of the cylinder.
		 */
		TArray<FVec3> ComputeSamplePoints(const int32 NumPoints) const;

		/** 
		 * Returns sample points at the current location of the cylinder.
		 *
		 * \p PointsPerUnitArea specifies how many points to generate per square 
		 *    unit (cm). 0.5 would generate 1 point per 2 square cm.
		 */
		TArray<FVec3> ComputeSamplePoints(const FReal PointsPerUnitArea, const int32 MinPoints = 0, const int32 MaxPoints = 1000) const
		{ return ComputeSamplePoints(FMath::Clamp(static_cast<int32>(ceil(PointsPerUnitArea * GetArea())), MinPoints, MaxPoints)); }

		virtual FReal PhiWithNormal(const FVec3& x, FVec3& Normal) const override
		{
			auto Dot = FMath::Clamp(FVec3::DotProduct(x - GetX1(), GetAxis()), (FReal)0., GetHeight());
			FVec3 ProjectedPoint = Dot * GetAxis() + GetX1();
			Normal = x - ProjectedPoint;
			return Normal.SafeNormalize() - GetRadius();
		}

		virtual const FAABB3 BoundingBox() const override
		{
			FAABB3 Box = MSegment.BoundingBox();
			Box.Thicken(GetRadius());
			return Box;
		}

		virtual FAABB3 CalculateTransformedBounds(const FRigidTransform3& Transform) const override
		{
			const FVec3 X1 = Transform.TransformPositionNoScale(MSegment.GetX1());
			const FVec3 X2 = Transform.TransformPositionNoScale(MSegment.GetX2());
			const FVec3 MinSegment = X1.ComponentwiseMin(X2);
			const FVec3 MaxSegment = X1.ComponentwiseMax(X2);

			const FVec3 RadiusV = FVec3(GetRadius());
			return FAABB3(MinSegment - RadiusV, MaxSegment + RadiusV);
		}

		static bool RaycastFast(FReal MRadius, FReal MHeight, const FVec3& MVector, const FVec3& X1, const FVec3& X2, const FVec3& StartPoint, const FVec3& Dir, const FReal Length, const FReal Thickness, FReal& OutTime, FVec3& OutPosition, FVec3& OutNormal, int32& OutFaceIndex)
		{
			ensure(FMath::IsNearlyEqual(MVector.SizeSquared(), (FReal)1, (FReal)UE_KINDA_SMALL_NUMBER));
			ensure(FMath::IsNearlyEqual(Dir.SizeSquared(), (FReal)1, (FReal)UE_KINDA_SMALL_NUMBER));
			ensure(Length > 0);

			const FReal R = MRadius + Thickness;
			const FReal R2 = R * R;
			OutFaceIndex = INDEX_NONE;

			// first test, segment to segment distance
			const FVec3 RayEndPoint = StartPoint + Dir * Length;
			FVec3 OutP1, OutP2;
			FMath::SegmentDistToSegmentSafe(X1, X2, StartPoint, RayEndPoint, OutP1, OutP2);
			const FReal DistanceSquared = (OutP2 - OutP1).SizeSquared();
			if (DistanceSquared > R2 + DBL_EPSILON)
			{
				return false;
			}

			// Raycast against capsule bounds.
			// We will use the intersection point as our ray start, this prevents precision issues if start is far away.
			// All calculations below should use LocalStart/LocalLength, and convert output time using input length if intersecting.
			FAABB3 CapsuleBounds;
			{
				CapsuleBounds.GrowToInclude(X1);
				CapsuleBounds.GrowToInclude(X2);
				CapsuleBounds.Thicken(R);
			}

			FReal InvLength = 1.0f / Length;
			FVec3 InvDir;
			bool bParallel[3];
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				bParallel[Axis] = Dir[Axis] == 0;
				InvDir[Axis] = bParallel[Axis] ? 0 : 1 / Dir[Axis];
			}

			FVec3 LocalStart = StartPoint;
			FReal LocalLength = Length;
			FReal RemovedLength = 0;
			{
				FReal OutBoundsTime;
				bool bStartHit = CapsuleBounds.RaycastFast(StartPoint, Dir, InvDir, bParallel, Length, InvLength, OutBoundsTime, OutPosition);
				if (bStartHit == false)
				{
					return false;
				}

				LocalStart = StartPoint + OutBoundsTime * Dir;
				RemovedLength = OutBoundsTime;
				LocalLength = Length - OutBoundsTime; // Note: This could be 0.
			}

			//First check if we are initially overlapping
			//Find closest point to cylinder core and check if it's inside the inflated capsule
			const FVec3 X1ToStart = LocalStart - X1;
			const FReal MVectorDotX1ToStart = FVec3::DotProduct(X1ToStart, MVector);
			if (MVectorDotX1ToStart >= -R && MVectorDotX1ToStart <= MHeight + R)
			{
				//projection is somewhere in the capsule. Clamp to cylinder length and check if inside sphere
				const FReal ClampedProjection = FMath::Clamp(MVectorDotX1ToStart, (FReal)0, MHeight);
				const FVec3 ClampedProjectionPosition = MVector * ClampedProjection;
				const FReal Dist2 = (X1ToStart - ClampedProjectionPosition).SizeSquared();
				if (Dist2 <= R2)
				{
					// In this case, clamped project position is either inside capsule or on the surface.

					OutTime = RemovedLength; // We may have shortened our ray, not actually 0 time.

					// We clipped ray against bounds, not a true initial overlap, compute normal/position
					if (RemovedLength > 0.0f)
					{	
						// Ray must have started outside capsule bounds, intersected bounds where it is touched capsule surface.
						OutNormal = (X1ToStart - ClampedProjectionPosition) / R;
						OutPosition = LocalStart - OutNormal * Thickness;
					}
					else
					{
						// Input ray started inside capsule, out time is 0, we are just filling out outputs so they aren't uninitialized.
						OutPosition = LocalStart;
						OutNormal = -Dir;
					}

					return true;
				}
			}

			if(FMath::IsNearlyEqual(LocalLength, 0., UE_KINDA_SMALL_NUMBER))
			{
				// If LocalLength is 0, this means the ray's endpoint is on the bounding AABB of thickened capsule.
				// At this point we have determined this point is not on surface of capsule, so the ray has missed.
				return false;
			}

			// Raycast against cylinder first

			//let <x,y> denote x \dot y
			//cylinder implicit representation: ||((X - x1) \cross MVector)||^2 - R^2 = 0, where X is any point on the cylinder surface (only true because MVector is unit)
			//Using Lagrange's identity we get ||X-x1||^2 ||MVector||^2 - <MVector, X-x1>^2 - R^2 = ||X-x1||^2 - <MVector, X-x1>^2 - R^2 = 0
			//Then plugging the ray into X we have: ||StartPoint + t Dir - x1||^2 - <MVector, Start + t Dir - x1>^2 - R^2
			// = ||StartPoint-x1||^2 + t^2 + 2t <StartPoint-x1, Dir> - <MVector, StartPoint-x1>^2 - t^2 <MVector,Dir>^2 - 2t<MVector, StartPoint -x1><MVector, Dir> - R^2 = 0
			//Solving for the quadratic formula we get:
			//a = 1 - <MVector,Dir>^2	Note a = 0 implies MVector and Dir are parallel
			//b = 2(<StartPoint-x1, Dir> - <MVector, StartPoint - x1><MVector, Dir>)
			//c = ||StartPoint-x1||^2 - <MVector, StartPoint-x1>^2 - R^2 Note this tells us if start point is inside (c < 0) or outside (c > 0) of cylinder

			const FReal MVectorDotX1ToStart2 = MVectorDotX1ToStart * MVectorDotX1ToStart;
			const FReal MVectorDotDir = FVec3::DotProduct(MVector, Dir);
			const FReal MVectorDotDir2 = MVectorDotDir * MVectorDotDir;
			const FReal X1ToStartDotDir = FVec3::DotProduct(X1ToStart, Dir);
			const FReal X1ToStart2 = X1ToStart.SizeSquared();
			const FReal A = 1 - MVectorDotDir2;
			const FReal C = X1ToStart2 - MVectorDotX1ToStart2 - R2;

			constexpr FReal Epsilon = (FReal)1e-4;
			bool bCheckCaps = false;

			if (C <= 0.f)
			{
				// We already tested initial overlap of start point, so start must be in cylinder
				// but above/below segment end points.
				bCheckCaps = true;
			}
			else
			{
				const FReal HalfB = (X1ToStartDotDir - MVectorDotX1ToStart * MVectorDotDir);
				const FReal QuarterUnderRoot = HalfB * HalfB - A * C;

				if (QuarterUnderRoot < 0)
				{
					bCheckCaps = true;
				}
				else
				{
					FReal Time;
					const bool bSingleHit = QuarterUnderRoot < Epsilon;
					if (bSingleHit)
					{
						Time = (A == 0) ? 0 : (-HalfB / A);

					}
					else
					{
						Time = (A == 0) ? 0 : ((-HalfB - FMath::Sqrt(QuarterUnderRoot)) / A); //we already checked for initial overlap so just take smallest time
						if (Time < 0)	//we must have passed the cylinder
						{
							return false;
						}
					}

					const FVec3 SpherePosition = LocalStart + Time * Dir;
					const FVec3 CylinderToSpherePosition = SpherePosition - X1;
					const FReal PositionLengthOnCoreCylinder = FVec3::DotProduct(CylinderToSpherePosition, MVector);
					if (PositionLengthOnCoreCylinder >= 0 && PositionLengthOnCoreCylinder < MHeight)
					{
						OutTime = Time + RemovedLength; // Account for ray clipped against bounds
						OutNormal = (CylinderToSpherePosition - MVector * PositionLengthOnCoreCylinder) / R;
						OutPosition = SpherePosition - OutNormal * Thickness;
						return true;
					}
					else
					{
						//if we have a single hit the ray is tangent to the cylinder.
						//the caps are fully contained in the infinite cylinder, so no need to check them
						bCheckCaps = !bSingleHit;
					}
				}
			}

			if (bCheckCaps)
			{
				//can avoid some work here, but good enough for now
				TSphere<FReal, 3> X1Sphere(X1, MRadius);
				TSphere<FReal, 3> X2Sphere(X2, MRadius);

				FReal Time1, Time2;
				FVec3 Position1, Position2;
				FVec3 Normal1, Normal2;	
				bool bHitX1 = X1Sphere.Raycast(LocalStart, Dir, LocalLength, Thickness, Time1, Position1, Normal1, OutFaceIndex);
				bool bHitX2 = X2Sphere.Raycast(LocalStart, Dir, LocalLength, Thickness, Time2, Position2, Normal2, OutFaceIndex);

				if (bHitX1 && bHitX2)
				{
					if (Time1 <= Time2)
					{
						OutTime = Time1 + RemovedLength;  // Account for ray clipped against bounds
						OutPosition = Position1;
						OutNormal = Normal1;
					}
					else
					{
						OutTime = Time2 + RemovedLength;  // Account for ray clipped against bounds
						OutPosition = Position2;
						OutNormal = Normal2;
					}

					return true;
				}
				else if (bHitX1)
				{
					OutTime = Time1 + RemovedLength;  // Account for ray clipped against bounds
					OutPosition = Position1;
					OutNormal = Normal1;
					return true;
				}
				else if (bHitX2)
				{
					OutTime = Time2 + RemovedLength;  // Account for ray clipped against bounds
					OutPosition = Position2;
					OutNormal = Normal2;
					return true;
				}
			}

			return false;
		}

		virtual bool Raycast(const FVec3& StartPoint, const FVec3& Dir, const FReal Length, const FReal Thickness, FReal& OutTime, FVec3& OutPosition, FVec3& OutNormal, int32& OutFaceIndex) const override
		{
			return RaycastFast(GetRadius(), GetHeight(), GetAxis(), GetX1(), GetX2(), StartPoint, Dir, Length, Thickness, OutTime, OutPosition, OutNormal, OutFaceIndex);
		}

		FORCEINLINE FVec3 Support(const FVec3& Direction, const FReal Thickness, int32& VertexIndex) const
		{
			return MSegment.Support(Direction, GetRadius() + Thickness, VertexIndex);
		}

		FORCEINLINE FVec3 SupportCore(const FVec3& Direction, const FReal InMargin, FReal* OutSupportDelta, int32& VertexIndex) const
		{
			// NOTE: Ignores InMargin, assumes Radius
			return MSegment.SupportCore(Direction, VertexIndex);
		}

		FORCEINLINE VectorRegister4Float SupportCoreSimd(const VectorRegister4Float& Direction, const FReal InMargin) const
		{
			// NOTE: Ignores InMargin, assumes Radius
			FVec3 DirectionVec3;
			VectorStoreFloat3(Direction, &DirectionVec3);
			int32 VertexIndex = INDEX_NONE;
			FVec3 SupportVert =  MSegment.SupportCore(DirectionVec3, VertexIndex);
			return MakeVectorRegisterFloatFromDouble(MakeVectorRegister(SupportVert.X, SupportVert.Y, SupportVert.Z, 0.0));
		}


		FORCEINLINE FVec3 SupportCoreScaled(const FVec3& Direction, const FReal InMargin, const FVec3& Scale, FReal* OutSupportDelta, int32& VertexIndex) const
		{
			// NOTE: Ignores InMargin, assumes Radius
			// Note: Scaling the direction vector like this, might not seem quite right, but works due to the commutativity of the single dot product that follows
			return SupportCore(Scale * Direction, GetMargin(), OutSupportDelta, VertexIndex) * Scale;
		}

		FORCEINLINE void SerializeImp(FArchive& Ar)
		{
			Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
			FImplicitObject::SerializeImp(Ar);
			MSegment.Serialize(Ar);

			// Radius is now stored in the base class Margin
			FRealSingle ArRadius = (FRealSingle)GetRadius(); // LWC_TODO : potential precision loss, to be changed when we can serialize FReal as double
			Ar << ArRadius;
			SetRadius(ArRadius);
			
			if(Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) < FExternalPhysicsCustomObjectVersion::CapsulesNoUnionOrAABBs)
			{
				FAABB3 DummyBox;	//no longer store this, computed on demand
				TBox<FReal,3>::SerializeAsAABB(Ar,DummyBox);
			}
		}

		virtual void Serialize(FChaosArchive& Ar) override
		{
			Ar.UsingCustomVersion(FExternalPhysicsCustomObjectVersion::GUID);
			FChaosArchiveScopedMemory ScopedMemory(Ar, GetTypeName());
			SerializeImp(Ar);

			if(Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) < FExternalPhysicsCustomObjectVersion::CapsulesNoUnionOrAABBs)
			{
				TUniquePtr<FImplicitObjectUnion> TmpUnion;
				Ar << TmpUnion;
			}
		}

		virtual FString ToString() const override
		{
			return FString::Printf(TEXT("Capsule: Height: %f Radius: %f"), GetHeight(), GetRadius());
		}
		
		virtual Chaos::FImplicitObjectPtr CopyGeometry() const override
		{
			return Chaos::FImplicitObjectPtr(new FCapsule(*this));
		}

		virtual Chaos::FImplicitObjectPtr CopyGeometryWithScale(const FVec3& Scale) const override
		{
			return  Chaos::FImplicitObjectPtr(new FCapsule(GetX1() * Scale, GetX2() * Scale, GetRadius() * Scale.Min()));
		}

		FReal GetHeight() const { return MSegment.GetLength(); }
		/** Returns the bottommost point on the capsule. */
		const FVec3 GetOrigin() const { return GetX1() + GetAxis() * -GetRadius(); }
		/** Returns the topmost point on the capsule. */
		const FVec3 GetInsertion() const { return GetX1() + GetAxis() * (GetHeight() + GetRadius()); }
		FVec3 GetCenter() const { return MSegment.GetCenter(); }
		/** Returns the centroid (center of mass). */
		FVec3 GetCenterOfMass() const { return GetCenter(); }
		const FVec3& GetAxis() const { return MSegment.GetAxis(); }
		const FVec3& GetX1() const { return MSegment.GetX1(); }
		FVec3 GetX2() const { return MSegment.GetX2(); }
		TSegment<FReal> GetSegment() const { return TSegment<FReal>(GetX1(), GetX2()); }

		FReal GetArea() const { return GetArea(GetHeight(), GetRadius()); }
		static FReal GetArea(const FReal Height, const FReal Radius)
		{
			static const FReal PI2 = 2.f * UE_PI;
			return PI2 * Radius * (Height + 2.f * Radius); 
		}

		FReal GetVolume() const { return GetVolume(GetHeight(), GetRadius()); }
		static FReal GetVolume(const FReal Height, const FReal Radius) { static const FReal FourThirds = 4.0f / 3.0f; return UE_PI * Radius * Radius * (Height + FourThirds * Radius); }

		FMatrix33 GetInertiaTensor(const FReal Mass) const { return GetInertiaTensor(Mass, GetHeight(), GetRadius()); }
		static FMatrix33 GetInertiaTensor(const FReal Mass, const FReal Height, const FReal Radius)
		{
			// https://www.wolframalpha.com/input/?i=capsule&assumption=%7B%22C%22,+%22capsule%22%7D+-%3E+%7B%22Solid%22%7D
			const FReal R = FMath::Clamp(Radius, (FReal)0., TNumericLimits<FReal>::Max());
			const FReal H = FMath::Clamp(Height, (FReal)0., TNumericLimits<FReal>::Max());
			const FReal RR = R * R;
			const FReal HH = H * H;

			// (5H^3 + 20*H^2R + 45HR^2 + 32R^3) / (60H + 80R)
			const FReal Diag12 = static_cast<FReal>(Mass * (5.*HH*H + 20.*HH*R + 45.*H*RR + 32.*RR*R) / (60.*H + 80.*R));
			// (R^2 * (15H + 16R) / (30H +40R))
			const FReal Diag3 = static_cast<FReal>(Mass * (RR * (15.*H + 16.*R)) / (30.*H + 40.*R));

			return FMatrix33(Diag12, Diag12, Diag3);
		}

		FRotation3 GetRotationOfMass() const { return GetRotationOfMass(GetAxis()); }
		static FRotation3 GetRotationOfMass(const FVec3& Axis)
		{
			// since the capsule stores an axis and the InertiaTensor is assumed to be along the ZAxis
			// we need to make sure to return the rotation of the axis from Z
			return FRotation3::FromRotatedVector(FVec3(0, 0, 1), Axis);
		}

		virtual uint32 GetTypeHash() const override
		{
			return HashCombine(UE::Math::GetTypeHash(GetX1()), UE::Math::GetTypeHash(GetAxis()));
		}

		FVec3 GetClosestEdgePosition(int32 PlaneIndexHint, const FVec3& Position) const
		{
			FVec3 P0 = GetX1();
			FVec3 P1 = GetX2();
			const FVec3 EdgePosition = FMath::ClosestPointOnLine(P0, P1, Position);
			return EdgePosition;
		}
		

		// The number of vertices that make up the corners of the specified face
		// In the case of a capsule the segment will act as a degenerate face
		// Used for manifold generation
		int32 NumPlaneVertices(int32 PlaneIndex) const
		{
			return 2;
		}

		// Returns a winding order multiplier used in the manifold clipping and required when we have negative scales (See ImplicitObjectScaled)
		// Not used for capsules
		// Used for manifold generation
		FORCEINLINE FReal GetWindingOrder() const
		{
			ensure(false);
			return 1.0f;
		}

		// Get the vertex at the specified index (e.g., indices from GetPlaneVertexs)
		// Used for manifold generation
		const FVec3 GetVertex(int32 VertexIndex) const
		{
			FVec3 Result;

			switch (VertexIndex)
			{
			case 0:
				Result = GetX1(); break;
			case 1:
				Result = GetX2(); break;
			}

			return Result;
		}

		// Get the index of the plane that most opposes the normal
		// not applicable for capsules
		int32 GetMostOpposingPlane(const FVec3& Normal) const
		{
			return 0;
		}

		int32 GetMostOpposingPlaneScaled(const FVec3& Normal, const FVec3& Scale) const
		{
			return 0;
		}

		// Get the vertex index of one of the vertices making up the corners of the specified face
		// Used for manifold generation
		int32 GetPlaneVertex(int32 PlaneIndex, int32 PlaneVertexIndex) const
		{
			return PlaneVertexIndex;
		}

		// Get the plane at the specified index (e.g., indices from FindVertexPlanes)
		const TPlaneConcrete<FReal, 3> GetPlane(int32 FaceIndex) const
		{
			return TPlaneConcrete<FReal, 3>(FVec3(0), FVec3(0));
		}

		void GetPlaneNX(const int32 FaceIndex, FVec3& OutN, FVec3& OutX) const
		{
			OutN = FVec3(0);
			OutX = FVec3(0);
		}

		// Get an array of all the plane indices that belong to a vertex (up to MaxVertexPlanes).
		// Returns the number of planes found.
		int32 FindVertexPlanes(int32 VertexIndex, int32* OutVertexPlanes, int32 MaxVertexPlanes) const
		{
			return 0; 
		}
		
		// Get up to the 3  plane indices that belong to a vertex
		// Returns the number of planes found.
		int32 GetVertexPlanes3(int32 VertexIndex, int32& PlaneIndex0, int32& PlaneIndex1, int32& PlaneIndex2) const
		{
			return 0;
		}

		// Capsules have no planes
		// Used for manifold generation
		int32 NumPlanes() const { return 0; }

#if INTEL_ISPC
		// See PerParticlePBDCollisionConstraint.cpp
		// ISPC code has matching structs for interpreting FImplicitObjects.
		// This is used to verify that the structs stay the same.
		struct FISPCDataVerifier
		{
			static constexpr int32 OffsetOfMSegment() { return offsetof(FCapsule, MSegment); }
			static constexpr int32 SizeOfMSegment() { return sizeof(FCapsule::MSegment); }
		};
		friend FISPCDataVerifier;
#endif // #if INTEL_ISPC

	private:
		void SetRadius(FReal InRadius) { SetMargin(InRadius); }

		TSegment<FReal> MSegment;
	};

	struct FCapsuleSpecializeSamplingHelper
	{
		static FORCEINLINE void ComputeSamplePoints(TArray<FVec3>& Points, const FCapsule& Capsule, const int32 NumPoints)
		{
			if (NumPoints <= 1 || Capsule.GetRadius() <= UE_SMALL_NUMBER)
			{
				const int32 Offset = Points.Num();
				if (Capsule.GetHeight() <= UE_SMALL_NUMBER)
				{
					Points.SetNumUninitialized(Offset + 1);
					Points[Offset] = Capsule.GetCenter();
				}
				else
				{
					Points.SetNumUninitialized(Offset + 3);
					Points[0] = Capsule.GetOrigin();
					Points[1] = Capsule.GetCenter();
					Points[2] = Capsule.GetInsertion();
				}
				return;
			}
			ComputeGoldenSpiralPoints(Points, Capsule, NumPoints);
		}

		static FORCEINLINE void ComputeGoldenSpiralPoints(TArray<FVec3>& Points, const FCapsule& Capsule, const int32 NumPoints)
		{ ComputeGoldenSpiralPoints(Points, Capsule.GetOrigin(), Capsule.GetAxis(), Capsule.GetHeight(), Capsule.GetRadius(), NumPoints); }

		static FORCEINLINE void ComputeGoldenSpiralPoints(
		    TArray<FVec3>& Points,
		    const FVec3& Origin,
		    const FVec3& Axis,
		    const FReal Height,
		    const FReal Radius,
		    const int32 NumPoints)
		{
			// Axis should be normalized.
			checkSlow(FMath::Abs(Axis.Size() - 1.0) < UE_KINDA_SMALL_NUMBER);

			// Evenly distribute points between the capsule body and the end caps.
			int32 NumPointsEndCap;
			int32 NumPointsCylinder;
			const FReal CapArea = 4 * UE_PI * Radius * Radius;
			const FReal CylArea = static_cast<FReal>(2.0 * UE_PI * Radius * Height);
			if (CylArea > UE_KINDA_SMALL_NUMBER)
			{
				const FReal AllArea = CylArea + CapArea;
				NumPointsCylinder = static_cast<int32>(round(CylArea / AllArea * static_cast<FReal>(NumPoints)));
				NumPointsCylinder += (NumPoints - NumPointsCylinder) % 2;
				NumPointsEndCap = (NumPoints - NumPointsCylinder) / 2;
			}
			else
			{
				NumPointsCylinder = 0;
				NumPointsEndCap = (NumPoints - (NumPoints % 2)) / 2;
			}
			const int32 NumPointsToAdd = NumPointsCylinder + NumPointsEndCap * 2;
			Points.Reserve(Points.Num() + NumPointsToAdd);

			const int32 Offset = Points.Num();
			const FReal HalfHeight = Height / 2;
			{
				// Points vary in Z: [-Radius-HalfHeight, -HalfHeight]
				TSphereSpecializeSamplingHelper<FReal, 3>::ComputeBottomHalfSemiSphere(
				    Points, TSphere<FReal, 3>(FVec3(0, 0, -HalfHeight), Radius), NumPointsEndCap, Points.Num());
#if 0
				{
					TSphere<FReal, 3> Sphere(FVec3(0, 0, -HalfHeight), Radius);
					for(int32 i=Offset; i < Points.Num(); i++)
					{
						const FVec3& Pt = Points[i];
						const FReal Phi = Sphere.SignedDistance(Pt);
						checkSlow(FMath::Abs(Phi) < KINDA_SMALL_NUMBER);
						checkSlow(Pt[2] > -Radius - HalfHeight - KINDA_SMALL_NUMBER && Pt[2] < -HalfHeight + KINDA_SMALL_NUMBER);
					}
				}
#endif
				// Points vary in Z: [-HalfHeight, HalfHeight], about the Z axis.
				FCylinderSpecializeSamplingHelper::ComputeGoldenSpiralPointsUnoriented(
				    Points, Radius, Height, NumPointsCylinder, false, Points.Num());
#if 0
				{
					TCylinder<FReal> Cylinder(FVec3(0, 0, -HalfHeight), FVec3(0, 0, HalfHeight), Radius);
					for(int32 i=TmpOffset; i < Points.Num(); i++)
					{
						const FVec3& Pt = Points[i];
						const FReal Phi = Cylinder.SignedDistance(Pt);
						checkSlow(FMath::Abs(Phi) < KINDA_SMALL_NUMBER);
						checkSlow(Pt[2] > -HalfHeight - KINDA_SMALL_NUMBER && Pt[2] < HalfHeight + KINDA_SMALL_NUMBER);
					}
				}
#endif
				// Points vary in Z: [HalfHeight, HalfHeight+Radius]
				TSphereSpecializeSamplingHelper<FReal, 3>::ComputeTopHalfSemiSphere(
				    Points, TSphere<FReal, 3>(FVec3(0, 0, HalfHeight), Radius), NumPointsEndCap, Points.Num());
#if 0
				{
					TSphere<FReal, 3> Sphere(FVec3(0, 0, HalfHeight), Radius);
					for(int32 i=TmpOffset; i < Points.Num(); i++)
					{
						const FVec3& Pt = Points[i];
						const FReal Phi = Sphere.SignedDistance(Pt);
						checkSlow(FMath::Abs(Phi) < KINDA_SMALL_NUMBER);
						checkSlow(Pt[2] > HalfHeight - KINDA_SMALL_NUMBER && Pt[2] < HalfHeight + Radius + KINDA_SMALL_NUMBER);
					}
				}
#endif
#if 0
				{
					FCapsule(FVec3(0, 0, -HalfHeight), FVec3(0, 0, HalfHeight), Radius);
					for(int32 i=Offset; i < Points.Num(); i++)
					{
						const FVec3& Pt = Points[i];
						const FReal Phi = Cylinder.SignedDistance(Pt);
						checkSlow(FMath::Abs(Phi) < KINDA_SMALL_NUMBER);
					}
				}
#endif
			}

			const FRotation3 Rotation = FRotation3::FromRotatedVector(FVec3(0, 0, 1), Axis);
			checkSlow(((Origin + Axis * (Height + Radius * 2)) - (Rotation.RotateVector(FVec3(0, 0, Height + Radius * 2)) + Origin)).Size() < UE_KINDA_SMALL_NUMBER);
			for (int32 i = Offset; i < Points.Num(); i++)
			{
				FVec3& Point = Points[i];
				const FVec3 PointNew = Rotation.RotateVector(Point + FVec3(0, 0, HalfHeight + Radius)) + Origin;
				checkSlow(FMath::Abs(FCapsule::NewFromOriginAndAxis(Origin, Axis, Height, Radius).SignedDistance(PointNew)) < UE_KINDA_SMALL_NUMBER);
				Point = PointNew;
			}
		}
	};

	FORCEINLINE TArray<FVec3> FCapsule::ComputeLocalSamplePoints(const int32 NumPoints) const
	{
		TArray<FVec3> Points;
		const FVec3 Mid = GetCenter();
		const FCapsule Capsule(GetX1() - Mid, GetX1() + (GetAxis() * GetHeight()) - Mid, GetRadius());
		FCapsuleSpecializeSamplingHelper::ComputeSamplePoints(Points, Capsule, NumPoints);
		return Points;
	}

	FORCEINLINE TArray<FVec3> FCapsule::ComputeSamplePoints(const int32 NumPoints) const
	{
		TArray<FVec3> Points;
		FCapsuleSpecializeSamplingHelper::ComputeSamplePoints(Points, *this, NumPoints);
		return Points;
	}

	template<class T>
	using TCapsule = FCapsule; // AABB<> is still using TCapsule<> so no deprecation message for now

	template<class T>
	using TCapsuleSpecializeSamplingHelper UE_DEPRECATED(4.27, "Deprecated. this class is to be deleted, use FCapsuleSpecializeSamplingHelper instead") = FCapsuleSpecializeSamplingHelper;

} // namespace Chaos
