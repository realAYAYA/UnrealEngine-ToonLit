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
#include "MuR/MemoryPrivate.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"


namespace mu
{


	//-------------------------------------------------------------------------------------------------
	// Eric Lengyel method's
	// http://www.terathon.com/code/tangent.html
	//-------------------------------------------------------------------------------------------------
	namespace
	{

#define MUTABLE_VERTEX_MERGE_TEX_RANGE          1024
#define MUTABLE_TANGENT_GENERATION_EPSILON_1    0.000001f
#define MUTABLE_TANGENT_GENERATION_EPSILON_2    0.001f
#define MUTABLE_TANGENT_MIN_AXIS_DIFFERENCE     0.999f

		struct TVertex
		{

			TVertex() = default;

			TVertex(const vec3<float>& p, const vec3<float>& n, const vec2<float>& t)
			{
				pos = p;
				nor = n;
				tex = t;
			}

			vec3<float> pos;
			vec3<float> nor;
			vec2<float> tex;

			inline bool operator< (const TVertex& other) const
			{
				if (pos < other.pos)
					return true;
				if (other.pos < pos)
					return false;

				if (nor < other.nor)
					return true;
				if (other.nor < nor)
					return false;

				// Compare the texture coordinates with a particular precission.
				vec2<int> uv0 = vec2<int>(int(tex[0] * MUTABLE_VERTEX_MERGE_TEX_RANGE),
					int(tex[1] * MUTABLE_VERTEX_MERGE_TEX_RANGE));
				vec2<int> uv1 = vec2<int>(int(other.tex[0] * MUTABLE_VERTEX_MERGE_TEX_RANGE),
					int(other.tex[1] * MUTABLE_VERTEX_MERGE_TEX_RANGE));

				if (uv0 < uv1)
					return true;
				if (uv1 < uv0)
					return false;

				return false;
			}

			inline bool operator== (const TVertex& other) const
			{
				return  !((*this) < other)
					&&
					!(other < (*this));
			}
		};


		struct TFace
		{
			vec3<float> T;
			vec3<float> B;
			vec3<float> N;

			TFace()
				: T(0, 0, 0)
				, B(0, 0, 0)
				, N(0, 0, 0)
			{}

			TFace
			(
				const vec3<float>& v1,
				const vec3<float>& v2,
				const vec3<float>& v3,
				const vec2<float>& w1,
				const vec2<float>& w2,
				const vec2<float>& w3
			)
			{
				vec3<float> E1 = v2 - v1;
				vec3<float> E2 = v3 - v1;

				vec2<float> UV1 = w2 - w1;
				vec2<float> UV2 = w3 - w1;

				float  UVdet = UV1[0] * UV2[1] - UV2[0] * UV1[1];

				N = normalise(cross(E1, E2));

				if (!(fabs(UVdet) <= MUTABLE_TANGENT_GENERATION_EPSILON_1))
				{
					double r = 1.0 / UVdet;

					T = vec3<float>
						(
							(UV2[1] * E1[0] - UV1[1] * E2[0]),
							(UV2[1] * E1[1] - UV1[1] * E2[1]),
							(UV2[1] * E1[2] - UV1[1] * E2[2])
							);

					B = vec3<float>
						(
							(UV1[0] * E2[0] - UV2[0] * E1[0]),
							(UV1[0] * E2[1] - UV2[0] * E1[1]),
							(UV1[0] * E2[2] - UV2[0] * E1[2])
							);

					T = T * float(r);
					B = B * float(r);

					T = normalise(T);
					B = normalise(B);
				}
				else
				{
					T = vec3<float>(0, 0, 0);
					B = vec3<float>(0, 0, 0);
					N = vec3<float>(0, 0, 0);
				}
			}

		};

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
					if (resultSemantic == MBS_TANGENTSIGN)
					{
						// Look for the full tangent space
						int tanXBuf, tanXChan, tanYBuf, tanYChan, tanZBuf, tanZChan;
						Source.FindChannel(MBS_TANGENT, resultSemanticIndex, &tanXBuf, &tanXChan);
						Source.FindChannel(MBS_BINORMAL, resultSemanticIndex, &tanYBuf, &tanYChan);
						Source.FindChannel(MBS_NORMAL, resultSemanticIndex, &tanZBuf, &tanZChan);

						if (tanXBuf >= 0 && tanYBuf >= 0 && tanZBuf >= 0)
						{
							generated = true;
							UntypedMeshBufferIteratorConst xIt(Source, MBS_TANGENT, resultSemanticIndex);
							UntypedMeshBufferIteratorConst yIt(Source, MBS_BINORMAL, resultSemanticIndex);
							UntypedMeshBufferIteratorConst zIt(Source, MBS_NORMAL, resultSemanticIndex);
							for (int v = 0; v < vCount; ++v)
							{
								mat3f mat;
								mat[0] = xIt.GetAsVec4f().xyz();
								mat[1] = yIt.GetAsVec4f().xyz();
								mat[2] = zIt.GetAsVec4f().xyz();
								float sign = mat.GetDeterminant() < 0 ? -1.0f : 1.0f;
								ConvertData(0, pResultBuf, resultFormat, &sign, MBF_FLOAT32);

								for (int i = 1; i < resultComponents; ++i)
								{
									// Add zeros
									FMemory::Memzero
									(
										pResultBuf + GetMeshFormatData(resultFormat).m_size * i,
										GetMeshFormatData(resultFormat).m_size
									);
								}
								pResultBuf += resultElemSize;
								xIt++;
								yIt++;
								zIt++;
							}
						}
					}

					// If we have to add colour channels, we will add them as white, to be neutral.
					// \todo: normal channels also should have special values.
					else if (resultSemantic == MBS_COLOUR)
					{
						generated = true;

						switch (resultFormat)
						{
						case MBF_FLOAT32:
						{
							for (int v = 0; v < vCount; ++v)
							{
								auto pTypedResultBuf = (float*)pResultBuf;
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
								auto pTypedResultBuf = (uint8_t*)pResultBuf;
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
								auto pTypedResultBuf = (uint16*)pResultBuf;
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
							auto pData = (uint8_t*)pResultBuf;

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

								mat3f mat;
								xIt += v;
								yIt += v;
								zIt += v;
								mat[0] = xIt.GetAsVec4f().xyz();
								mat[1] = yIt.GetAsVec4f().xyz();
								mat[2] = zIt.GetAsVec4f().xyz();

								uint8_t sign = 0;
								if (resultFormat == MBF_PACKEDDIR8_W_TANGENTSIGN)
								{
									sign = mat.GetDeterminant() < 0 ? 0 : 255;
								}
								else if (resultFormat == MBF_PACKEDDIRS8_W_TANGENTSIGN)
								{
									sign = mat.GetDeterminant() < 0 ? -128 : 127;
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
						|| sourceSemantic == MBS_CHART
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
	MeshPtr MeshFormat
	(
		const Mesh* pPureSource,
		const Mesh* pFormat,
		bool keepSystemBuffers,
		bool formatVertices,
		bool formatIndices,
		bool formatFaces,
		bool ignoreMissingChannels
	)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshFormat);

		if (!pPureSource) { return nullptr; }
		if (!pFormat) { return pPureSource->Clone(); }

		MeshPtrConst pSource = pPureSource;

		MeshPtr pResult = pFormat->Clone();

		// Make sure that the bone indices will fit in this format, or extend it.
		if (formatVertices)
		{
			auto& buffs = pSource->GetVertexBuffers();

			int semanticIndex = 0;
			while (true)
			{
				int buf = 0;
				int chan = 0;
				buffs.FindChannel(MBS_BONEINDICES, semanticIndex, &buf, &chan);
				if (buf < 0)
					break;

				int resultBuf = 0;
				int resultChan = 0;
				auto& formBuffs = pResult->GetVertexBuffers();
				formBuffs.FindChannel(MBS_BONEINDICES, semanticIndex, &resultBuf, &resultChan);
				if (resultBuf >= 0)
				{
					UntypedMeshBufferIteratorConst it(buffs, MBS_BONEINDICES, semanticIndex);
					int32_t maxBoneIndex = 0;
					for (int v = 0; v < buffs.GetElementCount(); ++v)
					{
						auto va = it.GetAsVec8i();
						for (int c = 0; c < it.GetComponents(); ++c)
						{
							maxBoneIndex = FMath::Max(maxBoneIndex, va[c]);
						}
						++it;
					}

					auto& format = formBuffs.m_buffers[resultBuf].m_channels[resultChan].m_format;
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

				semanticIndex++;
			}
		}

		// \todo Make sure that the vertex indices will fit in this format, or extend it.



		if (formatVertices)
		{

			FormatBufferSet(pSource->GetVertexBuffers(), pResult->GetVertexBuffers(),
				keepSystemBuffers, ignoreMissingChannels, true);
		}
		else
		{
			pResult->m_VertexBuffers = pSource->GetVertexBuffers();
		}

		if (formatIndices)
		{
			FormatBufferSet(pSource->GetIndexBuffers(), pResult->GetIndexBuffers(), keepSystemBuffers,
				ignoreMissingChannels, false);
		}
		else
		{
			pResult->m_IndexBuffers = pSource->GetIndexBuffers();
		}

		if (formatFaces)
		{
			FormatBufferSet(pSource->GetFaceBuffers(),
				pResult->GetFaceBuffers(),
				keepSystemBuffers,
				ignoreMissingChannels,
				false);
		}
		else
		{
			pResult->m_FaceBuffers = pSource->GetFaceBuffers();
		}

		// Copy the rest of the data
		pResult->SetSkeleton(pSource->GetSkeleton());
		pResult->SetPhysicsBody(pSource->GetPhysicsBody());

		pResult->m_layouts.Empty();
		for (const auto& Layout : pSource->m_layouts)
		{
			pResult->m_layouts.Add(Layout->Clone());
		}

		pResult->ResetStaticFormatFlags();
		pResult->EnsureSurfaceData();

		pResult->m_tags = pSource->m_tags;

		pResult->m_AdditionalBuffers = pSource->m_AdditionalBuffers;

		pResult->m_bonePoses = pSource->m_bonePoses;

		return pResult;

	}

}

