// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"
#include "MuR/OpMeshChartDifference.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Reference version
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshApplyPose(const Mesh* pBase, const Mesh* pPose)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshApplyPose);

        MeshPtr pDest = pBase->Clone();

		mu::SkeletonPtrConst pSkeleton = pBase->GetSkeleton();
		if (!pSkeleton)
		{
			return pDest;
		}

        // We assume the matrices are transforms from the binding pose bone to the new pose
        // For now we only convert the vertex positions.
        // \TODO: normals and tangents

        // Prepare the skin matrices. They may be in different order, and we only need the ones
        // relevant for the base mesh deformation.
		TArray<FTransform3f> skinTransforms;
		skinTransforms.SetNum(pSkeleton->GetBoneCount());
        for ( int32 b=0; b< pSkeleton->GetBoneCount(); ++b )
        {
            int32 poseBoneIndex = pPose->FindBonePose(pSkeleton->GetBoneName(b));

            if( poseBoneIndex != INDEX_NONE )
            {
				skinTransforms[b] = pPose->m_bonePoses[poseBoneIndex].m_boneTransform;
            }
            else
            {
                // This could happen if the model is compiled with maximum optimization and the skeleton
				// has bones used by other meshes.
				skinTransforms[b] = FTransform3f::Identity;
            }

        }

        // Get pointers to vertex position data
		MeshBufferIteratorConst<MBF_FLOAT32, float, 3> itSource( pBase->m_VertexBuffers, MBS_POSITION, 0 );
		MeshBufferIterator<MBF_FLOAT32, float, 3> itTarget( pDest->m_VertexBuffers, MBS_POSITION, 0 );
        if (!itSource.ptr() || !itTarget.ptr())
        {
            // Formats not implemented
            check(false);
            return pDest;
        }

        // Get pointers to skinning data
		UntypedMeshBufferIteratorConst itBoneIndices( pBase->m_VertexBuffers, MBS_BONEINDICES, 0 );
		UntypedMeshBufferIteratorConst itBoneWeights( pBase->m_VertexBuffers, MBS_BONEWEIGHTS, 0 );
        if (!itBoneIndices.ptr() || !itBoneWeights.ptr())
        {
            // No skinning data
            check(false);
            return pDest;
        }

        // Proceed
        int vertexCount = pBase->GetVertexCount();
        int weightCount = itBoneIndices.GetComponents();
        check( weightCount == itBoneWeights.GetComponents() );

        constexpr int MAX_BONES_PER_VERTEX = 16;
        check( weightCount < MAX_BONES_PER_VERTEX );

        for ( int v=0; v<vertexCount; ++v )
        {
            FVector3f sourcePosition = ToUnreal(*itSource);
			FVector3f position = FVector3f(0,0,0);

            float totalWeight = 0.0f;

            for ( int w=0; w<weightCount; ++w )
            {
                float weight[MAX_BONES_PER_VERTEX] = {};
                ConvertData( w, &weight, MBF_FLOAT32, itBoneWeights.ptr(), itBoneWeights.GetFormat() );

                uint32_t boneIndex[MAX_BONES_PER_VERTEX] = {};
                ConvertData( w, &boneIndex, MBF_UINT32, itBoneIndices.ptr(), itBoneIndices.GetFormat() );

                totalWeight += weight[w];

                //vec4f p = skinMatrices[ boneIndex[w] ]
                //        * vec4f( sourcePosition, 1.0f )
                //        * weight[w];
				FVector3f p = skinTransforms[boneIndex[w]].TransformPosition(sourcePosition);
				position += p * weight[w];
            }

            if (totalWeight<1e-5f)
            {
                position = sourcePosition;
            }
            else
            {
                position /= totalWeight;
            }

            *itTarget = FromUnreal(position);

            ++itTarget;
            ++itSource;
            ++itBoneIndices;
            ++itBoneWeights;
        }


        return pDest;
    }

}
