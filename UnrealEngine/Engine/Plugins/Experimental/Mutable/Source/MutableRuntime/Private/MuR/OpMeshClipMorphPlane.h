// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Platform.h"


namespace mu
{
	inline bool PointInBoundingBox(const FVector3f& point, const FShape& selectionShape)
	{
		FVector3f v = point - selectionShape.position;

		for (int i = 0; i < 3; ++i)
		{
			if (fabs(v[i]) > selectionShape.size[i])
			{
				return false;
			}
		}

		return true;
	}

	inline bool VertexIsInMaxRadius(const FVector3f& vertex, const FVector3f& origin, float vertexSelectionBoneMaxRadius)
	{
		if (vertexSelectionBoneMaxRadius < 0.f)
		{
			return true;
		}

		FVector3f radiusVec = vertex - origin;
		float radius = FVector3f::DotProduct(radiusVec, radiusVec);

		return radius < vertexSelectionBoneMaxRadius* vertexSelectionBoneMaxRadius;
	}

	struct vertex_bone_info
	{
		TArray<int32> bone_indices;
		TArray<int32> bone_weights;
	};

	inline bool VertexIsAffectedByBone(int32 vertex_idx, const TArray<bool>& bone_is_affected, const TArray<vertex_bone_info>& vertex_info)
	{
		if (vertex_idx >= (int)vertex_info.Num())
		{
			return false;
		}

		check(vertex_info[vertex_idx].bone_indices.Num() == vertex_info[vertex_idx].bone_weights.Num());		

        for (size_t i = 0; i < vertex_info[vertex_idx].bone_indices.Num(); ++i)
		{			
            check(vertex_info[vertex_idx].bone_indices[i] <= (int)bone_is_affected.Num());

			if (bone_is_affected[vertex_info[vertex_idx].bone_indices[i]] && vertex_info[vertex_idx].bone_weights[i] > 0)
			{
				return true;
			}
		}

		return false;
	}

	//---------------------------------------------------------------------------------------------
	//! Reference version
	//---------------------------------------------------------------------------------------------
	inline void MeshClipMorphPlane(Mesh* Result, const Mesh* pBase, const FVector3f& origin, const FVector3f& normal, float dist, float factor, float radius,
		float radius2, float angle, const FShape& selectionShape, bool& bOutSuccess, const int32 BoneId = INDEX_NONE, float vertexSelectionBoneMaxRadius = -1.f)
	{
		bOutSuccess = true;
		//float radius = 8.f;
		//float radius2 = 4.f;
		//float factor = 1.f;

		// Generate vector perpendicular to normal for ellipse rotation reference base
		FVector3f aux_base(0.f, 1.f, 0.f);

		if (FMath::Abs(FVector3f::DotProduct(normal, aux_base)) > 0.95f)		// fabs = absolute value
		{
			aux_base = FVector3f(0.f, 0.f, 1.f);
		}
		
		FVector3f origin_radius_vector = FVector3f::CrossProduct(normal, aux_base);		// PERPENDICULAR VECTOR TO THE PLANE normal and aux base
		check(FMath::Abs(FVector3f::DotProduct(normal, origin_radius_vector)) < 0.05f);

        uint32 vcount = pBase->GetVertexBuffers().GetElementCount();

		if (!vcount)
		{
			bOutSuccess = false;
			return;
		}

		TArray<vertex_bone_info> vertex_info;

        Ptr<const Skeleton> BaseSkeleton = pBase->GetSkeleton();

		TArray<bool> AffectedBoneMapIndices;
		const int32 BaseBoneIndex = BaseSkeleton ? BaseSkeleton->FindBone(BoneId) : INDEX_NONE;
        if (BaseBoneIndex != INDEX_NONE)
		{
			const TArray<uint16>& BoneMap = pBase->BoneMap;
			AffectedBoneMapIndices.SetNum(BoneMap.Num());

			const int32 BoneCount = BaseSkeleton->GetBoneCount();
			TArray<bool> AffectedSkeletonBones;
			AffectedSkeletonBones.SetNum(BoneCount);

            for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
			{
				const int32 ParentBoneIndex = BaseSkeleton->GetBoneParent(BoneIndex);
				check(ParentBoneIndex < BoneIndex);
				
				const bool bIsBoneAffected = (AffectedSkeletonBones.IsValidIndex(ParentBoneIndex) && AffectedSkeletonBones[ParentBoneIndex])
					|| BoneIndex == BaseBoneIndex;
				
				if (!bIsBoneAffected)
				{
					continue;
				}
				
				AffectedSkeletonBones[BoneIndex] = true;

				const uint16 AffectedBoneId = BaseSkeleton->GetBoneId(BoneIndex);
				const int32 AffectedBoneMapIndex = BoneMap.Find(AffectedBoneId);
				if (AffectedBoneMapIndex != INDEX_NONE)
				{
					AffectedBoneMapIndices[AffectedBoneMapIndex] = true;
				}
			}

			// Look for affected vertex indices
			vertex_info.SetNum(vcount);
			//int firstCount = pBase->GetVertexBuffers().GetElementCount();

			for (int32 vb = 0; vb < pBase->GetVertexBuffers().m_buffers.Num(); ++vb)
			{
				const MESH_BUFFER& result = pBase->GetVertexBuffers().m_buffers[vb];

				int elemSize = pBase->GetVertexBuffers().GetElementSize((int)vb);
				//int firstSize = firstCount * elemSize;

				for (int c = 0; c < pBase->GetVertexBuffers().GetBufferChannelCount((int)vb); ++c)
				{
					// Get info about the destination channel
					MESH_BUFFER_SEMANTIC semantic = MBS_NONE;
					int semanticIndex = 0;
					MESH_BUFFER_FORMAT format = MBF_NONE;
					int components = 0;
					int offset = 0;
					pBase->GetVertexBuffers().GetChannel((int)vb, c, &semantic, &semanticIndex, &format, &components, &offset);

					int secondOffset = offset;
					//int resultOffset = firstSize + offset;

					if (semantic == MBS_BONEINDICES)
					{
						for (int i = 0; i < pBase->GetVertexCount(); ++i)
						{
							switch (format)
							{
							case MBF_INT8:
							case MBF_UINT8:
							{
                                const uint8_t* pD = &result.m_data[secondOffset];
								
								for (int j = 0; j < components; ++j)
								{
									vertex_info[i].bone_indices.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

							case MBF_INT16:
							case MBF_UINT16:
							{
								const uint16* pD = reinterpret_cast<const uint16*>(&result.m_data[secondOffset]);

								for (int j = 0; j < components; ++j)
								{
									vertex_info[i].bone_indices.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

							case MBF_INT32:
							case MBF_UINT32:
							{
								const uint32* pD = reinterpret_cast<const uint32*>(&result.m_data[secondOffset]);

								for (int j = 0; j < components; ++j)
								{
									vertex_info[i].bone_indices.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

							default:
								checkf(false, TEXT("Bone index format not supported.") );
							}
						}
					}
					else if (semantic == MBS_BONEWEIGHTS)
					{
						for (int i = 0; i < pBase->GetVertexCount(); ++i)
						{
							switch (format)
							{
							case MBF_INT8:
							case MBF_UINT8:
							case MBF_NUINT8:
							{
								const uint8_t* pD = &result.m_data[secondOffset];

								for (int j = 0; j < components; ++j)
								{
                                    vertex_info[i].bone_weights.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

							case MBF_INT16:
							case MBF_UINT16:
							case MBF_NUINT16:
							{
								const uint16* pD = reinterpret_cast<const uint16*>(&result.m_data[secondOffset]);

								for (int j = 0; j < components; ++j)
								{
									vertex_info[i].bone_weights.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

							case MBF_INT32:
							case MBF_UINT32:
							case MBF_NUINT32:
							{
								const uint32* pD = reinterpret_cast<const uint32*>(&result.m_data[secondOffset]);

								for (int j = 0; j < components; ++j)
								{
									vertex_info[i].bone_weights.Add(pD[j]);
								}

								secondOffset += elemSize;
								break;
							}

                            case MBF_FLOAT32:
                            {
								const float* pD = reinterpret_cast<const float*>(&result.m_data[secondOffset]);

                                for (int j = 0; j < components; ++j)
                                {
                                    vertex_info[i].bone_weights.Add(pD[j]>0.0f);
                                }

                                secondOffset += elemSize;
                                break;
                            }

							default:
								checkf(false, TEXT("Bone weight format not supported.") );
							}
						}
					}
				}
			}
		}

		Result->CopyFrom(*pBase);

		// TODO: Replace with an array of bools?
        TArray<bool> RemovedVertices;
		RemovedVertices.Init(false, Result->GetVertexCount());

		const FMeshBufferSet& MBSPriv = Result->GetVertexBuffers();
		for (int32 b = 0; b < MBSPriv.m_buffers.Num(); ++b)
		{
			for (int32 c = 0; c < MBSPriv.m_buffers[b].m_channels.Num(); ++c)
			{
				MESH_BUFFER_SEMANTIC sem = MBSPriv.m_buffers[b].m_channels[c].m_semantic;
				int semIndex = MBSPriv.m_buffers[b].m_channels[c].m_semanticIndex;

				UntypedMeshBufferIterator it(Result->GetVertexBuffers(), sem, semIndex);

				switch (sem)
				{
				case MBS_POSITION:
                    for (uint32 v = 0; v < vcount; ++v)
					{
						FVector3f vertex(0.0f, 0.0f, 0.0f);
						for (int i = 0; i < 3; ++i)
						{
							ConvertData(i, &vertex[0], MBF_FLOAT32, it.ptr(), it.GetFormat());
						}

						const bool bIsVertexAffectedBone = 
								BaseBoneIndex != INDEX_NONE &&
								VertexIsInMaxRadius(vertex, origin, vertexSelectionBoneMaxRadius) &&
								VertexIsAffectedByBone(v, AffectedBoneMapIndices, vertex_info);
						
						const bool bIsVertexAffectedNoShape = 
								BaseBoneIndex == INDEX_NONE && 
								selectionShape.type == (uint8_t)FShape::Type::None;

						const bool bIsVertexAffectedBoundingBox = 
								(selectionShape.type == (uint8_t)FShape::Type::AABox && 
								PointInBoundingBox(vertex, selectionShape));

						if (bIsVertexAffectedBone || bIsVertexAffectedNoShape || bIsVertexAffectedBoundingBox)
						{
							FVector3f morph_plane_center = origin;					// MORPH PLANE POS relative to root of the selected bone
							FVector3f clip_plane_center = origin + normal * dist;	// CPLIPPING PLANE POS
							FVector3f aux_morph = vertex - morph_plane_center;		// MORPH PLANE --> CURRENT VERTEX
							FVector3f aux_clip = vertex - clip_plane_center;		// CLIPPING PLANE --> CURRENT VERTEX

							float dot_morph = FVector3f::DotProduct(aux_morph, normal);			// ANGLE (MORPH PLANE TO VERTEX AND NORMAL)
							float dot_cut = FVector3f::DotProduct(aux_clip, normal);				// ANGLE (CLIPPING PLANE TO VERTEX AND NORMAL )

							if (dot_morph >= 0.f || dot_cut >= 0.f)				// CHECK IF CLIPPING OR MORPH SHOULD BE COMPUTED FOR V VERTEX
							{
								FVector3f current_center = morph_plane_center + normal * dot_morph;	// PROJECTED POINT FROM THE MROPH PLANE (THE CLOSER THE DOT VALUE OF NORMAL AND VERTEX THE FURTHER IT GOES )
								FVector3f radius_vector = vertex - current_center;					// 	
								float radius_vector_len = radius_vector.Length();
								FVector3f radius_vector_unit = radius_vector_len != 0.f ? radius_vector / radius_vector_len : FVector3f(0.f, 0.f, 0.f); // UNITARY VECTOR THAT GOES FROM THE POINT TO THE VERTEX

								float angle_from_origin = acosf(FVector3f::DotProduct(radius_vector_unit, origin_radius_vector));

								// Cross product between the perpendicular vector from radius vector and origin radius and the normal vector
								if (FVector3f::DotProduct(FVector3f::CrossProduct(radius_vector_unit, origin_radius_vector), normal) < 0)
								{
									angle_from_origin = -angle_from_origin;
								}

								angle_from_origin += angle * PI / 180.f;

								float term1 = radius2 * cosf(angle_from_origin);
								float term2 = radius * sinf(angle_from_origin);
								float ellipse_radius_at_angle = radius * radius2 / sqrtf(term1 * term1 + term2 * term2);

								FVector3f vertex_proj_ellipse = current_center + radius_vector_unit * ellipse_radius_at_angle;
								//FVector3f vertex_proj_ellipse = current_center + radius_vector_unit * radius;

								float morph_alpha = dist != 0.f && dot_morph <= dist ? FMath::Clamp(powf(dot_morph / dist, factor),0.f, 1.f) : 1.f;

								vertex = vertex * (1.f - morph_alpha) + vertex_proj_ellipse * morph_alpha;

								if (dot_cut >= 0.f)		// CHECK IF THE VERTEX SHOULD BE CLIPPED
								{
									FVector3f vert_displ = normal * -dot_cut;
									vertex = vertex + vert_displ;
									RemovedVertices[v] = true;
								}
							}
						}

						for (int i = 0; i < 3; ++i)
						{
							ConvertData(i, it.ptr(), it.GetFormat(), &vertex[0], MBF_FLOAT32);
						}

						++it;
					}
					break;

				default:
					break;
				}
			}
		}

		// Now remove all the faces from the result mesh that have all vertices removed
		UntypedMeshBufferIteratorConst itBase(Result->GetIndexBuffers(), MBS_VERTEXINDEX);
		UntypedMeshBufferIterator itDest(Result->GetIndexBuffers(), MBS_VERTEXINDEX);
		int32 aFaceCount = Result->GetFaceCount();

		UntypedMeshBufferIteratorConst ito(Result->GetIndexBuffers(), MBS_VERTEXINDEX);
		for (int f = 0; f < aFaceCount; ++f)
		{
            FUint32Vector3 ov;
			ov[0] = ito.GetAsUINT32(); ++ito;
			ov[1] = ito.GetAsUINT32(); ++ito;
			ov[2] = ito.GetAsUINT32(); ++ito;
		
			bool all_vertexs_removed = RemovedVertices[ov[0]] && RemovedVertices[ov[1]] && RemovedVertices[ov[2]];

			if (!all_vertexs_removed)
			{
				if (itDest.ptr() != itBase.ptr())
				{
					FMemory::Memcpy(itDest.ptr(), itBase.ptr(), itBase.GetElementSize() * 3);
				}

				itDest += 3;
			}

			itBase += 3;
		}

		SIZE_T removedIndices = itBase - itDest;
		check(removedIndices % 3 == 0);

		Result->GetFaceBuffers().SetElementCount(aFaceCount - (int32)removedIndices / 3);
		Result->GetIndexBuffers().SetElementCount(aFaceCount * 3 - (int32)removedIndices);

		// TODO: Should redo/reorder the face buffer before SetElementCount since some deleted faces could be left and some remaining faces deleted.

        // Fix the surface data if present.
        if (Result->m_surfaces.Num())
        {
            // We assume there will be only one.
            check(Result->m_surfaces.Num()==1);

            Result->m_surfaces[0].m_indexCount -= (int32)removedIndices;
        }
	}
}
