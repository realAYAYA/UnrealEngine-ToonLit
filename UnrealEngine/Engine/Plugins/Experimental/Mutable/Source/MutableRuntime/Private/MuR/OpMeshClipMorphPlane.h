// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/Platform.h"


namespace mu
{
	inline bool PointInBoundingBox(const vec3f& point, const SHAPE& selectionShape)
	{
		vec3f v = point - selectionShape.position;

		for (int i = 0; i < 3; ++i)
		{
			if (fabs(v[i]) > selectionShape.size[i])
			{
				return false;
			}
		}

		return true;
	}

	inline bool VertexIsInMaxRadius(const vec3f& vertex, const vec3f& origin, float vertexSelectionBoneMaxRadius)
	{
		if (vertexSelectionBoneMaxRadius < 0.f)
		{
			return true;
		}

		vec3f radiusVec = vertex - origin;
		float radius = dot(radiusVec, radiusVec);

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
	inline MeshPtr MeshClipMorphPlane(const Mesh* pBase, const vec3f& origin, const vec3f& normal, float dist, float factor, float radius,
		float radius2, float angle, const SHAPE& selectionShape, const mu::string& boneName = mu::string(), float vertexSelectionBoneMaxRadius = -1.f)
	{
		//float radius = 8.f;
		//float radius2 = 4.f;
		//float factor = 1.f;

		// Generate vector perpendicular to normal for ellipse rotation reference base
		vec3f aux_base(0.f, 1.f, 0.f);

		if (fabs(dot(normal, aux_base)) > 0.95f)		// fabs = absolute value
		{
			aux_base = vec3f(0.f, 0.f, 1.f);
		}
		
		vec3f origin_radius_vector = cross(normal, aux_base);		// PERPENDICULAR VECTOR TO THE PLANE normal and aux base
		check(fabs(dot(normal, origin_radius_vector)) < 0.05f);

		MeshPtr pDest = pBase->Clone();

        uint32 vcount = pBase->GetVertexBuffers().GetElementCount();

		if (!vcount)
		{
			return pDest;
		}

		TArray<bool> bone_is_affected;
		TArray<vertex_bone_info> vertex_info;

        auto pBaseSkeleton = pBase->GetSkeleton();
        if (!boneName.empty() && pBaseSkeleton )
		{
            bone_is_affected.SetNum(pBaseSkeleton->GetBoneCount());

            for (int32 bone_idx = 0; bone_idx < pBaseSkeleton->GetBoneCount(); ++bone_idx)
			{
				int32 current_idx = bone_idx;
				bool found_bone = false;

				while (current_idx >= 0)
				{
                    if (pBaseSkeleton->GetBoneName(current_idx) == boneName)
					{
						found_bone = true;
						break;
					}
					else
					{
                        current_idx = pBaseSkeleton->GetBoneParent(current_idx);
					}
				}

				bone_is_affected[bone_idx] = found_bone;
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

		// TODO: Replace with an array of bools?
        TArray<bool> RemovedVertices;
		RemovedVertices.Init(false,pDest->GetVertexCount());

		const FMeshBufferSet& MBSPriv = pDest->GetVertexBuffers();
		for (int32 b = 0; b < MBSPriv.m_buffers.Num(); ++b)
		{
			for (int32 c = 0; c < MBSPriv.m_buffers[b].m_channels.Num(); ++c)
			{
				MESH_BUFFER_SEMANTIC sem = MBSPriv.m_buffers[b].m_channels[c].m_semantic;
				int semIndex = MBSPriv.m_buffers[b].m_channels[c].m_semanticIndex;

				UntypedMeshBufferIterator it(pDest->GetVertexBuffers(), sem, semIndex);

				switch (sem)
				{
				case MBS_POSITION:
                    for (uint32 v = 0; v < vcount; ++v)
					{
						vec3f vertex(0.0f, 0.0f, 0.0f);
						for (int i = 0; i < 3; ++i)
						{
							ConvertData(i, &vertex[0], MBF_FLOAT32, it.ptr(), it.GetFormat());
						}

						if (
							(  !boneName.empty() && VertexIsAffectedByBone(v, bone_is_affected, vertex_info) && VertexIsInMaxRadius(vertex, origin, vertexSelectionBoneMaxRadius))
                            || (boneName.empty() && selectionShape.type == (uint8_t)SHAPE::Type::None)
                            || (selectionShape.type == (uint8_t)SHAPE::Type::AABox && PointInBoundingBox(vertex, selectionShape))
							)
						{
							vec3f morph_plane_center = origin;					// MORPH PLANE POS relative to root of the selected bone
							vec3f clip_plane_center = origin + normal * dist;	// CPLIPPING PLANE POS
							vec3f aux_morph = vertex - morph_plane_center;		// MORPH PLANE --> CURRENT VERTEX
							vec3f aux_clip = vertex - clip_plane_center;		// CLIPPING PLANE --> CURRENT VERTEX

							float dot_morph = dot(aux_morph, normal);			// ANGLE (MORPH PLANE TO VERTEX AND NORMAL)
							float dot_cut = dot(aux_clip, normal);				// ANGLE (CLIPPING PLANE TO VERTEX AND NORMAL )

							if (dot_morph >= 0.f || dot_cut >= 0.f)				// CHECK IF CLIPPING OR MORPH SHOULD BE COMPUTED FOR V VERTEX
							{
								vec3f current_center = morph_plane_center + normal * dot_morph;	// PROJECTED POINT FROM THE MROPH PLANE (THE CLOSER THE DOT VALUE OF NORMAL AND VERTEX THE FURTHER IT GOES )
								vec3f radius_vector = vertex - current_center;					// 	
								float radius_vector_len = length(radius_vector);
								vec3f radius_vector_unit = radius_vector_len != 0.f ? radius_vector / radius_vector_len : vec3f(0.f, 0.f, 0.f); // UNITARY VECTOR THAT GOES FROM THE POINT TO THE VERTEX

								float angle_from_origin = acosf(dot(radius_vector_unit, origin_radius_vector));

								// Cross product between the perpendicular vector from radius vector and origin radius and the normal vector
								if (dot(cross(radius_vector_unit, origin_radius_vector), normal) < 0)
								{
									angle_from_origin = -angle_from_origin;
								}

								angle_from_origin += angle * PI / 180.f;

								float term1 = radius2 * cosf(angle_from_origin);
								float term2 = radius * sinf(angle_from_origin);
								float ellipse_radius_at_angle = radius * radius2 / sqrtf(term1 * term1 + term2 * term2);

								vec3f vertex_proj_ellipse = current_center + radius_vector_unit * ellipse_radius_at_angle;
								//vec3f vertex_proj_ellipse = current_center + radius_vector_unit * radius;

								float morph_alpha = dist != 0.f && dot_morph <= dist ? clamp(0.f, 1.f, powf(dot_morph / dist, factor)) : 1.f;

								vertex = vertex * (1.f - morph_alpha) + vertex_proj_ellipse * morph_alpha;

								if (dot_cut >= 0.f)		// CHECK IF THE VERTEX SHOULD BE CLIPPED
								{
									vec3f vert_displ = normal * -dot_cut;
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
		UntypedMeshBufferIteratorConst itBase(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
		UntypedMeshBufferIterator itDest(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
		int aFaceCount = pDest->GetFaceCount();

		UntypedMeshBufferIteratorConst ito(pDest->GetIndexBuffers(), MBS_VERTEXINDEX);
		for (int f = 0; f < aFaceCount; ++f)
		{
            vec3<uint32> ov;
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

		std::size_t removedIndices = itBase - itDest;
		check(removedIndices % 3 == 0);

		pDest->GetFaceBuffers().SetElementCount(aFaceCount - (int)removedIndices / 3);
		pDest->GetIndexBuffers().SetElementCount(aFaceCount * 3 - (int)removedIndices);

		// TODO: Should redo/reorder the face buffer before SetElementCount since some deleted faces could be left and some remaining faces deleted.

        // Fix the surface data if present.
        if (pDest->m_surfaces.Num())
        {
            // We assume there will be only one.
            check(pDest->m_surfaces.Num()==1);

            pDest->m_surfaces[0].m_indexCount -= (int32)removedIndices;
        }

		return pDest;
	}

}
