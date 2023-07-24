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
	inline MeshPtr MeshOptimizeSkinning(const Mesh* InMesh)
	{
		MUTABLE_CPUPROFILER_SCOPE(MeshOptimizeSkinning);

		if (!InMesh)
		{
			return nullptr;
		}

		// Prepare an array of BoneIndex to SkinnedBoneIndex to remap indices.
		TArray<uint16> RemapedBoneIndices;

		mu::SkeletonPtrConst Skeleton = InMesh->GetSkeleton();
		RemapedBoneIndices.AddZeroed(Skeleton->GetBoneCount());

		int32 SkinnedBonesCount = 0;

		const int32 BonePoseCount = InMesh->GetBonePoseCount();
		for (int32 BonePoseIndex = 0; BonePoseIndex < BonePoseCount; ++BonePoseIndex)
		{
			if (EnumHasAnyFlags(InMesh->BonePoses[BonePoseIndex].BoneUsageFlags, EBoneUsageFlags::Skinning))
			{
				const int32 SkeletonBoneIndex = Skeleton->FindBone(InMesh->BonePoses[BonePoseIndex].BoneName.c_str());
				RemapedBoneIndices[SkeletonBoneIndex] = SkinnedBonesCount;
				++SkinnedBonesCount;
			}
		}

		// Find the desired bone index format
		const MESH_BUFFER_FORMAT DesiredBoneIndexFormat = SkinnedBonesCount < MAX_uint8 ? mu::MBF_UINT8 : mu::MBF_UINT16;

		const mu::FMeshBufferSet& SourceVertexBuffers = InMesh->GetVertexBuffers();

		const int32 VertexBuffersCount = SourceVertexBuffers.GetBufferCount();
		const int32 ElementCount = SourceVertexBuffers.GetElementCount();

		// Clone mesh without VertexBuffers, they will be copied manually.
		MeshPtr Result = InMesh->Clone(~(EMeshCloneFlags::WithVertexBuffers));

		mu::FMeshBufferSet& VertexBuffers = Result->GetVertexBuffers();

		VertexBuffers.m_buffers.Reserve(VertexBuffersCount);
		VertexBuffers.SetElementCount(ElementCount);

		for (int32 BufferIndex = 0; BufferIndex < VertexBuffersCount; ++BufferIndex)
		{
			const MESH_BUFFER& SourceBuffer = SourceVertexBuffers.m_buffers[BufferIndex];
			const int32 ChannelsCount = SourceVertexBuffers.GetBufferChannelCount(BufferIndex);

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
					Format = DesiredBoneIndexFormat;
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
			if (BoneIndexChannelIndex != INDEX_NONE)
			{
				// This buffer has a BoneIndex channel that needs to be manually fixed. 
				VertexBuffers.SetBufferCount(BufferIndex + 1);
				VertexBuffers.SetBuffer(BufferIndex, ElementSize, ChannelsCount, Semantics.GetData(), SemanticIndices.GetData(), Formats.GetData(), Components.GetData(), Offsets.GetData());
				uint8* Data = VertexBuffers.GetBufferData(BufferIndex);

				const uint8* SourceData = SourceBuffer.m_data.GetData();
				const int32 SourceBoneIndexSize = GetMeshFormatData(SourceBoneIndexFormat).m_size;

				const int32 BoneIndexSize = GetMeshFormatData(DesiredBoneIndexFormat).m_size;
				const int32 BoneIndexComponentCount = Components[BoneIndexChannelIndex];
				const int32 BoneIndexChannelOffset = Offsets[BoneIndexChannelIndex];

				const int32 TailSize = AuxOffset - (BoneIndexChannelOffset + BoneIndexComponentCount * BoneIndexSize);

				for (int32 VertexIndex = 0; VertexIndex < ElementCount; ++VertexIndex)
				{
					FMemory::Memcpy(Data, SourceData, BoneIndexChannelOffset);
					Data += BoneIndexChannelOffset;
					SourceData += BoneIndexChannelOffset;

					for (int32 ComponentIndex = 0; ComponentIndex < BoneIndexComponentCount; ++ComponentIndex)
					{
						switch (DesiredBoneIndexFormat)
						{
						case MESH_BUFFER_FORMAT::MBF_UINT8:
						{
							switch (SourceBoneIndexFormat)
							{
							case MESH_BUFFER_FORMAT::MBF_UINT8:
							{
								int32 SourceIndex = *SourceData;
								if (SourceIndex < RemapedBoneIndices.Num())
								{
									*Data = (uint8)RemapedBoneIndices[SourceIndex];
								}
								break;
							}
							case MESH_BUFFER_FORMAT::MBF_UINT16:
							{
								int32 SourceIndex = *((uint16*)SourceData);
								if (SourceIndex< RemapedBoneIndices.Num())
								{
									*Data = (uint8)RemapedBoneIndices[SourceIndex];
								}
								break;
							}
							case MESH_BUFFER_FORMAT::MBF_INT32:
							{
								int32 SourceIndex = *((int32*)SourceData);
								if (SourceIndex < RemapedBoneIndices.Num())
								{
									*Data = (uint8)RemapedBoneIndices[SourceIndex];
								}
								break;
							}
							default:
								unimplemented();
								break;
							}
							break;
						}
						case MESH_BUFFER_FORMAT::MBF_UINT16:
						{
							check(SourceBoneIndexFormat == MESH_BUFFER_FORMAT::MBF_UINT16);
							*((uint16*)Data) = RemapedBoneIndices[*((uint16*)SourceData)];
							break;
						}
						default:
							unimplemented();
							break;
						}

						Data += BoneIndexSize;
						SourceData += SourceBoneIndexSize;
					}


					FMemory::Memcpy(Data, SourceData, TailSize);
					Data += TailSize;
					SourceData += TailSize;
				}
			}
			else
			{
				// Add buffers that don't require a fix up
				VertexBuffers.AddBuffer(SourceVertexBuffers, BufferIndex);
			}

		}

		return Result;
	}
}
