// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/ConvexTriangleContactPoint.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Collision/ConvexContactPointUtilities.h"
#include "Chaos/CollisionOneShotManifolds.h"
#include "Chaos/Convex.h"
#include "Chaos/DebugDrawQueue.h"
#include "Chaos/SAT.h"
#include "Chaos/Triangle.h"
#include "Misc/MemStack.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
	// Check whether the convex-triangle edge pair form part of the Minkowski Sum. Only edge pairs
	// that contribute to the Minkowki Sum surface need to be checked for separation. The inputs
	// are the convex normals for the two faces that share the convex edge, and the normal and
	// edge vector of the triangle.
	// 
	// This is a custom version of IsMinkowskiSum for triangles where the two normals are directly
	// opposing and therefore the regular edge vector calculation returns zero.
	// 
	// @param A ConvexNormalA
	// @param B ConvexNormalB
	// @param BA ConvexEdge
	// @param C TriNormal (negated)
	// @param DC TriEdge
	bool IsMinkowskiSumConvexTriangle(const FVec3& A, const FVec3& B, const FVec3& BA, const FVec3& C, const FVec3& DC)
	{
		const FReal CBA = FVec3::DotProduct(C, BA);		// TriNormal | ConvexEdge
		const FReal ADC = FVec3::DotProduct(A, DC);		// ConvexNormalA | TriEdge
		const FReal BDC = FVec3::DotProduct(B, DC);		// ConvexNormalB | TriEdge

		const FReal Tolerance = 1.e-2f;
		return ((ADC * BDC) < -Tolerance) && ((CBA * BDC) > Tolerance);
	}

	// Clip the vertices of a triangle to a face of a convex, using some arbitrary vector as the clipping axis
	// (the axis is assumed to not be parallel to the convex face surface).
	template <typename ConvexImplicitType>
	TArrayView<FVec3> ClipTriangleToConvex(
		const FTriangle& Triangle,
		const ConvexImplicitType& Convex,
		const int32 ConvexPlaneIndex,
		const FVec3& Axis,
		TArrayView<FVec3> VertexBuffer1,
		TArrayView<FVec3> VertexBuffer2)
	{
		check(VertexBuffer1.Num() == VertexBuffer2.Num());
		check(VertexBuffer1.Num() >= 3);

		// Start with the triangle vertices
		int32 ContactPointCount = 0;
		VertexBuffer1[0] = Triangle.GetVertex(0);
		VertexBuffer1[1] = Triangle.GetVertex(1);
		VertexBuffer1[2] = Triangle.GetVertex(2);
		ContactPointCount = 3;

		// Now clip against all planes that belong to the convex plane's, edges
		// Note winding order matters here, and we have to handle negative scales
		const FReal WindingOrder = Convex.GetWindingOrder();
		const int32 ConvexFaceVerticesNum = Convex.NumPlaneVertices(ConvexPlaneIndex);
		int32 ClippingPlaneCount = ConvexFaceVerticesNum;
		FVec3 PrevPoint = Convex.GetVertex(Convex.GetPlaneVertex(ConvexPlaneIndex, ClippingPlaneCount - 1));
		for (int32 ClippingPlaneIndex = 0; (ClippingPlaneIndex < ClippingPlaneCount) && (ContactPointCount > 1); ++ClippingPlaneIndex)
		{
			const FVec3 CurrentPoint = Convex.GetVertex(Convex.GetPlaneVertex(ConvexPlaneIndex, ClippingPlaneIndex));

			// Convex edge clipping plane
			// NOTE: Plane is not normalized, but the length cancels out in all clip operations
			const FVec3 ClippingPlaneNormal = WindingOrder * FVec3::CrossProduct(Axis, PrevPoint - CurrentPoint);
			if (ClippingPlaneNormal.SizeSquared() > UE_SMALL_NUMBER)
			{
				ContactPointCount = Collisions::ClipVerticesAgainstPlane(VertexBuffer1.GetData(), VertexBuffer2.GetData(), ContactPointCount, VertexBuffer1.Num(), ClippingPlaneNormal, FVec3::DotProduct(CurrentPoint, ClippingPlaneNormal));
				Swap(VertexBuffer1, VertexBuffer2); // VertexBuffer1 will now point to the latest
			}

			PrevPoint = CurrentPoint;
		}

		return TArrayView<FVec3>(VertexBuffer1.GetData(), ContactPointCount);
	}

	// Clip the vertices of a convex to a face of a triangle
	template <typename ConvexImplicitType>
	TArrayView<FVec3> ClipConvexToTriangle(
		const ConvexImplicitType& Convex,
		const int32 ConvexPlaneIndex,
		const FTriangle& Triangle,
		const FVec3& TriangleN,
		TArrayView<FVec3> VertexBuffer1,
		TArrayView<FVec3> VertexBuffer2)
	{
		check(VertexBuffer1.Num() == VertexBuffer2.Num());

		// Populate the clipped vertices with the convex face vertices
		int32 ContactPointCount = 0;
		const int32 ConvexFaceVerticesNum = Convex.NumPlaneVertices(ConvexPlaneIndex);
		ContactPointCount = FMath::Min(ConvexFaceVerticesNum, VertexBuffer1.Num()); // Number of face vertices
		for (int32 VertexIndex = 0; VertexIndex < ContactPointCount; ++VertexIndex)
		{
			// Todo Check for Grey code
			VertexBuffer1[VertexIndex] = Convex.GetVertex(Convex.GetPlaneVertex(ConvexPlaneIndex, VertexIndex));
		}

		// Now clip against all planes that belong to the reference plane's, edges
		// Note winding order matters here, and we have to handle negative scales
		int32 ClippingPlaneCount = 3;
		FVec3 PrevPoint = Triangle.GetVertex(2);
		for (int32 ClippingPlaneIndex = 0; (ClippingPlaneIndex < ClippingPlaneCount) && (ContactPointCount > 1); ++ClippingPlaneIndex)
		{
			const FVec3 CurrentPoint = Triangle.GetVertex(ClippingPlaneIndex);

			// Triangle edge clipping plane
			// NOTE: Plane is not normalized, but the length cancels out in all clip operations
			const FVec3 ClippingPlaneNormal = FVec3::CrossProduct(TriangleN, PrevPoint - CurrentPoint);
			if (ClippingPlaneNormal.SizeSquared() > UE_SMALL_NUMBER)
			{
				ContactPointCount = Collisions::ClipVerticesAgainstPlane(VertexBuffer1.GetData(), VertexBuffer2.GetData(), ContactPointCount, VertexBuffer1.Num(), ClippingPlaneNormal, FVec3::DotProduct(CurrentPoint, ClippingPlaneNormal));
				Swap(VertexBuffer1, VertexBuffer2); // VertexBuffer1 will now point to the latest
			}

			PrevPoint = CurrentPoint;
		}

		return TArrayView<FVec3>(VertexBuffer1.GetData(), ContactPointCount);
	}

	// Triangle is in (possibly scaled) Convex space
	template <typename ConvexType>
	void ConstructConvexTriangleOneShotManifold2(const ConvexType& Convex, const FTriangle& Triangle, const FReal CullDistance, FContactPointManifold& OutContactPoints)
	{
		const FReal NormalTolerance = FReal(1.e-8);
		const FReal NormalToleranceSq = NormalTolerance * NormalTolerance;

		// Triangle (same space as convex)
		const FVec3 TriN = Triangle.GetNormal();
		const FVec3 TriC = Triangle.GetCentroid();

		//
		// SAT: Triangle face vs Convex verts
		//
		
		// We store the convex vertex distances to the triangle face for use in the edge-edge culling
		FMemMark Mark(FMemStack::Get());
		TArray<FReal, TMemStackAllocator<alignof(FReal)>> ConvexVertexDs;
		ConvexVertexDs.SetNum(Convex.NumVertices());
		TArrayView<FReal> ConvexVertexDsView = MakeArrayView(ConvexVertexDs);

		// Project the convex onto the triangle normal, with distances relative to the triangle plane
		FReal TriPlaneDMin, TriPlaneDMax;
		int32 ConvexVertexIndexMin, ConvexVertexIndexMax;
		ProjectOntoAxis(Convex, TriN, TriC, TriPlaneDMin, TriPlaneDMax, ConvexVertexIndexMin, ConvexVertexIndexMax, &ConvexVertexDsView);

		// Distance culling
		if (TriPlaneDMin > CullDistance)
		{
			// Outside triangle face and separated by more than CullDistance
			return;
		}
		if (TriPlaneDMax < FReal(0))
		{
			// Inside triangle face (single-sided collision)
			return;
		}

		//
		// SAT: Convex faces vs triangle verts
		//

		// For each convex plane, project the Triangle onto the convex plane normal
		// and reject if the separation is more than cull distance.
		FVec3 ConvexPlaneN = FVec3(0);
		FVec3 ConvexPlaneX = FVec3(0);
		FReal ConvexPlaneDMin = std::numeric_limits<FReal>::lowest();
		int32 ConvexPlaneIndexMin = INDEX_NONE;
		for (int32 PlaneIndex = 0; PlaneIndex < Convex.NumPlanes(); ++PlaneIndex)
		{
			FVec3 ConN, ConX;
			Convex.GetPlaneNX(PlaneIndex, ConN, ConX);

			FReal DMin, DMax;
			int32 IndexMin, IndexMax;
			ProjectOntoAxis(Triangle, ConN, ConX, DMin, DMax, IndexMin, IndexMax);

			// Distance culling
			// @todo(chaos): Cull against the projected convex hull, not just the outward face 
			// (we can store the most-distance vertex for each face with the convex to avoid actually having to project)
			if (DMin > CullDistance)
			{
				// Separated by more than CullDistance
				return;
			}

			if (DMin > ConvexPlaneDMin)
			{
				ConvexPlaneN = ConN;
				ConvexPlaneX = ConX;
				ConvexPlaneDMin = DMin;
				ConvexPlaneIndexMin = PlaneIndex;
			}
		}

		//
		// SAT: Convex edges vs triangle edges
		//
		
		// Calculate the distance of triangle each edge to the convex separating plane
		FReal TriVertexConvexDMin0 = FVec3::DotProduct(Triangle.GetVertex(0) - ConvexPlaneX, ConvexPlaneN);
		FReal TriVertexConvexDMin1 = FVec3::DotProduct(Triangle.GetVertex(1) - ConvexPlaneX, ConvexPlaneN);
		FReal TriVertexConvexDMin2 = FVec3::DotProduct(Triangle.GetVertex(2) - ConvexPlaneX, ConvexPlaneN);
		FReal TriEdgeConvexDMin[3] =
		{
			FMath::Max(TriVertexConvexDMin2, TriVertexConvexDMin0),
			FMath::Max(TriVertexConvexDMin0, TriVertexConvexDMin1),
			FMath::Max(TriVertexConvexDMin1, TriVertexConvexDMin2),
		};

		FVec3 EdgeEdgeN = FVec3(0);
		FReal EdgeEdgeDMin = std::numeric_limits<FReal>::lowest();
		int32 ConvexEdgeIndexMin = INDEX_NONE;
		int32 TriEdgeIndexMin = INDEX_NONE;
		for (int32 ConvexEdgeIndex = 0; ConvexEdgeIndex < Convex.NumEdges(); ++ConvexEdgeIndex)
		{
			// Skip convex edges beyond CullDistance of the triangle face
			const int32 ConvexEdgeVertexIndex0 = Convex.GetEdgeVertex(ConvexEdgeIndex, 0);
			const int32 ConvexEdgeVertexIndex1 = Convex.GetEdgeVertex(ConvexEdgeIndex, 1);
			const FReal FaceConvexD0 = ConvexVertexDs[ConvexEdgeVertexIndex0];
			const FReal FaceConvexD1 = ConvexVertexDs[ConvexEdgeVertexIndex1];
			if ((FaceConvexD0 > CullDistance) && (FaceConvexD1 > CullDistance))
			{
				continue;
			}

			// Convex edge vertices
			const FVec3 ConvexEdgeV0 = Convex.GetVertex(ConvexEdgeVertexIndex0);
			const FVec3 ConvexEdgeV1 = Convex.GetVertex(ConvexEdgeVertexIndex1);

			// Convex planes that form the edge
			const int32 ConvexEdgePlaneIndexA = Convex.GetEdgePlane(ConvexEdgeIndex, 0);
			const int32 ConvexEdgePlaneIndexB = Convex.GetEdgePlane(ConvexEdgeIndex, 1);
			const FVec3 ConvexEdgePlaneNormalA = Convex.GetPlane(ConvexEdgePlaneIndexA).Normal();
			const FVec3 ConvexEdgePlaneNormalB = Convex.GetPlane(ConvexEdgePlaneIndexB).Normal();

			for (int32 TriEdgeIndex = 0; TriEdgeIndex < 3; ++TriEdgeIndex)
			{
				// Skip triangle edges beyond cull distance of the convex separating face
				if (TriEdgeConvexDMin[TriEdgeIndex] > CullDistance)
				{
					continue;
				}

				// Triangle edge vertices
				const FVec3& TriEdgeV0 = (TriEdgeIndex == 0) ? Triangle.GetVertex(2) : Triangle.GetVertex(TriEdgeIndex - 1);
				const FVec3& TriEdgeV1 = Triangle.GetVertex(TriEdgeIndex);

				// Skip edge pairs that do not contribute to the Minkowski Sum surface
				// NOTE: This relies on the ordering of the edge planes from above. 
				// I.e., we require Sign(ConvexEdgePlaneNormalA x ConvexEdgePlaneNormalB) == Sign(ConvexEdgeV1 - ConvexEdgeV0)
				// Also note that we must pass the negated triangle normal in
				if (!IsMinkowskiSumConvexTriangle(ConvexEdgePlaneNormalA, ConvexEdgePlaneNormalB, ConvexEdgeV1 - ConvexEdgeV0, -TriN, TriEdgeV1 - TriEdgeV0))
				{
					continue;
				}

				// Separating axis
				// NOTE: Not normalized at this stage. We perform the projection against the
				// non-normalized axis and defer the square root until we know we need it
				FVec3 Axis = FVec3::CrossProduct(ConvexEdgeV1 - ConvexEdgeV0, TriEdgeV1 - TriEdgeV0);
				const FReal AxisLenSq = Axis.SizeSquared();

				// Pick consistent axis direction: away from triangle (we want a signed distance)
				const FReal Sign = FVec3::DotProduct(TriEdgeV0 - TriC, Axis);
				if (Sign < FReal(0))
				{
					Axis = -Axis;
				}

				const FReal ScaledSeparation = FVec3::DotProduct(ConvexEdgeV0 - TriEdgeV0, Axis);

				// Check cull distance on projected segments
				// Comparing square distances scaled by axis length to defer square root (keep the sign)
				const FReal ScaledSeparationSq = ScaledSeparation * FMath::Abs(ScaledSeparation);
				const FReal ScaledCullDistanceSq = FMath::Square(CullDistance) * AxisLenSq;
				if (ScaledSeparationSq > ScaledCullDistanceSq)
				{
					return;
				}

				// Comparing square distances scaled by axis length to defer square root (keep the sign)
				const FReal ScaledEdgeEdgeDMinSq = EdgeEdgeDMin * FMath::Abs(EdgeEdgeDMin) * AxisLenSq;
				if (ScaledSeparationSq > ScaledEdgeEdgeDMinSq)
				{
					// Now we need to know the actual separation and axis
					const FReal AxisInvLen = FMath::InvSqrt(AxisLenSq);
					EdgeEdgeDMin = ScaledSeparation * AxisInvLen;
					EdgeEdgeN = Axis * AxisInvLen;
					ConvexEdgeIndexMin = ConvexEdgeIndex;
					TriEdgeIndexMin = TriEdgeIndex;
				}
			}
		}

		// Determine the best features to use for this collision
		FVec3 SeparatingAxis, ClipAxis;
		EContactPointType ContactType;
		bool bClipConvexToTriangle;
		bool bClipToFaceNormal;
		const FReal TriFaceBias = FReal(1.e-2);	// Prevent flip=flip on near parallel cases
		if ((TriPlaneDMin + TriFaceBias > ConvexPlaneDMin) && (TriPlaneDMin + TriFaceBias > EdgeEdgeDMin))
		{
			// Triangle face contact - clip the convex vertices to the triangle
			ContactType = EContactPointType::VertexPlane;
			bClipConvexToTriangle = true;
			bClipToFaceNormal = true;

			// The triangle normal is the separating axis
			SeparatingAxis = TriN;
			if (FVec3::DotProduct(SeparatingAxis, Convex.GetCenterOfMass() - TriC) < FReal(0))
			{
				SeparatingAxis = -SeparatingAxis;
			}

			ClipAxis = TriN;

			// Find the convex face most opposing the separating axis			
			ConvexPlaneIndexMin = Convex.GetMostOpposingPlane(TriN);		// @todo(chaos): should use the known vertex index
			Convex.GetPlaneNX(ConvexPlaneIndexMin, ConvexPlaneN, ConvexPlaneX);

		}
		else if (ConvexPlaneDMin > EdgeEdgeDMin)
		{
			// Convex face contact - clip the triangle to the convex face
			ContactType = EContactPointType::PlaneVertex;
			bClipConvexToTriangle = false;
			bClipToFaceNormal = true;

			// The convex face is the separating axis, but it must point from the triangle to the convex
			SeparatingAxis = ConvexPlaneN;
			if (FVec3::DotProduct(SeparatingAxis, Convex.GetCenterOfMass() - TriC) < FReal(0))
			{
				SeparatingAxis = -SeparatingAxis;
			}

			ClipAxis = ConvexPlaneN;
		}
		else
		{
			// Edge-edge contact - clip triangle vs convex or vice-versa based on most opposing normals
			ContactType = EContactPointType::EdgeEdge;

			// The separating axis must point from the triangle to the convex
			SeparatingAxis = EdgeEdgeN;
			if (FVec3::DotProduct(SeparatingAxis, Convex.GetCenterOfMass() - TriC) < FReal(0))
			{
				SeparatingAxis = -SeparatingAxis;
			}

			// Find the convex face most opposing the separating axis			
			ConvexPlaneIndexMin = Convex.GetMostOpposingPlane(SeparatingAxis);	// @todo(chaos): should use the known edge index
			Convex.GetPlaneNX(ConvexPlaneIndexMin, ConvexPlaneN, ConvexPlaneX);

			// Decide whether to clip against the triangle or the convex
			ClipAxis = SeparatingAxis;
			const FReal TriNDotAxis = FVec3::DotProduct(TriN, SeparatingAxis);
			const FReal ConvexNDotAxis = FVec3::DotProduct(ConvexPlaneN, SeparatingAxis);
			if (FMath::Abs(TriNDotAxis) > FMath::Abs(ConvexNDotAxis))
			{
				bClipConvexToTriangle = true;
				if (FVec3::DotProduct(ClipAxis, TriN) < 0)
				{
					ClipAxis = -ClipAxis;
				}
			}
			else
			{
				bClipConvexToTriangle = false;
				if (FVec3::DotProduct(ClipAxis, ConvexPlaneN) < 0)
				{
					ClipAxis = -ClipAxis;
				}
			}
			bClipToFaceNormal = false;
		}

		// @todo(chaos): scratch or stack allocation of clipped vertex buffers
		const int32 MaxContactPointCount = 32;
		TCArray<FVec3, MaxContactPointCount> ClippedVertexBuffers0 = TCArray<FVec3, MaxContactPointCount>::MakeFull();
		TCArray<FVec3, MaxContactPointCount> ClippedVertexBuffers1 = TCArray<FVec3, MaxContactPointCount>::MakeFull();

		// @todo(chaos): 2D Clip
		TArrayView<FVec3> ClippedVertices;
		if (bClipConvexToTriangle)
		{
			ClippedVertices = ClipConvexToTriangle(Convex, ConvexPlaneIndexMin, Triangle, ClipAxis, MakeArrayView(ClippedVertexBuffers0), MakeArrayView(ClippedVertexBuffers1));
		}
		else
		{
			ClippedVertices = ClipTriangleToConvex(Triangle, Convex, ConvexPlaneIndexMin, ClipAxis, MakeArrayView(ClippedVertexBuffers0), MakeArrayView(ClippedVertexBuffers1));
		}

		// Reduce number of contacts to the maximum allowed
		if (ClippedVertices.Num() > 4)
		{
			const FRotation3 RotateSeperationToZ = FRotation3::FromRotatedVector(SeparatingAxis, FVec3(0.0f, 0.0f, 1.0f));
			for (int32 ContactPointIndex = 0; ContactPointIndex < ClippedVertices.Num(); ++ContactPointIndex)
			{
				ClippedVertices[ContactPointIndex] = RotateSeperationToZ * ClippedVertices[ContactPointIndex];
			}

			const int32 ReducedContactPointCount = Collisions::ReduceManifoldContactPoints(ClippedVertices.GetData(), ClippedVertices.Num());
			ClippedVertices = MakeArrayView(ClippedVertices.GetData(), ReducedContactPointCount);

			for (int32 ContactPointIndex = 0; ContactPointIndex < ClippedVertices.Num(); ++ContactPointIndex)
			{
				ClippedVertices[ContactPointIndex] = RotateSeperationToZ.Inverse() * ClippedVertices[ContactPointIndex];
			}
		}

		// Add the clipped points to the contact list
		const auto& AddContact = [&CullDistance, &ContactType, &SeparatingAxis, &OutContactPoints](const FVec3& ConvexX, const FVec3 TriX, const FReal Distance)
		{
			if (Distance < CullDistance)
			{
				FContactPoint& ContactPoint = OutContactPoints[OutContactPoints.Add()];
				ContactPoint.ShapeContactPoints[0] = ConvexX;
				ContactPoint.ShapeContactPoints[1] = TriX;
				ContactPoint.ShapeContactNormal = SeparatingAxis;
				ContactPoint.Phi = Distance;
				ContactPoint.ContactType = ContactType;
				ContactPoint.FaceIndex = INDEX_NONE;
			}
		};

		check(ClippedVertices.Num() <= OutContactPoints.Max());
		if (bClipConvexToTriangle && bClipToFaceNormal)
		{
			// Clipped points are on the convex, and we clipped to the triangle face along its normal
			for (int32 ContactIndex = 0; ContactIndex < ClippedVertices.Num(); ++ContactIndex)
			{
				const FVec3& ConvexX = ClippedVertices[ContactIndex];
				const FReal Distance = FVec3::DotProduct(ConvexX - TriC, SeparatingAxis);
				const FVec3 TriX = ConvexX - Distance * SeparatingAxis;

				AddContact(ConvexX, TriX, Distance);
			}
		}
		else if (bClipConvexToTriangle && !bClipToFaceNormal)
		{
			// Clipped points are on the convex, and we clipped to the triangle face, but not along its normal
			for (int32 ContactIndex = 0; ContactIndex < ClippedVertices.Num(); ++ContactIndex)
			{
				const FVec3& ConvexX = ClippedVertices[ContactIndex];
				const FReal IntersectDenom = FVec3::DotProduct(SeparatingAxis, TriN);
				check(FMath::Abs(IntersectDenom) > UE_SMALL_NUMBER);	// Guaranteed based on axis selection above
				const FReal Distance = FVec3::DotProduct(ConvexX - TriC, TriN) / IntersectDenom;
				const FVec3 TriX = ConvexX - Distance * SeparatingAxis;

				AddContact(ConvexX, TriX, Distance);
			}
		}
		else if (!bClipConvexToTriangle && bClipToFaceNormal)
		{
			// Clipped points are on the triangle, and we clipped to the convex face along its normal
			for (int32 ContactIndex = 0; ContactIndex < ClippedVertices.Num(); ++ContactIndex)
			{
				const FVec3& TriX = ClippedVertices[ContactIndex];
				const FReal Distance = FVec3::DotProduct(ConvexPlaneX - TriX, SeparatingAxis);
				const FVec3 ConvexX = TriX + Distance * SeparatingAxis;

				AddContact(ConvexX, TriX, Distance);
			}
		}
		else if (!bClipConvexToTriangle && !bClipToFaceNormal)
		{
			// Clipped points are on the triangle, and we clipped to the convex face, but not along its normal
			for (int32 ContactIndex = 0; ContactIndex < ClippedVertices.Num(); ++ContactIndex)
			{
				const FVec3& TriX = ClippedVertices[ContactIndex];
				const FReal IntersectDenom = FVec3::DotProduct(SeparatingAxis, ConvexPlaneN);
				check(FMath::Abs(IntersectDenom) > UE_SMALL_NUMBER);	// Guaranteed based on axis selection above
				const FReal Distance = FVec3::DotProduct(ConvexPlaneX - TriX, ConvexPlaneN) / IntersectDenom;
				const FVec3 ConvexX = TriX + Distance * SeparatingAxis;

				AddContact(ConvexX, TriX, Distance);
			}
		}
	}


	template
	void ConstructConvexTriangleOneShotManifold2(
		const FImplicitConvex3& Convex, 
		const FTriangle& Triangle,
		const FReal CullDistance, 
		FContactPointManifold& OutContactPoints);

	template
	void ConstructConvexTriangleOneShotManifold2(
		const TImplicitObjectInstanced<FImplicitConvex3>& Convex,
		const FTriangle& Triangle,
		const FReal CullDistance,
		FContactPointManifold& OutContactPoints);

	template
	void ConstructConvexTriangleOneShotManifold2(
		const TImplicitObjectScaled<FImplicitConvex3>& Convex,
		const FTriangle& Triangle,
		const FReal CullDistance,
		FContactPointManifold& OutContactPoints);

	template
	void ConstructConvexTriangleOneShotManifold2(
		const FImplicitBox3& Convex,
		const FTriangle& Triangle,
		const FReal CullDistance,
		FContactPointManifold& OutContactPoints);

	template
	void ConstructConvexTriangleOneShotManifold2(
		const TImplicitObjectScaled<FImplicitBox3>& Convex,
		const FTriangle& Triangle,
		const FReal CullDistance,
		FContactPointManifold& OutContactPoints);

	template
	void ConstructConvexTriangleOneShotManifold2(
		const TImplicitObjectInstanced<FImplicitBox3>& Convex,
		const FTriangle& Triangle,
		const FReal CullDistance,
		FContactPointManifold& OutContactPoints);
}