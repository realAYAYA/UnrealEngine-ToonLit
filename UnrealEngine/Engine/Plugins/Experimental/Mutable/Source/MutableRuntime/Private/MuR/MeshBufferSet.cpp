// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuR/MeshBufferSet.h"

#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MutableTrace.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	static FMeshBufferFormatData s_meshBufferFormatData[MBF_COUNT] =
	{
		{ 0, 0 },
		{ 2, 0 },
		{ 4, 0 },

		{ 1, 8 },
		{ 2, 16 },
		{ 4, 32 },
		{ 1, 7 },
		{ 2, 15 },
		{ 4, 31 },

		{ 1, 0 },
		{ 2, 0 },
		{ 4, 0 },
		{ 1, 0 },
		{ 2, 0 },
		{ 4, 0 },

		{ 1, 0 },
		{ 1, 0 },
		{ 1, 0 },
		{ 1, 0 },

		{ 8, 0 }
	};


	//---------------------------------------------------------------------------------------------
	const FMeshBufferFormatData& GetMeshFormatData(MESH_BUFFER_FORMAT format)
	{
		check(format >= 0);
		check(format < MBF_COUNT);
		return s_meshBufferFormatData[format];
	}


	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	//---------------------------------------------------------------------------------------------
	int32 FMeshBufferSet::GetElementCount() const
	{
		return m_elementCount;
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::SetElementCount(int32 count)
	{
		check(count >= 0);
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		m_elementCount = count;

		for (auto& buf : m_buffers)
		{
			buf.m_data.SetNumUninitialized(buf.m_elementSize * count, false);
		}
	}


	//---------------------------------------------------------------------------------------------
	int32 FMeshBufferSet::GetBufferCount() const
	{
		return m_buffers.Num();
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::SetBufferCount(int32 count)
	{
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
		m_buffers.SetNum(count);
	}


	//---------------------------------------------------------------------------------------------
	int32 FMeshBufferSet::GetBufferChannelCount(int32 buffer) const
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		if (buffer >= 0 && buffer < m_buffers.Num())
		{
			return m_buffers[buffer].m_channels.Num();
		}
		return 0;
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::GetChannel
	(
		int32 buffer,
		int32 channel,
		MESH_BUFFER_SEMANTIC* pSemantic,
		int32* pSemanticIndex,
		MESH_BUFFER_FORMAT* pFormat,
		int32* pComponentCount,
		int32* pOffset
	) const
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		check(channel >= 0 && channel < m_buffers[buffer].m_channels.Num());

		const MESH_BUFFER_CHANNEL& chan = m_buffers[buffer].m_channels[channel];

		if (pSemantic)
		{
			*pSemantic = chan.m_semantic;
		}

		if (pSemanticIndex)
		{
			*pSemanticIndex = chan.m_semanticIndex;
		}

		if (pFormat)
		{
			*pFormat = chan.m_format;
		}

		if (pComponentCount)
		{
			*pComponentCount = chan.m_componentCount;
		}

		if (pOffset)
		{
			*pOffset = chan.m_offset;
		}
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::SetBuffer
	(
		int32 buffer,
		int32 elementSize,
		int32 channelCount,
		const MESH_BUFFER_SEMANTIC* pSemantics,
		const int32* pSemanticIndices,
		const MESH_BUFFER_FORMAT* pFormats,
		const int32* pComponentCount,
		const int32* pOffsets
	)
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		MESH_BUFFER& buf = m_buffers[buffer];

		unsigned minElemSize = 0;
		buf.m_channels.SetNum(channelCount);
		for (int c = 0; c < channelCount; ++c)
		{
			MESH_BUFFER_CHANNEL& chan = buf.m_channels[c];
			chan.m_semantic = pSemantics ? pSemantics[c] : MBS_NONE;
			chan.m_semanticIndex = pSemanticIndices ? ((uint8_t)pSemanticIndices[c]) : 0;
			chan.m_format = pFormats ? pFormats[c] : MBF_NONE;
			chan.m_componentCount = pComponentCount ? ((uint16)pComponentCount[c]) : 0;
			chan.m_offset = pOffsets ? ((uint8_t)pOffsets[c]) : 0;

			minElemSize = FMath::Max
			(
				minElemSize,
				chan.m_offset + chan.m_componentCount * GetMeshFormatData(chan.m_format).m_size
			);
		}

		// Set the user specified element size, or enlarge it if it was too small
		buf.m_elementSize = FMath::Max(elementSize, int32(minElemSize));

		// Update the buffer data
		buf.m_data.SetNumUninitialized(buf.m_elementSize * m_elementCount, false);
	}

	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::SetBufferChannel
	(
		int32 buffer,
		int32 channelIndex,
		MESH_BUFFER_SEMANTIC semantic,
		int32 semanticIndex,
		MESH_BUFFER_FORMAT format,
		int32 componentCount,
		int32 offset
	)
	{
		if (!(buffer >= 0 && buffer < m_buffers.Num()))
		{
			check(false);
			return;
		}

		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		MESH_BUFFER& buf = m_buffers[buffer];

		if (!(channelIndex >= 0 && channelIndex < buf.m_channels.Num()))
		{
			check(false);
			return;
		}

		MESH_BUFFER_CHANNEL& chan = buf.m_channels[channelIndex];
		chan.m_semantic = semantic;
		chan.m_semanticIndex = uint8_t(semanticIndex);
		chan.m_format = format;
		chan.m_componentCount = uint16(componentCount);
		chan.m_offset = uint8(offset);
	}


	//---------------------------------------------------------------------------------------------
	uint8* FMeshBufferSet::GetBufferData(int32 buffer)
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		uint8_t* pResult = m_buffers[buffer].m_data.GetData();
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	const uint8* FMeshBufferSet::GetBufferData(int32 buffer) const
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		const uint8_t* pResult = m_buffers[buffer].m_data.GetData();
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::FindChannel
	(
		MESH_BUFFER_SEMANTIC semantic, int32 semanticIndex,
		int32* pBuffer, int32* pChannel
	) const
	{
		check(pBuffer && pChannel);

		*pBuffer = -1;
		*pChannel = -1;

		int index = 0;

		for (size_t b = 0; b < m_buffers.Num(); ++b)
		{
			for (size_t c = 0; c < m_buffers[b].m_channels.Num(); ++c)
			{
				if (m_buffers[b].m_channels[c].m_semantic == semantic)
				{
					if (index == semanticIndex)
					{
						*pBuffer = int(b);
						*pChannel = int(c);
						return;
					}
					else
					{
						++index;
					}
				}
			}
		}
	}


	//---------------------------------------------------------------------------------------------
	int32 FMeshBufferSet::GetElementSize(int32 buffer) const
	{
		check(buffer >= 0 && buffer < m_buffers.Num());

		return m_buffers[buffer].m_elementSize;
	}


	//---------------------------------------------------------------------------------------------
	int32 FMeshBufferSet::GetChannelOffset
	(
		int32 buffer,
		int32 channel
	) const
	{
		check(buffer >= 0 && buffer < m_buffers.Num());
		check(channel >= 0 && channel < m_buffers[buffer].m_channels.Num());

		return m_buffers[buffer].m_channels[channel].m_offset;
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::AddBuffer(const FMeshBufferSet& Other, int32 buffer)
	{
		check(GetElementCount() == Other.GetElementCount());

		LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

		m_buffers.Add(Other.m_buffers[buffer]);
	}


	//---------------------------------------------------------------------------------------------
	void FMeshBufferSet::RemoveBuffer(int32 BufferIndex)
	{
		if (BufferIndex >= 0 && BufferIndex < m_buffers.Num())
		{
			m_buffers.RemoveAt(BufferIndex);
		}
		else
		{
			check(false);
		}
	}


	//---------------------------------------------------------------------------------------------
	bool FMeshBufferSet::HasSameFormat(const FMeshBufferSet& Other) const
	{
		int32 bc = GetBufferCount();
		if (Other.GetBufferCount() != bc)
		{
			return false;
		}

		for (int32 b = 0; b < bc; ++b)
		{
			if (!HasSameFormat(b, Other))
			{
				return false;
			}
		}

		return true;
	}


	//---------------------------------------------------------------------------------------------
	bool FMeshBufferSet::HasSameFormat(int32 buffer, const FMeshBufferSet& Other) const
	{
		if (m_buffers[buffer].m_channels == Other.m_buffers[buffer].m_channels
			&&
			m_buffers[buffer].m_elementSize == Other.m_buffers[buffer].m_elementSize
			)
		{
			return true;
		}

		return false;
	}


	//---------------------------------------------------------------------------------------------
	size_t FMeshBufferSet::GetDataSize() const
	{
		size_t res = 0;

		for (int32 b = 0; b < m_buffers.Num(); ++b)
		{
			res += m_buffers[b].m_elementSize * m_elementCount;
		}

		return res;
	}


	//-----------------------------------------------------------------------------------------
	void FMeshBufferSet::CopyElement(uint32_t fromIndex, uint32_t toIndex)
	{
		check(fromIndex < m_elementCount);
		check(toIndex < m_elementCount);

		if (fromIndex != toIndex)
		{
			for (auto& b : m_buffers)
			{
				FMemory::Memcpy(&b.m_data[b.m_elementSize * toIndex],
					&b.m_data[b.m_elementSize * fromIndex],
					b.m_elementSize);
			}
		}
	}


	//-----------------------------------------------------------------------------------------
	bool FMeshBufferSet::IsSpecialBufferToIgnoreInSimilar(const MESH_BUFFER& b) const
	{
		if (b.m_channels.Num() == 1
			&&
			b.m_channels[0].m_semantic == MBS_VERTEXINDEX)
		{
			return true;
		}
		if (b.m_channels.Num() == 1
			&&
			b.m_channels[0].m_semantic == MBS_LAYOUTBLOCK)
		{
			return true;
		}
		return false;
	}

	//-----------------------------------------------------------------------------------------
	bool FMeshBufferSet::IsSimilarRobust(const FMeshBufferSet& Other, bool bCompareUVs) const
	{
		MUTABLE_CPUPROFILER_SCOPE(FMeshBufferSet::IsSimilarRobust);

		if (m_elementCount != Other.m_elementCount)
		{
			return false;
		}

		// IsSimilar is mush faster but can give false negatives if the buffer data description 
		// omits parts of the data (e.g, memory layout paddings). It cannot have false positives. 
		if (IsSimilar(Other))
		{
			return true;
		}

		const int32 ThisNumBuffers = m_buffers.Num();
		const int32 OtherNumBuffers = Other.m_buffers.Num();

		int32 I = 0, J = 0;
		while (I < ThisNumBuffers && J < OtherNumBuffers)
		{
			if (IsSpecialBufferToIgnoreInSimilar(m_buffers[I]))
			{
				++I;
				continue;
			}

			if (IsSpecialBufferToIgnoreInSimilar(Other.m_buffers[J]))
			{
				++J;
				continue;
			}

			const int32 ThisNumChannels = m_buffers[I].m_channels.Num();
			const int32 OtherNumChannels = m_buffers[J].m_channels.Num();

			const MESH_BUFFER& ThisBuffer = m_buffers[I];
			const MESH_BUFFER& OtherBuffer = Other.m_buffers[J];

			if (!(ThisBuffer.m_channels == OtherBuffer.m_channels && ThisBuffer.m_elementSize==OtherBuffer.m_elementSize))
			{
				return false;
			}

			for (uint32 Elem = 0; Elem < m_elementCount; ++Elem)
			{
				for (int32 C = 0; C < ThisNumChannels; ++C)
				{
					if (!bCompareUVs && ThisBuffer.m_channels[C].m_semantic==MBS_TEXCOORDS)
					{						
						continue;
					}

					const SIZE_T SizeA = GetMeshFormatData(ThisBuffer.m_channels[C].m_format).m_size * ThisBuffer.m_channels[C].m_componentCount;
					const SIZE_T SizeB = GetMeshFormatData(OtherBuffer.m_channels[C].m_format).m_size * OtherBuffer.m_channels[C].m_componentCount;

					if (SizeA != SizeB)
					{
						return false;
					}

					const uint8* BuffA = ThisBuffer.m_data.GetData() + Elem * ThisBuffer.m_elementSize + ThisBuffer.m_channels[C].m_offset;
					const uint8* BuffB = OtherBuffer.m_data.GetData() + Elem * OtherBuffer.m_elementSize + OtherBuffer.m_channels[C].m_offset;

					if (FMemory::Memcmp(BuffA, BuffB, SizeA) != 0)
					{
						return false;
					}
				}
			}

			++J;
			++I;
		}

		// Whatever buffers are left should be irrelevant
		while (I < ThisNumBuffers)
		{
			if (!IsSpecialBufferToIgnoreInSimilar(m_buffers[I]))
			{
				return false;
			}
			++I;
		}

		while (J < OtherNumBuffers)
		{
			if (!IsSpecialBufferToIgnoreInSimilar(Other.m_buffers[J]))
			{
				return false;
			}
			++J;
		}

		return true;
	}

	//-----------------------------------------------------------------------------------------
	bool FMeshBufferSet::IsSimilar(const FMeshBufferSet& Other) const
	{
		MUTABLE_CPUPROFILER_SCOPE(FMeshBufferSet::IsSimilar);

		if (m_elementCount != Other.m_elementCount) return false;

		// Compare all buffers except the vertex index channel, which should always be alone in
		// the last buffer
		int32 Index = 0;
		int32 OtherIndex = 0;

		const int32 ThisNumBuffers = m_buffers.Num();
		const int32 OtherNumBuffers = Other.m_buffers.Num();

		while (Index < ThisNumBuffers && OtherIndex < OtherNumBuffers)
		{
			// Is it a special buffer that we should ignore?
			if (IsSpecialBufferToIgnoreInSimilar(m_buffers[Index]))
			{
				++Index;
				continue;
			}

			if (IsSpecialBufferToIgnoreInSimilar(Other.m_buffers[OtherIndex]))
			{
				++OtherIndex;
				continue;
			}

			if (!(m_buffers[Index] == Other.m_buffers[OtherIndex]))
			{
				return false;
			}
			++Index;
			++OtherIndex;
		}

		// Whatever buffers are left should be irrelevant
		while (Index < ThisNumBuffers)
		{
			if (!IsSpecialBufferToIgnoreInSimilar(m_buffers[Index]))
			{
				return false;
			}
			++Index;
		}

		while (OtherIndex < OtherNumBuffers)
		{
			if (!IsSpecialBufferToIgnoreInSimilar(Other.m_buffers[OtherIndex]))
			{
				return false;
			}
			++OtherIndex;
		}

		return true;
	}


	//-----------------------------------------------------------------------------------------
	void FMeshBufferSet::ResetBufferIndices()
	{
		uint8_t currentIndices[MBS_COUNT];
		memset(currentIndices, 0, sizeof(currentIndices));

		for (auto& b : m_buffers)
		{
			for (auto& c : b.m_channels)
			{
				c.m_semanticIndex = currentIndices[c.m_semantic];
				currentIndices[c.m_semantic]++;
			}
		}
	}


	//-----------------------------------------------------------------------------------------
	void FMeshBufferSet::UpdateOffsets(int32 b)
	{
		uint8_t offset = 0;
		for (auto& c : m_buffers[b].m_channels)
		{
			if (c.m_offset < offset)
			{
				c.m_offset = offset;
			}
			else
			{
				offset = c.m_offset;
			}
			offset += uint8_t(c.m_componentCount * GetMeshFormatData(c.m_format).m_size);
		}

		if (m_buffers[b].m_elementSize < offset)
			m_buffers[b].m_elementSize = offset;
	}

}