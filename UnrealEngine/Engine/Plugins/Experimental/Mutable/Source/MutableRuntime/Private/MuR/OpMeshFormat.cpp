// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/OpMeshFormat.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/AssertionMacros.h"
#include "MuR/ConvertData.h"
#include "MuR/Layout.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

#include "GPUSkinPublicDefs.h"


namespace mu
{

	namespace
	{

#define MUTABLE_VERTEX_MERGE_TEX_RANGE          1024
#define MUTABLE_TANGENT_GENERATION_EPSILON_1    0.000001f
#define MUTABLE_TANGENT_GENERATION_EPSILON_2    0.001f
#define MUTABLE_TANGENT_MIN_AXIS_DIFFERENCE     0.999f

		//struct TVertex
		//{

		//	TVertex() = default;

		//	TVertex(const FVector3f& p, const FVector3f& n, const FVector2f& t)
		//	{
		//		pos = p;
		//		nor = n;
		//		tex = t;
		//	}

		//	FVector3f pos;
		//	FVector3f nor;
		//	FVector2f tex;

		//	inline bool operator< (const TVertex& other) const
		//	{
		//		if (pos < other.pos)
		//			return true;
		//		if (other.pos < pos)
		//			return false;

		//		if (nor < other.nor)
		//			return true;
		//		if (other.nor < nor)
		//			return false;

		//		// Compare the texture coordinates with a particular precission.
		//		FIntVector2 uv0 = FIntVector2(int32(tex[0] * MUTABLE_VERTEX_MERGE_TEX_RANGE),
		//			int32(tex[1] * MUTABLE_VERTEX_MERGE_TEX_RANGE));
		//		FIntVector2 uv1 = FIntVector2(int32(other.tex[0] * MUTABLE_VERTEX_MERGE_TEX_RANGE),
		//			int32(other.tex[1] * MUTABLE_VERTEX_MERGE_TEX_RANGE));

		//		if (uv0 < uv1)
		//			return true;
		//		if (uv1 < uv0)
		//			return false;

		//		return false;
		//	}

		//	inline bool operator== (const TVertex& other) const
		//	{
		//		return  !((*this) < other)
		//			&&
		//			!(other < (*this));
		//	}
		//};


		//struct TFace
		//{
		//	FVector3f T;
		//	FVector3f B;
		//	FVector3f N;

		//	TFace()
		//		: T(0, 0, 0)
		//		, B(0, 0, 0)
		//		, N(0, 0, 0)
		//	{}

		//	TFace
		//	(
		//		const FVector3f& v1,
		//		const FVector3f& v2,
		//		const FVector3f& v3,
		//		const FVector2f& w1,
		//		const FVector2f& w2,
		//		const FVector2f& w3
		//	)
		//	{
		//		FVector3f E1 = v2 - v1;
		//		FVector3f E2 = v3 - v1;

		//		FVector2f UV1 = w2 - w1;
		//		FVector2f UV2 = w3 - w1;

		//		float  UVdet = UV1[0] * UV2[1] - UV2[0] * UV1[1];

		//		N = FVector3f::CrossProduct(E1, E2);
		//		N.Normalize();

		//		if (!(fabs(UVdet) <= MUTABLE_TANGENT_GENERATION_EPSILON_1))
		//		{
		//			double r = 1.0 / UVdet;

		//			T = FVector3f
		//				(
		//					(UV2[1] * E1[0] - UV1[1] * E2[0]),
		//					(UV2[1] * E1[1] - UV1[1] * E2[1]),
		//					(UV2[1] * E1[2] - UV1[1] * E2[2])
		//					);

		//			B = FVector3f
		//				(
		//					(UV1[0] * E2[0] - UV2[0] * E1[0]),
		//					(UV1[0] * E2[1] - UV2[0] * E1[1]),
		//					(UV1[0] * E2[2] - UV2[0] * E1[2])
		//					);

		//			T = T * float(r);
		//			B = B * float(r);

		//			T.Normalize();
		//			B.Normalize();
		//		}
		//		else
		//		{
		//			T = FVector3f(0, 0, 0);
		//			B = FVector3f(0, 0, 0);
		//			N = FVector3f(0, 0, 0);
		//		}
		//	}

		//};

	}


	//-------------------------------------------------------------------------------------------------
	void MeshFormatBuffer
	(
		const FMeshBufferSet& Source,
		FMeshBufferSet& Result,
		int bufferIndex
	)
	{
		int vCount = Source.GetElementCount();

		int b = bufferIndex;
		{
			// For every channel in this buffer
			for (int c = 0; c < Result.GetBufferChannelCount(b); ++c)
			{
				// Find this channel in the source mesh
				MESH_BUFFER_SEMANTIC resultSemantic;
				int resultSemanticIndex;
				MESH_BUFFER_FORMAT resultFormat;
				int resultComponents;
				int resultOffset;
				Result.GetChannel
				(
					b, c,
					&resultSemantic, &resultSemanticIndex,
					&resultFormat, &resultComponents,
					&resultOffset
				);

				int sourceBuffer;
				int sourceChannel;
				Source.FindChannel
				(resultSemantic, resultSemanticIndex, &sourceBuffer, &sourceChannel);

				int resultElemSize = Result.GetElementSize(b);
				int resultChannelSize = GetMeshFormatData(resultFormat).m_size * resultComponents;
				uint8_t* pResultBuf = Result.GetBufferData(b);
				pResultBuf += resultOffset;

				if (sourceBuffer < 0)
				{
					// Not found: fill with zeros.

					// Special case for derived channel data
					bool generated = false;

					// If we have to add colour channels, we will add them as white, to be neutral.
					// \todo: normal channels also should have special values.
					if (resultSemantic == MBS_COLOUR)
					{
						generated = true;

						switch (resultFormat)
						{
						case MBF_FLOAT32:
						{
							for (int v = 0; v < vCount; ++v)
							{
								float* pTypedResultBuf = (float*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 1.0f;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						case MBF_NUINT8:
						{
							for (int v = 0; v < vCount; ++v)
							{
								uint8_t* pTypedResultBuf = (uint8_t*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 255;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						case MBF_NUINT16:
						{
							for (int v = 0; v < vCount; ++v)
							{
								uint16* pTypedResultBuf = (uint16*)pResultBuf;
								for (int i = 0; i < resultComponents; ++i)
								{
									pTypedResultBuf[i] = 65535;
								}
								pResultBuf += resultElemSize;
							}
							break;
						}

						default:
							// Format not implemented
							check(false);
							break;
						}
					}

					if (!generated)
					{
						// TODO: and maybe raise a warning?
						for (int v = 0; v < vCount; ++v)
						{
							FMemory::Memzero(pResultBuf, resultChannelSize);
							pResultBuf += resultElemSize;
						}
					}
				}
				else
				{
					// Get the data about the source format
					MESH_BUFFER_SEMANTIC sourceSemantic;
					int sourceSemanticIndex;
					MESH_BUFFER_FORMAT sourceFormat;
					int sourceComponents;
					int sourceOffset;
					Source.GetChannel
					(
						sourceBuffer, sourceChannel,
						&sourceSemantic, &sourceSemanticIndex,
						&sourceFormat, &sourceComponents,
						&sourceOffset
					);
					check(sourceSemantic == resultSemantic
						&&
						sourceSemanticIndex == resultSemanticIndex);

					int sourceElemSize = Source.GetElementSize(sourceBuffer);
					const uint8_t* pSourceBuf = Source.GetBufferData(sourceBuffer);
					pSourceBuf += sourceOffset;

					// Copy element by element
					for (int v = 0; v < vCount; ++v)
					{
						if (resultFormat == sourceFormat && resultComponents == sourceComponents)
						{
							memcpy(pResultBuf, pSourceBuf, resultChannelSize);
						}
						else if (resultFormat == MBF_PACKEDDIR8_W_TANGENTSIGN
							||
							resultFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN)
						{
							check(sourceComponents >= 3);
							check(resultComponents == 4);

							// convert the 3 first components
							//memcpy(pResultBuf, pSourceBuf, resultChannelSize);
							for (int i = 0; i < 3; ++i)
							{
								if (i < sourceComponents)
								{
									ConvertData
									(
										i,
										pResultBuf, resultFormat,
										pSourceBuf, sourceFormat
									);
								}
							}


							// Add the tangent sign
							uint8_t* pData = (uint8_t*)pResultBuf;

							// Look for the full tangent space
							int tanXBuf, tanXChan, tanYBuf, tanYChan, tanZBuf, tanZChan;
							Source.FindChannel(MBS_TANGENT, resultSemanticIndex, &tanXBuf, &tanXChan);
							Source.FindChannel(MBS_BINORMAL, resultSemanticIndex, &tanYBuf, &tanYChan);
							Source.FindChannel(MBS_NORMAL, resultSemanticIndex, &tanZBuf, &tanZChan);

							if (tanXBuf >= 0 && tanYBuf >= 0 && tanZBuf >= 0)
							{
								UntypedMeshBufferIteratorConst xIt(Source, MBS_TANGENT, resultSemanticIndex);
								UntypedMeshBufferIteratorConst yIt(Source, MBS_BINORMAL, resultSemanticIndex);
								UntypedMeshBufferIteratorConst zIt(Source, MBS_NORMAL, resultSemanticIndex);

								xIt += v;
								yIt += v;
								zIt += v;

								FMatrix44f Mat(xIt.GetAsVec3f(), yIt.GetAsVec3f(), zIt.GetAsVec3f(), FVector3f(0, 0, 0));

								uint8_t sign = 0;
								if (resultFormat == MBF_PACKEDDIR8_W_TANGENTSIGN)
								{
									sign = Mat.RotDeterminant() < 0 ? 0 : 255;
								}
								else if (resultFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN)
								{
									sign = Mat.RotDeterminant() < 0 ? -128 : 127;
								}
								pData[3] = sign;
							}
						}
						else
						{
							// Convert formats
							for (int i = 0; i < resultComponents; ++i)
							{
								if (i < sourceComponents)
								{
									ConvertData
									(
										i,
										pResultBuf, resultFormat,
										pSourceBuf, sourceFormat
									);
								}
								else
								{
									// Add zeros. TODO: Warning?
									FMemory::Memzero
									(
										pResultBuf + GetMeshFormatData(resultFormat).m_size * i,
										GetMeshFormatData(resultFormat).m_size
									);
								}
							}


							// Extra step to normalise some semantics in some formats
							// TODO: Make it optional, and add different normalisation types n, n^2
							// TODO: Optimise
							if (sourceSemantic == MBS_BONEWEIGHTS)
							{
								if (resultFormat == MBF_NUINT8)
								{
									uint8_t* pData = (uint8_t*)pResultBuf;
									uint8_t accum = 0;
									for (int i = 0; i < resultComponents; ++i)
									{
										accum += pData[i];
									}
									pData[0] += 255 - accum;
								}

								else if (resultFormat == MBF_NUINT16)
								{
									uint16* pData = (uint16*)pResultBuf;
									uint16 accum = 0;
									for (int i = 0; i < resultComponents; ++i)
									{
										accum += pData[i];
									}
									pData[0] += 65535 - accum;
								}
							}
						}

						pResultBuf += resultElemSize;
						pSourceBuf += sourceElemSize;
					}
				}
			}
		}
	}


	//-------------------------------------------------------------------------------------------------
	void FormatBufferSet
	(
		const FMeshBufferSet& Source,
		FMeshBufferSet& Result,
		bool keepSystemBuffers,
		bool ignoreMissingChannels,
		bool isVertexBuffer
	)
	{
		if (ignoreMissingChannels)
		{
			// Remove from the result the channels that are not present in the source, and re-pack the
			// offsets.
			for (int b = 0; b < Result.GetBufferCount(); ++b)
			{
				TArray<MESH_BUFFER_SEMANTIC> resultSemantics;
				TArray<int> resultSemanticIndexs;
				TArray<MESH_BUFFER_FORMAT> resultFormats;
				TArray<int> resultComponentss;
				TArray<int> resultOffsets;
				int offset = 0;

				// For every channel in this buffer
				for (int c = 0; c < Result.GetBufferChannelCount(b); ++c)
				{
					MESH_BUFFER_SEMANTIC resultSemantic;
					int resultSemanticIndex;
					MESH_BUFFER_FORMAT resultFormat;
					int resultComponents;

					// Find this channel in the source mesh
					Result.GetChannel
					(
						b, c,
						&resultSemantic, &resultSemanticIndex,
						&resultFormat, &resultComponents,
						nullptr
					);

					int sourceBuffer;
					int sourceChannel;
					Source.FindChannel
					(resultSemantic, resultSemanticIndex, &sourceBuffer, &sourceChannel);

					if (sourceBuffer >= 0)
					{
						resultSemantics.Add(resultSemantic);
						resultSemanticIndexs.Add(resultSemanticIndex);
						resultFormats.Add(resultFormat);
						resultComponentss.Add(resultComponents);
						resultOffsets.Add(offset);

						offset += GetMeshFormatData(resultFormat).m_size * resultComponents;
					}
				}

				if (resultSemantics.IsEmpty())
				{
					Result.SetBuffer(b, 0, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
				}
				else
				{
					Result.SetBuffer(b, offset, resultSemantics.Num(),
						&resultSemantics[0],
						&resultSemanticIndexs[0],
						&resultFormats[0],
						&resultComponentss[0],
						&resultOffsets[0]);
				}
			}
		}


		// For every vertex buffer in result
		int vCount = Source.GetElementCount();
		Result.SetElementCount(vCount);
		for (int b = 0; b < Result.GetBufferCount(); ++b)
		{
			MeshFormatBuffer(Source, Result, b);
		}


		// Detect internal system buffers and clone them unmodified.
		if (keepSystemBuffers)
		{
			for (int b = 0; b < Source.GetBufferCount(); ++b)
			{
				// Detect system buffers and clone them unmodified.
				if (Source.GetBufferChannelCount(b) > 0)
				{
					MESH_BUFFER_SEMANTIC sourceSemantic;
					int sourceSemanticIndex;
					MESH_BUFFER_FORMAT sourceFormat;
					int sourceComponents;
					int sourceOffset;
					Source.GetChannel
					(
						b, 0,
						&sourceSemantic, &sourceSemanticIndex,
						&sourceFormat, &sourceComponents,
						&sourceOffset
					);

					if (sourceSemantic == MBS_LAYOUTBLOCK
						|| (isVertexBuffer && sourceSemantic == MBS_VERTEXINDEX)
						)
					{
						Result.AddBuffer(Source, b);
					}
				}
			}
		}

	}



	//-------------------------------------------------------------------------------------------------
	void MeshFormat
	(
		Mesh* Result, 
		const Mesh* pPureSource,
		const Mesh* pFormat,
		bool keepSystemBuffers,
		bool formatVertices,
		bool formatIndices,
		bool formatFaces,
		bool ignoreMissingChannels,
		bool& bOutSuccess
	)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshFormat);
		bOutSuccess = true;

		if (!pPureSource) 
		{
			check(false);
			bOutSuccess = false;	
			return;
		}

		if (!pFormat)
		{
			check(false);
			bOutSuccess = false;
			return;
		}

		Ptr<const Mesh> pSource = pPureSource;

		Result->CopyFrom(*pFormat);

		// Make sure that the bone indices will fit in this format, or extend it.
		if (formatVertices)
		{
			const FMeshBufferSet& VertexBuffers = pSource->GetVertexBuffers();

			const int32 BufferCount = VertexBuffers.GetBufferCount();
			for (int32 BufferIndex = 0; BufferIndex < BufferCount; ++BufferIndex)
			{
				const int32 ChannelCount = VertexBuffers.GetBufferChannelCount(BufferIndex);
				for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
				{
					const MESH_BUFFER_CHANNEL& Channel = VertexBuffers.m_buffers[BufferIndex].m_channels[ChannelIndex];

					if (Channel.m_semantic == MBS_BONEINDICES)
					{
						int resultBuf = 0;
						int resultChan = 0;
						FMeshBufferSet& formBuffs = Result->GetVertexBuffers();
						formBuffs.FindChannel(MBS_BONEINDICES, Channel.m_semanticIndex, &resultBuf, &resultChan);
						if (resultBuf >= 0)
						{
							UntypedMeshBufferIteratorConst it(VertexBuffers, MBS_BONEINDICES, Channel.m_semanticIndex);
							int32_t maxBoneIndex = 0;
							for (int v = 0; v < VertexBuffers.GetElementCount(); ++v)
							{
								// If MAX_TOTAL_INFLUENCES ever changed, the next line would no longer work or compile and 
								// GetAsVec12i would need to be changed accordingly
								int32 va[MAX_TOTAL_INFLUENCES];
								it.GetAsInt32Vec(va, MAX_TOTAL_INFLUENCES);
								for (int c = 0; c < it.GetComponents(); ++c)
								{
									maxBoneIndex = FMath::Max(maxBoneIndex, va[c]);
								}
								++it;
							}

							MESH_BUFFER_FORMAT& format = formBuffs.m_buffers[resultBuf].m_channels[resultChan].m_format;
							if (maxBoneIndex > 0xffff && (format == MBF_UINT8 || format == MBF_UINT16))
							{
								format = MBF_UINT32;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0x7fff && (format == MBF_INT8 || format == MBF_INT16))
							{
								format = MBF_UINT32;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0xff && format == MBF_UINT8)
							{
								format = MBF_UINT16;
								formBuffs.UpdateOffsets(resultBuf);
							}
							else if (maxBoneIndex > 0x7f && format == MBF_INT8)
							{
								format = MBF_INT16;
								formBuffs.UpdateOffsets(resultBuf);
							}
						}
					}
				}
			}
		}

		// \todo Make sure that the vertex indices will fit in this format, or extend it.



		if (formatVertices)
		{

			FormatBufferSet(pSource->GetVertexBuffers(), Result->GetVertexBuffers(),
				keepSystemBuffers, ignoreMissingChannels, true);
		}
		else
		{
			Result->m_VertexBuffers = pSource->GetVertexBuffers();
		}

		if (formatIndices)
		{
			FormatBufferSet(pSource->GetIndexBuffers(), Result->GetIndexBuffers(), keepSystemBuffers,
				ignoreMissingChannels, false);
		}
		else
		{
			Result->m_IndexBuffers = pSource->GetIndexBuffers();
		}

		if (formatFaces)
		{
			FormatBufferSet(pSource->GetFaceBuffers(),
				Result->GetFaceBuffers(),
				keepSystemBuffers,
				ignoreMissingChannels,
				false);
		}
		else
		{
			Result->m_FaceBuffers = pSource->GetFaceBuffers();
		}

		// Copy the rest of the data
		Result->SetSkeleton(pSource->GetSkeleton());
		Result->SetPhysicsBody(pSource->GetPhysicsBody());

		Result->m_layouts.Empty();
		for (const Ptr<const Layout>& Layout : pSource->m_layouts)
		{
			Result->m_layouts.Add(Layout->Clone());
		}

		Result->m_tags = pSource->m_tags;
		Result->StreamedResources = pSource->StreamedResources;

		Result->AdditionalBuffers = pSource->AdditionalBuffers;

		Result->BonePoses = pSource->BonePoses;
		Result->BoneMap = pSource->BoneMap;

		Result->SkeletonIDs = pSource->SkeletonIDs;

		// A shallow copy is done here, it should not be a problem.
		Result->AdditionalPhysicsBodies = pSource->AdditionalPhysicsBodies;

		Result->m_surfaces = pSource->m_surfaces;

		Result->ResetStaticFormatFlags();
		Result->EnsureSurfaceData();
	}

}
