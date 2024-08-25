// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"


namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Reference version
    //---------------------------------------------------------------------------------------------
    inline void MeshApplyPose(Mesh* Result, const Mesh* pBase, const Mesh* pPose, bool& bOutSuccess)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshApplyPose);

		bOutSuccess = true;

		mu::SkeletonPtrConst pSkeleton = pBase->GetSkeleton();
		if (!pSkeleton)
		{
			bOutSuccess = false;
			return;
		}

        // We assume the matrices are transforms from the binding pose bone to the new pose
        // For now we only convert the vertex positions.
        // \TODO: normals and tangents

		// Find closest bone affected by the pose for each bone in the skeleton.
		const int32 NumBones = pSkeleton->GetBoneCount();
		TArray<int32> BoneToPoseIndex;
		BoneToPoseIndex.Init(INDEX_NONE, NumBones);

		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			const int32 BonePoseIndex = pPose->FindBonePose(pSkeleton->GetBoneId(BoneIndex));
			if (BonePoseIndex != INDEX_NONE)
			{
				BoneToPoseIndex[BoneIndex] = BonePoseIndex;
				continue;
			}

			// Parent bones are in a strictly increassing order. Set the pose index from the parent bone.
			const int32 ParentIndex = pSkeleton->GetBoneParent(BoneIndex);
			if (ParentIndex != INDEX_NONE)
			{
				BoneToPoseIndex[BoneIndex] = BoneToPoseIndex[ParentIndex];
			}
		}


		TArray<uint16> BoneMap = pBase->GetBoneMap();
		const int32 NumBonesBoneMap = BoneMap.Num();

        // Prepare the skin matrices. They may be in different order, and we only need the ones
        // relevant for the base mesh deformation.
		TArray<FTransform3f> SkinTransforms;
		SkinTransforms.Reserve(NumBonesBoneMap);

		bool bBonesAffected = false;
		for (int32 Index = 0; Index < NumBonesBoneMap; ++Index)
		{
			const int32 BoneIndex = pSkeleton->FindBone(BoneMap[Index]);
			if (BoneToPoseIndex[BoneIndex] != INDEX_NONE)
			{
				SkinTransforms.Add(pPose->BonePoses[BoneToPoseIndex[BoneIndex]].BoneTransform);
				bBonesAffected = true;
			}
			else
			{
				// Bone not affected by the pose. Set identity.
				SkinTransforms.Add(FTransform3f::Identity);
			}
		}

		if (!bBonesAffected)
		{
			// The pose does not affect any vertex in the mesh. 
			bOutSuccess = false;
			return;
		}

        // Get pointers to vertex position data
		MeshBufferIteratorConst<MBF_FLOAT32, float, 3> itSource(pBase->m_VertexBuffers, MBS_POSITION, 0);
        if (!itSource.ptr())
        {
            // Formats not implemented
            check(false);
			bOutSuccess = false;
            return;
        }

        // Get pointers to skinning data
		UntypedMeshBufferIteratorConst itBoneIndices(pBase->m_VertexBuffers, MBS_BONEINDICES, 0);
		UntypedMeshBufferIteratorConst itBoneWeights(pBase->m_VertexBuffers, MBS_BONEWEIGHTS, 0);
        if (!itBoneIndices.ptr() || !itBoneWeights.ptr())
        {
            // No skinning data
            check(false);
			bOutSuccess = false;
            return;
        }

		Result->CopyFrom(*pBase);

		MeshBufferIterator<MBF_FLOAT32, float, 3> itTarget(Result->m_VertexBuffers, MBS_POSITION, 0);
		check(itTarget.ptr());

        // Proceed
        int vertexCount = pBase->GetVertexCount();
        int weightCount = itBoneIndices.GetComponents();
        check( weightCount == itBoneWeights.GetComponents() );

        constexpr int MAX_BONES_PER_VERTEX = 16;
        check( weightCount < MAX_BONES_PER_VERTEX );

        for ( int v=0; v<vertexCount; ++v )
        {
            FVector3f sourcePosition = itSource.GetAsVec3f();
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
				FVector3f p = SkinTransforms[boneIndex[w]].TransformPosition(sourcePosition);
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

			(*itTarget)[0] = position[0];
			(*itTarget)[1] = position[1];
			(*itTarget)[2] = position[2];

            ++itTarget;
            ++itSource;
            ++itBoneIndices;
            ++itBoneWeights;
        }
    }

}
