// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/Platform.h"
#include "MuR/MutableMath.h"
#include "MuR/OpMeshFormat.h"
#include "MuR/MutableTrace.h"


namespace mu
{

	struct FMeshMergeScratchMeshes
	{
		Ptr<Mesh> FirstReformat;
		Ptr<Mesh> SecondReformat;
	};

	//---------------------------------------------------------------------------------------------
	//! Merge two meshes into one new mesh
	//---------------------------------------------------------------------------------------------
	inline void MeshMerge(Mesh* Result, const Mesh* pFirst, const Mesh* pSecond, bool bMergeSurfaces, 
		FMeshMergeScratchMeshes& ScratchMeshes)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshMerge);

		// Should never happen, but fixes static analysis warnings.
		if (!(pFirst && pSecond))
		{
			return;
		}

		// Indices
		//-----------------
		if (pFirst->GetIndexBuffers().GetBufferCount() > 0)
		{
			MUTABLE_CPUPROFILER_SCOPE(Indices);

			const int32 FirstCount = pFirst->GetIndexBuffers().GetElementCount();
			const int32 SecondCount = pSecond->GetIndexBuffers().GetElementCount();
			Result->GetIndexBuffers().SetElementCount(FirstCount + SecondCount);

			check(pFirst->GetIndexBuffers().GetBufferCount() <= 1);
			check(pSecond->GetIndexBuffers().GetBufferCount() <= 1);
			Result->GetIndexBuffers().SetBufferCount(1);

			MESH_BUFFER& ResultIndexBuffer = Result->GetIndexBuffers().m_buffers[0];

			const MESH_BUFFER& FirstIndexBuffer = pFirst->GetIndexBuffers().m_buffers[0];
			const MESH_BUFFER& SecondIndexBuffer = pSecond->GetIndexBuffers().m_buffers[0];

			// Avoid unused variable warnings
			(void)FirstIndexBuffer;
			(void)SecondIndexBuffer;

			// This will be changed below if need to change the format of the index buffers.
			MESH_BUFFER_FORMAT IndexBufferFormat = MBF_NONE;

			if (FirstCount && SecondCount)
			{
				check(!FirstIndexBuffer.m_channels.IsEmpty());
				check(FirstIndexBuffer.m_channels == SecondIndexBuffer.m_channels);
				check(FirstIndexBuffer.m_elementSize == SecondIndexBuffer.m_elementSize);

				// We need to know the total number of vertices in case we need to adjust the index buffer format.
				const uint64 totalVertexCount = pFirst->GetVertexBuffers().GetElementCount() + pSecond->GetVertexBuffers().GetElementCount();
				const uint64 maxValueBits = GetMeshFormatData(pFirst->GetIndexBuffers().m_buffers[0].m_channels[0].m_format).m_maxValueBits;
				const uint64 maxSupportedVertices = uint64(1) << maxValueBits;
				
				if (totalVertexCount > maxSupportedVertices)
				{
					IndexBufferFormat = totalVertexCount > MAX_uint16 ? MBF_UINT32 : MBF_UINT16;
				}

			}
			
			if (IndexBufferFormat != MBF_NONE)
			{
				// We only support vertex indices in case of having to change the format.
				check(FirstIndexBuffer.m_channels.Num() == 1);

				ResultIndexBuffer.m_channels.SetNum(1);
				ResultIndexBuffer.m_channels[0].m_semantic = MBS_VERTEXINDEX;
				ResultIndexBuffer.m_channels[0].m_format = IndexBufferFormat;
				ResultIndexBuffer.m_channels[0].m_componentCount = 1;
				ResultIndexBuffer.m_channels[0].m_semanticIndex = 0;
				ResultIndexBuffer.m_channels[0].m_offset = 0;
				ResultIndexBuffer.m_elementSize = GetMeshFormatData(IndexBufferFormat).m_size;
			}
			else if (FirstCount)
			{
				ResultIndexBuffer.m_channels = FirstIndexBuffer.m_channels;
				ResultIndexBuffer.m_elementSize = FirstIndexBuffer.m_elementSize;
			}
			else if (SecondCount)
			{
				ResultIndexBuffer.m_channels = SecondIndexBuffer.m_channels;
				ResultIndexBuffer.m_elementSize = SecondIndexBuffer.m_elementSize;
			}

			ResultIndexBuffer.m_data.SetNum(ResultIndexBuffer.m_elementSize * (FirstCount + SecondCount));

			check(ResultIndexBuffer.m_channels.Num() == 1);
			check(ResultIndexBuffer.m_channels[0].m_semantic == MBS_VERTEXINDEX);

			if (!ResultIndexBuffer.m_data.IsEmpty())
			{
				if (FirstCount)
				{
					if (IndexBufferFormat == MBF_NONE
						|| IndexBufferFormat == FirstIndexBuffer.m_channels[0].m_format)
					{
						FMemory::Memcpy(&ResultIndexBuffer.m_data[0],
							&FirstIndexBuffer.m_data[0],
							FirstIndexBuffer.m_elementSize * FirstCount);
					}
					else
					{
						// Conversion required
						const uint8_t* pSource = &FirstIndexBuffer.m_data[0];
						uint8_t* pDest = &ResultIndexBuffer.m_data[0];
						switch (IndexBufferFormat)
						{
						case MBF_UINT32:
						{
							switch (FirstIndexBuffer.m_channels[0].m_format)
							{
							case MBF_UINT16:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint32_t*)pDest = *(const uint16*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							case MBF_UINT8:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint32_t*)pDest = *(const uint8_t*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
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
							switch (FirstIndexBuffer.m_channels[0].m_format)
							{

							case MBF_UINT8:
							{
								for (int v = 0; v < FirstCount; ++v)
								{
									*(uint16*)pDest = *(const uint8_t*)pSource;
									pSource += FirstIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
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

				if (SecondCount)
				{
					const uint8_t* pSource = &SecondIndexBuffer.m_data[0];
					uint8_t* pDest = &ResultIndexBuffer.m_data[ResultIndexBuffer.m_elementSize * FirstCount];

					uint32_t firstVertexCount = pFirst->GetVertexBuffers().GetElementCount();

					if (IndexBufferFormat == MBF_NONE
						|| IndexBufferFormat == SecondIndexBuffer.m_channels[0].m_format)
					{
						switch (SecondIndexBuffer.m_channels[0].m_format)
						{
						case MBF_INT32:
						case MBF_UINT32:
						case MBF_NINT32:
						case MBF_NUINT32:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint32_t*)pDest = firstVertexCount + *(const uint32_t*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
							}
							break;
						}

						case MBF_INT16:
						case MBF_UINT16:
						case MBF_NINT16:
						case MBF_NUINT16:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint16*)pDest = uint16(firstVertexCount) + *(const uint16*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
							}
							break;
						}

						case MBF_INT8:
						case MBF_UINT8:
						case MBF_NINT8:
						case MBF_NUINT8:
						{
							for (int v = 0; v < SecondCount; ++v)
							{
								*(uint8_t*)pDest = uint8_t(firstVertexCount) + *(const uint8_t*)pSource;
								pSource += SecondIndexBuffer.m_elementSize;
								pDest += ResultIndexBuffer.m_elementSize;
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
						switch (IndexBufferFormat)
						{

						case MBF_UINT32:
						{
							switch (SecondIndexBuffer.m_channels[0].m_format)
							{
							case MBF_INT16:
							case MBF_UINT16:
							case MBF_NINT16:
							case MBF_NUINT16:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint16*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
								}
								break;
							}

							case MBF_INT8:
							case MBF_UINT8:
							case MBF_NINT8:
							case MBF_NUINT8:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint32_t*)pDest = uint32_t(firstVertexCount) + *(const uint8_t*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
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
							switch (SecondIndexBuffer.m_channels[0].m_format)
							{
							case MBF_INT8:
							case MBF_UINT8:
							case MBF_NINT8:
							case MBF_NUINT8:
							{
								for (int v = 0; v < SecondCount; ++v)
								{
									*(uint16*)pDest = uint16(firstVertexCount) + *(const uint8_t*)pSource;
									pSource += SecondIndexBuffer.m_elementSize;
									pDest += ResultIndexBuffer.m_elementSize;
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

			const int32 FirstCount = pFirst->GetFaceBuffers().GetElementCount();
			const int32 SecondCount = pSecond->GetFaceBuffers().GetElementCount();
			Result->GetFaceBuffers().SetElementCount(FirstCount + SecondCount);
			Result->GetFaceBuffers().SetBufferCount(pFirst->GetFaceBuffers().GetBufferCount());

			// Merge only the buffers present in the first mesh
			for (int32 b = 0; b < Result->GetFaceBuffers().GetBufferCount(); ++b)
			{
				const MESH_BUFFER& first = pFirst->GetFaceBuffers().m_buffers[b];

				MESH_BUFFER& result = Result->GetFaceBuffers().m_buffers[b];
				result.m_channels = first.m_channels;
				result.m_elementSize = first.m_elementSize;
				result.m_data.SetNum(result.m_elementSize * (FirstCount + SecondCount));

				MESH_BUFFER_SEMANTIC semantic = MBS_NONE;
				int semanticIndex = 0;
				MESH_BUFFER_FORMAT type = MBF_NONE;
				int components = 0;
				int offset = 0;
				pFirst->GetFaceBuffers().GetChannel
				(b, 0, &semantic, &semanticIndex, &type, &components, &offset);

				if (FirstCount)
				{
					FMemory::Memcpy(&result.m_data[0],
						&first.m_data[0],
						result.m_elementSize * FirstCount
					);
				}

				if (SecondCount)
				{
					// Find in the second mesh
					int otherBuffer = -1;
					int otherChannel = -1;
					pSecond->GetFaceBuffers().FindChannel
					(semantic, 0, &otherBuffer, &otherChannel);

					if (otherBuffer >= 0)
					{
						const MESH_BUFFER& second =
							pSecond->GetFaceBuffers().m_buffers[otherBuffer];
						check(first.m_channels == second.m_channels);

						// Raw copy
						FMemory::Memcpy(&result.m_data[result.m_elementSize * FirstCount],
							&second.m_data[0],
							result.m_elementSize * SecondCount);
					}
					else
					{
						// Fill with zeroes
						FMemory::Memzero(&result.m_data[result.m_elementSize * FirstCount],
							result.m_elementSize * SecondCount);
					}
				}
			}
		}


		// Layouts
		//-----------------
		{
			MUTABLE_CPUPROFILER_SCOPE(Layouts);

			Result->m_layouts.SetNum(pFirst->m_layouts.Num());
			for (int i = 0; i < pFirst->m_layouts.Num(); ++i)
			{
				const Layout* pF = pFirst->m_layouts[i].get();
				LayoutPtr pR = pF->Clone();

				if (i < pSecond->m_layouts.Num())
				{
					const Layout* pS = pSecond->m_layouts[i].get();

					pR->m_blocks.Append(pS->m_blocks);
				}

				Result->m_layouts[i] = pR;
			}
		}


		// Skeleton
		//---------------------------

		// Add SkeletonIDs
		Result->SkeletonIDs = pFirst->SkeletonIDs;

		for (const int32 SkeletonID : pSecond->SkeletonIDs)
		{
			Result->SkeletonIDs.AddUnique(SkeletonID);
		}

		// Do they have the same skeleton?
		bool bMergeSkeletons = pFirst->GetSkeleton() != pSecond->GetSkeleton();

		// Are they different skeletons but with the same data?
		if (bMergeSkeletons && pFirst->GetSkeleton() && pSecond->GetSkeleton())
		{
			bMergeSkeletons = !(*pFirst->GetSkeleton() == *pSecond->GetSkeleton());
		}


		if (bMergeSkeletons)
		{
			MUTABLE_CPUPROFILER_SCOPE(MergeSkeleton);
			Ptr<Skeleton> pResultSkeleton;

			mu::SkeletonPtrConst pFirstSkeleton = pFirst->GetSkeleton();
			mu::SkeletonPtrConst pSecondSkeleton = pSecond->GetSkeleton();

			const int32 NumBonesFirst = pFirstSkeleton ? pFirstSkeleton->GetBoneCount() : 0;
			const int32 NumBonesSecond = pSecondSkeleton ? pSecondSkeleton->GetBoneCount() : 0;

			pResultSkeleton = pFirstSkeleton ? pFirstSkeleton->Clone() : new Skeleton;
			Result->SetSkeleton(pResultSkeleton);

			TArray<uint16> SecondToResultBoneIndices;
			SecondToResultBoneIndices.SetNumUninitialized(NumBonesSecond);

			// Merge pSecond and build the remap array 
			for (int32 SecondBoneIndex = 0; SecondBoneIndex < NumBonesSecond; ++SecondBoneIndex)
			{
				const uint16 BoneNameId = pSecondSkeleton->BoneIds[SecondBoneIndex];
				int32 Index = pResultSkeleton->FindBone(BoneNameId);

				// Add a new bone
				if (Index == INDEX_NONE)
				{
					Index = pResultSkeleton->BoneIds.Add(BoneNameId);

					// Add an incorrect index, to be fixed below in case the parent index is later in the bone array.
					pResultSkeleton->BoneParents.Add(pSecondSkeleton->BoneParents[SecondBoneIndex]);

					if (pSecondSkeleton->BoneNames.IsValidIndex(SecondBoneIndex))
					{
						pResultSkeleton->BoneNames.Add(pSecondSkeleton->BoneNames[SecondBoneIndex]);
					}
				}

				SecondToResultBoneIndices[SecondBoneIndex] = (uint16)Index;
			}

			// Fix second mesh bone parents
			for (int32 ob = NumBonesFirst; ob < pResultSkeleton->BoneParents.Num(); ++ob)
			{
				int16 secondMeshIndex = pResultSkeleton->BoneParents[ob];
				if (secondMeshIndex != INDEX_NONE)
				{
					pResultSkeleton->BoneParents[ob] = SecondToResultBoneIndices[secondMeshIndex];
				}
			}
		}
		else
		{
			Result->SetSkeleton(pFirst->GetSkeleton());
		}


		// Surfaces
		//---------------------------
		
		// Remap bone indices if we merge surfaces since bonemaps will be merged too.
		bool bRemapBoneIndices = false;
		TArray<uint16> RemappedBoneMapIndices;

		// Used to know the format of the bone index buffer
		uint32 MaxNumBonesInBoneMaps = 0;
		const int32 NumSecondBonesInBoneMap = pSecond->BoneMap.Num();

		{
			MUTABLE_CPUPROFILER_SCOPE(Surfaces);
			
			const int32 NumFirstBonesInBoneMap = pFirst->BoneMap.Num();
			Result->BoneMap = pFirst->BoneMap;

			if (bMergeSurfaces)
			{
				// Merge BoneMaps
				RemappedBoneMapIndices.SetNumUninitialized(NumSecondBonesInBoneMap);

				for (uint16 SecondBoneMapIndex = 0; SecondBoneMapIndex < NumSecondBonesInBoneMap; ++SecondBoneMapIndex)
				{
					const int32 BoneMapIndex = Result->BoneMap.AddUnique(pSecond->BoneMap[SecondBoneMapIndex]);
					RemappedBoneMapIndices[SecondBoneMapIndex] = BoneMapIndex;

					bRemapBoneIndices = bRemapBoneIndices || BoneMapIndex != SecondBoneMapIndex;
				}

				MESH_SURFACE& NewSurface = Result->m_surfaces.AddDefaulted_GetRef();
				NewSurface.m_vertexCount = pFirst->GetVertexCount() + pSecond->GetVertexCount();
				NewSurface.m_indexCount = pFirst->GetIndexCount() + pSecond->GetIndexCount();
				NewSurface.BoneMapCount = Result->BoneMap.Num();
				
				//All merged surfaces will have the same bCastShadow value. Decided by the first merged mesh
				NewSurface.bCastShadow = pFirst->m_surfaces.Last().bCastShadow;
			}
			else
			{
				// Add the BoneMap of the second mesh
				Result->BoneMap.Append(pSecond->BoneMap);

				// Add pFirst surfaces
				Result->m_surfaces = pFirst->m_surfaces;

				const int32 FirstVertexIndex = pFirst->GetVertexCount();
				const int32 FirstIndexIndex = pFirst->GetIndexCount();

				check(pSecond->m_surfaces.Num() == 1);
				MESH_SURFACE& NewSurface = Result->m_surfaces.Add_GetRef(pSecond->m_surfaces[0]);
				NewSurface.m_firstVertex += FirstVertexIndex;
				NewSurface.m_firstIndex += FirstIndexIndex;
				NewSurface.BoneMapIndex += NumFirstBonesInBoneMap;
			}

			for (const MESH_SURFACE& Surface : Result->m_surfaces)
			{
				MaxNumBonesInBoneMaps = FMath::Max(MaxNumBonesInBoneMaps, Surface.BoneMapCount);
			}

			Result->BoneMap.Shrink();
		}


		// Pose
		//---------------------------
		{
			MUTABLE_CPUPROFILER_SCOPE(Pose);

			Result->BonePoses.Reserve(Result->GetSkeleton()->GetBoneCount());

			// Copy poses from the first mesh
			Result->BonePoses = pFirst->BonePoses;

			// Add or override bone poses
			for (const Mesh::FBonePose& SecondBonePose : pSecond->BonePoses)
			{
				const int32 ResultBoneIndex = Result->FindBonePose(SecondBonePose.BoneId);

				if (ResultBoneIndex != INDEX_NONE)
				{
					Mesh::FBonePose& ResultBonePose = Result->BonePoses[ResultBoneIndex];

					// TODO: Not sure how to tune this priority, review it.
					// For now use a similar strategy as before. 
					auto ComputeBoneMergePriority = [](const Mesh::FBonePose& BonePose)
					{
						return (EnumHasAnyFlags(BonePose.BoneUsageFlags, EBoneUsageFlags::Skinning) ? 1 : 0) +
							(EnumHasAnyFlags(BonePose.BoneUsageFlags, EBoneUsageFlags::Reshaped) ? 1 : 0);
					};

					if (ComputeBoneMergePriority(ResultBonePose) < ComputeBoneMergePriority(SecondBonePose))
					{
						//ResultBonePose.BoneName = SecondBonePose.BoneName;
						ResultBonePose.BoneTransform = SecondBonePose.BoneTransform;
						// Merge usage flags
						EnumAddFlags(ResultBonePose.BoneUsageFlags, SecondBonePose.BoneUsageFlags);
					}
				}
				else
				{
					Result->BonePoses.Add(SecondBonePose);
				}
			}

			Result->BonePoses.Shrink();
		}


		// PhysicsBodies
		//---------------------------
		{
			MUTABLE_CPUPROFILER_SCOPE(PhysicsBodies);

			// Appends InPhysicsBody to OutPhysicsBody removing Bodies that are equal, have same bone and customId and its properies are identical.	
			auto AppendPhysicsBodiesUnique = [](PhysicsBody& OutPhysicsBody, const PhysicsBody& InPhysicsBody) -> bool
			{
				TArray<uint16>& OutBones = OutPhysicsBody.BoneIds;
				TArray<int32>& OutCustomIds = OutPhysicsBody.BodiesCustomIds;
				TArray<FPhysicsBodyAggregate>& OutBodies = OutPhysicsBody.Bodies;

				const TArray<uint16>& InBones = InPhysicsBody.BoneIds;
				const TArray<int32>& InCustomIds = InPhysicsBody.BodiesCustomIds;
				const TArray<FPhysicsBodyAggregate>& InBodies = InPhysicsBody.Bodies;

				const int32 InBodyCount = InPhysicsBody.GetBodyCount();
				const int32 OutBodyCount = OutPhysicsBody.GetBodyCount();

				bool bModified = false;

				for (int32 InBodyIndex = 0; InBodyIndex < InBodyCount; ++InBodyIndex)
				{
					int32 FoundIndex = INDEX_NONE;
					for (int32 OutBodyIndex = 0; OutBodyIndex < OutBodyCount; ++OutBodyIndex)
					{
						if (InCustomIds[InBodyIndex] == OutCustomIds[OutBodyIndex] && InBones[InBodyIndex] == OutBones[OutBodyIndex])
						{
							FoundIndex = OutBodyIndex;
							break;
						}
					}

					if (FoundIndex == INDEX_NONE)
					{
						OutBones.Add(InBones[InBodyIndex]);
						OutCustomIds.Add(InCustomIds[InBodyIndex]);
						OutBodies.Add(InBodies[InBodyIndex]);

						bModified |= true;

						continue;
					}

					for (const FSphereBody& Body : InBodies[InBodyIndex].Spheres)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Spheres.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Spheres.AddUnique(Body);
					}

					for (const FBoxBody& Body : InBodies[InBodyIndex].Boxes)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Boxes.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Boxes.AddUnique(Body);
					}

					for (const FSphylBody& Body : InBodies[InBodyIndex].Sphyls)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Sphyls.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Sphyls.AddUnique(Body);
					}

					for (const FTaperedCapsuleBody& Body : InBodies[InBodyIndex].TaperedCapsules)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].TaperedCapsules.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].TaperedCapsules.AddUnique(Body);
					}

					for (const FConvexBody& Body : InBodies[InBodyIndex].Convex)
					{
						const int32 NumBeforeAddition = OutBodies[FoundIndex].Convex.Num();
						bModified |= NumBeforeAddition == OutBodies[FoundIndex].Convex.AddUnique(Body);
					}
				}

				return bModified;
			};

			TTuple<const PhysicsBody*, bool> SharedResultPhysicsBody = Invoke([&]()
				-> TTuple<const PhysicsBody*, bool>
			{
				if (pFirst->GetPhysicsBody() == pSecond->GetPhysicsBody())
				{
					return MakeTuple(pFirst->GetPhysicsBody().get(), true);
				}

				if (pFirst->GetPhysicsBody() && !pSecond->GetPhysicsBody())
				{
					return MakeTuple(pFirst->GetPhysicsBody().get(), true);
				}

				if (!pFirst->GetPhysicsBody() && pSecond->GetPhysicsBody())
				{
					return MakeTuple(pSecond->GetPhysicsBody().get(), true);
				}

				return MakeTuple(nullptr, false);
			});

			if (SharedResultPhysicsBody.Get<1>())
			{
				// Only one or non of the meshes has physics, share the result.
				Result->SetPhysicsBody(SharedResultPhysicsBody.Get<0>());
			}
			else
			{
				check(pFirst->GetPhysicsBody() && pSecond->GetPhysicsBody());

				Ptr<PhysicsBody> MergedResultPhysicsBody = pFirst->GetPhysicsBody()->Clone();

				MergedResultPhysicsBody->bBodiesModified =
					AppendPhysicsBodiesUnique(*MergedResultPhysicsBody, *pSecond->GetPhysicsBody()) ||
					pFirst->GetPhysicsBody()->bBodiesModified || pSecond->GetPhysicsBody()->bBodiesModified;

				Result->SetPhysicsBody(MergedResultPhysicsBody);
			}

			// Additional physics bodies.
			const int32 MaxAdditionalPhysicsResultNum = pFirst->AdditionalPhysicsBodies.Num() + pSecond->AdditionalPhysicsBodies.Num();

			Result->AdditionalPhysicsBodies.Reserve(MaxAdditionalPhysicsResultNum);
			Result->AdditionalPhysicsBodies.Append(pFirst->AdditionalPhysicsBodies);
			
			// Not very many additional bodies expected, do a quadratic search to have unique bodies based on external id.
			const int32 NumSecondAdditionalBodies = pSecond->AdditionalPhysicsBodies.Num();
			for (int32 Index = 0; Index < NumSecondAdditionalBodies; ++Index)
			{
				const int32 CustomIdToMerge = pSecond->AdditionalPhysicsBodies[Index]->CustomId;

				const mu::Ptr<const PhysicsBody>* Found = pFirst->AdditionalPhysicsBodies.FindByPredicate(
					[CustomIdToMerge](const Ptr<const mu::PhysicsBody>& Body) { return CustomIdToMerge == Body->CustomId; });

				// TODO: current usages do not expect collisions, but same Id collision with bodies modified in differnet ways
				// may need to be contemplated at some point.
				if (!Found)
				{
					Result->AdditionalPhysicsBodies.Add(pSecond->AdditionalPhysicsBodies[Index]);
				}
			}
		}

		// Vertices
		//-----------------
		{
            MUTABLE_CPUPROFILER_SCOPE(Vertices);

            const int32 FirstCount = pFirst->GetVertexBuffers().GetElementCount();
			const int32 SecondCount = pSecond->GetVertexBuffers().GetElementCount();

			// TODO: when formats match, which at runtime should be always.
			bool bFastPath = pFirst->GetVertexBuffers().HasSameFormat( pSecond->GetVertexBuffers() );

			// Check if the format of the BoneIndex buffer has to change
			bool bChangeBoneIndicesFormat = false;
			MESH_BUFFER_FORMAT BoneIndexFormat = MaxNumBonesInBoneMaps > MAX_uint8 ? MBF_UINT16 : MBF_UINT8;

			// Iterate all vertex buffers to check if we need to format bone indices
			{
				{
					// Check if pFirst requires a reformat of the bone index buffers
					const FMeshBufferSet& VertexBuffers = pFirst->GetVertexBuffers();
					for (int32 VertexBufferIndex = 0; !bChangeBoneIndicesFormat && VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
					{
						const MESH_BUFFER& Buffer = VertexBuffers.m_buffers[VertexBufferIndex];

						const int32 elemSize = VertexBuffers.GetElementSize(VertexBufferIndex);
						const int32 firstSize = FirstCount * elemSize;

						const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
						for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
						{
							if (Buffer.m_channels[ChannelIndex].m_semantic == MBS_BONEINDICES)
							{
								bChangeBoneIndicesFormat = Buffer.m_channels[ChannelIndex].m_format != BoneIndexFormat;
								break;
							}
						}
					}
				}

				{
					// Check if pSecond requires a reformat of the bone index buffers
					const FMeshBufferSet& VertexBuffers = pSecond->GetVertexBuffers();
					for (int32 VertexBufferIndex = 0; !bChangeBoneIndicesFormat && VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
					{
						const MESH_BUFFER& Buffer = VertexBuffers.m_buffers[VertexBufferIndex];

						const int32 elemSize = VertexBuffers.GetElementSize(VertexBufferIndex);
						const int32 firstSize = FirstCount * elemSize;

						const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
						for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
						{
							if (Buffer.m_channels[ChannelIndex].m_semantic == MBS_BONEINDICES)
							{
								bChangeBoneIndicesFormat = Buffer.m_channels[ChannelIndex].m_format != BoneIndexFormat;
								break;
							}
						}
					}
				}

				bFastPath = bFastPath && !bChangeBoneIndicesFormat;
			}

			Ptr<const Mesh> pVFirst;
			Ptr<const Mesh> pVSecond;
			
			if (!bFastPath)
			{
                MUTABLE_CPUPROFILER_SCOPE(SlowPath);

                // Very slow! Should we warn somehow?

				// Expand component counts in vertex channels of the format mesh
				int32 vbcount = pFirst->GetVertexBuffers().m_buffers.Num();
				Result->GetVertexBuffers().SetBufferCount( (int)vbcount );

				for ( int32 vb = 0; vb<vbcount; ++vb )
				{
					MESH_BUFFER& result = Result->GetVertexBuffers().m_buffers[vb];
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

                // See if we need to add additional buffers from the second mesh (like vertex colours or additional UV Channels)
                // This is a bit ad-hoc: we only add buffers containing all new channels
                for ( const MESH_BUFFER& buf : pSecond->GetVertexBuffers().m_buffers )
                {
                    bool someChannel = false;
                    bool allNewChannels = true;
					for (const MESH_BUFFER_CHANNEL& chan : buf.m_channels)
					{
						// Skip system buffers
						if (chan.m_semantic == MBS_VERTEXINDEX
							||
							chan.m_semantic == MBS_LAYOUTBLOCK)
						{
							continue;
						}

						someChannel = true;

						int foundBuffer = -1;
						int foundChannel = -1;
						pFirst->GetVertexBuffers().FindChannel(chan.m_semantic, chan.m_semanticIndex, &foundBuffer, &foundChannel);
						if (foundBuffer >= 0)
						{
							// There's at least one channel that already exists in the first mesh. Don't add the buffer.
							allNewChannels = false;
							continue;
						}
						
						// If there are additional UV channels try to add them
						if(!allNewChannels && chan.m_semantic == MBS_TEXCOORDS
							&&
							chan.m_semanticIndex > 0)
						{
							// Add additional UV channels if the previous one is found.
							FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();
							VertexBuffers.FindChannel(MBS_TEXCOORDS, chan.m_semanticIndex - 1, &foundBuffer, &foundChannel);

							if (foundBuffer >= 0)
							{
								MESH_BUFFER& Buffer = VertexBuffers.m_buffers[foundBuffer];
								Buffer.m_channels.Insert(chan, foundChannel + 1);

								// Update offsets
								int32 Offset = Buffer.m_channels[foundChannel].m_offset;
								for (int32 c = foundChannel; c < Buffer.m_channels.Num(); ++c)
								{
									Buffer.m_channels[c].m_offset = (uint8)Offset;
									Offset += Buffer.m_channels[c].m_componentCount
										*
										GetMeshFormatData(Buffer.m_channels[c].m_format).m_size;
								}
								Buffer.m_elementSize = Offset;
							}
						}
					}

                    if (someChannel && allNewChannels)
                    {
                        Result->GetVertexBuffers().m_buffers.Add(buf);
                    }
                }

				// Change the format of the bone indices buffer
				if(bChangeBoneIndicesFormat)
				{
					// Iterate all vertex buffers and update the format
					FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();
					for (int32 VertexBufferIndex = 0; VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
					{
						MESH_BUFFER& result = VertexBuffers.m_buffers[VertexBufferIndex];

						const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
						for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
						{
							if (result.m_channels[ChannelIndex].m_semantic == MBS_BONEINDICES)
							{
								result.m_channels[ChannelIndex].m_format = BoneIndexFormat;

								// Reset offsets
								int32 offset = 0;
								for (int32 AuxChannelIndex = 0; AuxChannelIndex < ChannelsCount; ++AuxChannelIndex)
								{
									result.m_channels[AuxChannelIndex].m_offset = (uint8_t)offset;
									offset += result.m_channels[AuxChannelIndex].m_componentCount
										*
										GetMeshFormatData(result.m_channels[AuxChannelIndex].m_format).m_size;
								}
								result.m_elementSize = offset;
							}
						}
					}
				}

                // Allocate vertices
                Result->GetVertexBuffers().SetElementCount( FirstCount + SecondCount );
				
				// Convert the source meshes to the new format
                if (pFirst->GetVertexBuffers().HasSameFormat(Result->GetVertexBuffers()))
                {
                    pVFirst = pFirst;
                }
                else
                {
					check(ScratchMeshes.FirstReformat);

					bool bOutSuccess = false;
					MeshFormat(ScratchMeshes.FirstReformat.get(), pFirst, Result, false, true, false, false, false, bOutSuccess);
					
					if (bOutSuccess)
					{
						pVFirst = ScratchMeshes.FirstReformat;
					}
                }


                if (pSecond->GetVertexBuffers().HasSameFormat(Result->GetVertexBuffers()))
                {
                    pVSecond = pSecond;
                }
                else
                {
					check(ScratchMeshes.SecondReformat);

					bool bOutSuccess = false;
                    MeshFormat(ScratchMeshes.SecondReformat.get(), pSecond, Result, false, true, false, false, false, bOutSuccess);

					if (bOutSuccess)
					{
						pVSecond = ScratchMeshes.SecondReformat;
					}
                }

				check(pVFirst->GetVertexBuffers().HasSameFormat(pVSecond->GetVertexBuffers()));
			}
			else
			{
                MUTABLE_CPUPROFILER_SCOPE(FastPath);

                pVFirst = pFirst;
				pVSecond = pSecond;
			}

			Result->m_VertexBuffers = pVFirst->m_VertexBuffers;
			Result->GetVertexBuffers().SetElementCount( FirstCount + SecondCount );

            // first copy all the vertex data
			{
				MUTABLE_CPUPROFILER_SCOPE(CopyVertexData);
				for (int32 vb = 0; vb < Result->GetVertexBuffers().m_buffers.Num(); ++vb)
				{
					MESH_BUFFER& result = Result->GetVertexBuffers().m_buffers[vb];
					const MESH_BUFFER& second = pVSecond->GetVertexBuffers().m_buffers[vb];

					if (SecondCount)
					{
						int elemSize = Result->GetVertexBuffers().GetElementSize((int)vb);
						int firstSize = FirstCount * elemSize;
						int secondSize = SecondCount * elemSize;

						FMemory::Memcpy( &result.m_data[firstSize], &second.m_data[0], secondSize );
					}
				}
			}


            if (bRemapBoneIndices)
            {
                MUTABLE_CPUPROFILER_SCOPE(Remap);

				// We need to remap the bone indices of the second mesh vertices that we already copied
				// to result
				check(!RemappedBoneMapIndices.IsEmpty())

				// Iterate all vertex buffers and update the format
				FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();
				for (int32 VertexBufferIndex = 0; VertexBufferIndex < VertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
				{
					MESH_BUFFER& ResultBuffer = VertexBuffers.m_buffers[VertexBufferIndex];

					const int32 ElemSize = VertexBuffers.GetElementSize(VertexBufferIndex);
					const int32 FirstSize = FirstCount * ElemSize;

					const int32 ChannelsCount = VertexBuffers.GetBufferChannelCount(VertexBufferIndex);
					for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
					{
						if (ResultBuffer.m_channels[ChannelIndex].m_semantic != MBS_BONEINDICES)
						{
							continue;
						}
						
						int32 ResultOffset = FirstSize + ResultBuffer.m_channels[ChannelIndex].m_offset;

						const int32 NumComponents = ResultBuffer.m_channels[ChannelIndex].m_componentCount;

						// Bone indices may need remapping
						for (int32 VertexIndex = 0; VertexIndex < pVSecond->GetVertexCount(); ++VertexIndex)
						{
							switch (BoneIndexFormat)
							{
							case MBF_INT8:
							case MBF_UINT8:
							{
								uint8* pD = reinterpret_cast<uint8*>
									(&ResultBuffer.m_data[ResultOffset]);

								for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
								{
									uint8 BoneMapIndex = pD[ComponentIndex];

									// be defensive
									if (BoneMapIndex < NumSecondBonesInBoneMap)
									{
										pD[ComponentIndex] = (uint8)RemappedBoneMapIndices[BoneMapIndex];
									}
									else
									{
										pD[ComponentIndex] = 0;
									}
								}

								ResultOffset += ElemSize;
								break;
							}

							case MBF_INT16:
							case MBF_UINT16:
							{
								uint16* pD = reinterpret_cast<uint16*>
									(&ResultBuffer.m_data[ResultOffset]);

								for (int32 ComponentIndex = 0; ComponentIndex < NumComponents; ++ComponentIndex)
								{
									uint16 BoneMapIndex = pD[ComponentIndex];

									// be defensive
									if (BoneMapIndex < NumSecondBonesInBoneMap)
									{
										pD[ComponentIndex] = (uint16)RemappedBoneMapIndices[BoneMapIndex];
									}
									else
									{
										pD[ComponentIndex] = 0;
									}
								}

								ResultOffset += ElemSize;
								break;
							}

							case MBF_INT32:
							case MBF_UINT32:
							{
								// Unreal does not support 32 bit bone indices
								checkf(false, TEXT("Format not supported."));
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

		// Tags
		Result->m_tags = pFirst->m_tags;

		for (const FString& SecondTag : pSecond->m_tags)
		{
			Result->m_tags.AddUnique(SecondTag);
		}


		// Streamed Resources
		Result->StreamedResources = pFirst->StreamedResources;

		const int32 NumStreamedResources = pSecond->StreamedResources.Num();
		for (int32 Index = 0; Index < NumStreamedResources; ++Index)
		{
			Result->StreamedResources.AddUnique(pSecond->StreamedResources[Index]);
		}
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
            int32 resultBoneIndex = pBase->FindBone( pOther->GetBoneId(b) );
            if ( resultBoneIndex<0 )
            {
                int32 newIndex = pBase->BoneIds.Num();
                otherToResult.Add(b,newIndex);
                pBase->BoneIds.Add( pOther->BoneIds[b] );

                // Will be remapped below
                pBase->BoneParents.Add(pOther->BoneParents[b] );

				if (pOther->BoneNames.IsValidIndex(b))
				{
					pBase->BoneNames.Add(pOther->BoneNames[b]);
				}
            }
            else
            {
                otherToResult.Add(b,resultBoneIndex);
            }
        }

        // Fix bone parent indices of the bones added from pOther
        for ( int b=initialBones;b<pBase->GetBoneCount(); ++b)
        {
            int16_t sourceIndex = pBase->BoneParents[b];
            if (sourceIndex>=0)
            {
                pBase->BoneParents[b] = (int16_t)otherToResult[ sourceIndex ];
            }
        }
    }


    //---------------------------------------------------------------------------------------------
    //! Return null if there is no need to remap the mesh
    //---------------------------------------------------------------------------------------------
    inline void MeshRemapSkeleton(Mesh* Result, const Mesh* SourceMesh, const Skeleton* Skeleton, bool& bOutSuccess)
    {
		bOutSuccess = true;

        if (SourceMesh->GetVertexCount() == 0 || !SourceMesh->GetSkeleton() || SourceMesh->GetSkeleton()->GetBoneCount() == 0)
        {
			bOutSuccess = false;
            return;
        }

  //      mu::SkeletonPtrConst SourceSkeleton = SourceMesh->GetSkeleton();
		//const TArray<uint16>& SourceBoneMaps = SourceMesh->GetBoneMap();

  //      // Remap the indices of the bonemap to those of the new skeleton
		//TArray<uint16> RemappedBoneMap;

		//const int32 NumBonesBoneMap = SourceBoneMaps.Num();
		//RemappedBoneMap.Reserve(NumBonesBoneMap);

		//bool bBonesRemapped = false;

		//for (const uint16& SourceBoneIndex : SourceBoneMaps)
		//{
		//	const uint16 BoneIndex = Skeleton->FindBone(SourceSkeleton->GetBoneId(SourceBoneIndex));

		//	bBonesRemapped = bBonesRemapped || BoneIndex != SourceBoneIndex;
		//	RemappedBoneMap.Add(BoneIndex);
		//}

  //      if (!bBonesRemapped)
  //      {
		//	bOutSuccess = false;
  //          return;
  //      }

		Result->CopyFrom(*SourceMesh);
		Result->SetSkeleton(Skeleton);
		//Result->SetBoneMap(RemappedBoneMap);
    }
	
}
