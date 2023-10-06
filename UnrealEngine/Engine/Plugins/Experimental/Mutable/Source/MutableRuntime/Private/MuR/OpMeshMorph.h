// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"
#include "MuR/SparseIndexMap.h"

namespace mu
{

	//---------------------------------------------------------------------------------------------
	//! Optimized linear factor version for morphing 2 targets
	//---------------------------------------------------------------------------------------------
    inline void MeshMorph2(Mesh* Result, const Mesh* pBase, const Mesh* pMin, const Mesh* pMax, const float Factor, bool& bOutSuccess)
    {
        MUTABLE_CPUPROFILER_SCOPE(MeshMorph2);
		bOutSuccess = true;

		if (!pBase)
		{
			bOutSuccess = true; // return an empty mesh.
			return;
		}

		const auto MakeIndexMap = [](
				UntypedMeshBufferIteratorConst BaseIdIter, int32 BaseNum,
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> MorphIdIter, int32 MorphNum) 
		-> SparseIndexMap
		{	
            uint32 MinBaseId = TNumericLimits<uint32>::Max();
            uint32 MaxBaseId = 0;
			{
				for (int32 Index = 0; Index < BaseNum; ++Index, ++BaseIdIter)
				{
					const uint32 BaseId = BaseIdIter.GetAsUINT32();
					MinBaseId = FMath::Min(BaseId, MinBaseId);
					MaxBaseId = FMath::Max(BaseId, MaxBaseId);
				}
			}

            SparseIndexMap IndexMap(MinBaseId, MaxBaseId);
           
            for (int32 Index = 0; Index < MorphNum; ++Index, ++MorphIdIter)
            {
                const uint32 MorphId = (*MorphIdIter)[0];
                
                IndexMap.Insert(MorphId, Index);
            }

			return IndexMap;
		};

        const auto ApplyNormalMorph = [](
				UntypedMeshBufferIteratorConst BaseIdIter, const TStaticArray<UntypedMeshBufferIterator, 3>& BaseTangentFrameIters, const int32 BaseNum,
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> MorphIdIter, UntypedMeshBufferIteratorConst& MorphNormalIter, const int32 MorphNum,
				const SparseIndexMap& IndexMap, const float Factor)
        -> void
        {
			const UntypedMeshBufferIterator& BaseNormalIter = BaseTangentFrameIters[2];
			const UntypedMeshBufferIterator& BaseTangentIter = BaseTangentFrameIters[1];
			const UntypedMeshBufferIterator& BaseBiNormalIter = BaseTangentFrameIters[0];

            const MESH_BUFFER_FORMAT NormalFormat = BaseNormalIter.GetFormat();
            const int32 NormalComps = BaseNormalIter.GetComponents();

            const MESH_BUFFER_FORMAT TangentFormat = BaseTangentIter.GetFormat();
            const int32 TangentComps = BaseTangentIter.GetComponents();

            const MESH_BUFFER_FORMAT BiNormalFormat = BaseBiNormalIter.GetFormat();
            const int32 BiNormalComps = BaseBiNormalIter.GetComponents();

			// When normal is packed, binormal channel is not expected. It is not a big deal if it's there but we would be doing extra unused work in that case. 
			ensureAlways(!(NormalFormat == MBF_PACKEDDIR8_W_TANGENTSIGN || NormalFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN) || !BaseBiNormalIter.ptr());

            for (int32 VertexIndex = 0; VertexIndex < BaseNum; ++VertexIndex)
            {
                const uint32 BaseId = (BaseIdIter + VertexIndex).GetAsUINT32();
                const int32 MorphIndex = static_cast<int32>(IndexMap.Find(BaseId));

                if (MorphIndex == SparseIndexMap::NotFoundValue)
                {
                    continue;
                }

                // Find consecutive run.
                UntypedMeshBufferIteratorConst RunBaseIter = BaseIdIter + VertexIndex;
                MeshBufferIteratorConst<MBF_UINT32, uint32, 1> RunMorphIter = MorphIdIter + MorphIndex;

                int32 RunSize = 0;
                for ( ; VertexIndex + RunSize < BaseNum && MorphIndex + RunSize < MorphNum && RunBaseIter.GetAsUINT32() == (*RunMorphIter)[0];
                        ++RunSize, ++RunBaseIter, ++RunMorphIter);

				for (int32 RunIndex = 0; RunIndex < RunSize; ++RunIndex)
				{
					UntypedMeshBufferIterator NormalIter = BaseNormalIter + (VertexIndex + RunIndex);

					const FVector3f BaseNormal = NormalIter.GetAsVec3f();
					const FVector3f MorphNormal = (MorphNormalIter + (MorphIndex + RunIndex)).GetAsVec3f();
 
					const FVector3f Normal = (BaseNormal + MorphNormal*Factor).GetSafeNormal();
				
					// Leave the tangent basis sign untouched for packed normals formats.
					for (int32 C = 0; C < NormalComps && C < 3; ++C)
					{
						ConvertData(C, NormalIter.ptr(), NormalFormat, &Normal, MBF_FLOAT32);
					}

					// Tangent
					if (BaseTangentIter.ptr())
					{
						UntypedMeshBufferIterator TangentIter = BaseTangentIter + (VertexIndex + RunIndex);

						const FVector3f BaseTangent = TangentIter.GetAsVec3f();

						// Orthogonalize Tangent based on new Normal. This assumes Normal and BaseTangent are normalized and different.
						const FVector3f Tangent = (BaseTangent - FVector3f::DotProduct(Normal, BaseTangent) * Normal).GetSafeNormal();

						for (int32 C = 0; C < TangentComps && C < 3; ++C)
						{
							ConvertData(C, TangentIter.ptr(), TangentFormat, &Tangent, MBF_FLOAT32);
						}

						// BiNormal
						if (BaseBiNormalIter.ptr())
						{
							UntypedMeshBufferIterator BiNormalIter = BaseBiNormalIter + (VertexIndex + RunIndex);

							const FVector3f& N = BaseNormal;
							const FVector3f& T = BaseTangent;
							const FVector3f  B = BiNormalIter.GetAsVec3f();

							const float BaseTangentBasisDeterminant =  
									B.X*T.Y*N.Z + B.Z*T.X*N.Y + B.Y*T.Z*N.Y -
									B.Z*T.Y*N.X - B.Y*T.X*N.Z - B.X*T.Z*N.Y;

							const float BaseTangentBasisDeterminantSign = BaseTangentBasisDeterminant >= 0 ? 1.0f : -1.0f;

							const FVector3f BiNormal = FVector3f::CrossProduct(Tangent, Normal) * BaseTangentBasisDeterminantSign;

							for (int32 C = 0; C < BiNormalComps && C < 3; ++C)
							{
								ConvertData(C, BiNormalIter.ptr(), BiNormalFormat, &BiNormal, MBF_FLOAT32);
							}
						}
					}
				}

				VertexIndex += FMath::Max(RunSize - 1, 0);
            }
        };

        const auto ApplyGenericMorph = []( 
				UntypedMeshBufferIteratorConst BaseIdIter, const TArray<UntypedMeshBufferIterator>& BaseChannelsIters, const int32 BaseNum,
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> MorphIdIter, const TArray<UntypedMeshBufferIteratorConst>& MorphChannelsIters, const int32 MorphNum,
				const SparseIndexMap& IndexMap, const float Factor)
        -> void
        {
            for (int32 VertexIndex = 0; VertexIndex < BaseNum; ++VertexIndex)
            {
                const uint32 BaseId = (BaseIdIter + VertexIndex).GetAsUINT32();
                const uint32 MorphIdx = IndexMap.Find(BaseId);

                if (MorphIdx == SparseIndexMap::NotFoundValue)
                {
                    continue;
                }

                const int32 MorphIndex = static_cast<int32>(MorphIdx);

                // Find consecutive run.
                UntypedMeshBufferIteratorConst RunBaseIter = BaseIdIter + VertexIndex;
                MeshBufferIteratorConst<MBF_UINT32, uint32, 1> RunMorphIter = MorphIdIter + MorphIndex;

                int32 RunSize = 0;
                for ( ; VertexIndex + RunSize < BaseNum && MorphIndex + RunSize < MorphNum && RunBaseIter.GetAsUINT32() == (*RunMorphIter)[0];
                        ++RunSize, ++RunBaseIter, ++RunMorphIter);

                const int32 ChannelNum = MorphChannelsIters.Num();
                for (int32 ChannelIndex = 1; ChannelIndex < ChannelNum; ++ChannelIndex)
                {
                	if (!(BaseChannelsIters[ChannelIndex].ptr() && MorphChannelsIters[ChannelIndex].ptr()))
                	{
                		continue;
                	}
                
                    UntypedMeshBufferIterator ChannelBaseIter = BaseChannelsIters[ChannelIndex] + VertexIndex;
					UntypedMeshBufferIteratorConst ChannelMorphIter = MorphChannelsIters[ChannelIndex] + MorphIndex;
                   
                    const MESH_BUFFER_FORMAT DestChannelFormat = BaseChannelsIters[ChannelIndex].GetFormat();
                    const int32 DestChannelComps = BaseChannelsIters[ChannelIndex].GetComponents();

                    // Apply Morph to range found above.
                    for (int32 R = 0; R < RunSize; ++R, ++ChannelBaseIter, ++ChannelMorphIter)
                    {
                        const FVector4f Value = ChannelBaseIter.GetAsVec4f() + ChannelMorphIter.GetAsVec4f()*Factor;

                        // TODO: Optimize this for the specific components.
                        // Max 4 components
                        for (int32 Comp = 0; Comp < DestChannelComps && Comp < 4; ++Comp)
                        {
                            ConvertData(Comp, ChannelBaseIter.ptr(), DestChannelFormat, &Value, MBF_FLOAT32);
                        }
                    }
                }

				VertexIndex += FMath::Max(RunSize - 1, 0);
            }
        };


		// Number of vertices to modify
		const int32 MinNum = pMin ? pMin->GetVertexBuffers().GetElementCount() : 0;
		const int32 MaxNum = pMax ? pMax->GetVertexBuffers().GetElementCount() : 0;
		const int32 BaseNum = pBase ? pBase->GetVertexBuffers().GetElementCount() : 0;
		const Mesh* RefTarget = MinNum > 0 ? pMin : pMax;

		if (BaseNum == 0 || (MinNum + MaxNum) == 0)
		{
			bOutSuccess = false; // Use the passed pBase as result.
			return;
		}

		Result->CopyFrom(*pBase);

		if (RefTarget)
		{
			const int32 ChannelsNum = RefTarget->GetVertexBuffers().GetBufferChannelCount(0);

			TArray<UntypedMeshBufferIterator> BaseChannelsIters;
			BaseChannelsIters.SetNum(ChannelsNum);
			TArray<UntypedMeshBufferIteratorConst> MinChannelsIters;
			MinChannelsIters.SetNum(ChannelsNum);
			TArray<UntypedMeshBufferIteratorConst> MaxChannelsIters;
			MaxChannelsIters.SetNum(ChannelsNum);

			// {BiNormal, Tangent, Normal}
			TStaticArray<UntypedMeshBufferIterator, 3> BaseTangentFrameChannelsIters;
			UntypedMeshBufferIteratorConst MinNormalChannelIter;
			UntypedMeshBufferIteratorConst MaxNormalChannelIter;

			const bool bBaseHasNormals = UntypedMeshBufferIteratorConst(pBase->GetVertexBuffers(), MBS_NORMAL, 0).ptr() != nullptr;
			for (int32 ChannelIndex = 1; ChannelIndex < ChannelsNum; ++ChannelIndex)
			{
				const FMeshBufferSet& MBSPriv = RefTarget->GetVertexBuffers();
				MESH_BUFFER_SEMANTIC Sem = MBSPriv.m_buffers[0].m_channels[ChannelIndex].m_semantic;
				int32 SemIndex = MBSPriv.m_buffers[0].m_channels[ChannelIndex].m_semanticIndex;
			
				if (Sem == MBS_NORMAL && bBaseHasNormals)
				{
					BaseTangentFrameChannelsIters[2] = UntypedMeshBufferIterator(Result->GetVertexBuffers(), Sem, SemIndex);
					if (MinNum > 0)
					{
						MinNormalChannelIter = UntypedMeshBufferIteratorConst(pMin->GetVertexBuffers(), Sem, SemIndex);
					}

					if (MaxNum > 0)
					{
						MaxNormalChannelIter = UntypedMeshBufferIteratorConst(pMax->GetVertexBuffers(), Sem, SemIndex);
					}
				}
				else if (Sem == MBS_TANGENT && bBaseHasNormals)
				{
					BaseTangentFrameChannelsIters[1] = UntypedMeshBufferIterator(Result->GetVertexBuffers(), Sem, SemIndex);
				}
				else if (Sem == MBS_BINORMAL && bBaseHasNormals)
				{
					BaseTangentFrameChannelsIters[0] = UntypedMeshBufferIterator(Result->GetVertexBuffers(), Sem, SemIndex);
				}
				else
				{
					BaseChannelsIters[ChannelIndex] = UntypedMeshBufferIterator(Result->GetVertexBuffers(), Sem, SemIndex);
					if (MinNum > 0)
					{
						MinChannelsIters[ChannelIndex] = UntypedMeshBufferIteratorConst(pMin->GetVertexBuffers(), Sem, SemIndex);
					}

					if (MaxNum > 0)
					{
						MaxChannelsIters[ChannelIndex] = UntypedMeshBufferIteratorConst(pMax->GetVertexBuffers(), Sem, SemIndex);
					}
				}
			}
			
			UntypedMeshBufferIteratorConst BaseIdIter(pBase->GetVertexBuffers(), MBS_VERTEXINDEX);

			if (MinNum > 0)
			{
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> MinIdIter(pMin->GetVertexBuffers(), MBS_VERTEXINDEX);
				SparseIndexMap IndexMap = MakeIndexMap(BaseIdIter, BaseNum, MinIdIter, MinNum);

				ApplyGenericMorph(BaseIdIter, BaseChannelsIters, BaseNum, MinIdIter, MinChannelsIters, MinNum, IndexMap, 1.0f - Factor);
				if (MinNormalChannelIter.ptr())
				{
					ApplyNormalMorph(BaseIdIter, BaseTangentFrameChannelsIters, BaseNum, MinIdIter, MinNormalChannelIter, MinNum, IndexMap, 1.0f - Factor);
				}
			}

			if (MaxNum > 0)
			{
				MeshBufferIteratorConst<MBF_UINT32, uint32, 1> MaxIdIter(pMax->GetVertexBuffers(), MBS_VERTEXINDEX);
				SparseIndexMap IndexMap = MakeIndexMap(BaseIdIter, BaseNum, MaxIdIter, MaxNum);

				ApplyGenericMorph(BaseIdIter, BaseChannelsIters, BaseNum, MaxIdIter, MaxChannelsIters, MaxNum, IndexMap, Factor);
				
				if (MaxNormalChannelIter.ptr())
				{
					ApplyNormalMorph(BaseIdIter, BaseTangentFrameChannelsIters, BaseNum, MaxIdIter, MaxNormalChannelIter, MaxNum, IndexMap, Factor);
				}
			}
		}
    }

	//---------------------------------------------------------------------------------------------
	//! \TODO Optimized linear factor version
	//---------------------------------------------------------------------------------------------
	inline void MeshMorph(Mesh* Result, const Mesh* pBase, const Mesh* pMorph, float factor, bool& bOutSuccess)
	{
		MeshMorph2(Result, pBase, nullptr, pMorph, factor, bOutSuccess);
	}

    //---------------------------------------------------------------------------------------------
    //! \TODO Optimized Factor-less version
    //---------------------------------------------------------------------------------------------
	inline void MeshMorph(Mesh* Result, const Mesh* pBase, const Mesh* pMorph, bool& bOutSuccess)
    {
        // Trust the compiler to remove the factor
		MeshMorph(Result, pBase, pMorph, 1.0f, bOutSuccess);
    }
}
