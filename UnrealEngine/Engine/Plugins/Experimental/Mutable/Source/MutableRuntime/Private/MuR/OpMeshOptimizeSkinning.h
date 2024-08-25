// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MeshPrivate.h"
#include "MuR/ConvertData.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	inline void MeshOptimizeSkinning(Mesh* Result, const Mesh* InMesh, bool& bOutSuccess)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshOptimizeSkinning);

		bOutSuccess = true;

		if (!InMesh)
		{
			bOutSuccess = false;
			return;
		}

		uint32 MaxBoneMapIndex = 0;
		for (const MESH_SURFACE& Surface : InMesh->m_surfaces)
		{
			MaxBoneMapIndex = FMath::Max(MaxBoneMapIndex, Surface.BoneMapCount);
		}

		// We can't optimize the skinning if the mesh requires 16 bit bone indices 
		if (MaxBoneMapIndex > MAX_uint8)
		{
			bOutSuccess = false;
			return;
		}

		bool bRequiresFormatChange = false;

		// Desired format if MaxBoneMapIndex <= MAX_uint8
		const MESH_BUFFER_FORMAT DesiredBoneIndexFormat = mu::MBF_UINT8;
		
		// Iterate all vertex buffers and check if the BoneIndices buffers have the desired format
		const FMeshBufferSet& InMeshVertexBuffers = InMesh->GetVertexBuffers();
		for (int32 VertexBufferIndex = 0; !bRequiresFormatChange && VertexBufferIndex < InMeshVertexBuffers.m_buffers.Num(); ++VertexBufferIndex)
		{
			const MESH_BUFFER& Buffer = InMeshVertexBuffers.m_buffers[VertexBufferIndex];

			const int32 ChannelsCount = InMeshVertexBuffers.GetBufferChannelCount(VertexBufferIndex);
			for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
			{
				if (Buffer.m_channels[ChannelIndex].m_semantic == MBS_BONEINDICES)
				{
					bRequiresFormatChange = Buffer.m_channels[ChannelIndex].m_format != DesiredBoneIndexFormat;
					break;
				}
			}
		}

		// Not sure if bRequiresFormatChange will ever be true.
		if (!bRequiresFormatChange)
		{
			bOutSuccess = false;
			return;
		}

		MUTABLE_CPUPROFILER_SCOPE(MeshOptimizeSkinning_Format);

		// Format Bone Indices. For some reason, the bone index format is MBF_UINT16 and it should be MBF_UINT8.

		// TODO: Replace by MeshFormatInPlace once implemented
		const int32 VertexBuffersCount = InMeshVertexBuffers.GetBufferCount();
		const int32 ElementCount = InMeshVertexBuffers.GetElementCount();

		// Clone mesh without VertexBuffers, they will be copied manually.
		constexpr EMeshCopyFlags CopyFlags = ~EMeshCopyFlags::WithVertexBuffers;
		Result->CopyFrom(*InMesh, CopyFlags);

		mu::FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();

		VertexBuffers.m_buffers.Reserve(VertexBuffersCount);
		VertexBuffers.SetElementCount(ElementCount);

		for (int32 BufferIndex = 0; BufferIndex < VertexBuffersCount; ++BufferIndex)
		{
			const MESH_BUFFER& SourceBuffer = InMeshVertexBuffers.m_buffers[BufferIndex];
			const int32 ChannelsCount = InMeshVertexBuffers.GetBufferChannelCount(BufferIndex);

			int32 BoneIndexChannelIndex = INDEX_NONE;

			int32 ElementSize = 0;
			TArray<MESH_BUFFER_SEMANTIC> Semantics;
			TArray<int32> SemanticIndices;
			TArray<MESH_BUFFER_FORMAT> Formats;
			TArray<int32> Components;
			TArray<int32> Offsets;

			MESH_BUFFER_FORMAT SourceBoneIndexFormat = mu::MBF_NONE;

			// Offset accumulator
			int32 AuxOffset = 0;

			// Copy and fix channel details
			for (int32 ChannelIndex = 0; ChannelIndex < ChannelsCount; ++ChannelIndex)
			{
				const MESH_BUFFER_CHANNEL& Channel = SourceBuffer.m_channels[ChannelIndex];
				MESH_BUFFER_FORMAT Format = Channel.m_format;

				if (Channel.m_semantic == MBS_BONEINDICES)
				{
					SourceBoneIndexFormat = Channel.m_format;
					Format = mu::MBF_UINT8;
					BoneIndexChannelIndex = ChannelIndex;
				}

				Semantics.Add(Channel.m_semantic);
				SemanticIndices.Add(Channel.m_semanticIndex);
				Formats.Add(Format);
				Components.Add(Channel.m_componentCount);
				Offsets.Add(AuxOffset);

				const int32 FormatSize = GetMeshFormatData(Format).m_size;

				ElementSize += FormatSize;
				AuxOffset += FormatSize * Channel.m_componentCount;
			}

			// Copy buffers
			if (BoneIndexChannelIndex != INDEX_NONE && SourceBoneIndexFormat == mu::MBF_UINT16)
			{
				// This buffer has a BoneIndex channel that needs to be manually fixed. 
				VertexBuffers.SetBufferCount(BufferIndex + 1);
				VertexBuffers.SetBuffer(BufferIndex, ElementSize, ChannelsCount, Semantics.GetData(), SemanticIndices.GetData(), Formats.GetData(), Components.GetData(), Offsets.GetData());
				uint8* Data = VertexBuffers.GetBufferData(BufferIndex);

				const uint8* SourceData = (const uint8*)SourceBuffer.m_data.GetData();
				const int32 SourceBoneIndexSize = GetMeshFormatData(SourceBoneIndexFormat).m_size;

				const int32 BoneIndexSize = GetMeshFormatData(mu::MBF_UINT8).m_size;
				const int32 BoneIndexComponentCount = Components[BoneIndexChannelIndex];
				const int32 BoneIndexChannelOffset = Offsets[BoneIndexChannelIndex];

				const int32 TailSize = AuxOffset - (BoneIndexChannelOffset + BoneIndexComponentCount * BoneIndexSize);


				for (const MESH_SURFACE& Surface : InMesh->m_surfaces)
				{
					const uint32 NumBonesInBoneMap = Surface.BoneMapCount;

					for (int32 VertexIndex = 0; VertexIndex < Surface.m_vertexCount; ++VertexIndex)
					{
						FMemory::Memcpy(Data, SourceData, BoneIndexChannelOffset);
						Data += BoneIndexChannelOffset;
						SourceData += BoneIndexChannelOffset;

						for (int32 ComponentIndex = 0; ComponentIndex < BoneIndexComponentCount; ++ComponentIndex)
						{
							const uint16 SourceIndex = *((const uint16*)SourceData);
							if (SourceIndex < NumBonesInBoneMap)
							{
								*Data = (uint8)SourceIndex;
							}
							else
							{
								*Data = 0;
							}
							
							Data += BoneIndexSize;
							SourceData += SourceBoneIndexSize;
						}


						FMemory::Memcpy(Data, SourceData, TailSize);
						Data += TailSize;
						SourceData += TailSize;
					}
				}
			}
			else
			{
				// SourceBoneIndexFormat must be mu::MBF_UINT8 or none if the buffer doesn't have BoneIndices
				check(SourceBoneIndexFormat == mu::MBF_NONE || SourceBoneIndexFormat == mu::MBF_UINT8);

				// Add buffers that don't require a fix up
				VertexBuffers.AddBuffer(InMeshVertexBuffers, BufferIndex);
			}

		}
	}
}
