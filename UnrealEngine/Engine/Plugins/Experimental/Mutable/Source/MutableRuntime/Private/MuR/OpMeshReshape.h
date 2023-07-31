// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ConvertData.h"
#include "MuR/MeshPrivate.h"
#include "MuR/OpMeshBind.h"

#include "Math/Plane.h"
#include "Math/Ray.h"
#include "Math/NumericLimits.h"

#include "IndexTypes.h"
#include "Spatial/MeshAABBTree3.h"
#include "Distance/DistPoint3Triangle3.h"
#include "LineTypes.h"



// TODO: Make the handling of rotations an option. It is more expensive on CPU and memory, and for some
// cases it is not required at all.

// TODO: Face stretch to scale the deformation per-vertex? 

// TODO: Support multiple binding influences per vertex, to have smoother deformations.

// TODO: Support multiple binding sets, to have different shapes deformations at once.

// TODO: Deformation mask, to select the intensisty of the deformation per-vertex.

// TODO: This is a reference implementation with ample roof for optimization.

namespace mu
{

	struct FShapeMeshDescriptorApply
	{
		TArray<FVector3f> Positions;
		TArray<FVector3f> Normals;
		TArray<UE::Geometry::FIndex3i> Triangles;
	};

	// Method to actually deform a point
	inline bool GetDeform( const FShapeMeshDescriptorApply& Shape, const FReshapeVertexBindingData& Binding, FVector3f& NewPosition, FQuat4f& Rotation)
	{
		const TArray<FVector3f>& ShapePositions = Shape.Positions;
		const TArray<FVector3f>& ShapeNormals = Shape.Normals;
	    const TArray<UE::Geometry::FIndex3i>& ShapeTriangles = Shape.Triangles;
		
		if (Binding.Triangle < 0 || Binding.Triangle >= ShapeTriangles.Num() )
		{
			return false;
		}

		const UE::Geometry::FIndex3i& Triangle = ShapeTriangles[Binding.Triangle];

		FVector3f ProjectedVertexPosition
			= ShapePositions[Triangle.A] * (1.0f - Binding.S - Binding.T)
			+ ShapePositions[Triangle.B] * Binding.S
			+ ShapePositions[Triangle.C] * Binding.T;

		// This method approximates the shape face rotation
		FVector3f InterpolatedNormal
			= ShapeNormals[Triangle.A] * (1.0f - Binding.S - Binding.T)
			+ ShapeNormals[Triangle.B] * Binding.S
			+ ShapeNormals[Triangle.C] * Binding.T;
		
		FVector3f PositionOffset = InterpolatedNormal * Binding.D;
		NewPosition = ProjectedVertexPosition + PositionOffset;

		FVector3f CurrentShapeNormal = ((ShapePositions[Triangle.B] - ShapePositions[Triangle.A]) ^ (ShapePositions[Triangle.C] - ShapePositions[Triangle.A])).GetSafeNormal();
		Rotation = FQuat4f::FindBetween(Binding.ShapeNormal, CurrentShapeNormal);
		
		return true;
	}

    //---------------------------------------------------------------------------------------------
    //! Physics Bodies Reshape 
    //---------------------------------------------------------------------------------------------
	inline bool GetDeformedBox( const FShapeMeshDescriptorApply& Shape, const FReshapeVertexBindingData* BindingData,  TStaticArray<FVector3f, 8>& OutBox )
	{
		FQuat4f UnusedRotation;

		for ( int32 I = 0; I < OutBox.Num(); ++I )
		{
			bool bModified = GetDeform(Shape, *(BindingData + I), OutBox[I], UnusedRotation);

			if (!bModified)
			{
				// Invalidate the whole box. 
				return false;
			}
		}

		return true;
	}

	inline void GetDeformedConvex( const FShapeMeshDescriptorApply& Shape, const FReshapeVertexBindingData* BindingData, TArray<FVector3f>& InOutDeformedVertices )
	{
		const int32 ConvexVertCount = InOutDeformedVertices.Num();
		FQuat4f UnusedTangentCorrection;
		for ( int32 I = 0; I < ConvexVertCount; ++I )
		{
			GetDeform(Shape, *(BindingData + I), InOutDeformedVertices[I], UnusedTangentCorrection);	
		}
	}

	inline void ComputeSphereFromDeformedBox( 
			const TStaticArray<FVector3f, 8>& Box, 
			FVector3f& InOutP, float& InOutR, 
			const FTransform3f& InvBoneT )
	{
		TStaticArray<FVector3f, 8> TransformedBox = Box;
		for (int32 I = 0; I < 8; ++I)
		{
			TransformedBox[I] = InvBoneT.TransformPosition(Box[I]) - InOutP;
		}
		
		FVector3f Centroid(ForceInitToZero);
		for ( const FVector3f& V : TransformedBox )
		{
			Centroid += V;	
		}

		Centroid *= (1.0f/8.0f);

		InOutP = Centroid + InOutP;
		
		FVector3f Dim(ForceInitToZero);
		for ( const FVector3f& V : TransformedBox )
		{
			Dim += (V - Centroid).GetAbs();
		}

		Dim *= (1.0f/8.0f);
		
		InOutR = (Dim.X + Dim.Y + Dim.Z) * (1.0f/3.0f);
	}

	inline void ComputeBoxFromDeformedBox( 
			const TStaticArray<FVector3f, 8>& Box, 
			FVector3f& InOutP, FQuat4f& InOutQ, FVector3f& InOutS, 
			const FTransform3f& InvBoneT )
	{
		const FQuat4f InvQ = InOutQ.Inverse();
		
		TStaticArray<FVector3f, 8> TranformedBox;
		for (int32 I = 0; I < 8; ++I)
		{
			TranformedBox[I] = InvQ.RotateVector(InvBoneT.TransformPosition(Box[I]) - InOutP);
		}
	
		const FVector3f TopC = (Box[0] + Box[1] + Box[2] + Box[3])*0.25f;
		const FVector3f BottomC = (Box[4+0] + Box[4+1] + Box[4+2] + Box[4+3])*0.25f;
    
		const FVector3f FrontC = (Box[0] + Box[1] + Box[4+0] + Box[4+1])*0.25f;
		const FVector3f BackC = (Box[2] + Box[3] + Box[4+2] + Box[4+3])*0.25f;
    
		const FVector3f RightC =  (Box[1] + Box[2] + Box[4+1] + Box[4+2])*0.25f;
		const FVector3f LeftC =  (Box[3] + Box[0] + Box[4+3] + Box[4+0])*0.25f;
    
		FVector3f ZB = (TopC - BottomC).GetSafeNormal();
		FVector3f XB = (RightC - LeftC).GetSafeNormal();
		FVector3f YB = (FrontC - BackC).GetSafeNormal();


		// Pick the 2 most offaxis vectors and construct a rotation form those.	
		// TODO: Find a better way of finding and orientation form ZB, XB, TN maybe by averaging somehow different bases created from the vectors, with quaternions?
		FVector3f OF = FVector3f( XB.X, YB.Y, ZB.Y ).GetAbs();

		float M0 = FMath::Max3( OF.X, OF.Y, OF.X);
		float M1 = M0 == OF.X ? FMath::Max(OF.Y, OF.Z) : (M0 == OF.Y ? FMath::Max(OF.X,OF.Z) : FMath::Max(OF.Y, OF.X));
		
		FMatrix44f RotationMatrix = FMatrix44f::Identity;
		if (M0 == OF.X)
		{
			RotationMatrix = M1 == OF.Y 
					? FRotationMatrix44f::MakeFromXY(XB, YB)
					: FRotationMatrix44f::MakeFromXZ(XB, ZB);
		}
		else if (M0 == OF.Y)
		{
			RotationMatrix = M1 == XB.X 
					? FRotationMatrix44f::MakeFromYX(YB, XB)
					: FRotationMatrix44f::MakeFromYZ(YB, ZB);
		}
		else
		{
			RotationMatrix = M1 == XB.X 
					? FRotationMatrix44f::MakeFromZX(ZB, XB)
					: FRotationMatrix44f::MakeFromZY(ZB, YB);	
		} 
		
		InOutQ = RotationMatrix.ToQuat() * InOutQ;
		InOutP = (TopC + BottomC) * 0.5f + InOutP;
		InOutS = FVector3f((RightC - LeftC).Size(), (FrontC - BackC).Size(), (TopC - BottomC).Size()) * 0.5f;	
	}

	inline void ComputeSphylFromDeformedBox( 
			const TStaticArray<FVector3f, 8>& Box, 
			FVector3f& InOutP, FQuat4f& InOutQ, float& InOutR, float& InOutL, 
			const FTransform3f& InvBoneT)
	{
		const FQuat4f InvQ = InOutQ.Inverse();
    	
		TStaticArray<FVector3f, 8> TranformedBox;
		for (int32 I = 0; I < 8; ++I)
		{
			TranformedBox[I] = InvQ.RotateVector(InvBoneT.TransformPosition(Box[I]) - InOutP);
		}
       
		FVector3f TopCentroid(ForceInitToZero);
		FVector3f BottomCentroid(ForceInitToZero);
		for (int32 I = 0; I < 4; ++I)
		{
			TopCentroid += TranformedBox[I];
			BottomCentroid += TranformedBox[4+I];
		}
		
		TopCentroid *= 0.25f;
		BottomCentroid *= 0.25f;

		FVector3f DirVector = (TopCentroid - BottomCentroid).GetSafeNormal();

		InOutQ = FQuat4f::FindBetweenNormals(FVector3f::UnitZ(), DirVector) * InOutQ; 
		InOutP = (TopCentroid + BottomCentroid) * 0.5f + InOutP;

		FVector3f TopDim(ForceInitToZero);
		FVector3f BottomDim(ForceInitToZero);
		for (int32 I = 0; I < 4; ++I)
		{
			TopDim += (TopCentroid - TranformedBox[I]).GetAbs();
			BottomDim += (BottomCentroid - TranformedBox[4+I]).GetAbs();
		}

		TopDim *= 0.25f;
		BottomDim *= 0.25f;

		InOutR = (TopDim.X + TopDim.Y + BottomDim.X + BottomDim.Y) * 0.25f;
		InOutL = (TopCentroid - BottomCentroid).Size() - InOutR*2.0f;
	}

	inline void ComputeTaperedCapsuleFromDeformedBox( 
			const TStaticArray<FVector3f, 8>& Box, 
			FVector3f& InOutP, FQuat4f& InOutQ, float& InOutR0, float& InOutR1, float& InOutL, 
			const FTransform3f& InvBoneT)
	{
		const FQuat4f InvQ = InOutQ.Inverse();
    
		TStaticArray<FVector3f, 8> TranformedBox;
		for (int32 I = 0; I < 8; ++I)
		{
			TranformedBox[I] = InvQ.RotateVector(InvBoneT.TransformPosition(Box[I]) - InOutP);
		}
       
		FVector3f TopCentroid(ForceInitToZero);
		FVector3f BottomCentroid(ForceInitToZero);
		for (int32 I = 0; I < 4; ++I)
		{
			TopCentroid += TranformedBox[I];
			BottomCentroid += TranformedBox[4+I];
		}
		
		TopCentroid *= 0.25f;
		BottomCentroid *= 0.25f;

		FVector3f DirVector = (TopCentroid - BottomCentroid).GetSafeNormal();

		InOutQ = InOutQ * FQuat4f::FindBetweenNormals(FVector3f::UnitZ(), DirVector); 
		InOutP = (TopCentroid + BottomCentroid) * 0.5f + InOutP;

		FVector3f TopDim(ForceInitToZero);
		FVector3f BottomDim(ForceInitToZero);

		for (int32 I = 0; I < 4; ++I)
		{
			TopDim += (TopCentroid - TranformedBox[I]).GetAbs();
			BottomDim += (BottomCentroid - TranformedBox[4+I]).GetAbs();
		}

		TopDim *= 0.25f;
		BottomDim *= 0.25f;

		InOutR0 = (TopDim.X + TopDim.Y) * 0.5f;
		InOutR1 = (BottomDim.X + BottomDim.Y) * 0.5f;
		InOutL = (TopCentroid - BottomCentroid).Size() - (InOutR0 + InOutR1);
	}

	inline void ApplyToVertices(Mesh* Mesh, TArrayView<const FReshapeVertexBindingData> BindingData, const FShapeMeshDescriptorApply& Shape)
	{
		check(Mesh);
		check(Mesh->GetVertexCount() == BindingData.Num());
		

		UntypedMeshBufferIterator ItPosition(Mesh->GetVertexBuffers(), MBS_POSITION);
		UntypedMeshBufferIterator ItNormal(Mesh->GetVertexBuffers(), MBS_NORMAL);
		UntypedMeshBufferIterator ItTangent(Mesh->GetVertexBuffers(), MBS_TANGENT);

		const int32 MeshVertexCount = BindingData.Num();
		for (int32 MeshVertexIndex = 0; MeshVertexIndex < MeshVertexCount; ++MeshVertexIndex)
		{
			const FReshapeVertexBindingData& Binding = BindingData[MeshVertexIndex];

			FVector3f NewPosition;
			FQuat4f TangentSpaceCorrection;
			bool bModified = GetDeform( Shape, Binding, NewPosition, TangentSpaceCorrection );

			if (bModified)
			{
				FVector3f OldPosition = ItPosition.GetAsVec3f();
				
				if ( !FMath::IsNearlyEqual( Binding.Weight, 1.0f ) )
				{
					// Zero weighted vertices are already discarded at the binding phase so there is no gain
					// checking here.
					NewPosition = FMath::Lerp( OldPosition, NewPosition, Binding.Weight );
					TangentSpaceCorrection = FQuat4f::Slerp( FQuat4f::Identity, TangentSpaceCorrection, Binding.Weight );
				}
				
				// Non rigid vertices will not rotate since are attached to themselves 
				NewPosition = TangentSpaceCorrection.RotateVector(OldPosition - Binding.AttachmentPoint) + NewPosition;
				
				ItPosition.SetFromVec3f(NewPosition);
					
				if (ItNormal.ptr())
				{
					FVector3f OldNormal = ItNormal.GetAsVec3f();
					FVector3f NewNormal = TangentSpaceCorrection.RotateVector(OldNormal);
					ItNormal.SetFromVec3f(NewNormal);
				}

				if (ItTangent.ptr())
				{
					FVector3f OldTangent = ItTangent.GetAsVec3f();
					FVector3f NewTangent = TangentSpaceCorrection.RotateVector(OldTangent);
					ItTangent.SetFromVec3f(NewTangent);
				}
			}
			
			++ItPosition;

			if (ItNormal.ptr())
			{
				++ItNormal;
			}

			if (ItTangent.ptr())
			{
				++ItTangent;
			}
		}
		
	}

	inline void ApplyToPose(Mesh* Result, 
			TArrayView<const FReshapeVertexBindingData> BindingData, 
			TArrayView<const int> BoneIndices,
			const FShapeMeshDescriptorApply& Shape)
	{
		check(Result);

		const int32 NumBoneIndices = BoneIndices.Num();
		for (int32 b = 0; b < NumBoneIndices; ++b)
		{
			int32 BoneIndex = BoneIndices[b];

			const FReshapeVertexBindingData& Binding = BindingData[b];

			FTransform3f& t = Result->m_bonePoses[BoneIndex].m_boneTransform;

			FVector3f NewPosition;
			FQuat4f TangentSpaceCorrection;
			bool bModified = GetDeform(Shape, Binding, NewPosition, TangentSpaceCorrection);

			if (bModified)
			{
				// The pose has been modified and should not be replaced
				Result->m_bonePoses[BoneIndex].m_boneSkinned = true;

				// TODO: Review if the rotation also needs to be applied.
				t.SetLocation(FMath::Lerp(t.GetLocation(), NewPosition, Binding.Weight));
			}
		}
	}

	inline void ApplyToPhysicsBodies(
			PhysicsBody* PBody, const Mesh* BaseMesh, 
			TArrayView<const FReshapeVertexBindingData> BindingData, TArrayView<const int32> UsedIndices, 
			const FShapeMeshDescriptorApply& Shape)
	{
		check(PBody);
		check(BaseMesh);

		// Not used.	
		FQuat4f DummyTangentSpaceCorrection;
		
		int32 NumProcessedBindPoints = 0;

		bool bAnyModified = false;

		// Retrieve them in the same order the boxes where put in, so they can be linked to the physics body volumes.
		for ( const int32 B : UsedIndices )
		{	
			int32 BoneIdx = BaseMesh->FindBonePose(PBody->GetBodyBoneName(B));
			FTransform3f BoneTransform = FTransform3f::Identity;
			if (BoneIdx >= 0)
			{
				BaseMesh->GetBoneTransform(BoneIdx, BoneTransform);
			}
			
			FTransform3f InvBoneTransform = BoneTransform.Inverse();

			const int32 NumSpheres = PBody->GetSphereCount(B); 
			for ( int32 I = 0; I < NumSpheres; ++I )
			{
				FVector3f P;
				float R;

				TStaticArray<FVector3f, 8> Box; 
				const bool bDeformed = GetDeformedBox( Shape, &BindingData[NumProcessedBindPoints], Box );

				if (bDeformed)
				{
					PBody->GetSphere(B, I, P, R);
					ComputeSphereFromDeformedBox(Box, P, R, InvBoneTransform);
					PBody->SetSphere(B, I, P, R);
					bAnyModified = true;
				}

				NumProcessedBindPoints += Box.Num();
			}

			const int32 NumBoxes = PBody->GetBoxCount(B);
			for ( int32 I = 0; I < NumBoxes; ++I )
			{
				TStaticArray<FVector3f, 8> Box;
				const bool bDeformed = GetDeformedBox( Shape, &BindingData[NumProcessedBindPoints], Box );
				
				if (bDeformed)
				{
					FVector3f P;
					FQuat4f Q;
					FVector3f S;
					
					PBody->GetBox(B, I, P, Q, S);
					ComputeBoxFromDeformedBox(Box, P, Q, S, InvBoneTransform);
					PBody->SetBox(B, I, P, Q, S);
					bAnyModified = true;
				}

				NumProcessedBindPoints += Box.Num();
			}
			
			const int32 NumSphyls = PBody->GetSphylCount(B);
			for ( int32 I = 0; I < NumSphyls; ++I )
			{
				TStaticArray<FVector3f, 8> Box;
				const bool bDeformed = GetDeformedBox( Shape, &BindingData[NumProcessedBindPoints], Box );

				if ( bDeformed )
				{
					FVector3f P;
					FQuat4f Q;
					float R;
					float L;						

					PBody->GetSphyl(B, I, P, Q, R, L);
					ComputeSphylFromDeformedBox(Box, P, Q, R, L, InvBoneTransform);
					PBody->SetSphyl(B, I, P, Q, R, L);
					bAnyModified = true;
				}

				NumProcessedBindPoints += Box.Num();
			}

			const int32 NumTaperedCapsules = PBody->GetTaperedCapsuleCount(B);
			for ( int32 I = 0; I < NumTaperedCapsules; ++I )
			{
				TStaticArray<FVector3f, 8> Box;
				const bool bDeformed = GetDeformedBox( Shape, &BindingData[NumProcessedBindPoints], Box );

				if ( bDeformed )
				{
					FVector3f P;
					FQuat4f Q;
					float R0;
					float R1;
					float L;
				
					PBody->GetTaperedCapsule(B, I, P, Q, R0, R1, L);
					ComputeTaperedCapsuleFromDeformedBox(Box, P, Q, R0, R1, L, InvBoneTransform);
					PBody->SetTaperedCapsule(B, I, P, Q, R0, R1, L);
					bAnyModified = true;
				}
				
				NumProcessedBindPoints += Box.Num();
			}

			const int32 NumConvex = PBody->GetConvexCount(B);
			for ( int32 I = 0; I < NumConvex; ++I )
			{
				FVector3f const* VertexData;
				int32 VertexCount;
				int32 const* IndexData;
				int32 IndexCount;
				FTransform3f ConvexT;

				PBody->GetConvex(B, I, VertexData, VertexCount, IndexData, IndexCount, ConvexT);

				// TODO: Allow access and modification to the convex data directly to avoid a copy here.
				TArray<FVector3f> DeformedVertexData(VertexData, VertexCount);
				TArray<int32> Indices(IndexData, IndexCount);

				GetDeformedConvex( Shape, &BindingData[NumProcessedBindPoints], DeformedVertexData);

				FTransform3f InvConvexT = InvBoneTransform * ConvexT.Inverse();
				for ( FVector3f& V : DeformedVertexData )
				{
					V = InvConvexT.TransformPosition( V );				
				}
				
				PBody->SetConvex(B, I, DeformedVertexData.GetData(), DeformedVertexData.Num(), Indices.GetData(), Indices.Num(), ConvexT);

				NumProcessedBindPoints += VertexCount;
				bAnyModified = true;
			}
		}

		PBody->bBodiesModified = bAnyModified;

		check(NumProcessedBindPoints == BindingData.Num());	
	}

	//---------------------------------------------------------------------------------------------
	//! Rebuild the (previously bound) mesh data for a new shape.
	//! Proof-of-concept implementation.
	//---------------------------------------------------------------------------------------------
	inline MeshPtr MeshApplyShape(const Mesh* BaseMesh, const Mesh* ShapeMesh, 
			bool bReshapeVertices, bool bReshapeSkeleton, bool bReshapePhysicsVolumes)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshApplyReshape);

		if (!BaseMesh)
		{
			return nullptr;
		}
		
		// Early out if nothing will be modified and the vertices discarted.
		const bool bSkeletonModification = BaseMesh->GetSkeleton() && bReshapeSkeleton;
		const bool bPhysicsModification = BaseMesh->GetPhysicsBody() && bReshapePhysicsVolumes;

		if (!bReshapeVertices && !bSkeletonModification && !bPhysicsModification)
		{
			return nullptr;
		}
	
		// \TODO: Multiple binding data support
		int BindingDataIndex = 0;

		// If the base mesh has no binding data, just clone it.
		int BarycentricDataBuffer = 0;
		int BarycentricDataChannel = 0;
		const FMeshBufferSet& VB = BaseMesh->GetVertexBuffers();
		VB.FindChannel(MBS_BARYCENTRICCOORDS, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
		
		// Clone Without VertexBuffers or AdditionalBuffers
		constexpr EMeshCloneFlags CloneFlags = ~(EMeshCloneFlags::WithVertexBuffers | EMeshCloneFlags::WithAdditionalBuffers);
		MeshPtr Result = BaseMesh->Clone(CloneFlags);
	
		FMeshBufferSet& ResultBuffers = Result->GetVertexBuffers();

		check(ResultBuffers.m_buffers.Num() == 0);

		// Copy buffers skipping binding data. 
		ResultBuffers.m_elementCount = VB.m_elementCount;
		// Remove one element to the number of buffers if BarycentricDataBuffer found. 
		ResultBuffers.m_buffers.SetNum( FMath::Max( 0, VB.m_buffers.Num() - int32(BarycentricDataBuffer >= 0) ) );
		
		for (int32 B = 0, R = 0; B < VB.m_buffers.Num(); ++B)
		{
			if (B != BarycentricDataBuffer)
			{
				ResultBuffers.m_buffers[R++] = VB.m_buffers[B];
			}
		}

		// Copy the additional buffers skipping binding data.
		Result->m_AdditionalBuffers.Reserve( BaseMesh->m_AdditionalBuffers.Num() );
		for (const TPair<EMeshBufferType, FMeshBufferSet>& A : BaseMesh->m_AdditionalBuffers)
		{	
			const bool bIsBindOpBuffer = 
					A.Key == EMeshBufferType::SkeletonDeformBinding || 
					A.Key == EMeshBufferType::PhysicsBodyDeformBinding ||
					A.Key == EMeshBufferType::PhysicsBodyDeformSelection;
	
			if (!bIsBindOpBuffer)
			{
				Result->m_AdditionalBuffers.Add(A);
			}
		}
		if (!ShapeMesh)
		{
			return Result;
		}

		int32 ShapeVertexCount = ShapeMesh->GetVertexCount();
		int32 ShapeTriangleCount = ShapeMesh->GetFaceCount();
		if (!ShapeVertexCount || !ShapeTriangleCount)
		{
			return Result;
		}

		// Generate the temp vertex query data for the shape
		// TODO: Vertex data copy may be saved in most cases.
		TArray<FVector3f> ShapePositions;
		FShapeMeshDescriptorApply ShapeDescriptor;
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateVertexQueryData);

			ShapeDescriptor.Positions.SetNum(ShapeVertexCount);
			ShapeDescriptor.Normals.SetNum(ShapeVertexCount);

			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItPosition(ShapeMesh->GetVertexBuffers(), MBS_POSITION);
			UntypedMeshBufferIteratorConst ItNormal(ShapeMesh->GetVertexBuffers(), MBS_NORMAL);
			for (int32 ShapeVertexIndex = 0; ShapeVertexIndex < ShapeVertexCount; ++ShapeVertexIndex)
			{
				FVector3f Position = ItPosition.GetAsVec3f();
				ShapeDescriptor.Positions[ShapeVertexIndex] = Position;
				++ItPosition;

				FVector3f Normal = ItNormal.GetAsVec3f();
				ShapeDescriptor.Normals[ShapeVertexIndex] = Normal;
				++ItNormal;
			}
		}

		// Generate the temp face query data for the shape
		// TODO: Index data copy may be saved in most cases.
		{
			MUTABLE_CPUPROFILER_SCOPE(GenerateTriangleQueryData);

			ShapeDescriptor.Triangles.SetNum(ShapeTriangleCount);

			// \TODO: Simple but inefficient
			UntypedMeshBufferIteratorConst ItIndices(ShapeMesh->GetIndexBuffers(), MBS_VERTEXINDEX);
			for (int32 TriangleIndex = 0; TriangleIndex < ShapeTriangleCount; ++TriangleIndex)
			{
				UE::Geometry::FIndex3i Triangle;
				Triangle.A = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.B = int(ItIndices.GetAsUINT32());
				++ItIndices;
				Triangle.C = int(ItIndices.GetAsUINT32());
				++ItIndices;

				ShapeDescriptor.Triangles[TriangleIndex] = Triangle;
			}
		}

		if (bReshapeVertices && BarycentricDataBuffer >= 0)
		{
			MUTABLE_CPUPROFILER_SCOPE(ReshapeVertices);
			// \TODO: More checks
			check(BarycentricDataChannel == 0);
			check(VB.GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapeVertexBindingData));

			TArrayView<const FReshapeVertexBindingData> VerticesBindingData(
					(const FReshapeVertexBindingData*)VB.GetBufferData(BarycentricDataBuffer),
					VB.GetElementCount());

			ApplyToVertices(Result.get(), VerticesBindingData, ShapeDescriptor);
		}
	
		if (bReshapeSkeleton)
		{
			MUTABLE_CPUPROFILER_SCOPE(ReshapeSkeleton);

			// If the base mesh has no binding data for the skeleton don't do anything.
			const FMeshBufferSet* SkeletonBindBuffer = nullptr;
			for ( const TPair<EMeshBufferType, FMeshBufferSet>& A : BaseMesh->m_AdditionalBuffers )
			{
				if ( A.Key == EMeshBufferType::SkeletonDeformBinding )
				{
					SkeletonBindBuffer = &A.Value;
					break;
				}
			}
			
			BarycentricDataBuffer = -1;
			if (SkeletonBindBuffer)
			{
				SkeletonBindBuffer->FindChannel(MBS_BARYCENTRICCOORDS, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
			}

			if (SkeletonBindBuffer && BarycentricDataBuffer >= 0)
			{
				// \TODO: More checks
				check(BarycentricDataChannel == 0);
				check(SkeletonBindBuffer && SkeletonBindBuffer->GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapeVertexBindingData));

				TArrayView<const FReshapeVertexBindingData> SkeletonBindingData( 
						(const FReshapeVertexBindingData*)SkeletonBindBuffer->GetBufferData(BarycentricDataBuffer),
						SkeletonBindBuffer->GetElementCount());
			
				check(SkeletonBindBuffer->GetBufferCount() >= 2);
				TArrayView<const int32> BoneIndices( 
						(const int32*)SkeletonBindBuffer->GetBufferData(1), SkeletonBindBuffer->GetElementCount());

				ApplyToPose(Result.get(), SkeletonBindingData, BoneIndices, ShapeDescriptor);
			}
		}

		// Transform Physics Volumes based on the deformed bounding box points.
		const PhysicsBody* OldPhysicsBody = Result->m_pPhysicsBody.get();

		if (bReshapePhysicsVolumes && OldPhysicsBody)
		{	
			MUTABLE_CPUPROFILER_SCOPE(ReshapePhysicsBodies);
			
			using BufferEntryType = TPair<EMeshBufferType, FMeshBufferSet>;
			const BufferEntryType* FoundPhysicsBindBuffer = BaseMesh->m_AdditionalBuffers.FindByPredicate(
					[](BufferEntryType& E){ return E.Key == EMeshBufferType::PhysicsBodyDeformBinding; });

			const BufferEntryType* FoundPhysicsBindSelectionBuffer = BaseMesh->m_AdditionalBuffers.FindByPredicate(
					[](BufferEntryType& E){ return E.Key == EMeshBufferType::PhysicsBodyDeformSelection; });
	
			BarycentricDataBuffer = -1;
			if (FoundPhysicsBindBuffer)
			{
				FoundPhysicsBindBuffer->Value.FindChannel(MBS_BARYCENTRICCOORDS, BindingDataIndex, &BarycentricDataBuffer, &BarycentricDataChannel);
			}
			
			if (FoundPhysicsBindBuffer && FoundPhysicsBindSelectionBuffer && BarycentricDataBuffer >= 0)
			{
				const FMeshBufferSet& PhysicsBindBuffer = FoundPhysicsBindBuffer->Value;
				const FMeshBufferSet& PhyiscsBindSelectionBuffer = FoundPhysicsBindSelectionBuffer->Value;

				// \TODO: More checks
				check(BarycentricDataChannel == 0);
				check(PhysicsBindBuffer.GetElementSize(BarycentricDataBuffer) == (int)sizeof(FReshapeVertexBindingData));
					
				TArrayView<const FReshapeVertexBindingData> PhysicsBindingData(  
						(const FReshapeVertexBindingData*)PhysicsBindBuffer.GetBufferData(BarycentricDataBuffer),
						PhysicsBindBuffer.GetElementCount() );

				TArrayView<const int32> UsedIndices( 
						(const int32*)PhyiscsBindSelectionBuffer.GetBufferData(0), 
					    PhyiscsBindSelectionBuffer.GetElementCount());

				Ptr<PhysicsBody> NewPhysicsBody = OldPhysicsBody->Clone();
				Result->SetPhysicsBody(NewPhysicsBody);

				ApplyToPhysicsBodies( NewPhysicsBody.get(), BaseMesh, PhysicsBindingData, UsedIndices, ShapeDescriptor );
			}
		}
		
		return Result;
	}
}
