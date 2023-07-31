// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"
#include "MuR/MutableMath.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/MutableTrace.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	//! Merge two meshes into one new mesh
	//---------------------------------------------------------------------------------------------
	inline MeshPtr MeshMerge( const Mesh* pFirst, const Mesh* pSecond )
	{
        MUTABLE_CPUPROFILER_SCOPE(MeshMerge);

        MeshPtr pResult = new Mesh;

		// Should never happen, but fixes static analysis warnings.
		if (!(pFirst && pSecond))
		{
			return pResult;
		}

		// Indices
		//-----------------
		if ( pFirst->GetIndexBuffers().GetBufferCount()>0 )
		{
            MUTABLE_CPUPROFILER_SCOPE(Indices);

            int firstCount = pFirst->GetIndexBuffers().GetElementCount();
			int secondCount = pSecond->GetIndexBuffers().GetElementCount();
			pResult->GetIndexBuffers().SetElementCount( firstCount+secondCount );

			check( pFirst->GetIndexBuffers().GetBufferCount() <= 1 );
			check( pSecond->GetIndexBuffers().GetBufferCount() <= 1 );
			pResult->GetIndexBuffers().SetBufferCount( 1 );

            // This will be changed below if need to change the format of the index buffers.
            MESH_BUFFER_FORMAT requiredBufferFormatChange = MBF_NONE;

            if (firstCount && secondCount)
			{
				const MESH_BUFFER& first = pFirst->GetIndexBuffers().m_buffers[0];
				const MESH_BUFFER& second = pSecond->GetIndexBuffers().m_buffers[0];
				check(first.m_channels == second.m_channels);
				check(first.m_elementSize == second.m_elementSize);
                check(first.m_channels.IsEmpty() || first.m_channels[0].m_format == second.m_channels[0].m_format);
				
				// Avoid unused variable warnings
				(void)first;
				(void)second;

                // We need to know the total number of vertices in case we need to adjust the index buffer format.
                uint64_t totalVertexCount = pFirst->GetVertexBuffers().GetElementCount()
                        + pSecond->GetVertexBuffers().GetElementCount();
                uint64_t maxValueBits = GetMeshFormatData(pFirst->GetIndexBuffers().m_buffers[0].m_channels[0].m_format).m_maxValueBits;
                uint64_t maxSupportedVertices = uint64_t(1)<<maxValueBits;
                if (totalVertexCount>maxSupportedVertices)
                {
                    if (totalVertexCount > 0xFFFF)
                    {
                        requiredBufferFormatChange = MBF_UINT32;
                    }
                    else
                    {
                        requiredBufferFormatChange = MBF_UINT16;
                    }
                }
            }

			MESH_BUFFER& result = pResult->GetIndexBuffers().m_buffers[0];

            if (requiredBufferFormatChange != MBF_NONE)
            {
                // We only support vertex indices in case of having to change the format.
                check(pFirst->GetIndexBuffers().m_buffers[0].m_channels.Num()==1);

                result.m_channels.SetNum(1);
                result.m_channels[0].m_semantic = MBS_VERTEXINDEX;
                result.m_channels[0].m_format = requiredBufferFormatChange;
                result.m_channels[0].m_componentCount = 1;
                result.m_channels[0].m_semanticIndex = 0;
                result.m_channels[0].m_offset = 0;
                result.m_elementSize = GetMeshFormatData(requiredBufferFormatChange).m_size;
            }
            else if (firstCount)
			{
				const MESH_BUFFER& first = pFirst->GetIndexBuffers().m_buffers[0];
				result.m_channels = first.m_channels;
				result.m_elementSize = first.m_elementSize;
			}
            else if (secondCount)
			{
				const MESH_BUFFER& second = pSecond->GetIndexBuffers().m_buffers[0];
				result.m_channels = second.m_channels;
				result.m_elementSize = second.m_elementSize;
			}

			result.m_data.SetNum(result.m_elementSize*(firstCount + secondCount));

			check( result.m_channels.Num()==1 );
			check( result.m_channels[0].m_semantic == MBS_VERTEXINDEX );

			if ( result.m_data.Num() )
			{
				if ( firstCount )
				{
					const MESH_BUFFER& first = pFirst->GetIndexBuffers().m_buffers[0];

                    if (requiredBufferFormatChange==MBF_NONE
                            || requiredBufferFormatChange==first.m_channels[0].m_format)
                    {
                        FMemory::Memcpy( &result.m_data[0],
                                        &first.m_data[0],
                                        first.m_elementSize*firstCount );
                    }
                    else
                    {
                        // Conversion required
                        const uint8_t* pSource = &first.m_data[0];
                        uint8_t* pDest = &result.m_data[0];
                        switch( requiredBufferFormatChange )
                        {
                        case MBF_UINT32:
                        {
                            switch( first.m_channels[0].m_format )
                            {
                            case MBF_UINT16:
                            {
                                for ( int v=0; v<firstCount; ++v )
                                {
                                    *(uint32_t*)pDest = *(const uint16*)pSource;
                                    pSource+=first.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            case MBF_UINT8:
                            {
                                for ( int v=0; v<firstCount; ++v )
                                {
                                    *(uint32_t*)pDest = *(const uint8_t*)pSource;
                                    pSource+=first.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            default:
                                checkf( false, TEXT("Format not supported.") );
                                break;
                            }
                            break;
                        }

                        case MBF_UINT16:
                        {
                            switch( first.m_channels[0].m_format )
                            {

                            case MBF_UINT8:
                            {
                                for ( int v=0; v<firstCount; ++v )
                                {
                                    *(uint16*)pDest = *(const uint8_t*)pSource;
                                    pSource+=first.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            default:
								checkf(false, TEXT("Format not supported."));
								break;
                            }
                            break;
                        }

                        default:
							checkf(false, TEXT("Format not supported."));
							break;
                        }
                    }
                }

				if ( secondCount )
				{
					const MESH_BUFFER& second = pSecond->GetIndexBuffers().m_buffers[0];
                    const uint8_t* pSource = &second.m_data[0];
                    uint8_t* pDest = &result.m_data[result.m_elementSize*firstCount ];

                    uint32_t firstVertexCount = pFirst->GetVertexBuffers().GetElementCount();

                    if (requiredBufferFormatChange==MBF_NONE
                            || requiredBufferFormatChange==second.m_channels[0].m_format)
                    {
                        switch( second.m_channels[0].m_format )
                        {
                        case MBF_INT32:
                        case MBF_UINT32:
                        case MBF_NINT32:
                        case MBF_NUINT32:
                        {
                            for ( int v=0; v<secondCount; ++v )
                            {
                                *(uint32_t*)pDest = firstVertexCount + *(const uint32_t*)pSource;
                                pSource+=second.m_elementSize;
                                pDest+=result.m_elementSize;
                            }
                            break;
                        }

                        case MBF_INT16:
                        case MBF_UINT16:
                        case MBF_NINT16:
                        case MBF_NUINT16:
                        {
                            for ( int v=0; v<secondCount; ++v )
                            {
                                *(uint16*)pDest = uint16(firstVertexCount) + *(const uint16*)pSource;
                                pSource+=second.m_elementSize;
                                pDest+=result.m_elementSize;
                            }
                            break;
                        }

                        case MBF_INT8:
                        case MBF_UINT8:
                        case MBF_NINT8:
                        case MBF_NUINT8:
                        {
                            for ( int v=0; v<secondCount; ++v )
                            {
                                *(uint8_t*)pDest = uint8_t(firstVertexCount) + *(const uint8_t*)pSource;
                                pSource+=second.m_elementSize;
                                pDest+=result.m_elementSize;
                            }
                            break;
                        }

                        default:
							checkf(false, TEXT("Format not supported."));
							break;
                        }
                    }
                    else
                    {
                        // Format conversion required
                        switch (requiredBufferFormatChange)
                        {

                        case MBF_UINT32:
                        {
                            switch( second.m_channels[0].m_format )
                            {
                            case MBF_INT16:
                            case MBF_UINT16:
                            case MBF_NINT16:
                            case MBF_NUINT16:
                            {
                                for ( int v=0; v<secondCount; ++v )
                                {
                                    *(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint16*)pSource;
                                    pSource+=second.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            case MBF_INT8:
                            case MBF_UINT8:
                            case MBF_NINT8:
                            case MBF_NUINT8:
                            {
                                for ( int v=0; v<secondCount; ++v )
                                {
                                    *(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint8_t*)pSource;
                                    pSource+=second.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            default:
								checkf(false, TEXT("Format not supported."));
								break;
                            }

                            break;
                        }

                        case MBF_UINT16:
                        {
                            switch( second.m_channels[0].m_format )
                            {
                            case MBF_INT8:
                            case MBF_UINT8:
                            case MBF_NINT8:
                            case MBF_NUINT8:
                            {
                                for ( int v=0; v<secondCount; ++v )
                                {
                                    *(uint16*)pDest = uint16(firstVertexCount) + *(const uint8_t*)pSource;
                                    pSource+=second.m_elementSize;
                                    pDest+=result.m_elementSize;
                                }
                                break;
                            }

                            default:
								checkf(false, TEXT("Format not supported."));
								break;
                            }

                            break;
                        }

                        default:
							checkf(false, TEXT("Format not supported."));
							break;

                        }
                    }
                }
			}
		}

		// Faces
		//-----------------
		{
            MUTABLE_CPUPROFILER_SCOPE(Faces);

            int firstCount = pFirst->GetFaceBuffers().GetElementCount();
			int secondCount = pSecond->GetFaceBuffers().GetElementCount();
			pResult->GetFaceBuffers().SetElementCount( firstCount+secondCount );
			pResult->GetFaceBuffers().SetBufferCount( pFirst->GetFaceBuffers().GetBufferCount() );

			// Merge only the buffers present in the first mesh
			for ( int b = 0; b<pResult->GetFaceBuffers().GetBufferCount(); ++b )
			{
				const MESH_BUFFER& first = pFirst->GetFaceBuffers().m_buffers[b];

				MESH_BUFFER& result = pResult->GetFaceBuffers().m_buffers[b];
				result.m_channels = first.m_channels;
				result.m_elementSize = first.m_elementSize;
				result.m_data.SetNum( result.m_elementSize * (firstCount+secondCount) );

				MESH_BUFFER_SEMANTIC semantic = MBS_NONE;
				int semanticIndex = 0;
				MESH_BUFFER_FORMAT type = MBF_NONE;
				int components = 0;
				int offset = 0;
				pFirst->GetFaceBuffers().GetChannel
						( b, 0, &semantic, &semanticIndex, &type, &components, &offset );

				if ( firstCount )
				{
					FMemory::Memcpy( &result.m_data[0],
									&first.m_data[0],
									result.m_elementSize * firstCount
									);
				}

				if ( secondCount )
				{
					// Find in the second mesh
					int otherBuffer = -1;
					int otherChannel = -1;
					pSecond->GetFaceBuffers().FindChannel
							( semantic, 0, &otherBuffer, &otherChannel );

					if (otherBuffer>=0)
					{
						const MESH_BUFFER& second =
								pSecond->GetFaceBuffers().m_buffers[otherBuffer];
						check( first.m_channels == second.m_channels );

						// Raw copy
						FMemory::Memcpy( &result.m_data[ result.m_elementSize * firstCount ],
										&second.m_data[0],
										result.m_elementSize * secondCount );
					}
					else
					{
						// Fill with zeroes
						FMemory::Memzero( &result.m_data[ result.m_elementSize * firstCount ],
										result.m_elementSize * secondCount );
					}
				}
			}
		}


        // Layouts
        //-----------------
        {
            MUTABLE_CPUPROFILER_SCOPE(Layouts);

            pResult->m_layouts.SetNum( pFirst->m_layouts.Num() );
            for ( int i=0; i<pFirst->m_layouts.Num(); ++i )
            {
                const Layout* pF = pFirst->m_layouts[i].get();
                LayoutPtr pR = pF->Clone();

                if ( i<pSecond->m_layouts.Num() )
                {
                    const Layout* pS = pSecond->m_layouts[i].get();

                    pR->m_blocks.Append(pS->m_blocks);
                }

                pResult->m_layouts[i] = pR;
            }
        }


        // Surfaces
        //-----------------
        pResult->m_surfaces = pFirst->m_surfaces;

        for ( const auto& s : pSecond->m_surfaces )
        {
            pResult->m_surfaces.Add(s);
            pResult->m_surfaces.Last().m_firstVertex += pFirst->GetVertexCount();
            pResult->m_surfaces.Last().m_firstIndex += pFirst->GetIndexCount();
        }


		// Skeleton
		//---------------------------

        // Do they have the same skeleton?
        bool remapNeeded = pFirst->GetSkeleton() != pSecond->GetSkeleton();

        // Are they different skeletons but with the same data?
        if ( remapNeeded && pFirst->GetSkeleton() && pSecond->GetSkeleton() )
        {
            remapNeeded = ! ( *pFirst->GetSkeleton()
                              ==
                              *pSecond->GetSkeleton() );
        }


		TArray<int32> secondToFirstBones;

        if ( remapNeeded )
        {
            MUTABLE_CPUPROFILER_SCOPE(Skeleton);
            Ptr<Skeleton> pResultSkeleton;

			mu::SkeletonPtrConst pFirstSkeleton = pFirst->GetSkeleton();
			mu::SkeletonPtrConst pSecondSkeleton = pSecond->GetSkeleton();

			const int32 pFirstBoneCount = pFirstSkeleton ? pFirstSkeleton->GetBoneCount() : 0;
			const int32 pSecondBoneCount = pSecondSkeleton ? pSecondSkeleton->GetBoneCount() : 0;
			
			pResultSkeleton = pFirstSkeleton ? pFirstSkeleton->Clone() : new Skeleton; 
			pResult->SetSkeleton(pResultSkeleton);

			secondToFirstBones.SetNumUninitialized(pSecondBoneCount);

			// Merge pSecond and build the remap array 
            for ( int32 ob=0; ob< pSecondBoneCount; ++ob )
            {
                int32 tb = INDEX_NONE;
                for ( int32 c=0; tb<0 && c<pResultSkeleton->m_bones.Num(); ++c )
                {
                    if ( pResultSkeleton->m_bones[c] == pSecondSkeleton->GetBoneName( ob ) )
                    {
                        tb = c;
						break;
                    }
                }

                // Add a new bone
                if( tb == INDEX_NONE)
                {
                    tb = pResultSkeleton->m_bones.Num();
                    pResultSkeleton->m_bones.Add( pSecondSkeleton->m_bones[ob] );
					pResultSkeleton->m_boneIds.Add(pSecondSkeleton->m_boneIds[ob]);
					
					// Add an incorrect index, to be fixed below in case the parent index is later in the bone array.
					pResultSkeleton->m_boneParents.Add(pSecondSkeleton->m_boneParents[ob]);
                }

                secondToFirstBones[ob] = tb;
			}

            // Fix second mesh bone parents
            for (int32 ob = pFirstBoneCount; ob < pResultSkeleton->m_boneParents.Num(); ++ob)
            {
                int16_t secondMeshIndex = pResultSkeleton->m_boneParents[ob];
                if (secondMeshIndex != INDEX_NONE)
                {
                    pResultSkeleton->m_boneParents[ob] = (uint16)secondToFirstBones[secondMeshIndex];
                }
            }
        }
        else
        {
            pResult->SetSkeleton( pFirst->GetSkeleton() );
        }


		// Pose
		//---------------------------
		{
			MUTABLE_CPUPROFILER_SCOPE(Pose);

			pResult->m_bonePoses.Reserve(pResult->GetSkeleton()->GetBoneCount());
			
			// Copy poses from the first mesh
			pResult->m_bonePoses = pFirst->m_bonePoses;

			// Add or override bone poses
			for (const Mesh::FBonePose& pSecondBonePose : pSecond->m_bonePoses)
			{
				const int32 resultBoneIndex = pResult->FindBonePose(pSecondBonePose.m_boneName.c_str());

				if (resultBoneIndex != INDEX_NONE)
				{
					Mesh::FBonePose& pResultBonePose = pResult->m_bonePoses[resultBoneIndex];

					// override pose if the bone is not used by the skinning of the first mesh
					if (!pResultBonePose.m_boneSkinned)
					{
						pResultBonePose.m_boneName = pSecondBonePose.m_boneName;
						pResultBonePose.m_boneSkinned = pSecondBonePose.m_boneSkinned;
						pResultBonePose.m_boneTransform = pSecondBonePose.m_boneTransform;
					}
				}
				else
				{
					pResult->m_bonePoses.Add(pSecondBonePose);
				}
			}

			pResult->m_bonePoses.Shrink();
		}

		
		// PhysicsBodies
		//---------------------------

		// Appends InPhysicsBody to OutPhysicsBody removing Bodies that are equal, have same bone and customId and its properies are identical.	
		auto AppendPhysicsBodiesUnique = [](PhysicsBody& OutPhysicsBody, const PhysicsBody& InPhysicsBody)
		{	
			TArray<string>& OutBones = OutPhysicsBody.Bones;
			TArray<int32>& OutCustomIds = OutPhysicsBody.CustomIds;
			TArray<FPhysicsBodyAggregate>& OutBodies = OutPhysicsBody.Bodies;
			
			const TArray<string>& InBones = InPhysicsBody.Bones;
			const TArray<int32>& InCustomIds = InPhysicsBody.CustomIds;
			const TArray<FPhysicsBodyAggregate>& InBodies = InPhysicsBody.Bodies;
			
			const int32 InBodyCount = InPhysicsBody.GetBodyCount();
			const int32 OutBodyCount = OutPhysicsBody.GetBodyCount();

			for (int32 I = 0; I < InBodyCount; ++I)
			{
				int32 FoundIndex = INDEX_NONE;
				for (int32 O = 0; O < OutBodyCount; ++O)
				{
					if (InCustomIds[I] == OutCustomIds[O] && InBones[I] == OutBones[O])
					{
						FoundIndex = O;
						break;
					}
				}

				if (FoundIndex == INDEX_NONE)
				{
					OutBones.Add(InBones[I]);
					OutCustomIds.Add(InCustomIds[I]);
					OutBodies.Add(InBodies[I]);
				}
				else
				{
					for (const FSphereBody& Body : InBodies[I].Spheres)
					{
						OutBodies[FoundIndex].Spheres.AddUnique(Body);
					}

					for (const FBoxBody& Body : InBodies[I].Boxes)
					{
						OutBodies[FoundIndex].Boxes.AddUnique(Body);
					}

					for (const FSphylBody& Body : InBodies[I].Sphyls)
					{
						OutBodies[FoundIndex].Sphyls.AddUnique(Body);
					}

					for (const FTaperedCapsuleBody& Body : InBodies[I].TaperedCapsules)
					{
						OutBodies[FoundIndex].TaperedCapsules.AddUnique(Body);
					}

					for (const FConvexBody& Body : InBodies[I].Convex)
					{
						OutBodies[FoundIndex].Convex.AddUnique(Body);
					}
				}
			}
		};


		if (pFirst->GetPhysicsBody() == pSecond->GetPhysicsBody())
		{
			// Both meshes are sharing the physics body, let the result share it as well.
			pResult->SetPhysicsBody( pFirst->GetPhysicsBody() );
		}
		else
		{
			Ptr<PhysicsBody> ResultPhysicsBody = new PhysicsBody;

			if (pFirst->GetPhysicsBody())
			{
				AppendPhysicsBodiesUnique(*ResultPhysicsBody, *pFirst->GetPhysicsBody());
			}
			
			if (pSecond->GetPhysicsBody())
			{
				AppendPhysicsBodiesUnique(*ResultPhysicsBody, *pSecond->GetPhysicsBody());
			}
			bool FirstBodyModified = pFirst->GetPhysicsBody() ? pFirst->GetPhysicsBody()->bBodiesModified : false;
			bool SecondBodyModified = pSecond->GetPhysicsBody() ? pSecond->GetPhysicsBody()->bBodiesModified : false;
			ResultPhysicsBody->bBodiesModified = FirstBodyModified || SecondBodyModified;
			pResult->SetPhysicsBody( ResultPhysicsBody );
		}

		// Vertices
		//-----------------
		{
            MUTABLE_CPUPROFILER_SCOPE(Vertices);

            int firstCount = pFirst->GetVertexBuffers().GetElementCount();
			int secondCount = pSecond->GetVertexBuffers().GetElementCount();

			// TODO: when formats match, which at runtime should be always.
			bool fastPath = pFirst->GetVertexBuffers().HasSameFormat( pSecond->GetVertexBuffers() );

			fastPath = fastPath && !remapNeeded;

			MeshPtrConst pVFirst;
			MeshPtrConst pVSecond;

			if ( !fastPath )
			{
                MUTABLE_CPUPROFILER_SCOPE(SlowPath);

                // Very slow! Should we warn somehow?

				// Expand component counts in vertex channels of the format mesh
				int32 vbcount = pFirst->GetVertexBuffers().m_buffers.Num();
				pResult->GetVertexBuffers().SetBufferCount( (int)vbcount );

				for ( int32 vb = 0; vb<vbcount; ++vb )
				{
					MESH_BUFFER& result = pResult->GetVertexBuffers().m_buffers[vb];
					const MESH_BUFFER& first = pFirst->GetVertexBuffers().m_buffers[vb];

					result.m_channels = first.m_channels;
					result.m_elementSize = first.m_elementSize;

					// See if we need to enlarge the components of any of the result channels
					bool resetOffsets = false;
					for ( int32 c=0; c<result.m_channels.Num(); ++c )
					{
						int sb = -1;
						int sc = -1;
						pSecond->GetVertexBuffers().FindChannel
								(
									result.m_channels[c].m_semantic,
									result.m_channels[c].m_semanticIndex,
									&sb, &sc
								);
						if ( sb>=0 )
						{
							const MESH_BUFFER& second = pSecond->GetVertexBuffers().m_buffers[sb];

							if ( second.m_channels[sc].m_componentCount
								 >
								 result.m_channels[c].m_componentCount )
							{
								result.m_channels[c].m_componentCount =
										second.m_channels[sc].m_componentCount;
								resetOffsets = true;
							}
						}
					}

					// Reset the channel offsets if necessary
					if (resetOffsets)
					{
						int offset = 0;
						for ( int32 c=0; c<result.m_channels.Num(); ++c )
						{
                            result.m_channels[c].m_offset = (uint8_t)offset;
							offset += result.m_channels[c].m_componentCount
									*
									GetMeshFormatData(result.m_channels[c].m_format).m_size;
						}
						result.m_elementSize = offset;
					}
				}

                // See if we need to add additional buffers from the second mesh (like vertex colours)
                // This is a bit ad-hoc: we only add buffers containing all new channels
                for ( const auto& buf : pSecond->GetVertexBuffers().m_buffers )
                {
                    bool someChannel = false;
                    bool allNewChannels = true;
                    for ( const auto& chan : buf.m_channels )
                    {
                        // Not a system buffer
                        if ( chan.m_semantic!=MBS_VERTEXINDEX
                             &&
                             chan.m_semantic!=MBS_LAYOUTBLOCK
                             &&
                             chan.m_semantic!=MBS_CHART
                             )
                        {
                            someChannel = true;

                            int foundBuffer=-1;
                            int foundChannel=-1;
                            pFirst->GetVertexBuffers().FindChannel(chan.m_semantic, chan.m_semanticIndex,&foundBuffer,&foundChannel);
                            if (foundBuffer>=0)
                            {
                                // There is a meaningful channel that we don't have in the first
                                // mesh, we'll need to add the buffer.
                                allNewChannels = false;
                            }
                        }
                    }

                    if (someChannel && allNewChannels)
                    {
                        pResult->GetVertexBuffers().m_buffers.Add(buf);
                    }
                }

				// Change the format of the bone indices buffer if there are more than 256 bones.
                int pResultBoneCount =
                    pResult->GetSkeleton()  ? pResult->GetSkeleton()->GetBoneCount() : 0;
                if (pResultBoneCount > 256)
				{
					int foundBuffer = -1;
					int foundChannel = -1;
					pResult->GetVertexBuffers().FindChannel(MBS_BONEINDICES, 0, &foundBuffer, &foundChannel);

					if (foundChannel >= 0)
					{
						MESH_BUFFER& boneBuffer = pResult->GetVertexBuffers().m_buffers[foundBuffer];

						MESH_BUFFER_FORMAT format = pResultBoneCount > 65535 ? MBF_UINT32 : MBF_UINT16;
						boneBuffer.m_channels[foundChannel].m_format = format;

						// Reset offsets
						int offset = 0;
						for (int32 c = 0; c < boneBuffer.m_channels.Num(); ++c)
						{
							boneBuffer.m_channels[c].m_offset = (uint8_t)offset;
							offset += boneBuffer.m_channels[c].m_componentCount
								*
								GetMeshFormatData(boneBuffer.m_channels[c].m_format).m_size;
						}
						boneBuffer.m_elementSize = offset;
					}
				}

                // Allocate vertices
                pResult->GetVertexBuffers().SetElementCount( firstCount + secondCount );

				// Convert the source meshes to the new format
                if (pFirst->GetVertexBuffers().HasSameFormat(pResult->GetVertexBuffers()))
                {
                    pVFirst = pFirst;
                }
                else
                {
                    pVFirst = MeshFormat( pFirst, pResult.get(), false, true, false, false, false );
                }


                if (pSecond->GetVertexBuffers().HasSameFormat(pResult->GetVertexBuffers()))
                {
                    pVSecond = pSecond;
                }
                else
                {
                    pVSecond = MeshFormat( pSecond, pResult.get(), false, true, false, false, false );
                }

				check( pVFirst->GetVertexBuffers().HasSameFormat
                                ( pVSecond->GetVertexBuffers() ) );
			}
			else
			{
                MUTABLE_CPUPROFILER_SCOPE(FastPath);

                pVFirst = pFirst;
				pVSecond = pSecond;
			}


			pResult->m_VertexBuffers = pVFirst->m_VertexBuffers;
			pResult->GetVertexBuffers().SetElementCount( firstCount + secondCount );

            // first copy all the vertex data
			{
				for ( int32 vb = 0; vb<pResult->GetVertexBuffers().m_buffers.Num(); ++vb )
				{
					MESH_BUFFER& result = pResult->GetVertexBuffers().m_buffers[vb];
					const MESH_BUFFER& second = pVSecond->GetVertexBuffers().m_buffers[vb];
					if ( secondCount )
					{
						int elemSize = pResult->GetVertexBuffers().GetElementSize((int)vb);
						int firstSize = firstCount * elemSize;
						int secondSize = secondCount * elemSize;

						FMemory::Memcpy( &result.m_data[firstSize], &second.m_data[0], secondSize );
					}
				}
			}

            // TODO This could eventually be part of the mesh format: force the same skeleton.
            if (remapNeeded)
            {
                MUTABLE_CPUPROFILER_SCOPE(Remap);

                // We need to remap the bone indices of the second mesh vertices that we already copied
                // to result
				for ( int32 vb=0; vb<pResult->GetVertexBuffers().m_buffers.Num(); ++vb )
				{
					MESH_BUFFER& result = pResult->GetVertexBuffers().m_buffers[vb];

					int elemSize = pResult->GetVertexBuffers().GetElementSize((int)vb);
                    int firstSize = firstCount * elemSize;

					for ( int c=0; c<pResult->GetVertexBuffers().GetBufferChannelCount( (int)vb ); ++c )
					{
						// Get info about the destination channel
						MESH_BUFFER_SEMANTIC semantic = MBS_NONE;
						int semanticIndex = 0;
						MESH_BUFFER_FORMAT format = MBF_NONE;
						int components = 0;
						int offset = 0;
						pResult->GetVertexBuffers().GetChannel
								( (int)vb, c, &semantic, &semanticIndex, &format, &components, &offset );

                        int resultOffset = firstSize + offset;

						// Check special channels that need extra work
						if ( semantic==MBS_BONEINDICES )
						{
							// Bone indices may need remapping
                            for ( int si=0; si<pVSecond->GetVertexCount(); ++si )
							{
								switch ( format )
								{
								case MBF_INT8:
								case MBF_UINT8:
								{
                                    uint8_t* pD = reinterpret_cast<uint8_t*>
											( &result.m_data[resultOffset] );

                                    int comp=0;
                                    for ( ; comp<components; ++comp )
									{
                                        uint8_t bone = pD[comp];

                                        // be defensive
                                        if (bone<secondToFirstBones.Num())
                                        {
                                            pD[comp] = (uint8_t)secondToFirstBones[ bone ];
                                        }
                                        else
                                        {
                                            pD[comp] = 0;
                                        }
									}

									resultOffset += elemSize;
									break;
								}

								case MBF_INT16:
								case MBF_UINT16:
								{
                                    uint16* pD = reinterpret_cast<uint16*>
											( &result.m_data[resultOffset] );

                                    int comp=0;
                                    for ( ; comp<components; ++comp )
									{
                                        uint16 bone = pD[comp];

                                        // be defensive
                                        if (bone<secondToFirstBones.Num())
                                        {
                                            pD[comp] = (uint16)secondToFirstBones[ bone ];
                                        }
                                        else
                                        {
                                            pD[comp] = 0;
                                        }
                                    }

									resultOffset += elemSize;
									break;
								}

								case MBF_INT32:
								case MBF_UINT32:
								{
                                    uint32_t* pD = reinterpret_cast<uint32_t*>
											( &result.m_data[resultOffset] );

                                    for ( int comp=0; comp<components; ++comp )
									{
                                        uint32_t bone = pD[comp];

                                        // be defensive
                                        if (bone< (uint32_t)secondToFirstBones.Num())
                                        {
                                            pD[comp] = (uint32_t)secondToFirstBones[ bone ];
                                        }
                                        else
                                        {
                                            pD[comp] = 0;
                                        }
                                    }

									resultOffset += elemSize;
									break;
								}

								default:
									checkf(false, TEXT("Format not supported."));
								}
							}
						}
					}
				}
			}
		}

		// Tags
		pResult->m_tags = pFirst->m_tags;

		for (auto& secondTag : pSecond->m_tags)
		{
			bool repeated = false;

			for (auto& firstTag : pFirst->m_tags)
			{
				if (firstTag == secondTag)
				{
					repeated = true;
					break;
				}				
			}

			if (!repeated)
			{
				pResult->m_tags.Add(secondTag);
			}
		}

		return pResult;
	}


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    inline void ExtendSkeleton( Skeleton* pBase, const Skeleton* pOther )
    {
        TMap<int,int> otherToResult;

        int initialBones = pBase->GetBoneCount();
        for ( int b=0; pOther && b<pOther->GetBoneCount(); ++b)
        {
            int resultBoneIndex = pBase->FindBone( pOther->GetBoneName(b) );
            if ( resultBoneIndex<0 )
            {
                int32 newIndex = pBase->m_bones.Num();
                otherToResult.Add(b,newIndex);
                pBase->m_bones.Add( pOther->m_bones[b] );

                // Will be remapped below
                pBase->m_boneParents.Add(pOther->m_boneParents[b] );

                // Copy the external bone id
				pBase->m_boneIds.Add(pOther->m_boneIds[b]);
            }
            else
            {
                otherToResult.Add(b,resultBoneIndex);
            }
        }

        // Fix bone parent indices of the bones added from pOther
        for ( int b=initialBones;b<pBase->GetBoneCount(); ++b)
        {
            int16_t sourceIndex = pBase->m_boneParents[b];
            if (sourceIndex>=0)
            {
                pBase->m_boneParents[b] = (int16_t)otherToResult[ sourceIndex ];
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    //! Return null if there is no need to remap the mesh
    //---------------------------------------------------------------------------------------------
    inline MeshPtr MeshRemapSkeleton( const Mesh* pSource, const Skeleton* pSkeleton )
    {
        if ( pSource->GetVertexCount()==0
             ||
             !pSource->GetSkeleton()
             ||
             pSource->GetSkeleton()->GetBoneCount()==0 )
        {
            return nullptr;
        }

        auto pSourceSkeleton = pSource->GetSkeleton();

        MeshPtr pResult = pSource->Clone();
        pResult->SetSkeleton( pSkeleton );

        // Build a skeleton map
        TArray<int> sourceToSkeleton;
        for ( const auto& boneName: pSourceSkeleton->m_bones )
        {
            sourceToSkeleton.Add( pResult->GetSkeleton()->FindBone(boneName.c_str()) );
        }

        bool somethingRemapped = false;

        // Convert all bone index channels
        FMeshBufferSet& BufSet = pResult->m_VertexBuffers;
        for (auto& buf: BufSet.m_buffers)
        {
            for (auto& chan: buf.m_channels)
            {
                if (chan.m_semantic==MBS_BONEINDICES)
                {
                    uint8_t* pData = &buf.m_data[chan.m_offset];

                    switch ( chan.m_format )
                    {
                    case MBF_INT8:
                    case MBF_UINT8:
                    {
                        for (uint32_t v=0;v<BufSet.m_elementCount; ++v)
                        {
                            uint8_t* pTypedData = (uint8_t*)pData;

                            for (int c=0; c<chan.m_componentCount; ++c)
                            {
                                size_t boneIndex = size_t( pTypedData[c] );
                                //check( boneIndex < sourceToSkeleton.Num() );
                                if ( boneIndex < sourceToSkeleton.Num() )
                                {
                                    int bone = sourceToSkeleton[pTypedData[c]];
                                    if ( pTypedData[c] != uint8_t( bone ) )
                                    {
                                        pTypedData[c] = uint8_t( bone );
                                        somethingRemapped = true;
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogMutableCore, Error, TEXT("Bone index out of range.") );
                                }
                            }

                            pData += buf.m_elementSize;
                        }
                        break;
                    }

                    case MBF_INT16:
                    case MBF_UINT16:
                    {
                        for (uint32_t v=0;v<BufSet.m_elementCount; ++v)
                        {
                            uint16* pTypedData = (uint16*)pData;

                            for (int c=0; c<chan.m_componentCount; ++c)
                            {
                                size_t boneIndex = size_t(pTypedData[c]);
                                //check( boneIndex < sourceToSkeleton.size() );
                                if ( boneIndex < sourceToSkeleton.Num() )
                                {
                                    int bone = sourceToSkeleton[boneIndex];
                                    if ( pTypedData[c] != uint16(bone) )
                                    {
                                        pTypedData[c] = uint16(bone);
                                        somethingRemapped = true;
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogMutableCore, Error, TEXT("Bone index out of range.") );
                                }
                            }

                            pData += buf.m_elementSize;
                        }
                        break;
                    }

                    case MBF_INT32:
                    case MBF_UINT32: 
                    {
                        for (uint32_t v=0;v<BufSet.m_elementCount; ++v)
                        {
                            uint32_t* pTypedData = (uint32_t*)pData;

                            for (int c=0; c<chan.m_componentCount; ++c)
                            {
                                size_t boneIndex = size_t( pTypedData[c] );
                                //check( boneIndex < sourceToSkeleton.size() );
                                if ( boneIndex < sourceToSkeleton.Num() )
                                {
                                    int bone = sourceToSkeleton[pTypedData[c]];
                                    if ( pTypedData[c] != uint32_t( bone ) )
                                    {
                                        pTypedData[c] = uint32_t( bone );
                                        somethingRemapped = true;
                                    }
                                }
                                else
                                {
                                    UE_LOG(LogMutableCore, Error, TEXT("Bone index out of range.") );
                                }
                            }

                            pData += buf.m_elementSize;
                        }
                        break;
                    }

                    default:
						checkf(false, TEXT("Not implemented."));
						break;
                    }
                }
            }
        }

        if (!somethingRemapped)
        {
            pResult = nullptr;
        }

        return pResult;
    }

}
